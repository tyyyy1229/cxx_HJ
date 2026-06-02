
#pragma once
#include "../common_setup.hpp"
#include "paramsmodule.hpp"

namespace bhc { namespace module {

/**
 * Optionally read in reflection coefficient for Top or Bottom boundary
 *
 * flag set to 'F' if refl. coef. is to be read from a File
 */
template<bool O3D, bool ISTOP> class ReflCoef : public ParamsModule<O3D> {
public:
    ReflCoef() {}
    virtual ~ReflCoef() {}

    virtual void Init(bhcParams<O3D> &params) const override
    {
        ReflectionInfoTopBot *refltb = GetReflTopBot(params);
        refltb->r                    = nullptr;
    }

    virtual void Default(bhcParams<O3D> &params) const override
    {
        ReflectionInfoTopBot *refltb = GetReflTopBot(params);
    
        refltb->NPts = 0;
    }

    virtual void Read(bhcParams<O3D> &params, LDIFile &, HSInfo &) const override
    {
        ReflectionInfoTopBot *refltb = GetReflTopBot(params);
        PrintFileEmu &PRTFile        = GetInternal(params)->PRTFile;
        if(!IsFile(params)) {
            Default(params);
            return;
        }

        LDIFile xRCFile(GetInternal(params), GetInternal(params)->FileRoot + s_extension);
        if(!xRCFile.Good()) {
            PRTFile << s_RC << "File = " << GetInternal(params)->FileRoot << s_extension
                    << "\n";
            EXTERR(
                "ReadReflectionCoefficient: Unable to open %s Reflection "
                "Coefficient file",
                s_TopBottom);
        }
        LIST(xRCFile);
        xRCFile.Read(refltb->NPts);
        trackallocate(params, "reflection coefficients", refltb->r, refltb->NPts);

        LIST(xRCFile);
        for(int32_t itheta = 0; itheta < refltb->NPts; ++itheta) {
            xRCFile.Read(refltb->r[itheta].theta);
            xRCFile.Read(refltb->r[itheta].r);
            xRCFile.Read(refltb->r[itheta].phi);
        }
        refltb->inDegrees = true;
    }

    virtual void Write(bhcParams<O3D> &params, LDOFile &) const
    {
        ReflectionInfoTopBot *refltb = GetReflTopBot(params);
        PrintFileEmu &PRTFile        = GetInternal(params)->PRTFile;
        if(!IsFile(params)) return;

        LDOFile xRCFile;
        xRCFile.setStyle(LDOFile::Style::WRITTEN_BY_HAND);
        xRCFile.open(GetInternal(params)->FileRoot + s_extension);
        if(!xRCFile.good()) {
            PRTFile << s_RC << "File = " << GetInternal(params)->FileRoot << s_extension
                    << "\n";
            EXTERR(
                "WriteReflectionCoefficient: Unable to open new %s Reflection "
                "Coefficient file",
                s_TopBottom);
        }

        xRCFile << refltb->NPts;
        xRCFile.write("! " BHC_PROGRAMNAME "- ");
        xRCFile.write(s_TopBottom);
        xRCFile.write(" Reflection Coefficient file for");
        xRCFile.write(params.Title);
        xRCFile << '\n';

        for(int32_t itheta = 0; itheta < refltb->NPts; ++itheta) {
            xRCFile << refltb->r[itheta].theta;
            xRCFile << refltb->r[itheta].r;
            xRCFile << (refltb->r[itheta].phi * RadDeg);
            xRCFile << '\n';
        }
    }

    void ExtSetup(bhcParams<O3D> &params, int32_t NPts) const
    {
        ReflectionInfoTopBot *refltb = GetReflTopBot(params);
        GetModeFlag(params)          = 'F';
        refltb->NPts                 = NPts;
        trackallocate(params, "reflection coefficients", refltb->r, refltb->NPts);
    }

    virtual void Validate(bhcParams<O3D> &params) const override
    {
        if constexpr(!ISTOP) {
            // Optionally read in internal reflection coefficient data
            if(params.Bdry->Bot.hs.Opt[0] == 'P') {
                EXTERR("Internal reflections not supported by BELLHOP and therefore "
                       "not supported by " BHC_PROGRAMNAME);
            }
        }

        if(!IsFile(params)) return;
        ReflectionInfoTopBot *refltb = GetReflTopBot(params);

        if(!monotonic(
               &refltb->r[0].theta, refltb->NPts, sizeof(ReflectionCoef) / sizeof(real),
               0)) {
            EXTERR(
                "%s reflection coefficients must be monotonically increasing",
                s_TopBottom);
        }
    }

    virtual void Echo(bhcParams<O3D> &params) const override
    {
        if(!IsFile(params)) return;
        PrintFileEmu &PRTFile        = GetInternal(params)->PRTFile;
        ReflectionInfoTopBot *refltb = GetReflTopBot(params);
        PRTFile << "_____________________________________________________________________"
                   "_____\n\n";
        PRTFile << "Using tabulated " << s_topbottom << " reflection coef.\n";
        PRTFile << "Number of points in " << s_topbottom
                << " reflection coefficient = " << refltb->NPts << "\n";
    }

    virtual void Preprocess(bhcParams<O3D> &params) const override
    {
        if(!IsFile(params)) return;

        ReflectionInfoTopBot *refltb = GetReflTopBot(params);
        if(refltb->inDegrees) {
            refltb->inDegrees = false;
            for(int32_t itheta = 0; itheta < refltb->NPts; ++itheta) {
                refltb->r[itheta].phi *= DegRad; // convert to radians
            }
        }
    }

    virtual void Finalize(bhcParams<O3D> &params) const override
    {
        ReflectionInfoTopBot *refltb = GetReflTopBot(params);
        trackdeallocate(params, refltb->r);
    }

private:
    ReflectionInfoTopBot *GetReflTopBot(bhcParams<O3D> &params) const
    {
        if constexpr(ISTOP)
            return &params.refl->top;
        else
            return &params.refl->bot;
    }
    char &GetModeFlag(bhcParams<O3D> &params) const
    {
        if constexpr(ISTOP)
            return params.Bdry->Top.hs.Opt[1];
        else
            return params.Bdry->Bot.hs.Opt[0];
    }
    bool IsFile(bhcParams<O3D> &params) const { return GetModeFlag(params) == 'F'; }
    constexpr static const char *s_topbottom = ISTOP ? "top   " : "bottom";
    constexpr static const char *s_TopBottom = ISTOP ? "Top" : "Bottom";
    constexpr static const char *s_extension = ISTOP ? ".trc" : ".brc";
    constexpr static const char *s_RC        = ISTOP ? "TRC" : "BRC";
};

template<bool O3D> using TRC = ReflCoef<O3D, true>;
template<bool O3D> using BRC = ReflCoef<O3D, false>;

}} // namespace bhc::module
