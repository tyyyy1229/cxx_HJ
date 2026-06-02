#include "topopt.h"

topOpt::topOpt()
{
    this->firstVal = new topOpt_firstVal;
    this->firstVal->tOption = this;
    this->secondVal= new topOpt_secondVal;
    this->secondVal->tOption = this;

    this->thirdVal= new topOpt_thirdVal;
    this->thirdVal->tOption = this;

    this->forthVal= new topOpt_forthVal;
    this->forthVal->tOption = this;

    this->fifthVal= new topOpt_fifthVal;
    this->fifthVal->tOption = this;


    this->result = "CVW   ";//默认，CVW空格空格空格
}

/*————————————————————————————————————————————————————————————————————————————————————————————————————————————*/

void topOpt_firstVal::N2_linear_N()
{
    char result = 'N';
    this->tOption->result[0] = result;
}

void topOpt_firstVal::C_linear_C()
{
    char result = 'C';
    this->tOption->result[0] = result;
}

void topOpt_firstVal::PCHIP_P()
{
    char result = 'P';
    this->tOption->result[0] = result;
}

void topOpt_firstVal::Spline_S()
{
    char result = 'S';
    this->tOption->result[0] = result;
}

void topOpt_firstVal::Quad_Q()
{
    char result = 'Q';
    this->tOption->result[0] = result;
}

void topOpt_secondVal::Vacuum_V()
{
    char result = 'V';
    this->tOption->result[1] = result;
}

void topOpt_secondVal::Rigid_R()
{
    char result = 'R';
    this->tOption->result[1] = result;
}

/*————————————————————————————————————————————————————————————————————————————————————————————————————————————*/




void topOpt_thirdVal::F()
{
    char result = 'F';
    this->tOption->result[2] = result;
}

void topOpt_thirdVal::L()
{
    char result = 'L';
    this->tOption->result[2] = result;
}

void topOpt_thirdVal::M()
{
    char result = 'M';
    this->tOption->result[2] = result;
}

void topOpt_thirdVal::N()
{
    char result = 'N';
    this->tOption->result[2] = result;
}

void topOpt_thirdVal::Q()
{
    char result = 'Q';
    this->tOption->result[2] = result;
}

void topOpt_thirdVal::W()
{
    char result = 'W';
    this->tOption->result[2] = result;
}
/*————————————————————————————————————————————————————————————————————————————————————————————————————————————*/

void topOpt_forthVal::NoOption()
{
    char result = ' ';
    this->tOption->result[3] = result;
}

void topOpt_forthVal::T()
{
    char result = 'T';
    this->tOption->result[3] = result;
}
/*————————————————————————————————————————————————————————————————————————————————————————————————————————————*/



void topOpt_fifthVal::NoneOption()
{
    char result = ' ';
    this->tOption->result[4] = result;
}

void topOpt_fifthVal::useATI()
{
    char result = '*';
    this->tOption->result[4] = result;
}




