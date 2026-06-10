#include <iostream>
#include <cmath>
#include <algorithm>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "core_acoustic_single.h"
#include "core_acoustic_internal.h"
#include "nc_process2.h"

namespace CoreAcoustic {

using namespace Internal;

static int computeNumRangePts(double maxRange, double intervalKm) {
    double maxRangeKm = maxRange / 1000.0;
    return std::max(1, static_cast<int>(maxRangeKm / intervalKm) + 1);
}

static std::vector<double> generateDepthFromInterval(double interval, double maxDepth) {
    std::vector<double> depths;
    if (interval <= 0.0 || maxDepth <= 0.0) return depths;
    for (double d = 0.0; d <= maxDepth + 1e-9; d += interval)
        depths.push_back(d);
    return depths;
}

static inline size_t flatIndexTL(int dirIdx, int dIdx, int rIdx,
                                 int numDepth, int numRange) {
    return (static_cast<size_t>(dirIdx) * numDepth + dIdx) * numRange + rIdx;
}

// ==========================================
// computeSingleTL
// ==========================================

SingleAcousticResult computeSingleTL(const SingleAcousticConfig& cfg)
{
    SingleAcousticResult result;

    double srcLon = cfg.point.lon;
    double srcLat = cfg.point.lat;
    std::cout << "\n========== [Single] 入口: (" << srcLon << ", " << srcLat << ") ==========" << std::endl;

    // ---- 1. NC 缓存 ----
    std::cout << "[Single-1] 加载 NC 缓存..."
              << " ssp=" << cfg.soundSpeedPath
              << " bty=" << cfg.topographyPath
              << " sed=" << cfg.sedimentTypePath << std::endl;
    NcProcessConfig ncCfg;
    ncCfg.bty_nc = cfg.topographyPath;
    ncCfg.ssp_nc = cfg.soundSpeedPath;
    ncCfg.sed_nc = cfg.sedimentTypePath;
    ncCfg.r_max   = cfg.maxRange;
    int numRangePts = computeNumRangePts(cfg.maxRange, cfg.receiveRangeInterval);
    ncCfg.num_pts  = numRangePts;
    std::cout << "[Single-1] maxRange=" << cfg.maxRange << "m"
              << " receiveRangeInterval=" << cfg.receiveRangeInterval << "km"
              << " → numRangePts=" << numRangePts << std::endl;

    NcEnvCache cache = loadNcEnvCache(ncCfg);
    if (!cache.loaded) {
        std::cerr << "[Single] NC 缓存加载失败。" << std::endl;
        return result;
    }
    std::cout << "[Single-1] NC 加载完成, lev=" << cache.ssp.lev.size() << "层"
              << " lev范围=[" << cache.ssp.lev.front() << "," << cache.ssp.lev.back() << "]m"
              << std::endl;

    // ---- 1b. 底质查表 TXT ----
    SedimentTable sedTable = readSedimentTable(cfg.sedimentTablePath);
    std::cout << "--------------------------------------------------------" << std::endl;

    // ---- 2. 深度轴 ----
    std::cout << "[Single-2] 确定接收深度: receiveDepth.empty=" << cfg.receiveDepth.empty()
              << " receiveDepthInterval=" << cfg.receiveDepthInterval << std::endl;
    std::vector<double> receiveDepth;
    if (!cfg.receiveDepth.empty()) {
        receiveDepth = cfg.receiveDepth;
        std::cout << "[Single-2] 使用用户指定 receiveDepth: " << receiveDepth.size() << "层" << std::endl;
    } else if (cfg.receiveDepthInterval > 0.0 && !cache.ssp.lev.empty()) {
        double maxLev = cache.ssp.lev.back();
        receiveDepth = generateDepthFromInterval(cfg.receiveDepthInterval, maxLev);
        std::cout << "[Single-2] 按间隔生成: " << receiveDepth.size() << "层 (0~" << maxLev
                  << "m 每" << cfg.receiveDepthInterval << "m)" << std::endl;
    } else {
        receiveDepth.assign(cache.ssp.lev.begin(), cache.ssp.lev.end());
        std::cout << "[Single-2] 从 NC lev 拷贝: " << receiveDepth.size() << "层" << std::endl;
    }
    std::cout << "[Single-2] 接收深度前5: ";
    for (int i = 0; i < 5 && i < (int)receiveDepth.size(); ++i)
        std::cout << receiveDepth[i] << " ";
    std::cout << (receiveDepth.size() > 5 ? "..." : "") << std::endl;

    // ---- 3. 方位角 ----
    std::vector<double> azimuthAngles = generateAzimuthAngles(cfg.directionNum);
    int numDir   = cfg.directionNum;
    int numDepth = static_cast<int>(receiveDepth.size());
    double maxRangeKm = cfg.maxRange / 1000.0;
    double rBox = maxRangeKm;
    std::cout << "[Single-3] 方位: " << numDir << "个 (0°=北CW)"
              << " 深度: " << numDepth << "层"
              << " 距离: " << numRangePts << "点"
              << " rBox=" << rBox << "km" << std::endl;

    // ---- 4. 输出初始化 ----
    result.numDir        = numDir;
    result.numDepth      = numDepth;
    result.numRange      = numRangePts;
    result.receiverDepth = receiveDepth;
    result.receiverRange = linspace(0.0, maxRangeKm, numRangePts);
    result.azimuthAngles = azimuthAngles;
    result.tlData.assign(static_cast<size_t>(numDir) * numDepth * numRangePts, 0.0);
    result.topography.resize(numDir);
    if (cfg.isRayOutput) result.rayData.resize(numDir);
    std::cout << "[Single-4] 输出初始化: tlData=" << result.tlData.size() << " 个元素" << std::endl;

    // ---- 5. 构建局部网格（一次） ----
    std::cout << "[Single-5] 构建局部网格... src=(" << srcLon << "," << srcLat
              << ") r_max=" << cfg.maxRange << "m" << std::endl;
    LocalEnvGrid grid = buildLocalEnvGrid(cache, srcLon, srcLat, cfg.maxRange);
    if (!grid.built) {
        std::cerr << "[Single] 网格构建失败。" << std::endl;
        return result;
    }
    std::cout << "[Single-5] 网格完成: " << grid.Ny << "×" << grid.Nx
              << "×" << grid.lev_size << "层" << std::endl;

    // ---- 6. 遍历方向 ----
    std::cout << "[Single-6] 开始遍历 " << numDir << " 个方向..." << std::endl;
    for (int dirIdx = 0; dirIdx < numDir; ++dirIdx) {
        double alphaClient   = azimuthAngles[dirIdx];
        double thetaInternal = clientAzimuthToInternalTheta(alphaClient);
        std::cout << "\n--- [Single-6] dir[" << dirIdx << "/" << numDir
                  << "] client=" << alphaClient << "° internal=" << thetaInternal << "° ---" << std::endl;

        // 6a. 切片
        NcProcessConfig sliceCfg = ncCfg;
        sliceCfg.lon_0 = srcLon;
        sliceCfg.lat_0 = srcLat;
        std::cout << "[Single-6a] 提取切片..." << std::endl;
        NcProcessResult slice = extractSliceFromGrid(grid, thetaInternal, sliceCfg);

        if (!slice.success || slice.sspList.empty()) {
            std::cerr << "[Single-6a] ✗ 切片失败" << std::endl;
            continue;
        }
        std::cout << "[Single-6a] ✓ 切片: " << slice.sspList.size() << " SSP, "
                  << slice.btyPts.size() << " BTY"
                  << " maxBty=" << slice.maxBtyDepth
                  << " maxSsp=" << slice.maxSspDepth << std::endl;

        // 校验切片
        {
            int nanC = 0, infC = 0;
            float sspMin = 1e9, sspMax = -1e9;
            for (const auto& ssp : slice.sspList) {
                for (float v : ssp.cSSPV) {
                    if (!std::isfinite(v)) { if (std::isnan(v)) nanC++; else infC++; continue; }
                    if (v < sspMin) sspMin = v; if (v > sspMax) sspMax = v;
                }
            }
            std::cout << "[Single-6a] SSP值范围: [" << sspMin << ", " << sspMax << "]"
                      << " NaN=" << nanC << " Inf=" << infC << std::endl;

            float btyMin = 1e9, btyMax = -1e9;
            for (const auto& bty : slice.btyPts) {
                float v = bty.depth;
                if (!std::isfinite(v)) continue;
                if (v < btyMin) btyMin = v; if (v > btyMax) btyMax = v;
            }
            std::cout << "[Single-6a] BTY值范围: [" << btyMin << ", " << btyMax << "]m" << std::endl;

            if (nanC || infC)
                std::cerr << "[Single-6a] ⚠ SSP含NaN/Inf!" << std::endl;
        }

        // 6b. 地形采集
        {
            Topography& topo = result.topography[dirIdx];
            topo.topographyRange.reserve(slice.btyPts.size());
            topo.topographyDepth.reserve(slice.btyPts.size());
            for (const auto& bty : slice.btyPts) {
                topo.topographyRange.push_back(static_cast<double>(bty.x));
                topo.topographyDepth.push_back(static_cast<double>(bty.depth));
            }
            std::cout << "[Single-6b] 地形: " << topo.topographyRange.size() << " 点"
                      << " 范围[" << topo.topographyRange.front() << "," << topo.topographyRange.back() << "]km" << std::endl;
        }

        // 6b2. 逐点底质查表: sed_val → TXT查表 → btyPts.alphaR/I/Rho
        {
            int hitCount = 0, missCount = 0, invalidCount = 0;
            size_t nPts = std::min(slice.btyPts.size(), slice.sedVals.size());
            for (size_t pi = 0; pi < nPts; ++pi) {
                float rawVal = slice.sedVals[pi];
                if (!std::isfinite(rawVal) || rawVal <= -9990.0f) {
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
            std::cout << "[Sediment] dir=" << alphaClient << "°: "
                      << nPts << "点, 命中=" << hitCount
                      << " 未命中=" << missCount
                      << " 无效=" << invalidCount;
            if (hitCount > 0) {
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

        // 6c. TL 计算
        {
            // 截断适配：若切片被 10m 浅水截断，用实际射程计算，超出部分填 200
            bool wasTruncated = (!slice.btyPts.empty()
                                 && slice.btyPts.size() < static_cast<size_t>(numRangePts));
            double actualMaxRange = cfg.maxRange;
            int    actualNumPts   = numRangePts;
            double actualRBox     = rBox;
            if (wasTruncated) {
                actualMaxRange = slice.btyPts.back().x * 1000.0f;
                actualRBox     = actualMaxRange / 1000.0;
                actualNumPts   = std::max(2, static_cast<int>(actualMaxRange / 1000.0 / cfg.receiveRangeInterval) + 1);
                std::cout << "[Single-6c] 截断适配: maxRange " << cfg.maxRange << "→" << actualMaxRange
                          << "m, Npts " << numRangePts << "→" << actualNumPts << std::endl;
            }

            std::cout << "[Single-6c] 创建 bellhopParam..." << std::endl;
            bellhopParam parm;
            parm.RunType->firstVal->Coherent_TL_C();
            parm.RunType->secondVal->Gaussian_beam_B();
            parm.RunType->thirdVal->NoneOption();
            parm.RunType->forthVal->NoneOption();
            parm.RunType->fifthVal->NoneOption();

            std::cout << "[Single-6c] 配置参数..." << std::endl;
            configureCommonBellhopParams(parm,
                cfg.freq, cfg.isCoherent,
                cfg.beamAngle.angleUp, cfg.beamAngle.angleDown,
                cfg.beamNumber,
                cfg.sourceDepth, receiveDepth,
                actualMaxRange, actualNumPts, actualRBox,
                slice,
                cfg.bottomAlphaR, cfg.bottomAlphaI,
                cfg.bottomBetaR, cfg.bottomBetaI,
                cfg.bottomRho, cfg.bottomDepth);
            std::cout << "[Single-6c] 配置完成, 调用 runMod()..." << std::endl;

            parm.runMod();
            std::cout << "[Single-6c] runMod() 返回成功" << std::endl;

            auto tl2D = parm.get_TLField();
            if (wasTruncated) {
                for (auto& row : tl2D) row.resize(numRangePts, 200.0f);
            }
            std::cout << "[Single-6c] get_TLField: " << tl2D.size() << " 行";
            if (!tl2D.empty()) std::cout << " × " << tl2D[0].size() << " 列";
            if (wasTruncated) std::cout << " (截断填充200)";
            std::cout << std::endl;

            // 校验 TL
            int tlNan = 0, tlInf = 0;
            float tlMin = 1e9, tlMax = -1e9;
            for (const auto& row : tl2D) {
                for (float v : row) {
                    if (!std::isfinite(v)) { if (std::isnan(v)) tlNan++; else tlInf++; continue; }
                    if (v < tlMin) tlMin = v; if (v > tlMax) tlMax = v;
                }
            }
            std::cout << "[Single-6c] TL范围: [" << tlMin << ", " << tlMax << "]"
                      << " NaN=" << tlNan << " Inf=" << tlInf << std::endl;

            // 写入输出
            std::cout << "[Single-6c] 写入扁平数组... numDepth=" << numDepth
                      << " numRange=" << numRangePts << std::endl;
            for (int dIdx = 0; dIdx < numDepth; ++dIdx) {
                if (static_cast<size_t>(dIdx) >= tl2D.size()) break;
                const auto& row = tl2D[dIdx];
                for (int rIdx = 0; rIdx < numRangePts; ++rIdx) {
                    if (static_cast<size_t>(rIdx) >= row.size()) break;
                    size_t idx = flatIndexTL(dirIdx, dIdx, rIdx, numDepth, numRangePts);
                    result.tlData[idx] = static_cast<double>(row[rIdx]);
                }
            }
            std::cout << "[Single-6c] 写入完成" << std::endl;
        }

        // 6d. 声线 (可选)
        if (cfg.isRayOutput) {
            std::cout << "[Single-6d] 计算声线..." << std::endl;
            bellhopParam parm;
            parm.RunType->firstVal->Ray_trace_R();
            parm.RunType->secondVal->Geometric_beam_G();
            parm.RunType->thirdVal->NoneOption();
            parm.RunType->forthVal->Point_source_R();
            parm.RunType->fifthVal->Rectilinear_grid_R();

            configureCommonBellhopParams(parm,
                cfg.freq, cfg.isCoherent,
                cfg.beamAngle.angleUp, cfg.beamAngle.angleDown,
                cfg.beamNumber,
                cfg.sourceDepth, receiveDepth,
                cfg.maxRange, numRangePts, rBox,
                slice,
                cfg.bottomAlphaR, cfg.bottomAlphaI,
                cfg.bottomBetaR, cfg.bottomBetaI,
                cfg.bottomRho, cfg.bottomDepth);
            parm.runMod();

            std::vector<rayInfo> rays = parm.get_Ray();
            for (const auto& r : rays) {
                result.rayData[dirIdx].push_back(convertRayInfo(r));
            }
            std::cout << "[Single-6d] 声线: " << rays.size() << " 条" << std::endl;
        }

        std::cout << "[Single-6] dir[" << dirIdx << "] ✓ 完成" << std::endl;
    } // dirIdx

    std::cout << "\n========== [Single] ✓✓ 全部完成 ==========" << std::endl;
    std::cout << "TL: " << result.tlData.size() << " 值 ("
              << numDir << "×" << numDepth << "×" << numRangePts << ")" << std::endl;
    if (cfg.isRayOutput) {
        size_t totalRays = 0;
        for (auto& rd : result.rayData) totalRays += rd.size();
        std::cout << "声线: " << totalRays << " 条" << std::endl;
    }
    return result;
}

} // namespace CoreAcoustic
