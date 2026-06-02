#include "bellhopParam.h"
#include "bhc/math.hpp"
#include <cstddef>
#include <cstring>
#include <iterator>
#include <iomanip>  // 必须包含此头文件，用于设置精度（setprecision、fixed）
// void OutputCallback(const char *message)
// {
//     std::cout << "Out: " << message << std::endl << std::flush;
// }

// void PrtCallback(const char *message) { std::cout << message << std::flush; }

static std::ofstream* g_prtFile = nullptr; // 标准全局静态指针

void OutputCallback(const char *message)
{
    // 保持原样：RunWarning() / RunError() 等核心警告和错误依然打到控制台，方便排错
    std::cout << "Out: " << message << std::endl << std::flush;
}

void PrtCallback(const char *message) 
{ 
    if (g_prtFile) {
        *g_prtFile << message << std::flush; // 拦截刷屏参数：写入目标文件
    } else {
        std::cout << message << std::flush;  // 未调用 set_PrtFile 时，自动降级输出到控制台
    }
}

void bellhopParam:: set_Title(std::string  Title)
{
    this->Title = Title;
}

void bellhopParam:: set_freq(int freq)
{
    this->freq = freq;   
}

void bellhopParam::set_NMedia(int NMedia)
{
    this->NMedia  = NMedia;
}

void bellhopParam::set_stepLength(int len)
{
    this->stepLength = len;
}

void bellhopParam::set_SD(float SD, int NSD)
{
    this->SD = linspace(0, SD, NSD);
    this->NSD = NSD;
}

void bellhopParam::set_RD(float RD, int NRD)
{
    this->RD = linspace(0, RD, NRD);
    this->NRD = NRD;
}

void bellhopParam::set_R(float R, int NR)//R step
{
    this->R = linspace(0, R, NR);
    this->NR = NR;
}

void bellhopParam::set_SD(std::vector<float> &SD)
{
    this->SD = SD;
    this->NSD = SD.size();
}
void bellhopParam::set_RD(std::vector<float> &RD)
{
    this->RD = RD;
    this->NRD = RD.size();
}
void bellhopParam::set_R(std::vector<float>& R) //R step
{
    this->R = R;
    this->NR = R.size();
}



void bellhopParam::set_NBeam(int NBeam)
{
    this->NBeams = NBeam;
}

void bellhopParam::set_zBox(float ZBox)
{
    this->zbox = ZBox;
}
void bellhopParam::set_rBox(float rBox)
{
    this->rbox = rBox;
}
//
void bellhopParam::set_btyPts( std::vector<ati_bty>btyPts)// set sea depths  changes with distance 
{
    this->btyPts = btyPts;
}

void bellhopParam::set_atiPts(std::vector <ati_bty>atiPts)//  sea surface changes with distance
{
    this->atiPts = atiPts;
}

void bellhopParam::set_angle(std::vector<float> angle)//Beam angle
{
    this->angle = angle;
}

void bellhopParam::set_SSP( std::vector<SSP>  SSPLIst )
{
    this->SSPLIst = SSPLIst;

    auto maxElement = std::max_element(SSPLIst.begin(), SSPLIst.end(), [](const SSP& s1, const SSP& s2) {
        float maxZ1 = *std::max_element(s1.zSSPV.begin(), s1.zSSPV.end());
        float maxZ2 = *std::max_element(s2.zSSPV.begin(), s2.zSSPV.end());
        return maxZ1 < maxZ2;
    });

    if (maxElement != SSPLIst.end()) {
        float maxZ = *std::max_element(maxElement->zSSPV.begin(), maxElement->zSSPV.end());
        this->setDepthB(maxZ);
    }

}

void bellhopParam::set_bottomLine(Boundary_Line bottomLine)
{
    this->bottomLine = bottomLine;
}

void bellhopParam::set_surfaceLine(Boundary_Line surfaceLine)
{
    this->surfaceLine = surfaceLine;
}

//private
void bellhopParam::setDepthB(float DepthB) //change with zSPPV max;
{
    this->DepthB = DepthB;
}

void bellhopParam::setNSD(int NSD)
{
    this->NSD = NSD;
}

void bellhopParam::set_PrtFile(const std::string& filepath)
{
    this->prtFilePath = filepath;
}



