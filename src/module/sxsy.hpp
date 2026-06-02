
#pragma once
#include "../common_setup.hpp"
#include "paramsmodule.hpp"

namespace bhc { namespace module {

/**
 * source x-y coordinates
 */
template<bool O3D> class SxSy : public ParamsModule<O3D> {
public:
    SxSy() {}
    virtual ~SxSy() {}

    virtual void Init(bhcParams<O3D> &params) const override
    {
        params.Pos->Sx = nullptr;
        params.Pos->Sy = nullptr;
    }
    virtual void SetupPre(bhcParams<O3D> &params) const override
    {
        params.Pos->SxSyInKm = true;
        params.Pos->NSx      = 1;
        params.Pos->NSy      = 1;
    }
    virtual void Default(bhcParams<O3D> &params) const override
    {
        trackallocate(params, "default source x-coordinates", params.Pos->Sx, 1);
        trackallocate(params, "default source y-coordinates", params.Pos->Sy, 1);
        params.Pos->Sx[0] = FL(0.0); // dummy x-coordinate
        params.Pos->Sy[0] = FL(0.0); // dummy y-coordinate
    }
    virtual void Read(bhcParams<O3D> &params, LDIFile &ENVFile, HSInfo &) const override
    {
        if constexpr(O3D) {
            ReadVector(params, params.Pos->Sx, params.Pos->NSx, ENVFile, DescriptionX);
            ReadVector(params, params.Pos->Sy, params.Pos->NSy, ENVFile, DescriptionY);
        } else {
            Default(params);
        }
    }
    virtual void Write(bhcParams<O3D> &params, LDOFile &ENVFile) const
    {
        if constexpr(O3D) {
            UnSubTab(
                ENVFile, params.Pos->Sx, params.Pos->NSx, "NSX", "SX(1:NSX) (km)",
                FL(0.001));
            UnSubTab(
                ENVFile, params.Pos->Sy, params.Pos->NSy, "NSY", "SY(1:NSY) (km)",
                FL(0.001));
        }
    }
    void ExtSetup(bhcParams<O3D> &params, int32_t NSx, int32_t NSy) const
    {
        params.Pos->NSx = NSx;
        params.Pos->NSy = NSy;
        trackallocate(params, DescriptionX, params.Pos->Sx, params.Pos->NSx);
        trackallocate(params, DescriptionY, params.Pos->Sy, params.Pos->NSy);
    }
    virtual void Validate(bhcParams<O3D> &params) const override
    {
        if(params.Pos->NSx * params.Pos->NSy * params.Pos->NSz <= 0) {
            EXTERR(
                "Invalid number of sources: %d x %d y %d z", params.Pos->NSx,
                params.Pos->NSy, params.Pos->NSz);
        }
        if constexpr(!O3D) {
            if(params.Pos->NSx != 1 || params.Pos->NSy != 1
               || params.Pos->Sx[0] != RL(0.0) || params.Pos->Sy[0] != RL(0.0)) {
                EXTERR("Invalid Sx/Sy setup for 2D case");
            }
        } else {
            ValidateVector(params, params.Pos->Sx, params.Pos->NSx, DescriptionX);
            ValidateVector(params, params.Pos->Sy, params.Pos->NSy, DescriptionY);
        }
    }
    virtual void Echo(bhcParams<O3D> &params) const override
    {
        if constexpr(O3D) {
            Preprocess(params);
            EchoVectorWDescr(
                params, params.Pos->Sx, params.Pos->NSx, FL(0.001), DescriptionX, Units);
            EchoVectorWDescr(
                params, params.Pos->Sy, params.Pos->NSy, FL(0.001), DescriptionY, Units);
        }
    }
    virtual void Preprocess(bhcParams<O3D> &params) const override
    {
        if(params.Pos->SxSyInKm) {
            params.Pos->SxSyInKm = false;
            ToMeters(params.Pos->Sx, params.Pos->NSx);
            ToMeters(params.Pos->Sy, params.Pos->NSy);
        }
    }
    virtual void Finalize(bhcParams<O3D> &params) const override
    {
        trackdeallocate(params, params.Pos->Sx);
        trackdeallocate(params, params.Pos->Sy);
    }

private:
    constexpr static const char *DescriptionX = "Source   x-coordinates, Sx";
    constexpr static const char *DescriptionY = "Source   y-coordinates, Sy";
    constexpr static const char *Units        = "km";
};

}} // namespace bhc::module
