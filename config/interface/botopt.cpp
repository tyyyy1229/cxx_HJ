#include "botopt.h"

botOpt::botOpt()
{
    this->firstVal = new botOpt_firstVal;
    this->firstVal->bOption = this;

    this->secondVal = new botOpt_secondVal;
    this->secondVal->bOption = this;


    this->result = "R-    "; //后面4个空格
}

void botOpt_firstVal::Vacuum_V()
{
    char result = 'V';
    this->bOption->result[0] = result;
}

void botOpt_firstVal::Rigid_R()
{
    char result = 'R';
    this->bOption->result[0] = result;
}

void botOpt_firstVal::acousto_elastic_A()
{
    char result = 'A';
    this->bOption->result[0] = result;
}
/*————————————————————————————————————————————————————————————————————————————————————————————————————————————*/
void botOpt_secondVal::NoneOption()
{
    char result = ' ';
    this->bOption->result[1] = result;
}

void botOpt_secondVal::useBTY()
{
    char result = '*';
    this->bOption->result[1] = result;
}
