#include <iostream>
#include <vector>
#include <string>
#include <exception>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <limits>
#include <netcdf>
#include "nanoflann.hpp" // 请确保当前工作目录下存在该开源高性能标头库文件
#include "nc_process2.h" // 本项目配置/结果结构体 + buildBellhopEnvFromNc() 声明

using namespace netCDF;
using namespace netCDF::exceptions;

// 行业惯例的基础全局常量
const double PI = 3.14159265358979323846;
const double LAT_TO_METER = 111000.0; // 纬度 1° ≈ 111 km
const double GRID_RES = 200.0;        // 统一规则网格间距 200m

// ==========================================
// 1. 水声学常规划分数据结构定义
// ==========================================
// BTYData / SSPData / SedData 已提升至 nc_process2.h
//    → NcBtyData / NcSspData / NcSedData
// 以下 typedef 保持内部代码兼容，无需逐处重命名
typedef NcBtyData BTYData;
typedef NcSspData SSPData;
typedef NcSedData SedData;

/**
 * @brief 地理限界外包矩形边界
 */
struct BoundingBox {
    double min_lon = 1e9, max_lon = -1e9;
    double min_lat = 1e9, max_lat = -1e9;
};

/**
 * @brief nanoflann 点云骨架适配器结构 (契约对齐本地 nanoflann 库接口签名)
 */
struct PointCloud2D {
    struct Point { float x, y, value; };
    std::vector<Point> pts;

    inline size_t kdtree_get_point_count() const { return pts.size(); }
    inline float kdtree_get_pt(const size_t index, const size_t dim) const {
        return (dim == 0) ? pts[index].x : pts[index].y;
    }
    template <class BBOX>
    bool kdtree_get_bbox(BBOX& /* bb */) const { return false; }
};

typedef nanoflann::KDTreeSingleIndexAdaptor<
    nanoflann::L2_Simple_Adaptor<float, PointCloud2D>, PointCloud2D, 2
> PlanarKdTree;

/**
 * @brief 剖面配置控制类 (无缝承载 parm.set_R 规范接口)
 */
class ProfileParam {
public:
    double r_max = 0.0;  // 剖面最大射程长度（米）
    int num_pts = 0;     // 剖面线均匀控制采样点数

    void set_R(double r, int pts) {
        this->r_max = r;
        this->num_pts = pts;
    }
};

/**
 * @brief 离散切片提取出的单点垂向全要素成果数据单
 */
struct ProfilePoint {
    int idx;                        // 沿程控制点序号（0 ~ num_pts-1）
    double dist;                    // 该控制点距离发射起点的沿程累计水平距离（米）
    double x;                       // 局部平面直角米制坐标 X（米，以波源为中心0,0）
    double y;                       // 局部平面直角米制坐标 Y（米，以波源为中心0,0）
    int i;                          // 四舍五入后对应对齐网格的列索引 i
    int j;                          // 四舍五入后对应对齐网格的行索引 j
    bool is_valid;                  // 越界检查标识
    
    float bty_val;                  // 提取对齐出的当前位置海底地形深度值（米）
    float sed_val;                  // 提取对齐出的当前位置海底底质参数值
    std::vector<float> ssp_prof;    // 提取出的对应该直角坐标下的全层一维声速序列（m/s）
};

// ==========================================
// 2. 空间坐标转换与平面插值原子核心工具函数
// ==========================================

/**
 * @brief 按需升级：将球面经纬度地理坐标转换为以用户指定声呐波源中心(lon_0, lat_0)为绝对原点(0,0)的平面米制坐标
 * @param lon 输入的原始地理经度值（度）
 * @param lat 输入的原始地理纬度值（度）
 * @param lon_0 用户输入的声呐波源基准中心点经度（度）
 * @param lat_0 用户输入的声呐波源基准中心点纬度（度）
 * @param x 传出的相对于中心源的直角平面东向距离（米）
 * @param y 传出的相对于中心源的直角平面北向距离（米）
 */
void lonLatToXY(double lon, double lat, double lon_0, double lat_0, float& x, float& y) {
    y = static_cast<float>((lat - lat_0) * LAT_TO_METER);
    x = static_cast<float>((lon - lon_0) * LAT_TO_METER * std::cos(lat * PI / 180.0));
}

/**
 * @brief 利用 nanoflann 的 4 邻域检索实现反距离平方二维平面离散线性插值
 * @param tree 已经构建完毕的局部空间 KD-Tree 树句柄
 * @param cloud 该局部树所绑定的高密度剪裁点云底座
 * @param query_x 检索点相对于波源中心的直角平面米制坐标 X（米）
 * @param query_y 检索点相对于波源中心的直角平面米制坐标 Y（米）
 * @return float 二维反距离平方线性插值后平滑还原出的环境场物理量
 */
