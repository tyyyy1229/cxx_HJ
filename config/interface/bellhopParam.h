#if !defined(INTERFACE_H)
#define INTERFACE_H

//#define BHC_DLL_IMPORT 
#include <bhc/bhc.hpp>
#include <vector>
#include<iostream>
#include <algorithm>
#include <cstring>
#include "botopt.h"
#include "topopt.h"
#include "runtype.h"
#include <fstream>
#include <complex>
//one sound speed profile sturcture
struct SSP
{
        std::vector<float> zSSPV; //The depth corresponding to the ssp
        std::vector<float> cSSPV; //The depth corresponds to the speed of sound
        float Distance; // Distance from the starting point
};

struct ati_bty
{
    //One depth corresponds to one distance
    float x;
    float depth;
    float alphaR = 0.0f;   // 海底压缩波声速 (m/s), 0=使用全局 bottomLine
    float alphaI = 0.0f;   // 海底压缩波衰减 (dB/λ), 0=使用全局 bottomLine
    float rho    = 0.0f;   // 海底密度 (g/cm³), 0=使用全局 bottomLine

};


struct Boundary_Line //boundary parameter    (bottom/surface) When the boundary is selected as A
{
    float alphaR, betaR, alphaI, betaI; // compressional and shear wave speeds/attenuations
    float rho, Depth;                   // density, depth
};

struct rayPos
{
    float x;
    float y;
};
struct rayInfo
{
    std::vector<rayPos> ray; //ray position
    int NStep;                  //Number of rayPos 
    float SrcDeclAngle;  //Angle of ray transmission

};

struct arrival_ele
{
    float SrcDeclAngle;  // source decline angle
    float SrcAzimAngle; // source azimuth angle
    float RcvrDeclAngle; // received decline angle
    float RcvrAzimAngle; // received azimuth angle
    float Amplitude ; //Amplitude
    float Phase; //Phase
    std::complex<float> delay; //delay
     int32_t NumTopBnc;   // 新增：海面反射次数（对应Bellhop NTopBnc）
    int32_t NumBotBnc;   // 新增：海底反射次数（对应Bellhop NBotBnc）
};

//one receiver arrival information
struct arrinfo
{
    int NArr ;
    std::vector<arrival_ele> arr_thisReceiver;
};



class bellhopParam
{   

    //param Define
    private:
        std::string  Title; //title
        int freq ;               //frequency
        int NMedia;        //Number of media
        int stepLength; //step Length

        float DepthB; //Depth at the bottom

        std::vector<SSP>  SSPLIst ;
        Boundary_Line surfaceLine; //When the boundary is selected as A
        Boundary_Line bottomLine; //When the boundary is selected as A
        //
        std::vector<float> SD; //Source  Depth
        int  NSD; //Number of  Source  Depth
        std::vector<float> RD; //Receive Depth
        int NRD; //Number of  Receive Depth
        std::vector<float> R ; //Sound propagation range
        int NR; //
        int NBeams ; //Number of beams
       // float deltas; //
        float zbox;//grid box zmax(depth) 
        float rbox;//grid box rmax(range) 
        std::vector<ati_bty>btyPts ; //sea depths  changes with distance 
        std::vector <ati_bty>atiPts;//  sea surface changes with distance
        std::vector<float> angle; //Beam angle
        std::string prtFilePath;
    public:
        topOpt *topopt; //topOption
        botOpt *botopt; //bottom Option
        runtype *RunType;//runtype


    public:
        bhc::bhcInit bhc_INIT;
        bhc::bhcParams<false> *bhc_Params = new bhc::bhcParams<false>;
        bhc::bhcOutputs<false,false> bhc_OutPut;
        
        //setParm;
    public:
        bellhopParam();      
        ~bellhopParam();
        void runMod();
        
        void set_Title(std::string  Title);
        void set_freq(int freq);
        void set_NMedia(int NMedia);
        void set_stepLength(int len);

        void set_SD(float SD,int NSD);
        void set_RD(float RD,int NRD);
        void set_R(float R,int NR); //R step

        void set_SD(std::vector<float> &SD);
        void set_RD(std::vector<float> &RD);
        void set_R(std::vector<float> &R); //R step


        void set_NBeam(int NBeam);
        void set_zBox(float zBox);
        void set_rBox(float rBox);
        //
        void set_btyPts( std::vector<ati_bty>btyPts);// set sea depths  changes with distance 
        void set_atiPts(std::vector <ati_bty>atiPts);//  sea surface changes with distance
        void set_angle(std::vector<float> angle);//Beam angle
        void set_SSP( std::vector<SSP>  SSPLIst );//set ssp

        //When the boundary is selected as A
        void set_bottomLine(Boundary_Line bottomLine);
        void set_surfaceLine(Boundary_Line bottomLine);

        void set_EnvFile(const std::string& filepath);

        // ========== 0604新增：自加载环境文件类成员函数 ==========
        void load_SSP(const std::string& filepath, const std::vector<float>& zVector);
        void load_BTY(const std::string& filepath);
        



        std::vector<std::vector<float>> get_TLField(); //getTL   2D
        std::vector<std::vector<std::complex<float>>> get_TLField_P();
        std::vector<rayInfo> get_Ray(); //getRay
        
        std::vector<std::vector<arrinfo>> get_TimeDelay(); //getTimeDelay;

    private:
    std::vector<float> linspace(float start_in, float end_in, int num_in); //linspace for bty/ati

    private:
        void setDepthB(float DepthB); //change with zSPPV max;
        void setNSD(int NSD);
        void transformParam();//tranform input.
        std::vector<bhc::real>SSPList_to_CMat(); //transform 2DSSP to bhc_SSP.cMat;
    
    //exportData for matlab 
    public:
    void exportTL(std::vector<std::vector<float>> data,const std::string& filename);
    void exportRay(std::vector<rayInfo> data,const std::string& filename);
    void exportArr(std::vector<std::vector<arrinfo>> data, const std::string& filename);
    void exportTL_P(std::vector<std::vector<std::complex<float>>> data,const std::string& filename);
    void writeHeader(const std::string& filename);
};







#endif // INTERFACE_H



