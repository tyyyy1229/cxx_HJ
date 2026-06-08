#ifndef NC_PROCESS2_H
#define NC_PROCESS2_H

#include <string>
#include <vector>
#include <unordered_map>
#include "bellhopParam.h"  // 提供 SSP 和 ati_bty 类型定义

// ==========================================
// NC 处理配置与结果数据结构
// ==========================================

/**
 * @brief NC 文件处理输入配置（替代原 main() 的 8 个命令行参数）
 */
struct NcProcessConfig {
    double theta   = 30.0;                // 发射方位角（度，0°=东，逆时针）
    double lon_0   = 122.5;               // 声源基准点经度（度）
    double lat_0   = 31.2;                // 声源基准点纬度（度）
    double r_max   = 80000.0;             // 最大几何射程距离（米）
    int    num_pts = 400;                 // 沿程离散控制点个数
    std::string bty_nc = "topagraphy.nc"; // 地形 NC 文件名
    std::string ssp_nc = "CA2025010100.nc"; // 声速 NC 文件名
    std::string sed_nc = "sediment.nc";   // 底质 NC 文件名
};

/**
 * @brief NC 处理输出结果，可直接喂给 bellhopParam::set_SSP() / set_btyPts()
 */
struct NcProcessResult {
    std::vector<SSP> sspList;       // 每个沿程控制点 → 一个 SSP（Distance 单位 km）
    std::vector<ati_bty> btyPts;    // 每个沿程控制点 → 一个 ati_bty（x 单位 km, depth 单位 m）
    std::vector<float> sedVals;     // 每个沿程控制点 → 底质类型值（来自 NC 插值, 用于 TXT 查表）
    bool success = false;           // 处理是否成功
    float maxBtyDepth = 0.0f;       // 海底最大深度（米），供 main 校验
    float maxSspDepth = 0.0f;       // SSP 最大深度（米），供 main 校验
    size_t lev_size  = 0;           // 水体垂直层数
};

// ==========================================
// NC 原始数据缓存结构体（从 .cpp 内部提升至头文件，供 CoreAcoustic 复用）
// ==========================================

/**
 * @brief BTY 海底地形原始数据（对应 topagraphy.nc）
 */
struct NcBtyData {
    std::vector<float> lon;         // 1D 地形经度轴
    std::vector<float> lat;         // 1D 地形纬度轴
    std::vector<float> z_values;    // 2D 连续地形水深矩阵 [lat x lon]
    size_t lat_size = 0;           // 纬度轴点数
    size_t lon_size = 0;           // 经度轴点数
    bool  hasFillValue = false;    // 是否存在无效值标记
    float fillValue    = 0.0f;     // 无效填充值 (来自 _FillValue 或 missing_value)
};

/**
 * @brief SSP 声速剖面原始数据（对应 CA2025010100.nc）
 */
struct NcSspData {
    std::vector<float> lon_grid;   // 2D 原始非规则经度空间网格数组 [lat x lon]
    std::vector<float> lat_grid;   // 2D 原始非规则纬度空间网格数组 [lat x lon]
    std::vector<float> lev;        // 1D 垂直深度轴序列
    std::vector<float> sp_values;  // 4D 真实声速数值阵列 [time x lev x lat x lon]
    size_t lat_size = 0;           // 纬度格点数
    size_t lon_size = 0;           // 经度格点数
    size_t lev_size = 0;           // 水体垂直分层总数
    size_t time_size = 0;          // 时间维大小
    bool  hasFillValue = false;    // 是否存在无效值标记
    float fillValue    = 0.0f;     // 无效填充值
};

/**
 * @brief SED 海底底质原始数据（对应 sediment.nc）
 */
struct NcSedData {
    std::vector<float> lon;         // 1D 底质经度轴
    std::vector<float> lat;         // 1D 底质纬度轴
    std::vector<float> sed_values;  // 2D 海底底质属性矩阵 [lat x lon]
    size_t lat_size = 0;           // 纬度轴点数
    size_t lon_size = 0;           // 经度轴点数
    bool  hasFillValue = false;    // 是否存在无效值标记
    float fillValue    = 0.0f;     // 无效填充值
};

// ==========================================
// 底质声学参数查表
// ==========================================

/// 底质类型对应的声学参数（来自 TXT 查表文件）
struct SedimentParam {
    float alphaR;   // 压缩波声速 (m/s)
    float rho;      // 密度 (g/cm³)
    float alphaI;   // 压缩波衰减 (dB/λ)
};

/// 类型编号 → 声学参数映射表
using SedimentTable = std::unordered_map<int, SedimentParam>;