float interpolate2D(const PlanarKdTree& tree, const PointCloud2D& cloud, float query_x, float query_y) {
    // 空云守卫：区域内无有效数据点
    if (cloud.pts.empty()) return std::numeric_limits<float>::quiet_NaN();

    const size_t num_closest = 4; size_t out_indices[num_closest]; float out_dist_sq[num_closest];
    nanoflann::KNNResultSet<float> resultSet(num_closest); resultSet.init(out_indices, out_dist_sq);
    float query_pt[2] = { query_x, query_y };
    tree.findNeighbors(resultSet, &query_pt[0], nanoflann::SearchParameters());

    float weight_sum = 0.0f; float value_sum = 0.0f;
    for (size_t k = 0; k < num_closest; ++k) {
        if (out_dist_sq[k] < 1e-4f) return cloud.pts[out_indices[k]].value; // 精确重合
        float w = 1.0f / out_dist_sq[k]; weight_sum += w; value_sum += w * cloud.pts[out_indices[k]].value;
    }
    return (weight_sum > 0.0f) ? (value_sum / weight_sum) : std::numeric_limits<float>::quiet_NaN();
}

// ==========================================
// 3. 基础 NetCDF 原始环境数据加载函数模块
// ==========================================

/**
 * @brief 从原始离散地形 NetCDF 文件中无损解析 1D 坐标轴轴线及 2D 水深地形大矩阵
 * @param ncPath 地形 NC 文件的磁盘存储路径
 * @param outBTY 传出填充的地形内存持有实体
 * @return bool 读取是否成功
 */
bool readBTYFile(const std::string& ncPath, BTYData& outBTY) {
    try {
        NcFile nc(ncPath, NcFile::read);
        NcDim latDim = nc.getDim("lat"); NcDim lonDim = nc.getDim("lon");
        if (latDim.isNull() || lonDim.isNull()) return false;
        outBTY.lat_size = latDim.getSize(); outBTY.lon_size = lonDim.getSize();
        outBTY.lat.resize(outBTY.lat_size); outBTY.lon.resize(outBTY.lon_size);
        outBTY.z_values.resize(outBTY.lat_size * outBTY.lon_size);
        nc.getVar("lat").getVar(outBTY.lat.data()); nc.getVar("lon").getVar(outBTY.lon.data());
        nc.getVar("z").getVar(outBTY.z_values.data());

        // 检测无效值标记
        NcVar zVar = nc.getVar("z");
        NcVarAtt fillAtt = zVar.getAtt("_FillValue");
        if (fillAtt.isNull()) fillAtt = zVar.getAtt("missing_value");
        if (!fillAtt.isNull()) {
            fillAtt.getValues(&outBTY.fillValue);
            outBTY.hasFillValue = true;
            std::cout << "[I/O] 地形 _FillValue = " << outBTY.fillValue << std::endl;
        }

        std::cout << "[I/O 成功] 原始地形要素加载完毕。网格规模: " << outBTY.lat_size << " x " << outBTY.lon_size << std::endl;
        return true;
    } catch (...) { return false; }
}

/**
 * @brief 从声速剖面文件中加载 2D 原始非规则网格，并执行压缩数值物理值自动乘加解包
 * @param ncPath 声速剖面 NC 文件的磁盘存储路径
 * @param outSSP 传出填充并换算完毕的连续声速剖面结构
 * @return bool 读取与物理变换是否成功
 */
