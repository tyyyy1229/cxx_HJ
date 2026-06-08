#ifndef CORE_ACOUSTIC_SINGLE_H
#define CORE_ACOUSTIC_SINGLE_H

#include <vector>
#include <string>

// ==========================================
// CoreAcoustic 单点模式（对齐 AcousticCalcSingleLib.h）
//   单次调用 = 一个经纬度点
//   多点场景 = 调用方多次调用
// ==========================================

namespace CoreAcoustic {

/// 波束开角范围（对齐 AcousticCalcSingleLib.h::BeamAngle）
struct BeamAngle {
    double angleUp   = 30.0;   // 波束开角上限 (度)
    double angleDown = -30.0;  // 波束开角下限 (度)
};

// ==========================================
// 1. 数据类型
// ==========================================

/// 地理坐标点（对齐 AcousticCalcSingleLib.h::Point）
struct GeoPoint {
    double lon = 122.5;   // 经度 (度)
    double lat = 31.2;    // 纬度 (度)
};

/// 单点声场计算输入（对齐 AcousticCalcSingleLib.h::SingleAcousticCalcReq）
struct SingleAcousticConfig {
    // ---- 源点 ----
    GeoPoint point;

    // ---- 方向 & 距离 ----
    int    directionNum         = 72;       // 方位方向数
    double maxRange             = 80000.0;  // 最大水平射程 (米)
    double receiveRangeInterval = 0.2;      // 接收距离间隔 (km)
    // numRangePts 自动 = maxRange/1000 / receiveRangeInterval + 1

    // ---- 深度 ----
    double sourceDepth;                     // 声源深度 (米)
    double receiveDepthInterval = 0.0;      // 接收深度间隔 (米)。>0 时自动生成深度序列; =0 时取 NC lev
    std::vector<double> receiveDepth;       // 手动深度列表（优先级高于 interval）

    // ---- 频率 & 波束 ----
    double    freq = 5000.5;               // 频率 (Hz)
    BeamAngle beamAngle;                    // 波束开角范围
    int       beamNumber = 720;            // 声线数量

    // ---- 输出控制 ----
    bool isRayOutput = false;              // 是否同时计算声线轨迹

    // ---- NC 文件路径 ----
    std::string soundSpeedPath   = "CA2025010100.nc";   // 声速剖面
    std::string topographyPath   = "topagraphy.nc";      // 地形
    std::string sedimentTypePath = "sediment.nc";        // 底质
    std::string sedimentTablePath = "sedimentType.txt";  // 底质类型→声学参数 查表 TXT

    // ---- 介质参数 (内部默认值) ----
    bool   isCoherent   = true;
    double bottomAlphaR = 1500.0, bottomAlphaI = 0.0763;
    double bottomBetaR  = 0.0,    bottomBetaI  = 0.0;
    double bottomRho    = 1.4,    bottomDepth  = 4000.0;
};

// ==========================================
// 2. 输出类型
// ==========================================

/// 单条声线轨迹（对齐 AcousticCalcSingleLib.h::RayTrace）
struct SingleRayPath {
    double alpha = 0.0;              // 出射角 (度)
    int    numTopBnc = 0;            // 海面反射次数
    int    numBotBnc = 0;            // 海底反射次数
    std::vector<double> rr;          // 沿程距离 (km)
    std::vector<double> zz;          // 深度 (m)
};

/// 沿程地形剖面（对齐 AcousticCalcSingleLib.h::Topography）
struct Topography {
    std::vector<double> topographyRange;  // 沿程距离序列 (km)
    std::vector<double> topographyDepth;  // 地形深度序列 (m)
};

/// 单点声场计算结果（对齐 AcousticCalcSingleLib.h::SingleAcousticCalcResp，不含 CZ/SZ）
struct SingleAcousticResult {
    std::vector<double> receiverDepth;   // 深度轴 (m)
    std::vector<double> receiverRange;   // 距离轴 (km)
    std::vector<double> azimuthAngles;   // 方位角序列 (度, 0°=北 顺时针)

    // TL: [dir][depth][range], 最快变化维 = range
    int numDir = 0, numDepth = 0, numRange = 0;
    std::vector<double> tlData;

    // 地形剖面: [dirIdx], 每方向沿程地形（对齐客户 topography 字段）
    std::vector<Topography> topography;

    // 声线: [dirIdx][rayIdx], 仅 isRayOutput=true 时有值
    std::vector<std::vector<SingleRayPath>> rayData;
};

// ==========================================
// 3. 入口函数
// ==========================================

/**
 * @brief 单点传播损失计算（对齐 SingleSyncAcousticCalc）
 *
 * 方位角: 0°=正北, 顺时针 (客户约定)
 * 多点: 调用方多次调用，每次一个 GeoPoint
 */
SingleAcousticResult computeSingleTL(const SingleAcousticConfig& cfg);

} // namespace CoreAcoustic

#endif
