
#pragma once
#include "../common_setup.hpp"

namespace bhc { namespace mode {

/**
 * Like ParamsModule, but for outputs, and fewer steps.
 */
template<bool O3D, bool R3D> class ModeModule {
public:
    ModeModule() {}
    virtual ~ModeModule() {}

    /// Initialization and defaults.
    virtual void Init(bhcOutputs<O3D, R3D> &) const {}
    /// Preprocessing as part of run.
    virtual void Preprocess(bhcParams<O3D> &, bhcOutputs<O3D, R3D> &) const {}
    /// Run the simulation.
    virtual void Run(bhcParams<O3D> &, bhcOutputs<O3D, R3D> &) const = 0;
    /// Postprocess after run is complete.
    virtual void Postprocess(bhcParams<O3D> &, bhcOutputs<O3D, R3D> &) const {}
    /// Write results to disk.
    virtual void Writeout(const bhcParams<O3D> &, const bhcOutputs<O3D, R3D> &) const {}
    /// Read results from disk.
    virtual void Readout(bhcParams<O3D> &, bhcOutputs<O3D, R3D> &, const char *) const {}
    /// Deallocate memory.
    virtual void Finalize(bhcParams<O3D> &, bhcOutputs<O3D, R3D> &) const {}
};

}} // namespace bhc::mode
