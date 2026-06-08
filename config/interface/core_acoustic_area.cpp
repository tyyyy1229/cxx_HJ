#include <iostream>
#include <cmath>
#include <algorithm>

#include "core_acoustic_area.h"
#include "core_acoustic_internal.h"
#include "nc_process2.h"

namespace CoreAcoustic {

using namespace Internal;

// ==========================================
// 工具
// ==========================================

static int computeNumRangePts(double maxRange, double intervalKm) {
    double maxRangeKm = maxRange / 1000.0;
    return std::max(1, static_cast<int>(maxRangeKm / intervalKm) + 1);
}

// ==========================================
// 扁平寻址
// ==========================================

// 布局: [lon][lat][dir][range][depth], 最快变化维 = depth
static inline size_t flatIndexArea(int lonIdx, int latIdx, int dirIdx, int rIdx, int dIdx,
                                   int numLat, int numDir, int numRange, int numDepth) {
    return ((((static_cast<size_t>(lonIdx) * numLat + latIdx) * numDir + dirIdx)
             * numRange + rIdx) * numDepth + dIdx);
}

// ==========================================
// computeAreaTL
// ==========================================

AreaAcousticResult computeAreaTL(const AreaAcousticConfig& cfg)
{
    AreaAcousticResult result;

    // ---- 1. 网格点 ----
    int nLon = std::max(1, static_cast<int>(std::round(
        (cfg.area.maxLon - cfg.area.minLon) / cfg.lonlatInterval)) + 1);
    int nLat = std::max(1, static_cast<int>(std::round(
        (cfg.area.maxLat - cfg.area.minLat) / cfg.lonlatInterval)) + 1);

    result.lonGrid.resize(nLon);
    result.latGrid.resize(nLat);
    for (int i = 0; i < nLon; ++i) result.lonGrid[i] = cfg.area.minLon + i * cfg.lonlatInterval;
    for (int j = 0; j < nLat; ++j) result.latGrid[j] = cfg.area.minLat + j * cfg.lonlatInterval;

    // ---- 2. NC 缓存 ----
    NcProcessConfig ncCfg;
    ncCfg.bty_nc = cfg.topographyPath;
    ncCfg.ssp_nc = cfg.soundSpeedPath;
    ncCfg.sed_nc = cfg.sedimentTypePath;
    ncCfg.r_max   = cfg.maxRange;
    int numRangePts = computeNumRangePts(cfg.maxRange, cfg.receiveRangeInterval);
    ncCfg.num_pts  = numRangePts;

    NcEnvCache cache = loadNcEnvCache(ncCfg);
    if (!cache.loaded) {
        std::cerr << "[Area] NC 缓存加载失败。" << std::endl;
        return result;
    }
    std::cout << "--------------------------------------------------------" << std::endl;

    // ---- 2b. 底质查表 TXT ----
    SedimentTable sedTable = readSedimentTable(cfg.sedimentTablePath);
    std::cout << "--------------------------------------------------------" << std::endl;

    // ---- 3. 接收深度 ----
    // 生成 set_RD 的深度网格: linspace(0, rdMax, rdNum)
    int    rdNum      = std::max(2, cfg.rdNum);
    double rdStep     = cfg.rdMax / (rdNum - 1);  // 深度网格间距 (米)
    std::vector<double> rdList(rdNum);
    for (int i = 0; i < rdNum; ++i) rdList[i] = i * rdStep;

    // 输出过滤: receiveDepth 值(米) ÷ rdStep → 行索引
    std::vector<int> depthRows;
    if (!cfg.receiveDepth.empty()) {
        for (double val : cfg.receiveDepth) {
            int idx = static_cast<int>(std::round(val / rdStep));
            if (idx < 0) idx = 0;
            if (idx >= rdNum) idx = rdNum - 1;
            depthRows.push_back(idx);
        }
        std::cout << "[Area] receiveDepth 过滤: " << depthRows.size()
                  << " / " << rdNum << " 层" << std::endl;
    } else {
        depthRows.resize(rdNum);
        for (int i = 0; i < rdNum; ++i) depthRows[i] = i;
        std::cout << "[Area] receiveDepth 输出全 " << rdNum << " 层" << std::endl;
    }
    int numOutDepth = static_cast<int>(depthRows.size());

    // 输出深度轴 = 用户指定值, 或完整网格
    std::vector<double> outDepth(numOutDepth);
    for (int i = 0; i < numOutDepth; ++i)
        outDepth[i] = depthRows[i] * rdStep;

    // ---- 4. 方位角 ----
    std::vector<double> azimuthAngles = generateAzimuthAngles(cfg.directionNum);
    int numDir   = cfg.directionNum;
    double maxRangeKm = cfg.maxRange / 1000.0;
    double rBox = maxRangeKm;

    // ---- 5. 输出初始化 ----
    result.numLon        = nLon;
    result.numLat        = nLat;
    result.numDir        = numDir;
    result.numDepth      = numOutDepth;
    result.numRange      = numRangePts;
    result.receiverDepth = outDepth;
    result.receiverRange = linspace(0.0, maxRangeKm, numRangePts);
    result.azimuthAngles = azimuthAngles;
    size_t totalSize = static_cast<size_t>(nLon) * nLat * numDir * numOutDepth * numRangePts;
    result.tlData.assign(totalSize, 0.0);

    // ---- 6. 遍历网格 ----
    int progressCount = 0;

for (int lonIdx = 0; lonIdx < nLon; ++lonIdx) {
        for (int latIdx = 0; latIdx < nLat; ++latIdx) {

            double srcLon = result.lonGrid[lonIdx];
            double srcLat = result.latGrid[latIdx];

            LocalEnvGrid grid = buildLocalEnvGrid(cache, srcLon, srcLat, cfg.maxRange);
            if (!grid.built) {
std::cerr << "[Area] 网格构建失败 @ (" << srcLon << "," << srcLat << ")" << std::endl;
                continue;
            }

            for (int dirIdx = 0; dirIdx < numDir; ++dirIdx) {
                double alphaClient   = azimuthAngles[dirIdx];
                double thetaInternal = clientAzimuthToInternalTheta(alphaClient);

                NcProcessConfig sliceCfg = ncCfg;
                sliceCfg.lon_0 = srcLon;
                sliceCfg.lat_0 = srcLat;
                NcProcessResult slice = extractSliceFromGrid(grid, thetaInternal, sliceCfg);

                if (!slice.success || slice.sspList.empty()) {
std::cerr << "[Area] 切片失败 @ " << srcLon << "," << srcLat
                              << " dir=" << alphaClient << "°" << std::endl;
                    continue;
                }

                // ---- 逐点底质查表: sed_val → TXT查表 → btyPts.alphaR/I/Rho ----
                {
                    int hitCount = 0, missCount = 0, invalidCount = 0;
                    size_t nPts = std::min(slice.btyPts.size(), slice.sedVals.size());
                    for (size_t pi = 0; pi < nPts; ++pi) {
                        float rawVal = slice.sedVals[pi];
                        if (!std::isfinite(rawVal) || rawVal <= -9990.0f) {
                            // 无效底质值 → fallback 到 cfg 默认值
                            slice.btyPts[pi].alphaR = static_cast<float>(cfg.bottomAlphaR);
                            slice.btyPts[pi].alphaI = static_cast<float>(cfg.bottomAlphaI);
                            slice.btyPts[pi].rho    = static_cast<float>(cfg.bottomRho);
                            ++invalidCount;
                            continue;
                        }
                        int type = static_cast<int>(std::round(rawVal));
                        auto it = sedTable.find(type);
                        if (it != sedTable.end()) {
                            slice.btyPts[pi].alphaR = it->second.alphaR;
                            slice.btyPts[pi].alphaI = it->second.alphaI;
                            slice.btyPts[pi].rho    = it->second.rho;
                            ++hitCount;
                        } else {
                            slice.btyPts[pi].alphaR = static_cast<float>(cfg.bottomAlphaR);
                            slice.btyPts[pi].alphaI = static_cast<float>(cfg.bottomAlphaI);
                            slice.btyPts[pi].rho    = static_cast<float>(cfg.bottomRho);
                            ++missCount;
                        }
                    }
                    std::cout << "[Sediment] src=(" << srcLon << "," << srcLat
                              << ") dir=" << alphaClient << "°: "
                              << nPts << "点, 命中=" << hitCount
                              << " 未命中=" << missCount
                              << " 无效=" << invalidCount;
                    if (hitCount > 0) {
                        // 打印第一个命中点的参数供参考
                        for (size_t pi = 0; pi < nPts; ++pi) {
                            if (slice.btyPts[pi].alphaR > 0.1f) {
                                std::cout << " | 首点 type≈" << static_cast<int>(std::round(slice.sedVals[pi]))
                                          << " alphaR=" << slice.btyPts[pi].alphaR
                                          << " rho=" << slice.btyPts[pi].rho
                                          << " alphaI=" << slice.btyPts[pi].alphaI;
                                break;
                            }
                        }
                    }
                    std::cout << std::endl;
                }

                bellhopParam parm;
                if (cfg.isCoherent) {
                    parm.RunType->firstVal->Coherent_TL_C();
                } else {
                    parm.RunType->firstVal->Incoherent_TL_I();
                }
                parm.RunType->secondVal->Gaussian_beam_B();
                parm.RunType->thirdVal->NoneOption();
                parm.RunType->forthVal->NoneOption();
                parm.RunType->fifthVal->NoneOption();

                configureCommonBellhopParams(parm,
                    cfg.freq, cfg.isCoherent,
                    cfg.beamAngle.angleUp, cfg.beamAngle.angleDown,
                    cfg.beamNumber,
                    cfg.sourceDepth, rdList,     // set_RD(rdMax, rdNum)
                    cfg.maxRange, numRangePts, rBox,
                    slice,
                    cfg.bottomAlphaR, cfg.bottomAlphaI,
                    cfg.bottomBetaR, cfg.bottomBetaI,
                    cfg.bottomRho, cfg.bottomDepth);
                parm.runMod();

                auto tl2D = parm.get_TLField();

{
                    // tl2D: [numOutDepth][numRange], 输出布局: [lon][lat][dir][range][depth]
                    for (int rIdx = 0; rIdx < numRangePts; ++rIdx) {
                        for (int outD = 0; outD < numOutDepth; ++outD) {
                            int levRow = depthRows[outD];
                            if (static_cast<size_t>(levRow) >= tl2D.size()) break;
                            const auto& row = tl2D[levRow];
                            if (static_cast<size_t>(rIdx) >= row.size()) break;
                            size_t idx = flatIndexArea(lonIdx, latIdx, dirIdx, rIdx, outD,
                                                       nLat, numDir, numRangePts, numOutDepth);
                            result.tlData[idx] = static_cast<double>(row[rIdx]);
                        }
                    }
                }
            } // dirIdx

++progressCount;
            if (progressCount % 10 == 0 || progressCount == nLon * nLat) {
std::cout << "[Area] 进度: " << progressCount << "/" << (nLon * nLat)
                          << " 网格点完成" << std::endl;
            }
        } // latIdx
    } // lonIdx

    std::cout << "[Area] 完成。TL 总量: " << totalSize << std::endl;
    return result;
}

} // namespace CoreAcoustic