bool readSSPFile(const std::string& ncPath, SSPData& outSSP) {
    try {
        NcFile nc(ncPath, NcFile::read);
        NcDim latDim = nc.getDim("lat"); NcDim lonDim = nc.getDim("lon");
        NcDim levDim = nc.getDim("lev"); NcDim timeDim = nc.getDim("time");
        if (latDim.isNull() || lonDim.isNull() || levDim.isNull() || timeDim.isNull()) return false;
        outSSP.lat_size = latDim.getSize(); outSSP.lon_size = lonDim.getSize();
        outSSP.lev_size = levDim.getSize(); outSSP.time_size = timeDim.getSize();
        size_t grid_2d = outSSP.lat_size * outSSP.lon_size;
        size_t total_4d = outSSP.time_size * outSSP.lev_size * grid_2d;
        outSSP.lat_grid.resize(grid_2d); outSSP.lon_grid.resize(grid_2d);
        outSSP.lev.resize(outSSP.lev_size); outSSP.sp_values.resize(total_4d);
        nc.getVar("lat").getVar(outSSP.lat_grid.data()); nc.getVar("lon").getVar(outSSP.lon_grid.data());
        nc.getVar("lev").getVar(outSSP.lev.data());
        NcVar spVar = nc.getVar("SP");
        nc_type varType = spVar.getType().getId();

        if (varType == NC_SHORT || varType == NC_INT || varType == NC_BYTE) {
            // ---- 整型存储: short → scale+offset 解包为 float ----
            double scale = 1.0, offset = 0.0;
            if (!spVar.getAtt("scale_factor").isNull()) spVar.getAtt("scale_factor").getValues(&scale);
            if (!spVar.getAtt("add_offset").isNull()) spVar.getAtt("add_offset").getValues(&offset);

            NcVarAtt fillAtt = spVar.getAtt("_FillValue");
            if (fillAtt.isNull()) fillAtt = spVar.getAtt("missing_value");
            if (!fillAtt.isNull()) {
                short rawFill;
                fillAtt.getValues(&rawFill);
                outSSP.fillValue    = static_cast<float>(rawFill * scale + offset);
                outSSP.hasFillValue = true;
                std::cout << "[I/O] 声速 _FillValue = " << outSSP.fillValue
                          << " (short → float)" << std::endl;
            }

            std::vector<short> raw_sp(total_4d); spVar.getVar(raw_sp.data());
            for (size_t idx = 0; idx < total_4d; ++idx)
                outSSP.sp_values[idx] = static_cast<float>(raw_sp[idx] * scale + offset);
            std::cout << "[I/O] 声速 (short→float 解包) 完毕, " << outSSP.lev_size << " 层" << std::endl;

        } else {
            // ---- 浮点存储: float/double 直接读, NaN 原样保留 ----
            std::vector<float> raw_sp(total_4d); spVar.getVar(raw_sp.data());
            for (size_t idx = 0; idx < total_4d; ++idx)
                outSSP.sp_values[idx] = raw_sp[idx];
            // NaN 不需要 _FillValue 标记, isDataInvalid 中 !isfinite 自然过滤
            std::cout << "[I/O] 声速 (float 直读) 完毕, " << outSSP.lev_size << " 层" << std::endl;
        }
        return true;
    } catch (...) { return false; }
}

/**
 * @brief 从海底底质文件中解析离散 1D 轴坐标线以及二维连续特征底质大参数矩阵
 * @param ncPath 海底参数 NC 文件的磁盘存储路径
 * @param outSed 传出填充的海底参数离散持有实体
 * @return bool 读取是否成功
 */
bool readSedFile(const std::string& ncPath, SedData& outSed) {
    try {
        NcFile nc(ncPath, NcFile::read);
        NcDim latDim = nc.getDim("lat"); NcDim lonDim = nc.getDim("lon");
        if (latDim.isNull() || lonDim.isNull()) return false;
        outSed.lat_size = latDim.getSize(); outSed.lon_size = lonDim.getSize();
        outSed.lat.resize(outSed.lat_size); outSed.lon.resize(outSed.lon_size);
        outSed.sed_values.resize(outSed.lat_size * outSed.lon_size);
        nc.getVar("lat").getVar(outSed.lat.data()); nc.getVar("lon").getVar(outSed.lon.data());
        nc.getVar("sediment").getVar(outSed.sed_values.data());

        // 检测无效值标记
        NcVar sedVar = nc.getVar("sediment");
        NcVarAtt fillAtt = sedVar.getAtt("_FillValue");
        if (fillAtt.isNull()) fillAtt = sedVar.getAtt("missing_value");
        if (!fillAtt.isNull()) {
            fillAtt.getValues(&outSed.fillValue);
            outSed.hasFillValue = true;
            std::cout << "[I/O] 底质 _FillValue = " << outSed.fillValue << std::endl;
        }

        std::cout << "[I/O 成功] 原始海底底质要素加载完毕。参数网格: " << outSed.lat_size << " x " << outSed.lon_size << std::endl;
        return true;
    } catch (...) { return false; }
}

// ==========================================
// 4. 按需视口裁剪与局部正方形 A 网格高能线性插值封装模块
// ==========================================