//ParmInit
bellhopParam::bellhopParam()
{   
    std::vector<SSP> SSPLIst;
    // std::vector<float> zSSPV1 = {0,100,200,400,600,800,1000,1300,1500,2000,3000,5000};
    // std::vector<float> cSSPV1 = {1530,1520,1510,1500,1495,1490,1480,1460,1480,1500,1520,1520};
    // std::vector<float> zSSPV2 = {0,100,200,400,600,800,1000,1300,1500,2000,3000,5000};
    // std::vector<float> cSSPV2 = {1533,1523,1513,1503,1492,1493,1483,1463,1483,1503,1523,1523};
    // std::vector<float> zSSPV3 = {0,100,200,400,600,800,1000,1300,1500,2000,3000,5000};
    // std::vector<float> cSSPV3 = {1533,1523,1513,1503,1492,1493,1483,1463,1483,1503,1523,1523};

    std::vector<float> zSSPV1 = {0.0,100,200,400,600,800,1000,1300,1500,2000,3000,4000};
    std::vector<float> cSSPV1 = {1530,1520,1510,1500,1495,1490,1480,1460,1480,1500,1520,1520};
    std::vector<float> zSSPV2 = {0.0,100,200,400,600,800,1000,1300,1500,2000,3000,4000};
    std::vector<float> cSSPV2 = {1533,1523,1513,1503,1492,1493,1483,1463,1483,1503,1523,1523};
    std::vector<float> zSSPV3 = {0.0,100,200,400,600,800,1000,1300,1500,2000,3000,4000};
    std::vector<float> cSSPV3 = {1533,1523,1513,1503,1492,1493,1483,1463,1483,1503,1523,1523};





    SSP SSP_ele1 ,SSP_ele2,SSP_ele3;
    SSP_ele1.cSSPV = cSSPV1;
    SSP_ele1.zSSPV = zSSPV1;
    SSP_ele1.Distance = 0.0;
    SSP_ele2.cSSPV = cSSPV2;
    SSP_ele2.zSSPV = zSSPV2;
    SSP_ele2.Distance = 51.0;
    SSP_ele3.cSSPV = cSSPV3;
    SSP_ele3.zSSPV = zSSPV3;
    SSP_ele3.Distance = 101.0;

    SSPLIst.push_back(SSP_ele1);
    SSPLIst.push_back(SSP_ele2);
    SSPLIst.push_back(SSP_ele3);


    this->Title = "Title";

    this->angle = {-50,50};
    this->atiPts = {{0,0},{50,0},{101,0}};
    this->btyPts = {{0,4000},{50,4000},{60,4000}};

    this->stepLength = 500;
    this->freq = 525;
    this->NBeams =20;
    this->NMedia = 1;
    this->NR = 1001;
    this->NRD = 1001;
    this->NSD =1;
    this->R = this->linspace(0,100000,this->NR);//m
    this->rbox = 101;
    this->zbox = 5001;
    this->RD = this->linspace(0,3000,this->NRD);

    this->SD = {200};
    this->set_SSP(SSPLIst);
    
    this->topopt = new topOpt ; //topOption
    this ->botopt = new botOpt; //bottom Option
    this->RunType = new runtype;//runtype

    this->bottomLine.alphaI  = 0.5;
    this->bottomLine.alphaR = 1900.0;
    this->bottomLine.betaI  = 0.0;
    this->bottomLine.betaR = 0.0;
    this->bottomLine.rho  =2.0;
    this->bottomLine.Depth = this->DepthB;
    
    this->surfaceLine.alphaI = 0;
    this->surfaceLine.alphaR =1500;
    this->surfaceLine.betaI = 0;
    this->surfaceLine.betaR = 0;
    this->surfaceLine.Depth = 0;
    this->surfaceLine.rho = 1;
    




    this->bhc_INIT.FileRoot  = nullptr;
    this->bhc_INIT.prtCallback = PrtCallback;
    this->bhc_INIT.outputCallback = OutputCallback;

    bhc::setup(this->bhc_INIT,*this->bhc_Params,this->bhc_OutPut);//setup
}

