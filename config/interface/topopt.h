#ifndef TOPOPT_H
#define TOPOPT_H
#include <iostream>
#include <string.h>

class topOpt;//前向声明

//选项一
class topOpt_firstVal
{
public:
    topOpt * tOption;
    void N2_linear_N();//N2 线性插值；
    void C_linear_C();//C-线性插值； （默认）
    void PCHIP_P();//分段三次埃尔米特插值多项式
    void Spline_S();//三次样条插值；
    void Quad_Q();//声速场二次逼近；要输入ssp矩阵
    //H和A不做

};

class topOpt_secondVal
{
public:
    topOpt * tOption;
    void Vacuum_V(); //表面以上为真空   （默认）
    void Rigid_R(); //表面以上为完全刚性介质
    //A,G,F,W,P不做
};

class topOpt_thirdVal
{
public:
    topOpt * tOption;
    void F(); //衰减单位采用(dB/m)kHz；
    void L(); //衰减单位采用参数损失；
    void M(); //衰减单位采用 dB/m；
    void N(); //衰减单位采用 Nepers/m；
    void Q(); //衰减单位采用 Q 因子；
    void W(); //衰减单位采用 dB/λ(波长) （默认）
};

class topOpt_forthVal
{
public:
    topOpt * tOption;
    void NoOption(); //空格
    void T(); 


};


class topOpt_fifthVal
{
public:
    topOpt * tOption;
    void NoneOption(); //空
    void useATI(); //表面输入

};


class topOpt
{
public:
    topOpt();

public:

    topOpt_firstVal *firstVal;
    topOpt_secondVal *secondVal;
    topOpt_thirdVal *thirdVal;
    topOpt_forthVal *forthVal;
    topOpt_fifthVal *fifthVal;
    std::string result;

};

#endif // TOPOPT_H