/**
 * @brief 核心重构算法：实现指定基准点向外拓扑扩展 1.5倍 r_max 的正方形局部区域 A 的极速按需筛选与 200m 网格局部插值
 * @param bty 离散原始地形全局数据源引用
 * @param ssp 离散原始声速全局数据源引用
 * @param sed 离散原始底质全局数据源引用
 * @param lon_0 用户手动指定的声呐波源基准点地理经度（度）
 * @param lat_0 用户手动指定的声呐波源基准点地理纬度（度）
 * @param r_max 用户手动指定的剖面计算最大径向水平几何射程（米）
 * @param grid_bty 传出：局部区域 A 内部插值填充好的规则化 200m 地形一维连续平铺容器
 * @param grid_sed 传出：局部区域 A 内部插值填充好的规则化 200m 底质一维连续平铺容器
 * @param grid_ssp 传出：局部区域 A 内部插值填充好的规则化 200m 三维规则声速连续平铺容器 [lev x Ny x Nx]
 * @param Nx 传出：计算出来的正方形 A 局部规则画布东向最大列数边界
 * @param Ny 传出：计算出来的正方形 A 局部规则画布北向最大行数边界
 */
void processLocalOnDemandInterpolate(
    const BTYData& bty, const SSPData& ssp, const SedData& sed,
    double lon_0, double lat_0, double r_max,
    std::vector<float>& grid_bty, std::vector<float>& grid_sed, std::vector<float>& grid_ssp,
    size_t& Nx, size_t& Ny)
{
    // 4.1 由物理尺寸精算反推正方形地理范围 A 在球面大地上的绝对经纬度四至限界边界
    double lat_span = (1.5 * r_max) / LAT_TO_METER;
    double lon_span = (1.5 * r_max) / (LAT_TO_METER * std::cos(lat_0 * PI / 180.0));
    
    double geo_min_lon = lon_0 - lon_span; double geo_max_lon = lon_0 + lon_span;
    double geo_min_lat = lat_0 - lat_span; double geo_max_lat = lat_0 + lat_span;

    // 4.2 确立正方形 A 局画布在 200m 离散分辨率下的行/列总格点规模数目
    // 直角米制空间的计算范围被死死约束在 [-1.5*r_max, +1.5*r_max]，总跨度长度为 3.0*r_max
    Nx = static_cast<size_t>(std::ceil((3.0 * r_max) / GRID_RES)) + 1;
    Ny = Nx; // 正方形区域，行点数完全等于列点数
    size_t total_local_cells = Ny * Nx;

    // 无效值判定：NC 声明的 _FillValue / missing_value，或 NaN/Inf，或 ≤ -9990（地学惯用缺省值）
    auto isDataInvalid = [](float val, bool hasFill, float fillVal) -> bool {
        if (!std::isfinite(val))                     return true;  // NaN / Inf
        if (hasFill && val == fillVal)               return true;  // NC 声明的填充值
        if (val <= -9990.0f)                         return true;  // 未声明的缺省标记 (如 -9999)
        return false;
    };

    // ---- 【按需提速优化一】地形大地图 1D 线性扫描：仅框选局部视口下标窗口，从源头上斩断 1.5 亿次死循环 ----
    std::vector<size_t> valid_bty_is, valid_bty_js;
    for (size_t i = 0; i < bty.lon_size; ++i) { if (bty.lon[i] >= geo_min_lon && bty.lon[i] <= geo_max_lon) valid_bty_is.push_back(i); }
    for (size_t j = 0; j < bty.lat_size; ++j) { if (bty.lat[j] >= geo_min_lat && bty.lat[j] <= geo_max_lat) valid_bty_js.push_back(j); }

    // 地形 KD-tree：过滤 fill value 和 NaN/Inf
    PointCloud2D bty_cloud;
    bty_cloud.pts.reserve(valid_bty_js.size() * valid_bty_is.size());
    for (size_t j : valid_bty_js) {
        for (size_t i : valid_bty_is) {
            float val = bty.z_values[j * bty.lon_size + i];
            if (isDataInvalid(val, bty.hasFillValue, bty.fillValue)) continue;
            float px, py; lonLatToXY(bty.lon[i], bty.lat[j], lon_0, lat_0, px, py);
            bty_cloud.pts.push_back({px, py, val});
        }
    }
    PlanarKdTree bty_tree(2, bty_cloud, nanoflann::KDTreeSingleIndexAdaptorParams(10)); bty_tree.buildIndex();

    // ---- 【按需提速优化二】海底底质实施完全同理的 1D 下标窗口框选斩断 ----
    std::vector<size_t> valid_sed_is, valid_sed_js;
    for (size_t i = 0; i < sed.lon_size; ++i) { if (sed.lon[i] >= geo_min_lon && sed.lon[i] <= geo_max_lon) valid_sed_is.push_back(i); }
    for (size_t j = 0; j < sed.lat_size; ++j) { if (sed.lat[j] >= geo_min_lat && sed.lat[j] <= geo_max_lat) valid_sed_js.push_back(j); }

    // 底质 KD-tree：过滤 fill value 和 NaN/Inf
    PointCloud2D sed_cloud;
    sed_cloud.pts.reserve(valid_sed_js.size() * valid_sed_is.size());
    for (size_t j : valid_sed_js) {
        for (size_t i : valid_sed_is) {
            float val = sed.sed_values[j * sed.lon_size + i];
            if (isDataInvalid(val, sed.hasFillValue, sed.fillValue)) continue;
            float px, py; lonLatToXY(sed.lon[i], sed.lat[j], lon_0, lat_0, px, py);
            sed_cloud.pts.push_back({px, py, val});
        }
    }
    PlanarKdTree sed_tree(2, sed_cloud, nanoflann::KDTreeSingleIndexAdaptorParams(10)); sed_tree.buildIndex();

    // ---- 【按需提速优化三】声速剖面非规则 2D 网格闪电框选过滤 ----
    // 以表层 (lev=0) 为准过滤 fill value，表层无效则整个剖面跳过
    PointCloud2D ssp_geo_cloud;
    ssp_geo_cloud.pts.reserve(ssp.lon_grid.size() / 10);
    size_t ssp_grid_2d = ssp.lat_size * ssp.lon_size;
    for (size_t g_idx = 0; g_idx < ssp.lon_grid.size(); ++g_idx) {
        double cur_lon = ssp.lon_grid[g_idx]; double cur_lat = ssp.lat_grid[g_idx];
        if (cur_lon >= geo_min_lon && cur_lon <= geo_max_lon && cur_lat >= geo_min_lat && cur_lat <= geo_max_lat) {
            // 表层声速值（time=0, lev=0）
            float surfaceVal = ssp.sp_values[0 * ssp_grid_2d + g_idx];
            if (isDataInvalid(surfaceVal, ssp.hasFillValue, ssp.fillValue)) continue;
            float px, py; lonLatToXY(cur_lon, cur_lat, lon_0, lat_0, px, py);
            ssp_geo_cloud.pts.push_back({px, py, static_cast<float>(g_idx)});
        }
    }
    PlanarKdTree ssp_tree(2, ssp_geo_cloud, nanoflann::KDTreeSingleIndexAdaptorParams(10)); ssp_tree.buildIndex();

    // 4.3 仅在裁剪压缩后的正方形 A 画布内部（仅包含 Ny x Nx 约上百万次）激活局部密集空间线性加权插值
    grid_bty.assign(total_local_cells, 0.0f); 
    grid_sed.assign(total_local_cells, 0.0f);
    grid_ssp.assign(ssp.lev_size * total_local_cells, 0.0f);

    for (size_t j = 0; j < Ny; ++j) {
        // Y 轴物理范围严格从南侧极值 -1.5*r_max 朝北推进
        float query_y = static_cast<float>(-1.5 * r_max + j * GRID_RES);
        for (size_t i = 0; i < Nx; ++i) {
            // X 轴物理范围严格从西侧极值 -1.5*r_max 朝东推进
            float query_x = static_cast<float>(-1.5 * r_max + i * GRID_RES);
            size_t grid_idx = j * Nx + i;

            grid_bty[grid_idx] = interpolate2D(bty_tree, bty_cloud, query_x, query_y);
            grid_sed[grid_idx] = interpolate2D(sed_tree, sed_cloud, query_x, query_y);

            // 45纵向水听器深度轴层复用定位句柄
            const size_t num_closest = 4; size_t out_indices[num_closest]; float out_dist_sq[num_closest];
            nanoflann::KNNResultSet<float> resultSet(num_closest); resultSet.init(out_indices, out_dist_sq);
            float query_pt[2] = { query_x, query_y }; ssp_tree.findNeighbors(resultSet, &query_pt[0], nanoflann::SearchParameters());

            size_t ssp_g2d = ssp.lat_size * ssp.lon_size;
            for (size_t k = 0; k < ssp.lev_size; ++k) {
                float weight_sum = 0.0f; float value_sum = 0.0f; bool exact_match = false;
                for (size_t n = 0; n < num_closest; ++n) {
                    size_t orig_g_idx = static_cast<size_t>(ssp_geo_cloud.pts[out_indices[n]].value);
                    float raw_sp = ssp.sp_values[k * ssp_g2d + orig_g_idx];

                    // 逐层过滤 fill value 和 NaN/Inf
                    if (isDataInvalid(raw_sp, ssp.hasFillValue, ssp.fillValue)) continue;

                    if (out_dist_sq[n] < 1e-4f) {
                        grid_ssp[k * (Ny * Nx) + grid_idx] = raw_sp; exact_match = true; break;
                    }
                    float w = 1.0f / out_dist_sq[n]; weight_sum += w; value_sum += w * raw_sp;
                }
                if (!exact_match && weight_sum > 0.0f)
                    grid_ssp[k * (Ny * Nx) + grid_idx] = value_sum / weight_sum;
                else if (!exact_match)
                    grid_ssp[k * (Ny * Nx) + grid_idx] = std::numeric_limits<float>::quiet_NaN();
            }
        }
    }
    std::cout << "[按需局部插值成功] 算力仅收敛于波源周边的 " << Ny << "x" << Nx << " 个对齐像素格点！" << std::endl;
}

