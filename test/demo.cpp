#include "demo.h"

void normalSea(std::string filepath)
{
    bellhopParam parm ;
    parm.set_freq(500);
    parm.set_NMedia(1);


    std::vector<SSP> SSPLIst; 
    SSP ssp1; 

    std::vector<float> zSSPV1 = {0,50.7,100.4,150.1,250.5,500,1000,2000,3000,4000,5000,6000};
    std::vector<float> cSSPV1 = {1542.4,1529.5,1514.1,1509.5,1501.5,1500,1480,1490,1505,1520,1535,1550};
    ssp1.zSSPV = zSSPV1; 
    ssp1.cSSPV = cSSPV1; 
    ssp1.Distance = 0;
    SSPLIst.push_back(ssp1); 
    parm.set_SSP(SSPLIst);

    parm.topopt->firstVal->Spline_S();
    parm.topopt->secondVal->Vacuum_V();
    parm.topopt->thirdVal->W();
    parm.topopt->forthVal->NoOption();
    parm.topopt->fifthVal->NoneOption();

    parm.botopt->firstVal->acousto_elastic_A();
    parm.botopt->secondVal->NoneOption();
    std::vector<ati_bty>bty={{0,6050},{60,6050}};
    parm.set_btyPts(bty);

    //对应env顺序里面是，
    //depth深度、alphaR纵波波速，betaR横波波速，rho密度，alphaI纵波吸收，betaI横波吸收
    Boundary_Line bottomLine;
    bottomLine.alphaI=0.5;
    bottomLine.alphaR=1700;
    bottomLine.betaI=0;
    bottomLine.betaR=0;
    bottomLine.rho=1.5;
    bottomLine.Depth=6000;
    parm.set_bottomLine(bottomLine);



    parm.set_SD(500.0,1);//声源500m

    parm.set_RD(6000,1001);//接收深度6000m

    parm.set_R(500000,501);//传播距离500km

    parm.set_NBeam(1500);//声线条数

    
    parm.set_angle({-89,89});//射线开角

    parm.set_stepLength(1);//差分步距，1m，默认500
    parm.set_zBox(6050);
    parm.set_rBox(501);


    parm.RunType->firstVal->Coherent_TL_C();
    parm.RunType->secondVal->Geometric_beam_G();
    parm.RunType->thirdVal->NoneOption();
    parm.RunType->forthVal->Point_source_R();
    parm.RunType->fifthVal->Rectilinear_grid_R();

    parm.runMod();
    
    parm.exportTL(parm.get_TLField(),filepath);//不需要后缀
    

}


void DickinsB(std::string filepath)
{
    bellhopParam parm ;
    parm.set_freq(230);
    parm.set_NMedia(1);



    parm.topopt->firstVal->C_linear_C();
    parm.topopt->secondVal->Vacuum_V();
    parm.topopt->thirdVal->W();
    parm.topopt->forthVal->NoOption();
    parm.topopt->fifthVal->NoneOption();

    std::vector<SSP> SSPLIst; 
    SSP ssp1; 

    std::vector<float> zSSPV1 = {0,38,50,70,100,140,160,170,200,215,250,300,370,450,500,700,900,1000,1250,1500,2000,2500,3000};
    std::vector<float> cSSPV1 = {1476.7,1476.7,1472.6,1468.8,1467.2,1471.6,1473.6,1473.6,1472.7,1472.2,1471.6,1471.6,1472.0,1472.7,1473.1,1474.9,1477.0,1478.1,1480.7,1483.8,1490.5,1498.3,1506.5};
    
    ssp1.zSSPV = zSSPV1; 
    ssp1.cSSPV = cSSPV1; 
    ssp1.Distance = 0;
    SSPLIst.push_back(ssp1); 
    parm.set_SSP(SSPLIst);



    parm.botopt->firstVal->acousto_elastic_A();
    parm.botopt->secondVal->useBTY();
    std::vector<ati_bty>bty={{0,3000},{10,3000},{20,500},{30,3000},{100,3000}};
    parm.set_btyPts(bty);

    //对应env顺序里面是，
    //depth深度、alphaR纵波波速，betaR横波波速，rho密度，alphaI纵波吸收，betaI横波吸收
    Boundary_Line bottomLine;
    bottomLine.Depth=3000;
    bottomLine.alphaI=0.5;
    bottomLine.alphaR=1550;
    bottomLine.betaI=0;
    bottomLine.betaR=0;
    bottomLine.rho=1.0;
    parm.set_bottomLine(bottomLine);



    parm.set_SD(18.0,1);//声源500m

    parm.set_RD(3000,601);//接收深度6000m

    parm.set_R(100000,1001);//传播距离500km

    parm.set_NBeam(4600);//声线条数

    
    parm.set_angle({-89,89});//射线开角

    parm.set_stepLength(0);//差分步距，1m，默认500
    parm.set_zBox(3100);
    parm.set_rBox(101);


    parm.RunType->firstVal->Arrivals_A();
    parm.RunType->secondVal->Gaussian_beam_B();
    parm.RunType->thirdVal->NoneOption();
    parm.RunType->forthVal->Point_source_R();
    parm.RunType->fifthVal->Rectilinear_grid_R();


    parm.runMod();
    std::vector<std::vector<arrinfo>> result= parm.get_TimeDelay();
    // parm.exportTL(parm.get_TLField(),filepath);//不需要后缀
    



}

