
#pragma once
#include "../common_setup.hpp"
#include "modemodule.hpp"
#include "fieldimpl.hpp"
#include "../influence.hpp"

namespace bhc { namespace mode {

/**
 * Parent class for field modes (TL, eigen, arr).
 */
template<bool O3D, bool R3D> class Field : public ModeModule<O3D, R3D> {
public:
    Field() {}
    virtual ~Field() {}

    virtual void Preprocess(bhcParams<O3D> &params, bhcOutputs<O3D, R3D> &) const override
    {
        PreRun_Influence<O3D, R3D>(params);
    }

    virtual void Run(bhcParams<O3D> &params, bhcOutputs<O3D, R3D> &outputs) const override
    {
        RunFieldModesSelInfl<O3D, R3D>(params, outputs);
    }
};

}} // namespace bhc::mode
