
#pragma once
#include "../common_setup.hpp"

namespace bhc { namespace module {

/**
 * Source Beam Pattern, formerly "Pat" (as in ReadPat)
 */
template<bool O3D> class SBP : public ParamsModule<O3D> {
public:
    SBP() {}
    virtual ~SBP() {}

    virtual void Init(bhcParams<O3D> &params) const override
    {
        params.sbp->SrcBmPat = nullptr;
    }

    virtual void SetupPre(bhcParams<O3D> &params) const override
    {
        params.sbp->SBPFlag = params.Beam->RunType[2];
        params.sbp->SBPIndB = true;
    }

    virtual void Default(bhcParams<O3D> &params) const override
    {
        SBPInfo *sbp = params.sbp;
        sbp->NSBPPts = 2;
        trackallocate(
            params, "default/trivial source beam pattern", sbp->SrcBmPat, 2 * 2);
        sbp->SrcBmPat[0 * 2 + 0] = FL(-180.0);
        sbp->SrcBmPat[0 * 2 + 1] = FL(0.0);
        sbp->SrcBmPat[1 * 2 + 0] = FL(180.0);
        sbp->SrcBmPat[1 * 2 + 1] = FL(0.0);
    }

    virtual void Read(bhcParams<O3D> &params, LDIFile &, HSInfo &) const override
    {
        PrintFileEmu &PRTFile = GetInternal(params)->PRTFile;
        SBPInfo *sbp          = params.sbp;
        if(sbp->SBPFlag != '*') {
            Default(params);
            return;
        }

        LDIFile SBPFile(GetInternal(params), GetInternal(params)->FileRoot + ".sbp");
        if(!SBPFile.Good()) {
            PRTFile << "SBPFile = " << GetInternal(params)->FileRoot << ".sbp\n";
            EXTERR(BHC_PROGRAMNAME "-ReadPat: Unable to open source beampattern file");
        }

        LIST(SBPFile);
        SBPFile.Read(sbp->NSBPPts);
        trackallocate(params, Description, sbp->SrcBmPat, sbp->NSBPPts * 2);

        for(int32_t i = 0; i < sbp->NSBPPts; ++i) {
            LIST(SBPFile);
            SBPFile.Read(&sbp->SrcBmPat[2 * i], 2);
        }
    }

    virtual void Write(bhcParams<O3D> &params, LDOFile &) const
    {
        PrintFileEmu &PRTFile = GetInternal(params)->PRTFile;
        SBPInfo *sbp          = params.sbp;
        if(sbp->SBPFlag != '*') return;

        LDOFile SBPFile;
        SBPFile.setStyle(LDOFile::Style::WRITTEN_BY_HAND);
        SBPFile.open(GetInternal(params)->FileRoot + ".sbp");
        if(!SBPFile.good()) {
            PRTFile << "SBPFile = " << GetInternal(params)->FileRoot << ".sbp\n";
            EXTERR(BHC_PROGRAMNAME
                   "-WritePat: Unable to open new source beampattern file");
        }

        SBPFile << sbp->NSBPPts;
        SBPFile.write("! " BHC_PROGRAMNAME "- Source Beam Pattern file for ");
        SBPFile.write(params.Title);
        SBPFile << '\n';
        for(int32_t i = 0; i < sbp->NSBPPts; ++i) {
            real dB = FL(20.0) * STD::log10(sbp->SrcBmPat[2 * i + 1]);
            SBPFile << sbp->SrcBmPat[2 * i] << dB << '\n';
        }
    }

    void ExtSetup(bhcParams<O3D> &params, int32_t NSBPPts) const
    {
        params.sbp->NSBPPts = NSBPPts;
        trackallocate(params, Description, params.sbp->SrcBmPat, params.sbp->NSBPPts);
    }

    virtual void Validate(bhcParams<O3D> &params) const override
    {
        SBPInfo *sbp = params.sbp;
        if(!monotonic(sbp->SrcBmPat, sbp->NSBPPts, 2, 0)) {
            EXTERR(BHC_PROGRAMNAME
                   "-ReadPat: Source beam pattern angles are not monotonic");
        }
    }

    virtual void Echo(bhcParams<O3D> &params) const override
    {
        PrintFileEmu &PRTFile = GetInternal(params)->PRTFile;
        SBPInfo *sbp          = params.sbp;
        Preprocess(params);

        if(sbp->SBPFlag == '*') {
            PRTFile
                << "\n______________________________\nUsing source beam pattern file\n";
        }
       

        PRTFile << "Number of source beam pattern points " << sbp->NSBPPts << "\n";
        PRTFile << "\n Angle (degrees)  Power (dB)\n" << std::setprecision(3);
        for(int32_t i = 0; i < sbp->NSBPPts; ++i) {
            real dB = FL(20.0) * STD::log10(sbp->SrcBmPat[2 * i + 1]);
            PRTFile << sbp->SrcBmPat[2 * i] << " " << dB << "\n";
        }

        PRTFile << "\n";
    }

    virtual void Preprocess(bhcParams<O3D> &params) const override
    {
        SBPInfo *sbp = params.sbp;
        if(sbp->SBPIndB) {
            sbp->SBPIndB = false;
            // convert dB to linear scale
            for(int32_t i = 0; i < sbp->NSBPPts; ++i)
                sbp->SrcBmPat[i * 2 + 1]
                    = STD::pow(FL(10.0), sbp->SrcBmPat[i * 2 + 1] / FL(20.0));
        }
    }

    virtual void Finalize(bhcParams<O3D> &params) const override
    {
        trackdeallocate(params, params.sbp->SrcBmPat);
    }

private:
    constexpr static const char *Description = "source beam pattern";
};

}} // namespace bhc::module
