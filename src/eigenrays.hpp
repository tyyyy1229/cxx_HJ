
#pragma once
#include "common_run.hpp"

namespace bhc {

HOST_DEVICE inline void RecordEigenHit(
    int32_t itheta, int32_t ir, int32_t iz, int32_t is, const RayInitInfo &rinit,
    EigenInfo *eigen)
{
    int32_t mi = AtomicFetchAdd(&eigen->neigen, 1);
    if(mi >= eigen->memsize) return;
    // printf("Eigenray hit %d ir %d iz %d isrc %d ialpha %d is %d\n",
    //     mi, ir, iz, isrc, ialpha, is);
    eigen->hits[mi].is     = is;
    eigen->hits[mi].iz     = iz;
    eigen->hits[mi].ir     = ir;
    eigen->hits[mi].itheta = itheta;
    eigen->hits[mi].isx    = rinit.isx;
    eigen->hits[mi].isy    = rinit.isy;
    eigen->hits[mi].isz    = rinit.isz;
    eigen->hits[mi].ialpha = rinit.ialpha;
    eigen->hits[mi].ibeta  = rinit.ibeta;
}

} // namespace bhc