void bellhopParam::transformParam()
{
    /*----------------------------------- beamInfo----------------------------------- */
    this->bhc_Params->Beam->Box.y = this->zbox;
    this->bhc_Params->Beam->Box.x = this->rbox*1000;
    this->bhc_Params->Beam->deltas = this->stepLength;
    this->bhc_Params->Angles->alpha.n = this->NBeams;

    /*----------------------------------- botopt----------------------------------- */
    this->bhc_Params->Bdry->Bot.hs.Depth = this->DepthB; //set max Depth


    if(botopt->result[1] == '*')
    {
        int btyNum = this->btyPts.size()+2; //because in the "bd"Array ,the first and last deprecated
        this->bhc_Params->bdinfo->bot.NPts = btyNum;

        bhc::extsetup_bathymetry(*this->bhc_Params,btyNum);

        for (int i =1;i<btyNum-1;i++)
        {   
            this->bhc_Params->bdinfo->bot.bd[i].x[0] = this->btyPts[i-1].x;
            this->bhc_Params->bdinfo->bot.bd[i].x[1] = this->btyPts[i-1].depth;
        }
        //set bty limit, Prevents sound rays from reaching locations where the terrain is not defined
        this->bhc_Params->bdinfo->bot.bd[0].x[0] = this->btyPts[0].x-100;
        this->bhc_Params->bdinfo->bot.bd[0].x[1] = this->btyPts[0].depth;
        this->bhc_Params->bdinfo->bot.bd[btyNum-1].x[0] = this->btyPts.back().depth+10000;
        this->bhc_Params->bdinfo->bot.bd[btyNum-1].x[1] = this->btyPts.back().depth;
        
    }
    else
    {
        int btyNum = 2;
        this->bhc_Params->bdinfo->bot.NPts = btyNum;
        bhc::extsetup_bathymetry(*this->bhc_Params,btyNum);
        this->bhc_Params->bdinfo->bot.bd[0].x[0] = this->atiPts[0].x-100;
        this->bhc_Params->bdinfo->bot.bd[0].x[1] = this->SSPLIst[0].zSSPV.back();
        this->bhc_Params->bdinfo->bot.bd[btyNum-1].x[0] = this->SSPLIst.back().zSSPV.back()+1000;
        this->bhc_Params->bdinfo->bot.bd[btyNum-1].x[1] = this->SSPLIst.back().zSSPV.back();

    }





    /*----------------------------------- atimetry----------------------------------- */
    
    if(topopt->result[4] == '*')
    {
        int atiNum = this->atiPts.size()+2; //because in the "bd"Array ,the first and last deprecated
        this->bhc_Params->bdinfo->top.NPts = atiNum;
        
        bhc::extsetup_altimetry(*this->bhc_Params,atiNum);

        for (int i =1;i<atiNum-1;i++)
        {   
            this->bhc_Params->bdinfo->top.bd[i].x[0] = this->atiPts[i-1].x;
            this->bhc_Params->bdinfo->top.bd[i].x[1] = this->atiPts[i-1].depth;
        }
        //set bty limit, Prevents sound rays from reaching locations where the terrain is not defined
        this->bhc_Params->bdinfo->top.bd[0].x[0] =this->atiPts[0].x-100;
        this->bhc_Params->bdinfo->top.bd[0].x[1] = this->atiPts[0].depth;
        this->bhc_Params->bdinfo->top.bd[atiNum-1].x[0] =this->atiPts.back().x+10000;
        this->bhc_Params->bdinfo->top.bd[atiNum-1].x[1] = this->atiPts.back().depth;
    
    }




    /*----------------------------------- freq----------------------------------- */
    this->bhc_Params->freqinfo->freq0 = this->freq;
    this->bhc_Params->freqinfo->freqVec[0] = this->freq;
    this->bhc_Params->freqinfo->Nfreq = 1; //only one frequency

    
    /*----------------------------------- rayAngles----------------------------------- */
    std::vector<float>angle_interp = this->linspace(this->angle.front(), this->angle.back(), this->NBeams);
    this->bhc_Params->Angles->alpha.n = angle_interp.size(); 



    bhc::extsetup_rayelevations(*this->bhc_Params,angle_interp.size());

    for(int i =0;i<angle_interp.size();i++)
    {
            this->bhc_Params->Angles->alpha.angles[i] = angle_interp[i];
    }

    /*----------------------------------- ssp----------------------------------- */
        //防止声线跑到SSP外面
    SSP ssp1,ssp2;
    ssp1.zSSPV = this->SSPLIst.front().zSSPV;
    ssp1.cSSPV = this->SSPLIst.front().cSSPV;
    ssp1.Distance = this->SSPLIst.front().Distance-100;

    ssp2.zSSPV = this->SSPLIst.back().zSSPV;
    ssp2.cSSPV = this->SSPLIst.back().cSSPV;
    ssp2.Distance = ceil(this->SSPLIst.back().Distance+100);

    this->SSPLIst.insert(this->SSPLIst.begin(),ssp1);
    this->SSPLIst.insert(this->SSPLIst.end(),ssp2);




    this->bhc_Params->ssp->Nr = this->SSPLIst.size(); //Number of SSP
    this->bhc_Params->ssp->Nz = this->SSPLIst[0].zSSPV.size(); //Number of Depth
    this->bhc_Params->ssp->NPts = this->SSPLIst[0].zSSPV.size(); //Number of Depth
    
    for(int i =0;i<this->SSPLIst[0].zSSPV.size();i++)
    {   
        this->bhc_Params->ssp->z[i] = this->SSPLIst[0].zSSPV[i]; //set Depth List
        this->bhc_Params->ssp->alphaR[i]= this->SSPLIst[0].cSSPV[i]; //set Depth List
        this->bhc_Params->ssp->betaR[i] = 0;
        this->bhc_Params->ssp->rho[i] = 1.0;
        this->bhc_Params->ssp->alphaI[i] = 0;
        this->bhc_Params->ssp->betaI[i] = 0;
    }

    //set  x Range
    if(topopt->result[0]=='Q')
    {
        this->bhc_Params->ssp->rangeInKm = true;
        //this->bhc_Params->ssp->Nr = this->SSPLIst.size();
        std::vector<bhc::real>cMat = this->SSPList_to_CMat(); //2D to 1D SSP

        bhc::extsetup_ssp_quad(*this->bhc_Params,this->SSPLIst[0].zSSPV.size(),this->SSPLIst.size());


        for(int i =0;i<this->SSPLIst.size();i++)
        {
                this->bhc_Params->ssp->Seg.r[i] = this->SSPLIst[i].Distance;
        }
        //set SSP matrix
        for(int i =0;i<cMat.size();i++)
        {
            this->bhc_Params->ssp->cMat[i] = cMat[i];
        }
    }
    
    this->bhc_Params->ssp->dirty = true;

    // /*----------------------------------- rcvrranges----------------------------------- */
        // bhc::extsetup_sz(*this->bhc_Params,1);
        // this->bhc_Params->Pos->NSz = 1; //one source
        // this->bhc_Params->Pos->Sz = this->SD.data() ; //source depth


        bhc::extsetup_sz(*this->bhc_Params, 1);
        this->bhc_Params->Pos->NSz = 1; // 既然固定为1，就写死
        this->bhc_Params->Pos->Sz[0] = this->SD[0]; // ✅ 正确：将深度值赋给 Bellhop 已经申请好的内存


        //if Runtype->firstVal !="A":
            //copy memory
            this->bhc_Params->Pos->NRr = this->NR; //Number of receiver R 
            this->bhc_Params->Pos->NRz = this->NRD; //Number of  receiver Z

            bhc::extsetup_rcvrranges(*this->bhc_Params,this->NR);
            bhc::extsetup_rcvrdepths(*this->bhc_Params,this->NRD);

        if (this->RunType->result[0]!='A'){
            std::vector<float> RList = this->R; //linspace receiver r-coordinates
            std::vector<float> RDList = this->RD;//linspace  receiver z-coordinates

            std::copy(RList.begin(), RList.end(), this->bhc_Params->Pos->Rr);
            std::copy(RDList.begin(), RDList.end(), this->bhc_Params->Pos->Rz);
        }
        else
        {
            //test
            // this->bhc_Params->Pos->Rr[0]=5000;
            // this->bhc_Params->Pos->Rz[0]=100;
            std::vector<float> RList = this->R; //linspace receiver r-coordinates
            std::vector<float> RDList = this->RD;//linspace  receiver z-coordinates

            std::copy(RList.begin(), RList.end(), this->bhc_Params->Pos->Rr);
            std::copy(RDList.begin(), RDList.end(), this->bhc_Params->Pos->Rz);

        }






    /*----------------------------------- runtype----------------------------------- */
    this->botopt->result.copy(this->bhc_Params->Bdry->Bot.hs.Opt,sizeof(this->bhc_Params->Bdry->Bot.hs.Opt));
    this->bhc_Params->Bdry->Bot.hs.bc = this->botopt->result[0];


    this->bhc_Params->ssp->Type = this->topopt->result[0];
    this->bhc_Params->ssp->AttenUnit[0] = this->topopt->result[0];
    this->bhc_Params->Bdry->Top.hs.bc = this->topopt->result[1];
    this->bhc_Params->ssp->AttenUnit[0] = this->topopt->result[2];
    this->bhc_Params->ssp->AttenUnit[1] = this->topopt->result[3];
 
    this->RunType->result.copy(this->bhc_Params->Beam->RunType,sizeof(this->bhc_Params->Beam->RunType));
    
     /*----------------------------------- boundary----------------------------------- */
    this->bhc_Params->Bdry->Top.hs.Depth = 0;
    this->bhc_Params->Bdry->Bot.hs.Depth = this->DepthB;

    if(this->topopt->result[1] == 'A')
    {
        this->bhc_Params->Bdry->Top.hs.alphaI = this->surfaceLine.alphaI;
        this->bhc_Params->Bdry->Top.hs.alphaR = this->surfaceLine.alphaR;
        this->bhc_Params->Bdry->Top.hs.betaI = this->surfaceLine.betaI;
        this->bhc_Params->Bdry->Top.hs.betaR = this->surfaceLine.betaR;
        this->bhc_Params->Bdry->Top.hs.rho = this->surfaceLine.rho;
        this->bhc_Params->Bdry->Top.hsx.zTemp = 0;

    }
    if(this->botopt->result[0]=='A')
    {   
        
        this->bhc_Params->Bdry->Bot.hs.alphaI = this->bottomLine.alphaI;
        this->bhc_Params->Bdry->Bot.hs.alphaR = this->bottomLine.alphaR;
        this->bhc_Params->Bdry->Bot.hs.betaI = this->bottomLine.betaI;
        this->bhc_Params->Bdry->Bot.hs.betaR = this->bottomLine.betaR;
        this->bhc_Params->Bdry->Bot.hs.rho = this->bottomLine.rho;
        this->bhc_Params->Bdry->Bot.hsx.zTemp = this->DepthB;

        
    }




}