//测试本征声线
void test_eigenRay(std::string filepath)
{
        bellhopParam parm ;
    parm.set_freq(230);
    parm.set_NMedia(1);



    parm.topopt->firstVal->C_linear_C();
    parm.topopt->secondVal->Vacuum_V();
    parm.topopt->thirdVal->W();
    parm.topopt->forthVal->NoOption();
    parm.topopt->fifthVal->NoneOption();

    std::vector<SSP> SSPLIst; 
    SSP ssp1; 

    std::vector<float> zSSPV1 = {0,38,50,70,100,140,160,170,200,215,250,300,370,450,500,700,900,1000,1250,1500,2000,2500,3000};
    std::vector<float> cSSPV1 = {1476.7,1476.7,1472.6,1468.8,1467.2,1471.6,1473.6,1473.6,1472.7,1472.2,1471.6,1471.6,1472.0,1472.7,1473.1,1474.9,1477.0,1478.1,1480.7,1483.8,1490.5,1498.3,1506.5};
    
    ssp1.zSSPV = zSSPV1; 
    ssp1.cSSPV = cSSPV1; 
    ssp1.Distance = 0;
    SSPLIst.push_back(ssp1); 
    parm.set_SSP(SSPLIst);



    parm.botopt->firstVal->acousto_elastic_A();
    parm.botopt->secondVal->useBTY();
    std::vector<ati_bty>bty={{0,3000},{10,3000},{20,500},{30,3000},{100,3000}};
    parm.set_btyPts(bty);

    //对应env顺序里面是，
    //depth深度、alphaR纵波波速，betaR横波波速，rho密度，alphaI纵波吸收，betaI横波吸收
    Boundary_Line bottomLine;
    bottomLine.Depth=3000;
    bottomLine.alphaI=0.5;
    bottomLine.alphaR=1550;
    bottomLine.betaI=0;
    bottomLine.betaR=0;
    bottomLine.rho=1.0;
    parm.set_bottomLine(bottomLine);


    std::vector<float> sd = {18};
    std::vector<float> rd = {3000};
    std::vector<float> rr = {100000};

    parm.set_SD(sd);//声源500m
    parm.set_RD(rd);//接收深度6000m
    parm.set_R(rr);//传播距离500km

    parm.set_NBeam(4600);//声线条数

    
    parm.set_angle({-89,89});//射线开角

    parm.set_stepLength(0);//差分步距，1m，默认500
    parm.set_zBox(3100);
    parm.set_rBox(101);


    parm.RunType->firstVal->Eigenray_trace_E();
    parm.RunType->secondVal->Gaussian_beam_B();
    parm.RunType->thirdVal->NoneOption();
    parm.RunType->forthVal->Point_source_R();
    parm.RunType->fifthVal->Rectilinear_grid_R();


    parm.runMod();
    std::vector<rayInfo> rays = parm.get_Ray();
    for(auto& ray : rays)
    {
        for(auto& pos : ray.ray)
        {
                std::cout<<pos.x<<" "<<pos.y<<std::endl;
        }

    }

}



