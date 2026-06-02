// #include "demo.h"


// int main()
// {
//     std::string filepathTL = "/home/wzw/bellhop_old1204/20251207/bellhopcxx_copy/output/TL_result";//不需要后缀
//     std::string filepathRay = "/home/wzw/bellhop_old1204/20251207/bellhopcxx_copy/output/Ray_result";
//  std::string filepathEigen = "/home/wzw/bellhop_old1204/20251207/bellhopcxx_copy/output/EigenRay_result";
// std::string filepathArr = "/home/wzw/bellhop_old1204/20251207/bellhopcxx_copy/output/Arr_result";//不需要后缀
//     // normalSea(filepathTL);
//     // test_eigenRay(filepathTL);l
//     test_flatwav_R(filepathRay);
//     // test_flatwav_E(filepathEigen);

//     // test_flatwav_C(fistlepathTL);
//     //  test_flatwav_A(filepathArr);
// }
#include <thread>
#include <chrono>
#include <ctime>
#include <algorithm> // 用于清理字符串中的单引号
#include <iostream>
#include <fstream>
#include "bellhopParam.h"//用的地方include这个头文件
#include "bhc/bhc.hpp"

void load_SSP(bellhopParam& parm, const std::string& filepath, const std::vector<float>& zVector) {
    std::ifstream infile(filepath);
    if (!infile.is_open()) {
        std::cerr << "Error: 无法打开声速文件 " << filepath << std::endl;
        return;
    }   

    int numRanges;
    infile >> numRanges;
    
    // 关键修正1：把第一行 175 后面的所有干扰文本（如 'Q'）全盘吃掉，防止卡死
    std::string dummyLine;
    std::getline(infile, dummyLine);

    int numDepths = zVector.size();
    std::vector<float> ranges(numRanges);
    for (int r = 0; r < numRanges; ++r) {
        infile >> ranges[r];
        // 关键修正2：如果遇到夹杂的文本导致读取失败，自动跳过它
        if (infile.fail()) {
            infile.clear(); // 清除报错状态
            std::string skipStr;
            infile >> skipStr; // 把非数字的字符串作为垃圾吃掉
            r--; // 让循环重试当前位置
            continue;
        }
    }

    // 读取巨大的二维声速矩阵
    std::vector<std::vector<float>> cMatrix(numDepths, std::vector<float>(numRanges));
    for (int d = 0; d < numDepths; ++d) {
        for (int r = 0; r < numRanges; ++r) {
            infile >> cMatrix[d][r];
            if (infile.fail()) {
                infile.clear();
                std::string skipStr;
                infile >> skipStr;
                r--;
            }
        }
    }

    // 组装成 bellhopParam 需要的结构
    std::vector<SSP> sspList;
    for (int r = 0; r < numRanges; ++r) {
        SSP ssp_col;
        ssp_col.Distance = ranges[r];
        ssp_col.zSSPV = zVector;
        
        std::vector<float> colSpeed(numDepths);
        for (int d = 0; d < numDepths; ++d) {
            colSpeed[d] = cMatrix[d][r];
        }
        ssp_col.cSSPV = colSpeed;
        
        sspList.push_back(ssp_col);
    }

    // 交给封装类
    parm.set_SSP(sspList);
    infile.close();
    
    // 关键修正3：打印前3个距离点，如果输出是 0, 0, 0 就说明文件格式还有大问题
    std::cout << "=== DEBUG SSP ===" << std::endl;
    std::cout << "成功组装 " << sspList.size() << " 个声速剖面" << std::endl;
    if (numRanges >= 3) {
        std::cout << "前三个距离点是: " << ranges[0] << ", " << ranges[1] << ", " << ranges[2] << std::endl;
    }
    std::cout << "=================" << std::endl;
}

