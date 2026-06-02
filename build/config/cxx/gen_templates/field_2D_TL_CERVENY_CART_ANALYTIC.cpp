
#include "/home/ty/bellhopcxx/bellhopcxx_copy/src/mode/fieldimpl.hpp"
#include "/home/ty/bellhopcxx/bellhopcxx_copy/src/trace.hpp"

#include <vector>

namespace bhc { namespace mode {

using GENCFG = CfgSel<'C', 'C', 'A'>;

template<> void FieldModesWorker<GENCFG, false, false>(
    bhcParams<false> &params,
    bhcOutputs<false, false> &outputs,
    ErrState *errState)
{
    SetupThread();
    while(true) {
        int32_t job = GetInternal(params)->sharedJobID++;
        RayInitInfo rinit;
        if(!GetJobIndices<false>(rinit, job, params.Pos, params.Angles)) break;

        MainFieldModes<GENCFG, false, false>(
            rinit, outputs.uAllSources, params.Bdry, params.bdinfo, params.refl,
            params.ssp, params.Pos, params.Angles, params.freqinfo, params.Beam,
            params.sbp, outputs.eigen, outputs.arrinfo, errState);
    }
}

template<> void RunFieldModesImpl<GENCFG, false, false>(
    bhcParams<false> &params,
    bhcOutputs<false, false> &outputs)
{
    ErrState errState;
    ResetErrState(&errState);
    GetInternal(params)->sharedJobID  = 0;
    int32_t numThreads = GetInternal(params)->numThreads;
    std::vector<std::thread> threads;
    for(int32_t i = 0; i < numThreads; ++i)
        threads.push_back(std::thread(
            FieldModesWorker<GENCFG, false, false>, std::ref(params),
            std::ref(outputs), &errState));
    for(int32_t i = 0; i < numThreads; ++i) threads[i].join();
    CheckReportErrors(GetInternal(params), &errState);
}

}} // namespace bhc::mode
