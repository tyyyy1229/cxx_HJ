#ifndef CORE_ACOUSTIC_AREA_H
#define CORE_ACOUSTIC_AREA_H

#include <vector>
#include <string>
#include "core_acoustic_single.h"  // BeamAngle

// ==========================================
// CoreAcoustic 区域模式（对齐 AcousticCalcLib.h）
// ==========================================

namespace CoreAcoustic {

// ==========================================
// 1. 数据类型
// ==========================================

/// 矩形地理范围（对齐 AcousticCalcLib.h::PointParam）
struct GeoRect {
    double minLat = 31.2;   // 最小纬度 (下边)
    double maxLat = 31.2;   // 最大纬度 (上边)
    double minLon = 122.5;  // 最小经度 (左边)
    double maxLon = 122.5;  // 最大经度 (右边)
};

/// 区域声场计算输入（对齐 AcousticCalcLib.h::AcousticCalcReq）
struct AreaAcousticConfig {
    // ---- 区域范围 ----
    GeoRect area;
    double  lonlatInterval = 0.5;          // 经纬度网格间距 (度)

    // ---- 方向 & 距离 ----
    int    directionNum         = 72;       // 方位方向数
    double maxRange             = 80000.0;  // 最大水平射程 (米)
    double receiveRangeInterval = 0.2;      // 接收距离间隔 (km)

    // ---- 深度 ----
    double sourceDepth;                     // 声源深度 (米)
    double rdMax = 5000.0;                  // 接收深度最大值 (米), set_RD(rdMax, rdNum)
    int    rdNum = 501;                     // 接收深度网格数
    // 输出过滤: 非空时从 TL depth 维提取对应深度值(米)的行
    // 行索引 = round(value / (rdMax/(rdNum-1))), 空 = 输出全层
    std::vector<double> receiveDepth;

    // ---- 频率 & 波束 ----
    double    freq = 5000.5;
    BeamAngle beamAngle;
    int       beamNumber = 720;

    // ---- NC 文件路径 ----
    std::string soundSpeedPath   = "CA2025010100.nc";
    std::string topographyPath   = "topagraphy.nc";
    std::string sedimentTypePath = "sediment.nc";
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

/// 区域声场计算结果（对齐 AcousticCalcLib.h::FileData，不含 CZ/SZ）
struct AreaAcousticResult {
    std::vector<double> lonGrid;         // 经度轴 (度)
    std::vector<double> latGrid;         // 纬度轴 (度)
    std::vector<double> receiverDepth;   // 深度轴 (m)
    std::vector<double> receiverRange;   // 距离轴 (km)
    std::vector<double> azimuthAngles;   // 方位角序列 (度, 0°=北 顺时针)

    // TL 扁平数组: [lon][lat][dir][range][depth], 最快变化维 = depth (对齐客户 FileData)
    int numLon = 0, numLat = 0, numDir = 0, numDepth = 0, numRange = 0;
    std::vector<double> tlData;
};

// ==========================================
// 3. 入口函数
// ==========================================

/**
 * @brief 区域网格传播损失批量计算
 *
 * 遍历 area 内 lonlatInterval 间距的所有网格点，每个点计算 directionNum
 * 个方位的 TL，结果存入 5D 扁平数组。
 */
AreaAcousticResult computeAreaTL(const AreaAcousticConfig& cfg);

} // namespace CoreAcoustic

#endif