/// 从 TXT 文件读取底质类型查表
/// @param txtPath TXT 文件路径, 格式: type,c_p,rho_p,atten_p
/// @return 查表 map, 文件不存在或解析失败则返回空 map
SedimentTable readSedimentTable(const std::string& txtPath);

/**
 * @brief NC 原始环境数据缓存（加载一次，跨方向/跨源点复用）
 */
struct NcEnvCache {
    NcBtyData bty;
    NcSspData ssp;
    NcSedData sed;
    bool loaded = false;
};

// ==========================================
// 局部插值网格（按源点构建一次，各方向共享）
// ==========================================

/**
 * @brief 以源点为中心的 200m 分辨率规则正方形局部网格 A
 *
 * 画布覆盖范围：[-1.5*r_max, +1.5*r_max] × [-1.5*r_max, +1.5*r_max]
 * 网格分辨率：GRID_RES = 200m
 */
struct LocalEnvGrid {
    std::vector<float> grid_bty;   // 2D 地形插值矩阵 [Ny x Nx]，row-major
    std::vector<float> grid_sed;   // 2D 底质插值矩阵 [Ny x Nx]，row-major
    std::vector<float> grid_ssp;   // 3D 声速插值矩阵 [lev x Ny x Nx]
    std::vector<float> lev;        // 水体垂直深度轴 (m)，源自 SSP NC 文件
    size_t Nx = 0;                 // 东向格点数
    size_t Ny = 0;                 // 北向格点数
    size_t lev_size = 0;           // 水体垂直层数
    bool   built = false;          // 是否已成功构建
};

// ==========================================
// 核心入口函数
// ==========================================

/**
 * @brief 从 NC 文件构建 Bellhop 环境数据（SSP + BTY）
 *
 * 内部流程：
 *   1. 加载三个 NC 文件（地形 / 声速 / 底质）
 *   2. 按需裁剪局部正方形区域 A（1.5× r_max），200m 网格高精度插值
 *   3. 沿指定方位角提取均匀间距切片（ProfilePoint 序列）
 *   4. 将 ProfilePoint 转换为 SSP + ati_bty 并输出
 *
 * @param cfg 输入配置（路径、坐标、射程等）
 * @return NcProcessResult 包含可直接传入 bellhopParam 的 SSP 列表和 BTY 列表
 *
 * @note 单位转换：dist（米）→ SSP.Distance / ati_bty.x（千米）
 *       depth 保持米不变
 */
NcProcessResult buildBellhopEnvFromNc(const NcProcessConfig& cfg);

// ==========================================
// 分解式 API（供 CoreAcoustic 高频循环复用）
// ==========================================

/**
 * @brief 一次性加载三个 NC 文件到内存缓存
 *
 * 用途：区域模式 / 多方向模式下，NC 文件只打开一次，
 *       后续各源点 / 各方向从内存缓存中高速提取数据。
 *
 * @param cfg   NC 文件路径配置（仅使用 bty_nc / ssp_nc / sed_nc 字段）
 * @return      NcEnvCache 包含原始 NC 数据内存副本
 */
NcEnvCache loadNcEnvCache(const NcProcessConfig& cfg);

/**
 * @brief 为指定声源点构建局部 200m 规则插值网格
 *
 * 以 (lon_0, lat_0) 为中心，构建覆盖 3.0×r_max 正方形区域 A 的
 * 地形 / 底质 / 声速三维规则插值网格。
 *
 * @param cache  已加载的 NC 原始数据缓存
 * @param lon_0  声源点经度（度）
 * @param lat_0  声源点纬度（度）
 * @param r_max  最大射程（米），用于确定画布尺寸
 * @return       LocalEnvGrid 局部规则网格
 */
LocalEnvGrid buildLocalEnvGrid(const NcEnvCache& cache,
                               double lon_0, double lat_0,
                               double r_max);

/**
 * @brief 从已构建的局部网格中沿指定方位角提取剖面切片
 *
 * 等价于 buildBellhopEnvFromNc() 的"切片 + 转换"后半段，
 * 但跳过了 NC 加载和网格构建步骤。
 *
 * @param grid   已构建的局部规则网格
 * @param theta  发射方位角（度，0°=东，逆时针，内部坐标系约定）
 * @param cfg    射程 / 控制点数配置（仅使用 r_max / num_pts 字段）
 * @return       NcProcessResult 包含沿该方位的 SSP 列表和 BTY 列表
 */
NcProcessResult extractSliceFromGrid(const LocalEnvGrid& grid,
                                     double theta,
                                     const NcProcessConfig& cfg);

#endif // NC_PROCESS2_H