void bellhopParam::runMod()
{   

    this->transformParam(); //transform input var


    bhc::echo(*this->bhc_Params);
    bhc::run(*this->bhc_Params, this->bhc_OutPut);
    
}



std::vector<std::vector<float>> bellhopParam::get_TLField()
{
    std::vector<std::vector<float>> result;
    for(int iz = 0; iz < this->bhc_Params->Pos->NRz; ++iz) {
        std::vector<float>rowList;

        for(int ir = 0; ir < this->bhc_Params->Pos->NRr; ++ir) {
            
            //std::cout << this->bhc_OutPut.uAllSources[iz * this->bhc_Params->Pos->NRr + ir] << " " << std::endl;
            float TL = -20.0 * std::log10(std::abs(this->bhc_OutPut.uAllSources[iz * this->bhc_Params->Pos->NRr + ir]));
            rowList.push_back(TL);
        }
        result.push_back(rowList);

    }
    
    return result;

}

std::vector<std::vector<std::complex<float>>> bellhopParam::get_TLField_P()
{
    std::vector<std::vector<std::complex<float>>> result;
    for(int iz = 0; iz < this->bhc_Params->Pos->NRz; ++iz) {
        std::vector<std::complex<float>>rowList;

        for(int ir = 0; ir < this->bhc_Params->Pos->NRr; ++ir) {
            
            //std::cout << this->bhc_OutPut.uAllSources[iz * this->bhc_Params->Pos->NRr + ir] << " " << std::endl;
            std::complex<float> P = (this->bhc_OutPut.uAllSources[iz * this->bhc_Params->Pos->NRr + ir]);
            rowList.push_back(P);
        }
        result.push_back(rowList);

    }
    
    return result;

}


