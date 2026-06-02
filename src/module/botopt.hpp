
#pragma once
#include "../common_setup.hpp"
#include "paramsmodule.hpp"
#include "boundarycond.hpp"

namespace bhc { namespace module {

template<bool O3D> class BotOpt : public ParamsModule<O3D> {
public:
    BotOpt() {}
    virtual ~BotOpt() {}

    virtual void Default(bhcParams<O3D> &params) const override
    {
        memcpy(params.Bdry->Bot.hs.Opt, "R-    ", 6);
    }
    virtual void Read(bhcParams<O3D> &params, LDIFile &ENVFile, HSInfo &) const override
    {
        LIST(ENVFile);
        ENVFile.Read(params.Bdry->Bot.hs.Opt, 6); // LP: LDIFile fills rest with ' '
        ENVFile.Read(params.Bdry->Bot.hsx.Sigma);
    }
    virtual void Write(bhcParams<O3D> &params, LDOFile &ENVFile) const
    {
        ENVFile << std::string(params.Bdry->Bot.hs.Opt, 6) << params.Bdry->Bot.hsx.Sigma;
        ENVFile.write("! bot bc (");
        BoundaryCond<O3D, false>::WriteBCTag(params.Bdry->Bot.hs.Opt[0], ENVFile);
        ENVFile.write("), bathymetry, 4 spaces; Sigma (printed but ignored)\n");
    }
    virtual void SetupPost(bhcParams<O3D> &params) const override
    {
        params.Bdry->Bot.hs.bc = params.Bdry->Bot.hs.Opt[0];
    }
    virtual void Validate(bhcParams<O3D> &params) const override
    {
        switch(params.Bdry->Bot.hs.Opt[1]) {
        case '~':
        case '*': break;
        case '-':
        case '_':
        case ' ': break;
        default:
            EXTERR(
                "Unknown bottom option letter in second position: Bdry->Bot.hs.Opt[1] == "
                "'%c'",
                params.Bdry->Bot.hs.Opt[1]);
        }
    }
    virtual void Echo(bhcParams<O3D> &params) const override
    {
        PrintFileEmu &PRTFile = GetInternal(params)->PRTFile;

        PRTFile << "\n RMS roughness = " << std::setw(10) << std::setprecision(3)
                << params.Bdry->Bot.hsx.Sigma << "\n";

        switch(params.Bdry->Bot.hs.Opt[1]) {
        case '~':
        case '*': PRTFile << "    Bathymetry file selected\n";
        }
    }
};

}} // namespace bhc::module