void load_BTY(bellhopParam& parm, const std::string& filepath) {
    std::ifstream infile(filepath);
    if (!infile.is_open()) {
        std::cerr << "Error: 无法打开地形文件 " << filepath << std::endl;
        return;
    }

   std::string interpType;
    infile >> interpType;

    // 清理单引号，例如 "'LL'" -> "LL"
    std::string cleanType = interpType;
    cleanType.erase(std::remove(cleanType.begin(), cleanType.end(), '\''), cleanType.end());

    char interpChar = (cleanType.length() >= 1) ? cleanType[0] : 'L';  // 第一个字符：L/C
    char formatChar = (cleanType.length() >= 2) ? cleanType[1] : 'S';  // 第二个字符：S/L

    // 吞掉第一行后续的垃圾字符
    std::string dummyLine;
    std::getline(infile, dummyLine);

    int numPoints;
    infile >> numPoints;
    std::getline(infile, dummyLine); // 吞掉点数后面的垃圾字符

    std::vector<ati_bty> btyList;
    float last_range = -999999.0f; // 记录上一个点的距离

    for (int i = 0; i < numPoints; ++i) {
        float range, depth;
        infile >> range >> depth;

        // 【保护机制1】遇到表头文本卡死时，跳过该字符串并重试
        if (infile.fail()) {
            infile.clear();
            std::string skipStr;
            infile >> skipStr; 
            i--; 
            continue;
        }

        // ⚠️ 【核心修正】将文件中的公里(km)乘以 1000 转换为底层引擎所需要的米(m)
        //float range_meters = range * 1000.0f;

        // 【保护机制2】防止垂直悬崖或距离倒退 (在“米”单位下进行单调递增修正)
        // 如果当前距离小于或等于上一个点的距离，强制向后拉开 0.1 米 (即 10 厘米)
        if (range <= last_range) {
            range = last_range + 0.0001f; 
        }
        last_range = range; // 更新最后距离

        ati_bty pt;
        pt.x = range;
        pt.depth = depth;

        // // 【保护机制2】防止垂直悬崖或距离倒退 (单调递增修正)
        // // 如果当前距离小于或等于上一个点的距离，强制向后拉开 0.0001 km (约 10 厘米)
        // if (pt.x <= last_range) {
        //     pt.x = last_range + 0.0001f; 
        // }
        //last_range = pt.x; // 更新最后距离

        

        // 如果底质随距离变化，继续读取后5列参数
        // 如果是 long-format BTY，例如 'LL'，继续读取后 5 列 geoacoustic 参数
    if (formatChar == 'L') {
        infile >> pt.alphaR >> pt.betaR >> pt.rho >> pt.alphaI >> pt.betaI;

    if (infile.fail()) {
        infile.clear();
        std::cerr << "Warning: 读取 BTY long-format 底质参数失败，第 "
                  << i << " 个点使用默认底质参数。" << std::endl;

        pt.alphaR = 1500.0f;
        pt.betaR  = 0.0f;
        pt.rho    = 1.0f;
        pt.alphaI = 0.0f;
        pt.betaI  = 0.0f;

        std::getline(infile, dummyLine);
    }
    
}
    btyList.push_back(pt);
    }

    // 交给封装类去处理
    parm.set_btyPts(btyList); 
    parm.set_btyType(interpChar, formatChar); // 告诉封装类 BTY 的类型，方便它正确调用底层接口
    infile.close();

    // 打印调试信息，验证地形是否被正确读取和排列
    std::cout << "=== DEBUG BTY ===" << std::endl;
    std::cout << "成功组装 " << btyList.size() << " 个地形点" << std::endl;
    if (btyList.size() >= 3) {
        std::cout << "BTY type = " << interpChar << formatChar << std::endl;
std::cout << "成功组装 " << btyList.size() << " 个地形点" << std::endl;

if (btyList.size() >= 3) {
    std::cout << "前三个点距离(km): "
              << btyList[0].x << ", "
              << btyList[1].x << ", "
              << btyList[2].x << std::endl;

    std::cout << "前三个点深度(m): "
              << btyList[0].depth << ", "
              << btyList[1].depth << ", "
              << btyList[2].depth << std::endl;

    std::cout << "第一个点底质: alphaR="
              << btyList[0].alphaR
              << ", betaR=" << btyList[0].betaR
              << ", rho=" << btyList[0].rho
              << ", alphaI=" << btyList[0].alphaI
              << ", betaI=" << btyList[0].betaI
              << std::endl;
}
    }
    std::cout << "=================" << std::endl;
}
// === 新增：读取当前程序占用的物理内存 ===
void printMemoryUsage(int step) {
    std::ifstream statusFile("/proc/self/status");
    std::string line;
    while (std::getline(statusFile, line)) {
        if (line.substr(0, 6) == "VmRSS:") {
            std::cout << ">>> 第 " << step << " 次运行结束 | 当前物理内存占用:" << line.substr(6) << std::endl;
            break;
        }
    }
}

