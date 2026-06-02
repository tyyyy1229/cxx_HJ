
#pragma once
#include "../common_setup.hpp"

namespace bhc { namespace module {


template<bool O3D> class ParamsModule {
public:
    ParamsModule() {}
    virtual ~ParamsModule() {}

    /// True one-time initialization, e.g. set pointers to arrays to nullptr.
    virtual void Init(bhcParams<O3D> &) const {}
    /// Called before Default or Read for common setup. Can set some defaults
    /// here which are needed in case the env file does not write certain
    /// variables.
    virtual void SetupPre(bhcParams<O3D> &) const {}
    /// Set the parameters to some reasonable default values in place of Read.
    virtual void Default(bhcParams<O3D> &) const = 0;
    /// Read the parameters from the environment file or other input files.
    virtual void Read(bhcParams<O3D> &, LDIFile &, HSInfo &) const {}
    /// Write the parameters to an environment file and/or other output files.
    virtual void Write(bhcParams<O3D> &, LDOFile &) const {}
    /// Called after Default or Read for common setup.
    virtual void SetupPost(bhcParams<O3D> &) const {}
    /// Check if the parameters are valid values. Throws errors if not.
    virtual void Validate(bhcParams<O3D> &) const {}
    /// Writes info about the parameters to the print file emulator.
    virtual void Echo(bhcParams<O3D> &) const {}
    /// Modifies the parameters before processing, e.g. km to m. Module must add
    /// flags to params to track whether this has been done or not.
    virtual void Preprocess(bhcParams<O3D> &) const {}
    /// Deallocate memory.
    virtual void Finalize(bhcParams<O3D> &) const {}
};

}} // namespace bhc::module
