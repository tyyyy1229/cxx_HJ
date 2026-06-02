
#pragma once
#include "../common_setup.hpp"
#include "paramsmodule.hpp"

namespace bhc { namespace module {

template<bool O3D> cpx crci(
    const bhcParams<O3D> &params, real z, real c, real alpha, const char (&AttenUnit)[2]);
extern template cpx crci<false>(
    const bhcParams<false> &params, real z, real c, real alpha,
    const char (&AttenUnit)[2]);
extern template cpx crci<true>(
    const bhcParams<true> &params, real z, real c, real alpha,
    const char (&AttenUnit)[2]);

template<bool O3D> class Atten : public ParamsModule<O3D> {
public:
    Atten() {}
    virtual ~Atten() {}

    virtual void SetupPre(bhcParams<O3D> &params) const override
    {
        params.atten->t        = FL(20.0);
        params.atten->Salinity = FL(35.0);
        params.atten->pH       = FL(8.0);
        params.atten->z_bar    = FL(0.0);
    }
    virtual void Default(bhcParams<O3D> &) const override {}
};

}} // namespace bhc::module