/**
 * @brief 在对齐好的局部正方形 A 大矩阵网格中，以自身中心原点(0,0)为纯声呐发射起点，沿指定发射方位角快速四舍五入匹配捞取 400 点切片
 * @param theta_deg 外部参数接口传入的发射方位角：以正东方向为 0°，逆时针旋转增加，正北为 90°
 * @param parm 切片总射程跨度及 400 控制采样点分配对象引用的只读引用
 * @param r_max 用户手动输入的计算最大径向射程长度边界
 * @param Nx 规则局部直角坐标系系统的总列数常数
 * @param Ny 规则局部直角坐标系系统的总行数常数
 * @param lev_size 水体分层垂向总层数大小 (45)
 * @param grid_bty 正方形 A 内部插值就绪的规则地形一维向量
 * @param grid_sed 正方形 A 内部插值就绪的规则底质一维向量
 * @param grid_ssp 正方形 A 内部插值就绪的规则立体三维声速一维连续向量
 * @return std::vector<ProfilePoint> 400 个沿程物理控制采样点下的垂直全深度层 2D 切片要素集合
 */
std::vector<ProfilePoint> extractProfileSliceFromLocalRegion(
    double theta_deg, const ProfileParam& parm, double r_max,
    size_t Nx, size_t Ny, size_t lev_size,
    const std::vector<float>& grid_bty, const std::vector<float>& grid_sed, const std::vector<float>& grid_ssp)
{
    std::vector<ProfilePoint> sliceOutputs;
    sliceOutputs.reserve(parm.num_pts);

    double theta_rad = theta_deg * PI / 180.0;
    double step_dist = (parm.num_pts > 1) ? (parm.r_max / (parm.num_pts - 1)) : 0.0;

    for (int m = 0; m < parm.num_pts; ++m) {
        ProfilePoint pt;
        pt.idx = m;
        pt.dist = m * step_dist;

        // 起点完美退化还原为相对原点 (0.0, 0.0)，极坐标步进计算直线目标轨迹
        pt.x = pt.dist * std::cos(theta_rad);
        pt.y = pt.dist * std::sin(theta_rad);

        // 重构就近矩阵匹配取整索引公式：核心对齐减去左下角起点平移分量 -1.5*r_max 
        pt.i = static_cast<int>(std::round((pt.x - (-1.5 * r_max)) / GRID_RES));
        pt.j = static_cast<int>(std::round((pt.y - (-1.5 * r_max)) / GRID_RES));

        // 正方形局部区域边界自检与保护
        if (pt.i >= 0 && static_cast<size_t>(pt.i) < Nx && pt.j >= 0 && static_cast<size_t>(pt.j) < Ny) {
            size_t offset = static_cast<size_t>(pt.j) * Nx + static_cast<size_t>(pt.i);
            float btyVal = grid_bty[offset];

            // 无效值检测：地形为 NaN 则该点整体标记无效（陆地区域无数据）
            pt.is_valid = std::isfinite(btyVal);
            pt.bty_val = pt.is_valid ? btyVal : -999.0f;
            pt.sed_val = pt.is_valid ? grid_sed[offset] : -999.0f;

            pt.ssp_prof.resize(lev_size);
            for (size_t k = 0; k < lev_size; ++k) {
                float sspVal = grid_ssp[k * (Ny * Nx) + offset];
                pt.ssp_prof[k] = pt.is_valid ? sspVal : 0.0f;
            }
        } else {
            pt.is_valid = false;
            pt.bty_val = -999.0f; pt.sed_val = -999.0f;
            pt.ssp_prof.assign(lev_size, 0.0f);
        }
        sliceOutputs.push_back(pt);
    }
    return sliceOutputs;
}

