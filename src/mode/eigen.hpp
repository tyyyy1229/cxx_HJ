
#pragma once
#include "../common_setup.hpp"
#include "ray.hpp"
#include "field.hpp"

namespace bhc { namespace mode {

template<bool O3D, bool R3D> void PostProcessEigenrays(
    bhcParams<O3D> &params, bhcOutputs<O3D, R3D> &outputs);
extern template void PostProcessEigenrays<false, false>(
    bhcParams<false> &params, bhcOutputs<false, false> &outputs);
extern template void PostProcessEigenrays<true, false>(
    bhcParams<true> &params, bhcOutputs<true, false> &outputs);
extern template void PostProcessEigenrays<true, true>(
    bhcParams<true> &params, bhcOutputs<true, true> &outputs);

template<bool O3D, bool R3D> class Eigen : public Field<O3D, R3D> {
public:
    Eigen() {}
    virtual ~Eigen() {}

    virtual void Init(bhcOutputs<O3D, R3D> &outputs) const override
    {
        outputs.eigen->hits    = nullptr;
        outputs.eigen->neigen  = 0;
        outputs.eigen->memsize = 0;
    }

    virtual void Preprocess(
        bhcParams<O3D> &params, bhcOutputs<O3D, R3D> &outputs) const override
    {
        Field<O3D, R3D>::Preprocess(params, outputs);

        EigenInfo *eigen = outputs.eigen;
        trackdeallocate(params, eigen->hits); // Free memory if previously run
        // Use 1 / hitsMemFraction of the available memory for eigenray hits
        // (the rest for rays).
        constexpr size_t hitsMemFraction = 500;
        size_t mem     = GetInternal(params)->maxMemory - GetInternal(params)->usedMemory;
        eigen->memsize = (int32_t)
            std::min(mem / (hitsMemFraction * sizeof(EigenHit)), (size_t)0x7FFFFFFF);
        if(eigen->memsize == 0) {
            EXTERR("Insufficient memory to allocate any eigen hits at all");
        } else if(eigen->memsize < 10000) {
            EXTWARN(
                "There is only enough memory to allocate %d eigen hits, using 1/%dth "
                "of the total available memory for eigen hits",
                eigen->memsize, hitsMemFraction);
        }
        trackallocate(params, "eigenray hits", eigen->hits, eigen->memsize);
        eigen->neigen = 0;
    }

    virtual void Postprocess(
        bhcParams<O3D> &params, bhcOutputs<O3D, R3D> &outputs) const override
    {
        PostProcessEigenrays<O3D, R3D>(params, outputs);
    }

    virtual void Writeout(
        const bhcParams<O3D> &params, const bhcOutputs<O3D, R3D> &outputs) const override
    {
        Ray<O3D, R3D> raymode;
        raymode.Writeout(params, outputs);
    }

    virtual void Finalize(
        bhcParams<O3D> &params, bhcOutputs<O3D, R3D> &outputs) const override
    {
        trackdeallocate(params, outputs.eigen->hits);
    }
};

}} // namespace bhc::mode