std::vector<rayInfo> bellhopParam::get_Ray()
{
    std::cout << "\n" << this->bhc_OutPut.rayinfo->NRays << " rays:\n";
    std::vector<rayInfo> allRay;

    for(int r=0; r<this->bhc_OutPut.rayinfo->NRays; ++r)
    {
        rayInfo ele;
        ele.NStep = this->bhc_OutPut.rayinfo->results[r].Nsteps;
        ele.SrcDeclAngle = this->bhc_OutPut.rayinfo->results[r].SrcDeclAngle;
        for(int s=0; s<this->bhc_OutPut.rayinfo->results[r].Nsteps; ++s)
        {
                rayPos pos;
                pos.x = this->bhc_OutPut.rayinfo->results[r].ray[s].x[0];
                pos.y =  this->bhc_OutPut.rayinfo->results[r].ray[s].x[1];
                ele.ray.push_back(pos);
        }
        allRay.push_back(ele);
    }



    return allRay;

}

std::vector<std::vector<arrinfo>> bellhopParam::get_TimeDelay()
{   
    int col = this->NR; //Number of R
    int row = this->NRD;//Number of Z
    std::vector<std::vector<arrinfo>>result;
    for(int i =0;i<row;i++)
    {   
        std::vector<arrinfo> rowResult;
        for(int j =0;j<col;j++)
        {
            arrinfo ele1;
            std::vector<arrival_ele> ele1Result;
            int base = i*col+j;
            ele1.NArr  = this->bhc_OutPut.arrinfo->NArr[i*col+j];
            //std::cout<<"[i]="<<i<<" [j]="<<j<<"\tNArr:"<<ele1.NArr<<std::endl;
            int N = ele1.NArr;
            for(int k =0;k<N;k++)
            {              
                bhc::Arrival *arrResult
                    = &this->bhc_OutPut.arrinfo->Arr[base * this->bhc_OutPut.arrinfo->MaxNArr + k];

                arrival_ele ele;


                ele.Amplitude = arrResult->a;
                ele.Phase = arrResult->Phase;
                ele.RcvrAzimAngle = arrResult->RcvrAzimAngle;
                ele.RcvrDeclAngle = arrResult->RcvrDeclAngle;
                ele.SrcAzimAngle = arrResult->SrcAzimAngle;
                ele.SrcDeclAngle = arrResult->SrcDeclAngle;
                float imag = arrResult->delay.imag();
                float real = arrResult->delay.real();
                ele.delay.imag(imag);
                ele.delay.real(real);
                // std::cout<<ele.Amplitude<<"    ";
                 // ========== 新增：反射次数字段 ==========
                ele.NumTopBnc = arrResult->NTopBnc; // 海面反射次数
                ele.NumBotBnc = arrResult->NBotBnc; // 海底反射次数

                ele1Result.push_back(ele);
            }
            // std::cout<<std::endl;
            ele1.arr_thisReceiver = ele1Result;
            rowResult.push_back(ele1);

        }
        result.push_back(rowResult);
    }

    return result;

}