// ==========================================
// 4b. 底质查表 TXT 文件读取
// ==========================================

SedimentTable readSedimentTable(const std::string& txtPath) {
    SedimentTable table;
    std::ifstream infile(txtPath);
    if (!infile.is_open()) {
        std::cerr << "[底质查表] ⚠ 无法打开 TXT 文件: " << txtPath
                  << " — 将使用 cfg 默认海底参数" << std::endl;
        return table;
    }

    std::string line;
    int lineNo = 0, loadedCount = 0;
    while (std::getline(infile, line)) {
        ++lineNo;
        // 跳过空行
        if (line.empty()) continue;
        // 跳过标题行
        if (lineNo == 1 && line.find("type") != std::string::npos) {
            std::cout << "[底质查表] TXT 标题: " << line << std::endl;
            continue;
        }

        std::istringstream iss(line);
        std::string token;
        std::vector<std::string> tokens;
        while (std::getline(iss, token, ',')) {
            tokens.push_back(token);
        }

        if (tokens.size() < 4) {
            std::cerr << "[底质查表] ⚠ 行 " << lineNo << " 字段不足 (需4列), 跳过: " << line << std::endl;
            continue;
        }

        try {
            int   type   = std::stoi(tokens[0]);
            float alphaR = std::stof(tokens[1]);
            float rho    = std::stof(tokens[2]);
            float alphaI = std::stof(tokens[3]);

            table[type] = {alphaR, rho, alphaI};
            ++loadedCount;
        } catch (const std::exception& e) {
            std::cerr << "[底质查表] ⚠ 行 " << lineNo << " 解析失败: " << e.what()
                      << " — 跳过: " << line << std::endl;
        }
    }

    std::cout << "[底质查表] ✓ TXT 加载完成: " << loadedCount << " 种底质类型 ("
              << txtPath << ")" << std::endl;
    return table;
}

