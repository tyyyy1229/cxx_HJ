#include <cmath>
#include <algorithm>
#include "core_acoustic_internal.h"

namespace CoreAcoustic {
namespace Internal {

// ==========================================
// 方位角转换
// ==========================================

double clientAzimuthToInternalTheta(double alphaDeg) {
    // 客户: 0°=北 CW  →  内部: 0°=东 CCW
    return std::fmod(90.0 - alphaDeg + 360.0, 360.0);
}

std::vector<double> generateAzimuthAngles(int N) {
    std::vector<double> angles(N);
    double step = 360.0 / N;
    for (int i = 0; i < N; ++i) angles[i] = i * step;
    return angles;
}

// ==========================================
// 通用工具
// ==========================================

std::vector<double> linspace(double start, double end, int num) {
    std::vector<double> result(num);
    if (num <= 0) return result;
    if (num == 1) { result[0] = start; return result; }
    double step = (end - start) / (num - 1);
    for (int i = 0; i < num; ++i) result[i] = start + i * step;
    return result;
}

// ==========================================
// Bellhop 公共参数配置
// ==========================================

void configureCommonBellhopParams(
    bellhopParam& parm,
    double freq, bool isCoherent,
    double beamAngleUp, double beamAngleDown,
    int beamNumber,
    double sourceDepth,
    const std::vector<double>& receiveDepth,
    double maxRange, int numRangePts,
    double rBox,
    const NcProcessResult& slice,
    double bottomAlphaR, double bottomAlphaI,
    double bottomBetaR, double bottomBetaI,
    double bottomRho, double bottomDepth)
{
    // ---- 环境 ----
    std::vector<SSP> sspList = slice.sspList;
    float maxSspDepth = slice.maxSspDepth;
    float maxBtyDepth = slice.maxBtyDepth;
    float targetDepth  = maxBtyDepth * 1.05f;

    // ---- 每个剖面独立用斜率外推填充末尾 NaN ----
    if (!sspList.empty()) {
        bool anyFilled = false;
        for (auto& ssp : sspList) {
            size_t n = ssp.cSSPV.size();
            size_t valid = n;
            while (valid > 0 && !std::isfinite(ssp.cSSPV[valid - 1])) --valid;
            if (valid < 2) continue;   // 不足2个有效点，无法算斜率
            if (valid == n) continue;  // 无NaN，无需填充

            float z1 = ssp.zSSPV[valid - 2], c1 = ssp.cSSPV[valid - 2];
            float z2 = ssp.zSSPV[valid - 1], c2 = ssp.cSSPV[valid - 1];
            float slope = (c2 - c1) / (z2 - z1);

            for (size_t i = valid; i < n; ++i)
                ssp.cSSPV[i] = c2 + slope * (ssp.zSSPV[i] - z2);

            anyFilled = true;
        }
        if (anyFilled)
            std::cout << "[SSP填充] 各剖面独立斜率外推填充末尾NaN" << std::endl;
    }

    // SSP 深度层不足以覆盖海底时，线性外推一层
    if (maxSspDepth < maxBtyDepth && !sspList.empty()) {
        std::cout << "[SSP延伸] maxSsp=" << maxSspDepth << " maxBty=" << maxBtyDepth
                  << " target=" << targetDepth
                  << " → 每个剖面追加1层 (共" << sspList.size() << "个剖面)" << std::endl;

        int nanSkipCount = 0;
        for (auto& ssp : sspList) {
            size_t n = ssp.zSSPV.size();
            if (n < 2) {
                float c0 = (!ssp.cSSPV.empty() && std::isfinite(ssp.cSSPV.back()))
                               ? ssp.cSSPV.back() : 1500.0f;
                ssp.zSSPV.push_back(targetDepth);
                ssp.cSSPV.push_back(c0);
                continue;
            }
            float z1 = ssp.zSSPV[n - 2], c1 = ssp.cSSPV[n - 2];
            float z2 = ssp.zSSPV[n - 1], c2 = ssp.cSSPV[n - 1];

            // 跳过末尾 NaN
            for (int i = static_cast<int>(n) - 3;
                 i >= 0 && (!std::isfinite(c2) || !std::isfinite(c1)); --i) {
                if (!std::isfinite(c2)) { z2 = ssp.zSSPV[i + 1]; c2 = ssp.cSSPV[i + 1]; ++nanSkipCount; }
                if (!std::isfinite(c1)) { z1 = ssp.zSSPV[i];     c1 = ssp.cSSPV[i];     ++nanSkipCount; }
            }
            if (!std::isfinite(c2)) c2 = 1500.0f;
            if (!std::isfinite(c1)) { c1 = c2; z1 = z2 - 1.0f; }

            float slope = (c2 - c1) / (z2 - z1);
            if (!std::isfinite(slope)) slope = 0.0f;
            float c_new = c2 + slope * (targetDepth - z2);

            ssp.zSSPV.push_back(targetDepth);
            ssp.cSSPV.push_back(c_new);
        }

        if (nanSkipCount) std::cout << "[SSP延伸] ⚠ 跳过末尾NaN " << nanSkipCount << " 次" << std::endl;

        // 抽样打印前3个剖面的延伸结果
        for (size_t si = 0; si < sspList.size() && si < 3; ++si) {
            const auto& ssp = sspList[si];
            size_t n = ssp.zSSPV.size();
            std::cout << "[SSP延伸] 剖面[" << si << "] "
                      << "z[n-3]=" << ssp.zSSPV[n-3] << " c=" << ssp.cSSPV[n-3] << " | "
                      << "z[n-2]=" << ssp.zSSPV[n-2] << " c=" << ssp.cSSPV[n-2] << " | "
                      << "z[n-1]=" << ssp.zSSPV[n-1] << " c=" << ssp.cSSPV[n-1] << " ←新"
                      << std::endl;
        }
        maxSspDepth = targetDepth;
    }

    std::cout << "[DBG] configure: SSP=" << sspList.size()
              << " BTY=" << slice.btyPts.size()
              << " RD=" << receiveDepth.size() << "层"
              << " R=" << numRangePts << "点"
              << " zBox=" << std::max(maxBtyDepth, maxSspDepth) * 1.1f
              << " step=" << std::max(static_cast<int>(maxRange / numRangePts), 100)
              << std::endl;
    parm.set_SSP(sspList);
    parm.set_btyPts(slice.btyPts);

    // ---- 声源 / 接收 ----
    parm.set_SD(static_cast<float>(sourceDepth), 1);
    std::vector<float> rdFloat;
    rdFloat.reserve(receiveDepth.size());
    for (double d : receiveDepth) rdFloat.push_back(static_cast<float>(d));
    parm.set_RD(rdFloat);
    parm.set_R(static_cast<float>(maxRange), numRangePts);

    // ---- 声线 ----
    parm.set_NBeam(beamNumber);
    parm.set_angle({ static_cast<float>(beamAngleDown),
                     static_cast<float>(beamAngleUp) });

    // ---- 步长 / 网格盒 ----
    int autoStepLen = std::max(static_cast<int>(maxRange / numRangePts), 100);
    parm.set_stepLength(autoStepLen);
    float autoZBox = std::max(slice.maxBtyDepth, slice.maxSspDepth) * 1.1f;
    parm.set_zBox(autoZBox);
    parm.set_rBox(static_cast<float>(rBox));

    // ---- 频率 ----
    parm.set_freq(static_cast<int>(freq));
    parm.set_NMedia(1);

    // ---- 海面 ----
    Boundary_Line surfaceLine;
    surfaceLine.alphaR = 1500.0f; surfaceLine.alphaI = 0.0f;
    surfaceLine.betaR  = 0.0f;    surfaceLine.betaI  = 0.0f;
    surfaceLine.rho    = 1.0f;    surfaceLine.Depth  = 0.0f;
    parm.set_surfaceLine(surfaceLine);

    // ---- 海底 ----
    Boundary_Line bottomLine;
    bottomLine.alphaR = static_cast<float>(bottomAlphaR);
    bottomLine.alphaI = static_cast<float>(bottomAlphaI);
    bottomLine.betaR  = static_cast<float>(bottomBetaR);
    bottomLine.betaI  = static_cast<float>(bottomBetaI);
    bottomLine.rho    = static_cast<float>(bottomRho);
    bottomLine.Depth  = static_cast<float>(bottomDepth);
    parm.set_bottomLine(bottomLine);

    // ---- 顶部 / 底部选项 ----
    parm.topopt->firstVal->Quad_Q();
    parm.topopt->secondVal->Vacuum_V();
    parm.topopt->thirdVal->M();
    parm.topopt->forthVal->T();
    parm.topopt->fifthVal->NoneOption();
    parm.botopt->firstVal->acousto_elastic_A();
    parm.botopt->secondVal->useBTY();

    parm.set_EnvFile("CoreAcoustic_auto");
}

// ==========================================
// 声线适配器
// ==========================================

SingleRayPath convertRayInfo(const rayInfo& src) {
    SingleRayPath dst;
    dst.alpha     = static_cast<double>(src.SrcDeclAngle);
    dst.numTopBnc = 0;
    dst.numBotBnc = 0;
    for (const auto& pos : src.ray) {
        if (!std::isfinite(pos.x) || !std::isfinite(pos.y)) continue;
        dst.rr.push_back(static_cast<double>(pos.x) / 1000.0);  // m → km
        dst.zz.push_back(static_cast<double>(pos.y));            // m
    }
    return dst;
}

} // namespace Internal
} // namespace CoreAcoustic
