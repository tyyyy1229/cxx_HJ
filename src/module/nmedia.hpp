
#pragma once
#include "../common_setup.hpp"
#include "paramsmodule.hpp"

namespace bhc { namespace module {

template<bool O3D> class NMedia : public ParamsModule<O3D> {
public:
    NMedia() {}
    virtual ~NMedia() {}

    virtual void Default(bhcParams<O3D> &params) const override
    {
        params.atten->NMedia = 1;
    }
    virtual void Read(bhcParams<O3D> &params, LDIFile &ENVFile, HSInfo &) const override
    {
        LIST(ENVFile);
        ENVFile.Read(params.atten->NMedia);
    }
    virtual void Write(bhcParams<O3D> &params, LDOFile &ENVFile) const
    {
        ENVFile << params.atten->NMedia;
        ENVFile.write("! NMEDIA\n");
    }
    virtual void Validate(bhcParams<O3D> &params) const override
    {
        if(params.atten->NMedia != 1) {
            EXTWARN("ReadEnvironment: Only one medium or layer is allowed in BELLHOP; "
                    "sediment layers must be handled using a reflection coefficient");
        }
    }
    virtual void Echo(bhcParams<O3D> &params) const override
    {
        PrintFileEmu &PRTFile = GetInternal(params)->PRTFile;
        PRTFile << "Dummy parameter NMedia = " << params.atten->NMedia << "\n";
    }
};

}} // namespace bhc::module
