
#pragma once
#include "../common_setup.hpp"
#include "field.hpp"

namespace bhc { namespace mode {

template<bool O3D, bool R3D> void PostProcessTL(
    const bhcParams<O3D> &params, bhcOutputs<O3D, R3D> &outputs);
extern template void PostProcessTL<false, false>(
    const bhcParams<false> &params, bhcOutputs<false, false> &outputs);
extern template void PostProcessTL<true, false>(
    const bhcParams<true> &params, bhcOutputs<true, false> &outputs);
extern template void PostProcessTL<true, true>(
    const bhcParams<true> &params, bhcOutputs<true, true> &outputs);

template<bool O3D, bool R3D> void WriteOutTL(
    const bhcParams<O3D> &params, const bhcOutputs<O3D, R3D> &outputs);
extern template void WriteOutTL<false, false>(
    const bhcParams<false> &params, const bhcOutputs<false, false> &outputs);
extern template void WriteOutTL<true, false>(
    const bhcParams<true> &params, const bhcOutputs<true, false> &outputs);
extern template void WriteOutTL<true, true>(
    const bhcParams<true> &params, const bhcOutputs<true, true> &outputs);

template<bool O3D, bool R3D> void ReadOutTL(
    bhcParams<O3D> &params, bhcOutputs<O3D, R3D> &outputs, const char *FileRoot);
extern template void ReadOutTL<false, false>(
    bhcParams<false> &params, bhcOutputs<false, false> &outputs, const char *FileRoot);
extern template void ReadOutTL<true, false>(
    bhcParams<true> &params, bhcOutputs<true, false> &outputs, const char *FileRoot);
extern template void ReadOutTL<true, true>(
    bhcParams<true> &params, bhcOutputs<true, true> &outputs, const char *FileRoot);

template<bool O3D, bool R3D> class TL : public Field<O3D, R3D> {
public:
    TL() {}
    virtual ~TL() {}

    virtual void Init(bhcOutputs<O3D, R3D> &outputs) const override
    {
        outputs.uAllSources = nullptr;
    }

    virtual void Preprocess(
        bhcParams<O3D> &params, bhcOutputs<O3D, R3D> &outputs) const override
    {
        Field<O3D, R3D>::Preprocess(params, outputs);

        trackdeallocate(params, outputs.uAllSources); // Free if previously run
        // for a TL calculation, allocate space for the pressure matrix
        const Position *Pos = params.Pos;
        size_t n            = (size_t)Pos->NSz * (size_t)Pos->NSx * (size_t)Pos->NSy
            * (size_t)Pos->Ntheta * (size_t)Pos->NRz_per_range * (size_t)Pos->NRr;
        trackallocate(params, "sound field / transmission loss", outputs.uAllSources, n);
        memset(outputs.uAllSources, 0, n * sizeof(cpxf));
    }

    virtual void Postprocess(
        bhcParams<O3D> &params, bhcOutputs<O3D, R3D> &outputs) const override
    {
        PostProcessTL<O3D, R3D>(params, outputs);
    }

    virtual void Writeout(
        const bhcParams<O3D> &params, const bhcOutputs<O3D, R3D> &outputs) const override
    {
        WriteOutTL<O3D, R3D>(params, outputs);
    }

    virtual void Readout(
        bhcParams<O3D> &params, bhcOutputs<O3D, R3D> &outputs,
        const char *FileRoot) const override
    {
        ReadOutTL<O3D, R3D>(params, outputs, FileRoot);
    }

    virtual void Finalize(
        bhcParams<O3D> &params, bhcOutputs<O3D, R3D> &outputs) const override
    {
        trackdeallocate(params, outputs.uAllSources);
    }
};

}} // namespace bhc::mode
