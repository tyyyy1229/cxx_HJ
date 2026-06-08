#ifndef CORE_ACOUSTIC_INTERNAL_H
#define CORE_ACOUSTIC_INTERNAL_H

#include <vector>
#include "bellhopParam.h"
#include "nc_process2.h"
#include "core_acoustic_single.h"  // BeamAngle, SingleRayPath

namespace CoreAcoustic {
namespace Internal {

// ---- 方位角 ----
double clientAzimuthToInternalTheta(double alphaDeg);
std::vector<double> generateAzimuthAngles(int N);

// ---- 通用工具 ----
std::vector<double> linspace(double start, double end, int num);

// ---- Bellhop 配置 (TL / Ray 共用) ----
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
    double bottomRho, double bottomDepth);

// ---- 声线适配器 (非侵入式) ----
SingleRayPath convertRayInfo(const rayInfo& src);

} // namespace Internal
} // namespace CoreAcoustic

#endif