//测试声线轨迹wzw
void test_flatwav_R(std::string filepath)
{
    bellhopParam parm ;
    parm.set_freq(50);
    parm.set_NMedia(1);

     std::vector<SSP> SSPLIst; 
    SSP ssp1; 
// 第一列（zSSPV1）：水深值；第二列（cSSPV1）：对应水深的声速值
std::vector<float> zSSPV1 = {0.0, 200.0, 250.0, 400.0, 600.0, 800.0, 1000.0, 1200.0, 1400.0, 1600.0, 1800.0, 2000.0, 2200.0, 2400.0, 2600.0, 2800.0, 3000.0, 3200.0, 3400.0, 3600.0, 3800.0, 4000.0, 4200.0, 4400.0, 4600.0, 4800.0, 5000.0};
std::vector<float> cSSPV1 = {1548.52, 1530.29, 1526.69, 1517.78, 1509.49, 1504.30, 1501.38, 1500.14, 1500.12, 1501.02, 1502.57, 1504.62, 1507.02, 1509.69, 1512.55, 1515.56, 1518.67, 1521.85, 1525.10, 1528.38, 1531.70, 1535.04, 1538.39, 1541.76, 1545.14, 1548.52, 1551.91};
ssp1.zSSPV = zSSPV1; 
    ssp1.cSSPV = cSSPV1; 
    ssp1.Distance = 0;
    SSPLIst.push_back(ssp1); 
    parm.set_SSP(SSPLIst);

    parm.topopt->firstVal->C_linear_C();
    parm.topopt->secondVal->Vacuum_V();
    parm.topopt->thirdVal->W();
    parm.topopt->forthVal->NoOption();
    parm.topopt->fifthVal->NoneOption();

    parm.botopt->firstVal->acousto_elastic_A();
    parm.botopt->secondVal->NoneOption();


    //depth深度、alphaR纵波波速，betaR横波波速，rho密度，alphaI纵波吸收，betaI横波吸收
    Boundary_Line bottomLine;
    bottomLine.alphaI=0.8;
    bottomLine.alphaR=1600;
    bottomLine.betaI=0;
    bottomLine.betaR=0;
    bottomLine.rho=1.8;
    bottomLine.Depth=5000;
    parm.set_bottomLine(bottomLine);

     parm.set_SD(1000,1);//声源500m

    parm.set_RD(1000,1);//接收深度6000m

    parm.set_R(101000,1);//传播距离500km

    parm.set_NBeam(70);//声线条数

    
    parm.set_angle({-13,13});//射线开角

    parm.set_stepLength(100);//差分步距，1m，默认500
    parm.set_zBox(5500);
    parm.set_rBox(102);
    parm.RunType->firstVal->Ray_trace_R();
    parm.RunType->secondVal->Geometric_beam_G();
    parm.RunType->thirdVal->NoneOption();
    parm.RunType->forthVal->Point_source_R();
    parm.RunType->fifthVal->Rectilinear_grid_R();

    parm.runMod();
    
    parm.exportRay(parm.get_Ray(),filepath);//不需要后缀

}