void bellhopParam::exportTL(std::vector<std::vector<float>> data,const std::string& filename)
{   
    this->writeHeader(filename); //
    std::string filepath = filename + ".csv";
    std::ofstream file(filepath);
    if (file.is_open()) {
        for (const auto& row : data) {
            for (size_t i = 0; i < row.size(); ++i) {
                file << row[i];
                if (i != row.size() - 1) {
                    file << ",";
                }
            }
            file << "\n";
        }
        file.close();
        std::cout << "CSV file has been written successfully." << std::endl;
    } else {
        std::cerr << "Unable to open the CSV file." << std::endl;
    }
}

void bellhopParam::exportRay(std::vector<rayInfo> data, const std::string& filename)
{   

    this->writeHeader(filename);
    std::string filepath = filename + ".ray";
    std::ofstream file(filepath);
    if (file.is_open())
    {
        // ========== 核心：设置输出精度 ==========
        file << std::fixed;          // 固定小数格式（禁用科学计数法）
        file << std::setprecision(6);// 设置精度为6位小数（可根据需求调整，如3/8位）

        std::cout << data.size();

        
        for(int i =0;i<data.size();i++)
        {
            file<< "Ray:"<<i<<"\tNStep:"<<data[i].NStep<<"\tAngle:"<<data[i].SrcDeclAngle<<std::endl;
            file<<"Ray Start"<<std::endl;
            for(int j =0;j<data[i].ray.size();j++)
            {
                file<<data[i].ray[j].x<<"\t"<<data[i].ray[j].y<<std::endl;

            }

            file<<"Ray END"<<std::endl;
        }
    file.close();
    std::cout << "RAY file has been written successfully." << std::endl;
    }
    else
    {
        std::cerr << "Unable to open the RAY file." << std::endl;
    }


}


