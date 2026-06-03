#ifndef BOTOPT_H
#define BOTOPT_H
#include <iostream>
#include <string.h>
class botOpt;


class botOpt_firstVal
{
public:
    botOpt * bOption;
    void Vacuum_V(); //水体之下为真空   （默认）
    void Rigid_R(); //水体之下为完全刚性介质
    void acousto_elastic_A();//水体之下为声学半空间


};

class botOpt_secondVal
{
public:
    botOpt * bOption;
    void NoneOption(); //空格
    void useBTY(); //使用地形

};



class botOpt
{
public:
    botOpt();
    botOpt_firstVal *firstVal;
    botOpt_secondVal *secondVal;



    std::string result;


};

#endif // BOTOPT_H
