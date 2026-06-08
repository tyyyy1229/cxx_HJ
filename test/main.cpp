#include <ctime>
#include <iostream>
#include "core_acoustic.h"   // 聚合头文件: single + area

int main()
{
    // ============================================================
    // 方式一：单点模式 — computeSingleTL
    //   对齐 AcousticCalcSingleLib.h::SingleAcousticCalcReq
    //   单次调用 = 一个点。多点场景多次调用即可
    // ============================================================
    {
        std::cout << "========== 单点模式 (computeSingleTL) ==========" << std::endl;

        CoreAcoustic::SingleAcousticConfig cfg;

        // ---- 源点 (对齐 AcousticCalcSingleLib.h::Point) ----
        cfg.point.lon = 122.5;              // 经度 (度)
        cfg.point.lat = 31.2;               // 纬度 (度)

        // ---- 方向 & 距离 ----
        cfg.directionNum         = 4;       // 方位方向数 (360° 等分, 0°=北 顺时针: 0°/90°/180°/270°)
        cfg.maxRange             = 80000.0; // 最大水平射程 (米)
        cfg.receiveRangeInterval = 0.8;     // 接收距离间隔 (km) → 自动算 numRangePts = 80/0.8+1 = 101

        // ---- 深度 (优先级: receiveDepth 列表 > receiveDepthInterval > NC lev 全层) ----
        cfg.sourceDepth = 1300.0;           // 声源深度 (米)
        // cfg.receiveDepthInterval = 200.0; // 接收深度间隔 (米) → 从 0 到 maxLev 按间隔自动生成
        // cfg.receiveDepth = {0,200,400};   // 手动指定接收深度列表 (米)
        // 全留空 → 自动从 SSP NC 文件的 lev 维度取全层深度

        // ---- 频率 & 波束 (对齐 AcousticCalcSingleLib.h::BeamAngle) ----
        cfg.freq       = 5000.5;            // 频率 (Hz)
        cfg.beamAngle  = { 30.0, -30.0 };   // angleUp=30°(上限), angleDown=-30°(下限)
        cfg.beamNumber = 100;               // 声线数量

        // ---- 输出控制 ----
        cfg.isRayOutput = false;            // true = 同时计算声线轨迹 (SingleRayPath)

        // ---- NC 文件路径 (默认值, 可覆盖) ----
        // cfg.soundSpeedPath   = "CA2025010100.nc";   // 声速剖面
        // cfg.topographyPath   = "topagraphy.nc";      // 地形
        // cfg.sedimentTypePath = "sediment.nc";        // 底质
         cfg.sedimentTablePath = "sediment_tbl.txt";  // 底质类型→声学参数 查表 TXT
        // cfg.bottomAlphaR = 1500.0;  cfg.bottomAlphaI = 0.0763;  // 海底参数 (TXT查表未命中时的 fallback)
        // cfg.bottomRho    = 1.4;     cfg.bottomDepth  = 4000.0;

        // ---- 自动计算 (无需用户设置) ----
        // zBox       = max(地形最深, SSP最深) × 1.1
        // stepLength = max(maxRange/numRangePts, 100m)

        clock_t s = clock();
        CoreAcoustic::SingleAcousticResult r = CoreAcoustic::computeSingleTL(cfg);
        clock_t e = clock();

        if (!r.tlData.empty()) {
            std::cout << "维度: " << r.numDir << "方向 × "
                      << r.numDepth << "深度 × " << r.numRange << "距离  = "
                      << r.tlData.size() << " TL值" << std::endl;
            std::cout << "方位角: ";
            for (double a : r.azimuthAngles) std::cout << a << "° ";
            std::cout << std::endl;
            std::cout << "深度: " << r.receiverDepth.size() << " 层 ("
                      << r.receiverDepth.front() << "~" << r.receiverDepth.back() << "m)" << std::endl;
            std::cout << "距离: " << r.receiverRange.size() << " 点 ("
                      << r.receiverRange.front() << "~" << r.receiverRange.back() << "km)" << std::endl;
            std::cout << "耗时: " << (e - s) * 1000.0 / CLOCKS_PER_SEC << " ms" << std::endl;
        }
    }

    // ============================================================
    // 方式二：区域模式 — computeAreaTL
    //   对齐 AcousticCalcLib.h::AcousticCalcReq
    //   遍历区域内所有网格点，仅 TL，无声线
    // ============================================================
    {
        std::cout << "\n========== 区域模式 (computeAreaTL) ==========" << std::endl;

        CoreAcoustic::AreaAcousticConfig cfg;

        // ---- 区域范围 (对齐 AcousticCalcLib.h::PointParam) ----
        cfg.area.minLat = 31.0;   // 最小纬度 (下边界, 度)
        cfg.area.maxLat = 31.5;   // 最大纬度 (上边界, 度)
        cfg.area.minLon = 122.0;  // 最小经度 (左边界, 度)
        cfg.area.maxLon = 122.5;  // 最大经度 (右边界, 度)
        cfg.lonlatInterval = 0.5; // 经纬度网格间距 (度) → (31.5-31.0)/0.5+1=2 × (122.5-122.0)/0.5+1=2 = 4 个网格点

        // ---- 方向 & 距离 ----
        cfg.directionNum         = 4;       // 方位方向数 (360° 等分, 0°=北 顺时针)
        cfg.maxRange             = 80000.0; // 最大水平射程 (米)
        cfg.receiveRangeInterval = 0.8;     // 接收距离间隔 (km) → 自动算 numRangePts = 80/0.8+1 = 101

        // ---- 深度 ----
        cfg.sourceDepth = 1300.0;           // 声源深度 (米)
        cfg.rdMax       = 5000.0;           // 接收深度最大值 (米) → set_RD(rdMax, rdNum)
        cfg.rdNum       = 501;              // 接收深度网格数   → linspace(0,5000,501), 间隔=10m
        // cfg.receiveDepth = {100, 500};   // 输出过滤: 深度值(米) → 行索引=round(值/间隔)
        // 留空 → 输出全 501 层

        // ---- 频率 & 波束 (对齐 AcousticCalcLib.h::BeamAngle) ----
        cfg.freq       = 5000.5;            // 频率 (Hz)
        cfg.beamAngle  = { 30.0, -30.0 };   // angleUp=30°(上限), angleDown=-30°(下限)
        cfg.beamNumber = 100;               // 声线数量

        // ---- NC 文件路径 (默认值, 可覆盖) ----
        // cfg.soundSpeedPath    = "CA2025010100.nc";   // 声速剖面
        // cfg.topographyPath    = "topagraphy.nc";      // 地形
        // cfg.sedimentTypePath  = "sediment.nc";        // 底质
        // cfg.sedimentTablePath = "sediment_tbl.txt";  // 底质类型→声学参数 查表 TXT
        // cfg.bottomAlphaR = 1500.0;  cfg.bottomAlphaI = 0.0763;  // 海底参数 (TXT查表未命中时的 fallback)
        // cfg.bottomRho    = 1.4;     cfg.bottomDepth  = 4000.0;

        // ---- 自动计算 (无需用户设置) ----
        // zBox       = max(地形最深, SSP最深) × 1.1
        // stepLength = max(maxRange/numRangePts, 100m)

        clock_t s = clock();
        CoreAcoustic::AreaAcousticResult r = CoreAcoustic::computeAreaTL(cfg);
        clock_t e = clock();

        if (!r.tlData.empty()) {
            std::cout << "网格: " << r.numLon << "×" << r.numLat << " = "
                      << (r.numLon * r.numLat) << " 点" << std::endl;
            std::cout << "维度: " << r.numLon << "×" << r.numLat << "×"
                      << r.numDir << "×" << r.numDepth << "×" << r.numRange
                      << " = " << r.tlData.size() << " TL值" << std::endl;
            std::cout << "经度轴: ";
            for (double v : r.lonGrid) std::cout << v << " ";
            std::cout << std::endl;
            std::cout << "纬度轴: ";
            for (double v : r.latGrid) std::cout << v << " ";
            std::cout << std::endl;
            std::cout << "耗时: " << (e - s) * 1000.0 / CLOCKS_PER_SEC << " ms" << std::endl;
        }
    }

    printf("\nGCC Version: %d.%d.%d\n", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
    return 0;
}
