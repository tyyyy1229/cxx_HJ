
#pragma once
#include "../common_setup.hpp"
#include "paramsmodule.hpp"

namespace bhc { namespace module {

template<bool O3D> class RcvrRanges : public ParamsModule<O3D> {
public:
    RcvrRanges() {}
    virtual ~RcvrRanges() {}

    virtual void Init(bhcParams<O3D> &params) const override { params.Pos->Rr = nullptr; }
    virtual void SetupPre(bhcParams<O3D> &params) const override
    {
        params.Pos->NRr    = 1;
        params.Pos->RrInKm = true;
    }
    virtual void Default(bhcParams<O3D> &params) const override
    {
        params.Pos->NRr = 10;
        trackallocate(
            params, "default receiver r-coordinates", params.Pos->Rr, params.Pos->NRr);
        for(int32_t i = 0; i < params.Pos->NRr; ++i) {
            params.Pos->Rr[i] = RL(5.0) * (real)(i + 1);
        }
    }
    virtual void Read(bhcParams<O3D> &params, LDIFile &ENVFile, HSInfo &) const override
    {
        ReadVector(params, params.Pos->Rr, params.Pos->NRr, ENVFile, Description2);
    }
    virtual void Write(bhcParams<O3D> &params, LDOFile &ENVFile) const
    {
        UnSubTab(
            ENVFile, params.Pos->Rr, params.Pos->NRr, "NR", "R(1:NR ) (km)", FL(0.001));
    }
    virtual void SetupPost(bhcParams<O3D> &params) const override
    {
        // RayAngles::EstimateNumAngles uses Rr[NRr-1], must be in meters by then
        Preprocess(params);
    }
    void ExtSetup(bhcParams<O3D> &params, int32_t NRr) const
    {
        params.Pos->NRr = NRr;
        trackallocate(params, Description2, params.Pos->Rr, params.Pos->NRr);
    }
    virtual void Validate(bhcParams<O3D> &params) const override
    {
        ValidateVector(params, params.Pos->Rr, params.Pos->NRr, Description2);
    }
    virtual void Echo(bhcParams<O3D> &params) const override
    {
        Preprocess(params);
        EchoVectorWDescr(
            params, params.Pos->Rr, params.Pos->NRr, FL(0.001), Description, Units);
    }
    virtual void Preprocess(bhcParams<O3D> &params) const override
    {
        Position *Pos = params.Pos;
        if(Pos->RrInKm) {
            ToMeters(Pos->Rr, Pos->NRr);
            Pos->RrInKm = false;
        }

        // calculate range spacing
        Pos->Delta_r = FL(0.0);
        if(Pos->NRr >= 2) Pos->Delta_r = Pos->Rr[Pos->NRr - 1] - Pos->Rr[Pos->NRr - 2];
    }
    virtual void Finalize(bhcParams<O3D> &params) const override
    {
        trackdeallocate(params, params.Pos->Rr);
    }

private:
    constexpr static const char *Description  = "Receiver r-coordinates, Rr";
    constexpr static const char *Description2 = "Receiver ranges";
    constexpr static const char *Units        = "km";
};

}} // namespace bhc::module
