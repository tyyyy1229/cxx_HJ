
#pragma once
#include "common_run.hpp"

namespace bhc {


template<bool R3D> HOST_DEVICE inline bool IsSecondStepOfPair(
    real omega, real Phase, cpx delay, Arrival *baseArr, int32_t Nt)
{
    // arrivals with essentially the same phase are grouped into one
    const float PhaseTol = /*R3D ? FL(0.5) :*/ FL(0.05); // LP: 0.5 for 2D removed by mbp
                                                         // in 2022 revisions.
    return Nt >= 1 && omega * STD::abs(delay - Cpxf2Cpx(baseArr[Nt - 1].delay)) < PhaseTol
        && STD::abs(baseArr[Nt - 1].Phase - Phase) < PhaseTol;
}

/**
 * Adds the amplitude and delay for an ARRival into a matrix of same.
 * Extra logic included to keep only the strongest arrivals.
 */
template<bool R3D> HOST_DEVICE inline void AddArr(
    int32_t itheta, int32_t id, int32_t ir, real Amp, real omega, real Phase, cpx delay,
    const RayInitInfo &rinit, real RcvrDeclAngle, real RcvrAzimAngle, int32_t NumTopBnc,
    int32_t NumBotBnc, const ArrInfo *arrinfo, const Position *Pos)
{
    size_t base      = GetFieldAddr(rinit.isx, rinit.isy, rinit.isz, itheta, id, ir, Pos);
    Arrival *baseArr = &arrinfo->Arr[base * arrinfo->MaxNArr];
    int32_t *baseNArr = &arrinfo->NArr[base];
    int32_t Nt;

    if(arrinfo->AllowMerging) {
   

        Nt = *baseNArr; // # of arrivals

        if(!IsSecondStepOfPair<R3D>(omega, Phase, delay, baseArr, Nt)) {
            int32_t iArr;
            if(Nt >= arrinfo->MaxNArr) { // space not available to add an arrival?
                // replace weakest arrival
                iArr         = -1;
                real weakest = Amp;
                for(Nt = 0; Nt < arrinfo->MaxNArr; ++Nt) {
                    if(baseArr[Nt].a < weakest) {
                        weakest = baseArr[Nt].a;
                        iArr    = Nt;
                    }
                }
                if(iArr < 0) return; 
            } else {
                iArr      = Nt;
                *baseNArr = Nt + 1; // # of arrivals
            }
            baseArr[iArr].a            = (float)Amp;                // amplitude
            baseArr[iArr].Phase        = (float)Phase;              // phase
            baseArr[iArr].delay        = Cpx2Cpxf(delay);           // delay time
            baseArr[iArr].SrcDeclAngle = (float)rinit.SrcDeclAngle; // launch angle from
                                                                    // source
            baseArr[iArr].SrcAzimAngle = (float)rinit.SrcAzimAngle; // launch angle from
                                                                    // source
            baseArr[iArr].RcvrDeclAngle = (float)RcvrDeclAngle;     // angle ray reaches
                                                                    // receiver
            baseArr[iArr].RcvrAzimAngle = (float)RcvrAzimAngle;     // angle ray reaches
                                                                    // receiver
            baseArr[iArr].NTopBnc = NumTopBnc; // Number of top    bounces
            baseArr[iArr].NBotBnc = NumBotBnc; //   "       bottom
        } else {                               // not a new ray
            // PhaseArr[<base> + Nt-1] = PhaseArr[<base> + Nt-1] // LP: ???

            // calculate weightings of old ray information vs. new, based on amplitude of
            // the arrival
            float AmpTot = baseArr[Nt - 1].a + (float)Amp;
            float w1     = baseArr[Nt - 1].a / AmpTot;
            float w2     = (float)Amp / AmpTot;

            baseArr[Nt - 1].delay = w1 * baseArr[Nt - 1].delay
                + w2 * Cpx2Cpxf(delay); // weighted sum
            baseArr[Nt - 1].a            = AmpTot;
            baseArr[Nt - 1].SrcDeclAngle = w1 * baseArr[Nt - 1].SrcDeclAngle
                + w2 * (float)rinit.SrcDeclAngle;
            baseArr[Nt - 1].SrcAzimAngle = w1 * baseArr[Nt - 1].SrcAzimAngle
                + w2 * (float)rinit.SrcAzimAngle;
            baseArr[Nt - 1].RcvrDeclAngle = w1 * baseArr[Nt - 1].RcvrDeclAngle
                + w2 * (float)RcvrDeclAngle;
            baseArr[Nt - 1].RcvrAzimAngle = w1 * baseArr[Nt - 1].RcvrAzimAngle
                + w2 * (float)RcvrAzimAngle;
        }

    } else {
     
        Nt = AtomicFetchAdd(baseNArr, 1);
        if(Nt >= arrinfo->MaxNArr) return;
        baseArr[Nt].a             = (float)Amp;                // amplitude
        baseArr[Nt].Phase         = (float)Phase;              // phase
        baseArr[Nt].delay         = Cpx2Cpxf(delay);           // delay time
        baseArr[Nt].SrcDeclAngle  = (float)rinit.SrcDeclAngle; // launch angle from source
        baseArr[Nt].SrcAzimAngle  = (float)rinit.SrcAzimAngle; // launch angle from source
        baseArr[Nt].RcvrDeclAngle = (float)RcvrDeclAngle; // angle ray reaches receiver
        baseArr[Nt].RcvrAzimAngle = (float)RcvrAzimAngle; // angle ray reaches receiver
        baseArr[Nt].NTopBnc       = NumTopBnc;            // Number of top    bounces
        baseArr[Nt].NBotBnc       = NumBotBnc;            //   "       bottom
    }
}

} // namespace bhc