// ==========================================
// 5. 按需局部流处理高层串联总控引擎
// ==========================================

// ==========================================
// 5a. 分解式 API 实现 — NC 缓存加载
// ==========================================

NcEnvCache loadNcEnvCache(const NcProcessConfig& cfg)
{
    NcEnvCache cache;
    if (!readBTYFile(cfg.bty_nc, cache.bty) ||
        !readSSPFile(cfg.ssp_nc, cache.ssp) ||
        !readSedFile(cfg.sed_nc, cache.sed)) {
        std::cerr << "[拦截崩溃] 环境原件读取不全！缓存加载失败。" << std::endl;
        return cache;  // loaded = false
    }
    cache.loaded = true;
    std::cout << "[缓存就绪] NC 原始环境数据已全部加载至内存。" << std::endl;
    return cache;
}

// ==========================================
// 5b. 分解式 API 实现 — 按源点构建局部网格
// ==========================================

LocalEnvGrid buildLocalEnvGrid(const NcEnvCache& cache,
                               double lon_0, double lat_0,
                               double r_max)
{
    LocalEnvGrid grid;
    if (!cache.loaded) {
        std::cerr << "[错误] NC 缓存未加载，无法构建局部网格。" << std::endl;
        return grid;
    }

    std::cout << "[核心跃迁] 系统已切换至局部视口 A 按需插值模式。波源物理零点: ("
              << lon_0 << "°, " << lat_0 << "°)" << std::endl;

    processLocalOnDemandInterpolate(cache.bty, cache.ssp, cache.sed,
        lon_0, lat_0, r_max,
        grid.grid_bty, grid.grid_sed, grid.grid_ssp,
        grid.Nx, grid.Ny);

    grid.lev_size = cache.ssp.lev_size;
    grid.lev      = cache.ssp.lev;   // 深拷贝深度轴，供后续 SSP 构建
    grid.built    = true;

    std::cout << "[网格构建完成] 局部画布: " << grid.Ny << " x " << grid.Nx
              << " 格点, " << grid.lev_size << " 水层" << std::endl;
    return grid;
}

// ==========================================
// 5c. 分解式 API 实现 — 从网格沿方向提取切片
// ==========================================

