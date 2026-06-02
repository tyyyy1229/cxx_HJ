
#pragma once
#include "../common_setup.hpp"
#include "paramsmodule.hpp"

namespace bhc { namespace module {

template<bool O3D> class Title : public ParamsModule<O3D> {
public:
    Title() {}
    virtual ~Title() {}

    virtual void Default(bhcParams<O3D> &params) const override
    {
        SetTitle(params, "no env file");
    }
    virtual void Read(bhcParams<O3D> &params, LDIFile &ENVFile, HSInfo &) const override
    {
        std::string TempTitle;
        LIST(ENVFile);
        ENVFile.Read(TempTitle);
        SetTitle(params, TempTitle);
    }
    virtual void Write(bhcParams<O3D> &params, LDOFile &ENVFile) const
    {
        ENVFile << std::string(params.Title);
        ENVFile.write("! TITLE\n");
    }
    virtual void Echo(bhcParams<O3D> &params) const override
    {
        PrintFileEmu &PRTFile = GetInternal(params)->PRTFile;
        PRTFile << BHC_PROGRAMNAME "- " << params.Title << "\n";
    }

    inline void SetTitle(bhcParams<O3D> &params, const std::string &TempTitle) const
    {
        size_t l = bhc::min(sizeof(params.Title) - 1, TempTitle.size());
        memcpy(params.Title, TempTitle.c_str(), l);
        params.Title[l] = 0;
    }
};

}} // namespace bhc::module
