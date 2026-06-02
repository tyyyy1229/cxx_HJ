
#pragma once
#include "../common_setup.hpp"
#include "paramsmodule.hpp"

namespace bhc { namespace module {

template<bool O3D> class RcvrBearings : public ParamsModule<O3D> {
public:
    RcvrBearings() {}
    virtual ~RcvrBearings() {}

    virtual void Init(bhcParams<O3D> &params) const override
    {
        params.Pos->theta  = nullptr;
        params.Pos->t_rcvr = nullptr;
    }
    virtual void SetupPre(bhcParams<O3D> &params) const override
    {
        params.Pos->Ntheta           = 1;
        params.Pos->thetaDuplRemoved = false;
    }
    virtual void Default(bhcParams<O3D> &params) const override
    {
        if constexpr(O3D) { params.Pos->Ntheta = 5; }
        trackallocate(
            params, "default receiver bearings", params.Pos->theta, params.Pos->Ntheta);
        for(int32_t i = 0; i < params.Pos->Ntheta; ++i) {
            params.Pos->theta[i] = RL(72.0) * (real)i;
        }
    }
    virtual void Read(bhcParams<O3D> &params, LDIFile &ENVFile, HSInfo &) const override
    {
        if constexpr(O3D) {
            ReadVector(
                params, params.Pos->theta, params.Pos->Ntheta, ENVFile, Description);
            CheckFix360Sweep(
                params.Pos->theta, params.Pos->Ntheta, params.Pos->thetaDuplRemoved);
        } else {
            Default(params);
        }
    }
    virtual void Write(bhcParams<O3D> &params, LDOFile &ENVFile) const
    {
        if constexpr(O3D) {
            UnSubTab(
                ENVFile, params.Pos->theta, params.Pos->Ntheta,
                "Ntheta (number of bearings)", "bearing angles (degrees)");
        }
    }
    void ExtSetup(bhcParams<O3D> &params, int32_t Ntheta) const
    {
        params.Pos->Ntheta = Ntheta;
        trackallocate(params, Description, params.Pos->theta, params.Pos->Ntheta);
    }
    virtual void Validate(bhcParams<O3D> &params) const override
    {
        if constexpr(!O3D) {
            if(params.Pos->Ntheta != 1 || params.Pos->theta[0] != RL(0.0)) {
                EXTERR("Invalid receiver bearings setup for 2D");
            }
        } else {
            ValidateVector(params, params.Pos->theta, params.Pos->Ntheta, Description);
        }
    }
    virtual void Echo(bhcParams<O3D> &params) const override
    {
        if constexpr(O3D) {
            PrintFileEmu &PRTFile = GetInternal(params)->PRTFile;
            Preprocess(params);
            EchoVectorWDescr(
                params, params.Pos->theta, params.Pos->Ntheta, FL(1.0), Description,
                Units);
            if(params.Pos->thetaDuplRemoved) {
                PRTFile << "(Duplicate angle at " << params.Pos->theta[0] + FL(360.0)
                        << " degrees removed--this occurs in BELLHOP(3D) too,\n"
                           "but that version echoes the vector before removing the "
                           "duplicate)\n";
            }
        }
    }
    virtual void Preprocess(bhcParams<O3D> &params) const override
    {
        Position *Pos = params.Pos;
        // calculate angular spacing
        Pos->Delta_theta = FL(0.0);
        if(Pos->Ntheta != 1)
            Pos->Delta_theta = Pos->theta[Pos->Ntheta - 1] - Pos->theta[Pos->Ntheta - 2];

        trackallocate(params, "receiver bearing sin/cos table", Pos->t_rcvr, Pos->Ntheta);
        for(int32_t i = 0; i < params.Pos->Ntheta; ++i) {
            real theta            = DegRad * params.Pos->theta[i];
            params.Pos->t_rcvr[i] = vec2(STD::cos(theta), STD::sin(theta));
        }
    }
    virtual void Finalize(bhcParams<O3D> &params) const override
    {
        trackdeallocate(params, params.Pos->theta);
        trackdeallocate(params, params.Pos->t_rcvr);
    }

private:
    constexpr static const char *Description = "Receiver bearings, theta";
    constexpr static const char *Units       = "degrees";
};

}} // namespace bhc::module