NcProcessResult extractSliceFromGrid(const LocalEnvGrid& grid,
                                     double theta,
                                     const NcProcessConfig& cfg)
{
    NcProcessResult result;

    if (!grid.built) {
        std::cerr << "[错误] 局部网格未构建，无法提取切片。" << std::endl;
        return result;
    }

    // 1. 射程跨度设定
    ProfileParam parm;
    parm.set_R(cfg.r_max, cfg.num_pts);

    // 2. 切片提取
    std::vector<ProfilePoint> profileSlice = extractProfileSliceFromLocalRegion(
        theta, parm, cfg.r_max,
        grid.Nx, grid.Ny, grid.lev_size,
        grid.grid_bty, grid.grid_sed, grid.grid_ssp
    );

    // 统计有效点数
    int validCount = 0;
    for (const auto& pt : profileSlice) if (pt.is_valid) validCount++;
    std::cout << "[Slice] theta=" << theta << "° 提取 " << profileSlice.size()
              << " 点, 有效 " << validCount << " 点" << std::endl;

    // 3. ProfilePoint → SSP + ati_bty 转换
    result.lev_size    = grid.lev_size;
    result.maxSspDepth = grid.lev.empty() ? 0.0f : grid.lev.back();

    for (const auto& pt : profileSlice) {
        if (!pt.is_valid) continue;

        SSP ssp;
        ssp.Distance = static_cast<float>(pt.dist / 1000.0);   // m → km
        ssp.zSSPV    = grid.lev;                                 // 深度轴（米）
        ssp.cSSPV    = pt.ssp_prof;                              // 声速（m/s）
        result.sspList.push_back(ssp);

        ati_bty bty;
        bty.x     = static_cast<float>(pt.dist / 1000.0);       // m → km
        bty.depth = pt.bty_val;                                  // 深度（米）
        result.btyPts.push_back(bty);

        result.sedVals.push_back(pt.sed_val);                    // 底质类型值（用于后续 TXT 查表）

        if (pt.bty_val > result.maxBtyDepth) {
            result.maxBtyDepth = pt.bty_val;
        }
    }

    result.success = true;
    return result;
}

// ==========================================
// 5d. 向后兼容入口 — buildBellhopEnvFromNc
//     (内部调用分解式 API，行为不变)
// ==========================================

/**
 * @brief 核心入口：从 NC 文件构建 Bellhop 环境数据（SSP + BTY）
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
NcProcessResult buildBellhopEnvFromNc(const NcProcessConfig& cfg)
{
    // 1. 加载 NC 原始数据
    NcEnvCache cache = loadNcEnvCache(cfg);
    if (!cache.loaded) {
        return NcProcessResult();  // success = false
    }
    std::cout << "--------------------------------------------------------" << std::endl;

    // 2. 构建局部插值网格
    LocalEnvGrid grid = buildLocalEnvGrid(cache, cfg.lon_0, cfg.lat_0, cfg.r_max);
    std::cout << "--------------------------------------------------------" << std::endl;

    // 3. 提取切片
    std::cout << "[切片提取激活] 局部视口行/列格点: " << grid.Ny << "x" << grid.Nx
              << "，系统正全层提取 " << cfg.num_pts << " 点垂直数据..." << std::endl;
    NcProcessResult result = extractSliceFromGrid(grid, cfg.theta, cfg);

    // 4. 自检单（保持原有详细输出）
    if (result.success) {
        // 重新提取 profileSlice 用于自检打印（与 extractSliceFromGrid 内部一致）
        ProfileParam parm;
        parm.set_R(cfg.r_max, cfg.num_pts);
        std::vector<ProfilePoint> profileSlice = extractProfileSliceFromLocalRegion(
            cfg.theta, parm, cfg.r_max,
            grid.Nx, grid.Ny, grid.lev_size,
            grid.grid_bty, grid.grid_sed, grid.grid_ssp
        );

        std::cout << "\n[准三维 (Nx2D) 优化版切片采样自检单 (抽样前 3 点数据明细)]:" << std::endl;
        std::cout << "===========================================================================================" << std::endl;
        std::cout << "控制点号  累计射程(米)    米制平面坐标(X,Y)     就近取整矩阵号[i,j]   海底地形(m)  表层声速(m/s)" << std::endl;
        std::cout << "===========================================================================================" << std::endl;
        for (int m = 0; m < 3; ++m) {
            if (static_cast<size_t>(m) >= profileSlice.size()) break;
            const auto& pt = profileSlice[m];
            std::cout << std::left << std::setw(10) << pt.idx
                      << std::setw(16) << std::fixed << std::setprecision(1) << pt.dist
                      << "(" << (int)pt.x << ", " << (int)pt.y << ")\t    "
                      << "[" << pt.i << ", " << pt.j << "]\t    "
                      << std::setw(13) << pt.bty_val
                      << pt.ssp_prof[0] << std::endl;
        }
        std::cout << "===========================================================================================" << std::endl;

        std::cout << "[🎉 流水线解算完成] 局部视口按需插值计算流圆满闭环！" << std::endl;
        std::cout << "  生成 SSP 剖面: " << result.sspList.size() << " 个" << std::endl;
        std::cout << "  生成 BTY 点:   " << result.btyPts.size() << " 个" << std::endl;
        std::cout << "  SSP 最大深度:  " << result.maxSspDepth << " m" << std::endl;
        std::cout << "  BTY 最大深度:  " << result.maxBtyDepth << " m" << std::endl;
    }

    return result;
}

// ==========================================
// 原 main() 已移除 — nc_process2 现为纯库模块，
// 入口由 test/main.cpp 通过 buildBellhopEnvFromNc() 调用
// ==========================================