void bellhopParam::exportArr(std::vector<std::vector<arrinfo>> data, const std::string& filename)
{
    this->writeHeader(filename);
    std::string filepath = filename + ".arr";
    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Unable to open arrivals file: " << filepath << std::endl;
        return;
    }

    // 写入2D格式标识（对齐原版排版）
    file << "   '2D'" << std::endl;

    // 写入频率（15位小数精度，匹配原版）
    file << "   " << std::fixed << std::setprecision(15) << this->freq << "     " << std::endl;
    
    // 写入声源深度数+深度值
    file << "           " << 1 << "   " << std::fixed << std::setprecision(5) << this->SD[0] << "    " << std::endl;
    
    // 写入接收深度数+深度值
    file << "           " << this->NRD << "   ";
    file << std::fixed << std::setprecision(5) << this->RD[0];
    file << "    " << std::endl;
    
    // 写入接收距离数+距离值
    file << "           " << this->NR << "   ";
    file << std::fixed << std::setprecision(3) << this->R[0];
    file << "    " << std::endl;

    // 遍历写入到达信息
    int Nsz = this->SD.size();
    for (int isd=0; isd<Nsz; isd++) {
        // 统计最大到达数并写入
        int max_arr = 0;
        for (auto& row : data) for (auto& ele : row) max_arr = std::max(max_arr, ele.NArr);
        file << "          " << max_arr << std::endl;

        // 遍历接收器维度
        for (int irz=0; irz<this->NRD; irz++) {
            for (int irr=0; irr<this->NR; irr++) {
                arrinfo& ele = data[irz][irr];
                file << "          " << ele.NArr << std::endl;

                // 写入每条声线的8列参数
                if (ele.NArr > 0) {
                    for (arrival_ele& arr_ele : ele.arr_thisReceiver) {
                        // Phase弧度转角度
                        float phase_deg = arr_ele.Phase * 180.0 / M_PI;

                        // 格式化输出（匹配原版列宽/精度/大写E）
                        char buf[256];
                        snprintf(buf, sizeof(buf), 
                                 "   %13.8E   %12.6f       %13.7f       %12.8f      %13.8f      %13.8f               %d           %d\n",
                                 arr_ele.Amplitude, phase_deg, arr_ele.delay.real(), arr_ele.delay.imag(),
                                 arr_ele.SrcDeclAngle, arr_ele.RcvrDeclAngle, arr_ele.NumTopBnc, arr_ele.NumBotBnc);
                        file << buf;
                    }
                }
            }
        }
    }

    file.close();
    std::cout << "Arrivals file written successfully: " << filepath << std::endl;
}


void bellhopParam::exportTL_P(std::vector<std::vector<std::complex<float>>> data, const std::string& filename)
{
    this->writeHeader(filename); // 复用头部写入逻辑

    // 1. 定义实部/虚部文件路径（后缀区分）
    std::string real_filepath = filename + "_real.csv";
    std::string imag_filepath = filename + "_imag.csv";

    // 2. 打开实部/虚部文件
    std::ofstream file_real(real_filepath);
    std::ofstream file_imag(imag_filepath);

    // 检查文件是否都成功打开
    if (file_real.is_open() && file_imag.is_open())
    {
        // 设置高精度输出：固定小数位 + 6位精度（可按需调整）
        file_real << std::fixed << std::setprecision(8);
        file_imag << std::fixed << std::setprecision(8);

        // 遍历复数二维数组，分别写入实部/虚部
        for (const auto& row : data)
        {
            for (size_t i = 0; i < row.size(); ++i)
            {
                // 写入实部
                file_real << row[i].real();
                // 写入虚部（保留原始符号，无需取绝对值）
                file_imag << row[i].imag();

                // 列分隔符：最后一列不加逗号
                if (i != row.size() - 1)
                {
                    file_real << ",";
                    file_imag << ",";
                }
            }
            // 行结束：换行
            file_real << "\n";
            file_imag << "\n";
        }

        // 关闭文件
        file_real.close();

         std::cout << "Real part CSV written successfully: " << real_filepath << std::endl;
         
        file_imag.close();

        // 输出成功提示
       
        std::cout << "Imag part CSV written successfully: " << imag_filepath << std::endl;
    }
    else
    {
        // 错误提示：明确哪个文件打开失败
        if (!file_real.is_open()) {
            std::cerr << "Unable to open real part CSV file: " << real_filepath << std::endl;
        }
        if (!file_imag.is_open()) {
            std::cerr << "Unable to open imag part CSV file: " << imag_filepath << std::endl;
        }
        // 关闭已打开的文件（避免资源泄漏）
        if (file_real.is_open()) file_real.close();
        if (file_imag.is_open()) file_imag.close();
    }
}