int main()
{

    // std::cout << "开始内存泄漏测试..." << std::endl;

    // // 写一个循环，连续运行6次
    // for(int i = 1; i <= 1; i++) {
    //     std::cout << "\n--- 正在执行第 " << i << " 次 ---" << std::endl;
    
   auto t0 = std::chrono::steady_clock::now();
  
  bellhopParam parm ;
    parm.set_freq(5000);
    parm.set_NMedia(1);

    // std::vector<SSP> SSPLIst; 
    // SSP ssp1; 
    // // 第一列（zSSPV1）：水深值；第二列（cSSPV1）：对应水深的声速值
    // // 第一列（zSSPV1）：水深值；第二列（cSSPV1）：对应水深的声速值
    // //std::vector<float> zSSPV1 = {0.0, 200.0, 250.0, 400.0, 600.0, 800.0, 1000.0, 1200.0, 1400.0, 1600.0, 1800.0, 2000.0, 2200.0, 2400.0, 2600.0, 2800.0, 3000.0, 3200.0, 3400.0, 3600.0, 3800.0, 4000.0, 4200.0, 4400.0, 4600.0, 4800.0, 5000.0};
    //    std::vector<float> zSSPV1 = {
    //             0.0, 5.0, 10.0, 15.0, 20.0, 25.0, 50.0, 75.0, 100.0, 125.0, 
    //             150.0, 175.0, 200.0, 225.0, 250.0, 275.0, 300.0, 325.0, 350.0, 375.0, 
    //             400.0, 500.0, 600.0, 700.0, 800.0, 900.0, 1000.0, 1100.0, 1200.0, 1300.0, 
    //             1400.0, 1500.0, 1750.0, 2000.0, 2250.0, 2500.0, 2750.0, 3000.0, 3250.0, 3500.0, 
    //             4000.0};
    // std::vector<float> cSSPV1 ={
    //             1536.024,1535.712,1535.287,1535.086,1535.073,1535.068,1534.973,1532.897,1528.087,
    //             1521.965,1517.203,1512.817,1508.673,1505.207,1502.39,1500.05,1498.058,1496.342,1494.846,
    //             1493.537,1492.371,1488.822,1486.408,1484.914,1484.483,1484.487,1484.537,1484.731,1485.149,
    //             1485.661,1486.203,1486.967,1489.823,1492.931,1496.489,1500.509,1504.792,1509.187,1513.569,1517.95,1500};
    // // {1548.52, 1530.29, 1526.69, 1517.78, 1509.49, 1504.30, 1501.38, 1500.14, 1500.12, 1501.02, 1502.57, 1504.62, 1507.02, 1509.69, 1512.55, 1515.56, 1518.67, 1521.85, 1525.10, 1528.38, 1531.70, 1535.04, 1538.39, 1541.76, 1545.14, 1548.52, 1551.91};
    // ssp1.zSSPV = zSSPV1; 
    // ssp1.cSSPV = cSSPV1; 
    // ssp1.Distance = 0;
    // SSPLIst.push_back(ssp1); 
    // parm.set_SSP(SSPLIst);

  
   std::vector<float> zSSPV1 = {
    0.0, 5.0, 10.0, 15.0, 20.0, 25.0, 50.0, 75.0, 100.0, 125.0, 
    150.0, 175.0, 200.0, 225.0, 250.0, 275.0, 300.0, 325.0, 350.0, 375.0, 
    400.0, 500.0, 600.0, 700.0, 800.0, 900.0, 1000.0, 1100.0, 1200.0, 1300.0, 
    1400.0, 1500.0, 1750.0, 2000.0, 2250.0, 2500.0, 2750.0, 3000.0, 3250.0, 3500.0, 
    4000.0};
    //读声速梯度
    load_SSP(parm,"/home/ty/bellhopcxx/bellhopcxx_copy/00013.ssp",zSSPV1);

   
    
    parm.topopt->firstVal->Quad_Q();
    parm.topopt->secondVal->Vacuum_V();
    parm.topopt->thirdVal->M();
    parm.topopt->forthVal->T();
    parm.topopt->fifthVal->NoneOption();

    //depth深度、alphaR纵波波速，betaR横波波速，rho密度，alphaI纵波吸收，betaI横波吸收
    Boundary_Line bottomLine;
    bottomLine.alphaI=0.0763;
    bottomLine.alphaR=1500;
    bottomLine.betaI=0;
    bottomLine.betaR=0;
    bottomLine.rho=1.4;
    bottomLine.Depth= 4000;
    parm.set_bottomLine(bottomLine);

    parm.botopt->firstVal->acousto_elastic_A();
    parm.botopt->secondVal->useBTY();
    
    //读地形
    load_BTY(parm,"/home/ty/bellhopcxx/bellhopcxx_copy/00013.bty");

     

    parm.set_SD(1300,1);//声源1000m

    parm.set_RD(4000,500);//接收深度4000m

    parm.set_R(80000,800);//传播距离500km

    parm.set_NBeam(80000);//声线条数

     
    parm.set_angle({-30,30});//射线开角

    parm.set_stepLength(1);//差分步距，1m，默认500
    parm.set_zBox(5500);
    parm.set_rBox(80);


    parm.RunType->firstVal->Coherent_TL_C();
    parm.RunType->secondVal->Gaussian_beam_B();
    parm.RunType->thirdVal->NoneOption();
    parm.RunType->forthVal->NoneOption();
    parm.RunType->fifthVal->NoneOption();

    parm.set_PrtFile("test_c4");//不需要后缀
    parm.runMod();

    //parm.exportTL(parm.get_TLField(),"test_c3");
    
    //parm.exportTL_P(parm.get_TLField_P(),"test_C3");//不需要后缀

    bhc::writeout(*(parm.bhc_Params),parm.bhc_OutPut,"test_c4");

    //    bellhopParam parm ;
    // parm.set_freq(50);
    // parm.set_NMedia(1);

    // std::vector<SSP> SSPLIst; 
    // SSP ssp1; 
    // // 第一列（zSSPV1）：水深值；第二列（cSSPV1）：对应水深的声速值
    // // 第一列（zSSPV1）：水深值；第二列（cSSPV1）：对应水深的声速值
    // std::vector<float> zSSPV1 = {0.0, 200.0, 250.0, 400.0, 600.0, 800.0, 1000.0, 1200.0, 1400.0, 1600.0, 1800.0, 2000.0, 2200.0, 2400.0, 2600.0, 2800.0, 3000.0, 3200.0, 3400.0, 3600.0, 3800.0, 4000.0, 4200.0, 4400.0, 4600.0, 4800.0, 5000.0};
    // std::vector<float> cSSPV1 = {1548.52, 1530.29, 1526.69, 1517.78, 1509.49, 1504.30, 1501.38, 1500.14, 1500.12, 1501.02, 1502.57, 1504.62, 1507.02, 1509.69, 1512.55, 1515.56, 1518.67, 1521.85, 1525.10, 1528.38, 1531.70, 1535.04, 1538.39, 1541.76, 1545.14, 1548.52, 1551.91};
    // ssp1.zSSPV = zSSPV1; 
    // ssp1.cSSPV = cSSPV1; 
    // ssp1.Distance = 0;
    // SSPLIst.push_back(ssp1); 
    // parm.set_SSP(SSPLIst);

    // parm.topopt->firstVal->C_linear_C();
    // parm.topopt->secondVal->Vacuum_V();
    // parm.topopt->thirdVal->W();
    // parm.topopt->forthVal->NoOption();
    // parm.topopt->fifthVal->NoneOption();

    // parm.botopt->firstVal->acousto_elastic_A();
    // parm.botopt->secondVal->NoneOption();

    //  //depth深度、alphaR纵波波速，betaR横波波速，rho密度，alphaI纵波吸收，betaI横波吸收
    // Boundary_Line bottomLine;
    // bottomLine.alphaI=0.0;
    // bottomLine.alphaR=1600;
    // bottomLine.betaI=0;
    // bottomLine.betaR=0;
    // bottomLine.rho=1.8;
    // bottomLine.Depth=5000;
    // parm.set_bottomLine(bottomLine);
    // parm.set_SD(1000,1);//声源1000m
    // parm.set_RD(1000,1);//接收深度6000m
    // parm.set_R(101000,1);//传播距离500km
    // parm.set_NBeam(101);//声线条数

    
    // parm.set_angle({-14,14});//射线开角

    // parm.set_stepLength(100);//差分步距，1m，默认500
    // parm.set_zBox(5500);
    // parm.set_rBox(102);


    // parm.RunType->firstVal->Arrivals_A();
    // parm.RunType->secondVal->Geometric_beam_G();
    // parm.RunType->thirdVal->NoneOption();
    // parm.RunType->forthVal->Point_source_R();
    // parm.RunType->fifthVal->Rectilinear_grid_R();

    // parm.runMod();



    printf("GCC Version: %d.%d.%d\n", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
    std::cout << "当前编译器路径: /usr/bin/gcc" << std::endl;

    auto t1 = std::chrono::steady_clock::now();

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    std::cout << "耗时：" << ms << " ms" << std::endl;

    // // 调用探针打印内存
    //     printMemoryUsage(i);

    //     // 暂停2秒，留给你在终端观察的时间
    //     std::this_thread::sleep_for(std::chrono::seconds(1));
    // }
    return 0;
}