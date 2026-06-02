#include "runtype.h"

runtype::runtype()
{   
    
    this->firstVal = new runtype_firstVal;
    this->firstVal->rt = this;

    this->secondVal = new runtype_secondVal;
    this->secondVal->rt = this;

    this->thirdVal = new runtype_thirdVal;
    this->thirdVal->rt = this;

    this->forthVal = new runtype_forthVal;
    this->forthVal->rt = this;

    this->fifthVal = new runtype_fifthVal;
    this->fifthVal->rt = this;




    this->result = "CG RR  ";//默认CG空格RR空格空格
}
/*————————————————————————————————————————————————————————————————————————————————————————————————————————————*/
void runtype_firstVal::Ray_trace_R()
{
    char result = 'R';
    this->rt->result[0] = result;
}


void runtype_firstVal::Eigenray_trace_E()
{
    char result = 'E';
    this->rt->result[0] = result;
}

void runtype_firstVal::Coherent_TL_C()
{
    char result = 'C';
    this->rt->result[0] = result;
}

void runtype_firstVal::Arrivals_A()
{
    char result = 'A';
    this->rt->result[0] = result;
}

void runtype_firstVal::Incoherent_TL_I()
{
    char result = 'I';
    this->rt->result[0] = result;
}

void runtype_firstVal::Semi_coherent_TL_S()
{
    char result = 'S';
    this->rt->result[0] = result;
}
/*————————————————————————————————————————————————————————————————————————————————————————————————————————————*/


void runtype_secondVal::Geometric_beam_G()
{
    char result = 'G';
    this->rt->result[1] = result;
}

void runtype_secondVal::Cartesian_beams_C()
{
    char result = 'C';
    this->rt->result[1] = result;
}

void runtype_secondVal::Ray_centered_beam_R()
{
    char result = 'R';
    this->rt->result[1] = result;
}

void runtype_secondVal::Gaussian_beam_B()
{
    char result = 'B';
    this->rt->result[1] = result;
}
/*————————————————————————————————————————————————————————————————————————————————————————————————————————————*/

void runtype_thirdVal::NoneOption()
{
    // char result = ' ';
    // this->rt->result[2] = result;
}
/*————————————————————————————————————————————————————————————————————————————————————————————————————————————*/

void runtype_forthVal::Point_source_R()
{
    char result = 'R';
    this->rt->result[3] = result;
}

void runtype_forthVal::Line_source_X()
{
    char result = 'X';
    this->rt->result[3] = result;
}

void runtype_forthVal::NoneOption()
{
     char result = ' ';
     this->rt->result[2] = result;
}

/*————————————————————————————————————————————————————————————————————————————————————————————————————————————*/

void runtype_fifthVal::Irregular_grid_I()
{
    char result = 'I';
    this->rt->result[4] = result;
}

void runtype_fifthVal::Rectilinear_grid_R()
{
    char result = 'R';
    this->rt->result[4] = result;
}

void runtype_fifthVal::NoneOption()
{
     char result = ' ';
     this->rt->result[2] = result;
}