//测试本征声线wzw
void test_flatwav_E(std::string filepath)
{
    bellhopParam parm ;
    parm.set_freq(50);
    parm.set_NMedia(1);

     std::vector<SSP> SSPLIst; 
    SSP ssp1; 
// 第一列（zSSPV1）：水深值；第二列（cSSPV1）：对应水深的声速值
std::vector<float> zSSPV1 = {0.0, 200.0, 250.0, 400.0, 600.0, 800.0, 1000.0, 1200.0, 1400.0, 1600.0, 1800.0, 2000.0, 2200.0, 2400.0, 2600.0, 2800.0, 3000.0, 3200.0, 3400.0, 3600.0, 3800.0, 4000.0, 4200.0, 4400.0, 4600.0, 4800.0, 5000.0};
std::vector<float> cSSPV1 = {1548.52, 1530.29, 1526.69, 1517.78, 1509.49, 1504.30, 1501.38, 1500.14, 1500.12, 1501.02, 1502.57, 1504.62, 1507.02, 1509.69, 1512.55, 1515.56, 1518.67, 1521.85, 1525.10, 1528.38, 1531.70, 1535.04, 1538.39, 1541.76, 1545.14, 1548.52, 1551.91};
ssp1.zSSPV = zSSPV1; 
    ssp1.cSSPV = cSSPV1; 
    ssp1.Distance = 0;
    SSPLIst.push_back(ssp1); 
    parm.set_SSP(SSPLIst);

    parm.topopt->firstVal->C_linear_C();
    parm.topopt->secondVal->Vacuum_V();
    parm.topopt->thirdVal->W();
    parm.topopt->forthVal->NoOption();
    parm.topopt->fifthVal->NoneOption();

    parm.botopt->firstVal->acousto_elastic_A();
    parm.botopt->secondVal->NoneOption();


    //depth深度、alphaR纵波波速，betaR横波波速，rho密度，alphaI纵波吸收，betaI横波吸收
    Boundary_Line bottomLine;
    bottomLine.alphaI=0.8;
    bottomLine.alphaR=1600;
    bottomLine.betaI=0;
    bottomLine.betaR=0;
    bottomLine.rho=1.8;
    bottomLine.Depth=5000;
    parm.set_bottomLine(bottomLine);

     parm.set_SD(1000,1);//声源500m

    parm.set_RD(1000,1);//接收深度6000m

    parm.set_R(101000,1);//传播距离500km

    parm.set_NBeam(70);//声线条数

    
    parm.set_angle({-13,13});//射线开角

    parm.set_stepLength(100);//差分步距，1m，默认500
    parm.set_zBox(5500);
    parm.set_rBox(102);
    parm.RunType->firstVal->Eigenray_trace_E();
    parm.RunType->secondVal->Geometric_beam_G();
    parm.RunType->thirdVal->NoneOption();
    parm.RunType->forthVal->Point_source_R();
    parm.RunType->fifthVal->Rectilinear_grid_R();

    parm.runMod();
    
    parm.exportRay(parm.get_Ray(),filepath);//不需要后缀

}


//测试传播损失wzw
void test_flatwav_C(std::string filepath){
    bellhopParam parm ;
    parm.set_freq(50);
    parm.set_NMedia(1);

    std::vector<SSP> SSPLIst; 
    SSP ssp1; 
    // 第一列（zSSPV1）：水深值；第二列（cSSPV1）：对应水深的声速值
    // 第一列（zSSPV1）：水深值；第二列（cSSPV1）：对应水深的声速值
std::vector<float> zSSPV1 = {0.0, 200.0, 250.0, 400.0, 600.0, 800.0, 1000.0, 1200.0, 1400.0, 1600.0, 1800.0, 2000.0, 2200.0, 2400.0, 2600.0, 2800.0, 3000.0, 3200.0, 3400.0, 3600.0, 3800.0, 4000.0, 4200.0, 4400.0, 4600.0, 4800.0, 5000.0};
std::vector<float> cSSPV1 = {1548.52, 1530.29, 1526.69, 1517.78, 1509.49, 1504.30, 1501.38, 1500.14, 1500.12, 1501.02, 1502.57, 1504.62, 1507.02, 1509.69, 1512.55, 1515.56, 1518.67, 1521.85, 1525.10, 1528.38, 1531.70, 1535.04, 1538.39, 1541.76, 1545.14, 1548.52, 1551.91};
ssp1.zSSPV = zSSPV1; 
    ssp1.cSSPV = cSSPV1; 
    ssp1.Distance = 0;
    SSPLIst.push_back(ssp1); 
    parm.set_SSP(SSPLIst);

       parm.topopt->firstVal->C_linear_C();
    parm.topopt->secondVal->Vacuum_V();
    parm.topopt->thirdVal->W();
    parm.topopt->forthVal->NoOption();
    parm.topopt->fifthVal->NoneOption();

    parm.botopt->firstVal->acousto_elastic_A();
    parm.botopt->secondVal->NoneOption();

     //depth深度、alphaR纵波波速，betaR横波波速，rho密度，alphaI纵波吸收，betaI横波吸收
    Boundary_Line bottomLine;
    bottomLine.alphaI=0.0;
    bottomLine.alphaR=1600;
    bottomLine.betaI=0;
    bottomLine.betaR=0;
    bottomLine.rho=1.8;
    bottomLine.Depth=5000;
    parm.set_bottomLine(bottomLine);

    parm.set_SD(1000,1);//声源1000m

    parm.set_RD(5000,501);//接收深度6000m

    parm.set_R(101000,10001);//传播距离500km

    parm.set_NBeam(1000);//声线条数

    
    parm.set_angle({-14,14});//射线开角

    parm.set_stepLength(100);//差分步距，1m，默认500
    parm.set_zBox(5500);
    parm.set_rBox(102);


    parm.RunType->firstVal->Coherent_TL_C();
    parm.RunType->secondVal->Geometric_beam_G();
    parm.RunType->thirdVal->NoneOption();
    parm.RunType->forthVal->Point_source_R();
    parm.RunType->fifthVal->Rectilinear_grid_R();

    parm.runMod();
    
    parm.exportTL_P(parm.get_TLField_P(),filepath);//不需要后缀

}