std::vector<bhc::real> bellhopParam::SSPList_to_CMat()
{
    int num_distances = this->SSPLIst.size();
    int num_depths = this->SSPLIst[0].cSSPV.size();
    std::vector<bhc::real>result(num_distances*num_depths) ; 
    
    for(int i =0;i<num_distances;i++)
    {
        for (int j =0;j<num_depths;j++)
        {   
            int index = j*num_distances + i;
            result[index] = this->SSPLIst[i].cSSPV[j];
        }
    }

    return result;
}


std::vector<float> bellhopParam::linspace(float start_in, float end_in, int num_in)
{
    std::vector<float> linspaced;

    float start = static_cast<float>(start_in);
    float end = static_cast<float>(end_in);
    //double num = static_cast<double>(num_in);
	int num = num_in;
    if (num == 0) { return linspaced; }
    if (num == 1)
    {
        linspaced.push_back(end);
        return linspaced;
    }

    double delta = (end - start) / (num - 1);

    for (int i = 0; i < num - 1; ++i)
    {
        linspaced.push_back(start + delta * i);
    }
    linspaced.push_back(end); //确保末尾为最后一个值

    return linspaced;
}

void bellhopParam::writeHeader(const std::string& filename)
{   
    std::string filepath = filename + ".header";
    std::ofstream file(filepath);
    if (file.is_open())
    {
        //写入文件头 
        //SD,RD,NR,NRD,NBeams,SSP,TL
        for(auto&ele:this->SD)
        {
            file<<"SD = "<<ele<<",";
        }
        file<<std::endl;
        for(auto&ele:this->RD)
        {
            file<<"RD = "<<ele<<",";
        }
        file<<std::endl;

        file<<"NR = "<<this->NR<<std::endl;
        file<<"NRD = "<<this->NRD<<std::endl;
        file<<"NBeams = "<<this->NBeams<<std::endl;

        for (auto&ele:this->R)
        {
            file<<"R = "<<ele/1000<<",";
        }
        file<<std::endl;
    
        file<<"NSSP = "<<this->SSPLIst.size()<<std::endl;
        file<<"z = "<<std::endl;
        for(auto&ele:this->SSPLIst[0].zSSPV)
        {
            file<<ele<<" ";
        }
        file<<std::endl;
        file<<"distance = "<<std::endl;
        for(auto &ele:this->SSPLIst)
        {
            file<<ele.Distance<<" ";
        }
        file<<std::endl;
        file<<"c = "<<std::endl;
        for(auto &ele:this->SSPLIst)
        {
            for(auto &ele1:ele.cSSPV)
            {
                file<<ele1<<" ";
            }
            file<<std::endl;
        }
        //file<<std::endl;

        file<<"btyR = "<<std::endl;
        for(auto &ele:this->btyPts)
        {
            file<<ele.x<<" ";
        }
        file<<std::endl;

        file<<"btyZ = "<<std::endl;
        for(auto &ele:this->btyPts)
        {
            file<<ele.depth<<" ";
        }
        file<<std::endl;




    file.close();
    std::cout << "hearder file has been written successfully." << std::endl;
    }
    else
    {
        std::cerr << "Unable to open the hearder file." << std::endl;
    }





}



 bellhopParam::~bellhopParam()
 {
    bhc::finalize(*this->bhc_Params, this->bhc_OutPut);
    delete this->topopt;
    delete this->botopt;
    delete this->RunType;
 }

