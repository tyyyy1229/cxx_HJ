
#pragma once
#include "../common_setup.hpp"
#include "paramsmodule.hpp"

namespace bhc { namespace module {

template<bool O3D> class Freq0 : public ParamsModule<O3D> {
public:
    Freq0() {}
    virtual ~Freq0() {}

    virtual void Default(bhcParams<O3D> &params) const override
    {
        params.freqinfo->freq0 = RL(50.0);
    }
    virtual void Read(bhcParams<O3D> &params, LDIFile &ENVFile, HSInfo &) const override
    {
        LIST(ENVFile);
        ENVFile.Read(params.freqinfo->freq0);
    }
    virtual void Write(bhcParams<O3D> &params, LDOFile &ENVFile) const
    {
        ENVFile << params.freqinfo->freq0;
        ENVFile.write("! FREQ (Hz)\n");
    }
    virtual void Echo(bhcParams<O3D> &params) const override
    {
        PrintFileEmu &PRTFile = GetInternal(params)->PRTFile;
        PRTFile << std::setiosflags(std::ios::scientific) << std::setprecision(4);
        PRTFile << " frequency = " << std::setw(11) << params.freqinfo->freq0 << " Hz\n";
    }
};

}} // namespace bhc::module
