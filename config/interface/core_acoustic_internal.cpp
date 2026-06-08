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
    std::cout << "[DBG] configure: SSP=" << slice.sspList.size()
              << " BTY=" << slice.btyPts.size()
              << " RD=" << receiveDepth.size() << "层"
              << " R=" << numRangePts << "点"
              << " zBox=" << std::max(slice.maxBtyDepth, slice.maxSspDepth) * 1.1f
              << " step=" << std::max(static_cast<int>(maxRange / numRangePts), 100)
              << std::endl;
    parm.set_SSP(slice.sspList);
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
    dst.rr.reserve(src.ray.size());
    dst.zz.reserve(src.ray.size());
    for (const auto& pos : src.ray) {
        dst.rr.push_back(static_cast<double>(pos.x) / 1000.0);  // m → km
        dst.zz.push_back(static_cast<double>(pos.y));            // m
    }
    return dst;
}

} // namespace Internal
} // namespace CoreAcoustic
