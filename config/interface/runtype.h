#ifndef RUNTYPE_H
#define RUNTYPE_H
#include <iostream>
#include <string.h>

class runtype;

class runtype_firstVal
{
public:
    runtype *rt;
    void Ray_trace_R(); //ray 射线坐标
    void Eigenray_trace_E(); //本征射线坐标
    void Coherent_TL_C(); //相干声压
    void Arrivals_A(); //振幅和传播时间(时延)
    void Incoherent_TL_I(); //非相干声压
    void Semi_coherent_TL_S(); //相干声压

    //小写a不做
};

class runtype_secondVal
{
public:
    runtype *rt;
    void Geometric_beam_G(); //笛卡尔坐标系的几何坐标 默认
    void Cartesian_beams_C();//Cerveny高斯波束的笛卡尔坐标系形式
    void Ray_centered_beam_R();//Cerveny高斯波束的射线中心形式
    void Gaussian_beam_B(); //笛卡尔坐标中的几何高斯波束

};

class runtype_thirdVal
{
public:
    runtype *rt;
    void NoneOption();
    //波束文件不做
};

class runtype_forthVal
{
public:
    runtype *rt;
    void Point_source_R(); //圆柱坐标系中的点源 （默认）
    void Line_source_X(); //笛卡尔坐标系中的线源
    void NoneOption();

};

class runtype_fifthVal
{
public:
    runtype *rt;
    void Irregular_grid_I();//不规则网格
    void Rectilinear_grid_R();//线列接收阵网格 （默认）
    void NoneOption();

};






class runtype
{
public:
    runtype();

    runtype_firstVal *firstVal;
    runtype_secondVal *secondVal;
    runtype_thirdVal *thirdVal;
    runtype_forthVal *forthVal;
    runtype_fifthVal *fifthVal;


    std::string result;

};

#endif // RUNTYPE_H