//测试到达结构wzw
void test_flatwav_A(std::string filepath){
    bellhopParam parm ;
    parm.set_freq(50);
    parm.set_NMedia(1);

    std::vector<SSP> SSPLIst; 
    SSP ssp1; 
    // 第一列（zSSPV1）：水深值；第二列（cSSPV1）：对应水深的声速值
    // 第一列（zSSPV1）：水深值；第二列（cSSPV1）：对应水深的声速值
std::vector<float> zSSPV1 = {0.0, 200.0, 250.0, 400.0, 600.0, 800.0, 1000.0, 1200.0, 1400.0, 1600.0, 1800.0, 2000.0, 2200.0, 2400.0, 2600.0, 2800.0, 3000.0, 3200.0, 3400.0, 3600.0, 3800.0, 4000.0, 4200.0, 4400.0, 4600.0, 4800.0, 5000.0};
std::vector<float> cSSPV1 = {1548.52, 1530.29, 1526.69, 1517.78, 1509.49, 1504.30, 1501.38, 1500.14, 1500.12, 1501.02, 1502.57, 1504.62, 1507.02, 1509.69, 1512.55, 1515.56, 1518.67, 1521.85, 1525.10, 1528.38, 1531.70, 1535.04, 1538.39, 1541.76, 1545.14, 1548.52, 1551.91};
ssp1.zSSPV = zSSPV1; 
    ssp1.cSSPV = cSSPV1; 
    ssp1.Distance = 0;
    SSPLIst.push_back(ssp1); 
    parm.set_SSP(SSPLIst);

       parm.topopt->firstVal->C_linear_C();
    parm.topopt->secondVal->Vacuum_V();
    parm.topopt->thirdVal->W();
    parm.topopt->forthVal->NoOption();
    parm.topopt->fifthVal->NoneOption();

    parm.botopt->firstVal->acousto_elastic_A();
    parm.botopt->secondVal->NoneOption();

     //depth深度、alphaR纵波波速，betaR横波波速，rho密度，alphaI纵波吸收，betaI横波吸收
    Boundary_Line bottomLine;
    bottomLine.alphaI=0.0;
    bottomLine.alphaR=1600;
    bottomLine.betaI=0;
    bottomLine.betaR=0;
    bottomLine.rho=1.8;
    bottomLine.Depth=5000;
    parm.set_bottomLine(bottomLine);

    parm.set_SD(1000,1);//声源1000m

    parm.set_RD(1000,1);//接收深度6000m

    parm.set_R(101000,1);//传播距离500km

    parm.set_NBeam(101);//声线条数

    
    parm.set_angle({-14,14});//射线开角

    parm.set_stepLength(100);//差分步距，1m，默认500
    parm.set_zBox(5500);
    parm.set_rBox(102);


    parm.RunType->firstVal->Arrivals_A();
    parm.RunType->secondVal->Geometric_beam_G();
    parm.RunType->thirdVal->NoneOption();
    parm.RunType->forthVal->Point_source_R();
    parm.RunType->fifthVal->Rectilinear_grid_R();

    parm.runMod();
    
    parm.exportArr(parm.get_TimeDelay(),filepath);//不需要后缀

}