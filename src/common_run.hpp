
#pragma once

#include "common.hpp"

#define _BHC_INCLUDING_COMPONENTS_ 1
#include "util/atomics.hpp"
#undef _BHC_INCLUDING_COMPONENTS_

namespace bhc {

////////////////////////////////////////////////////////////////////////////////
// Beam box
////////////////////////////////////////////////////////////////////////////////

template<bool O3D> inline HOST_DEVICE VEC23<O3D> BeamBoxCenter(const VEC23<O3D> &xs)
{
    VEC23<O3D> ret = xs;
    // box is centered at z=0
    DEP(ret) = RL(0.0);
    return ret;
}

template<bool O3D, int DIM> inline HOST_DEVICE bool IsOutsideBeamBoxDim(
    const VEC23<O3D> &x, const BeamStructure<O3D> *Beam, const VEC23<O3D> &xs)
{
    static_assert(DIM >= 0 && DIM <= ZDIM<O3D>(), "Invalid use of IsOutsideBoxDim!");
   
    return STD::abs(x[DIM] - BeamBoxCenter<O3D>(xs)[DIM]) >= Beam->Box[DIM];
}

////////////////////////////////////////////////////////////////////////////////
// Algorithms
////////////////////////////////////////////////////////////////////////////////


template<typename REAL> HOST_DEVICE inline int32_t BinarySearchLEQ(
    REAL *arr, int32_t n, const int32_t stride, const int32_t offset, real target)
{
    CHECK_REAL_T();
    int32_t low = 0; // Low is included
    int32_t hi  = n; // Hi is excluded
    while(low < hi) {
        int32_t t = (low + hi) / 2;
        if((real)arr[t * stride + offset] > target) {
            hi = t;
        } else if(t >= n - 1) {
            return n - 1;
        } else if((real)arr[(t + 1) * stride + offset] > target) {
            return t;
        } else {
            low = t + 1;
        }
    }
    return low;
}


template<typename REAL> HOST_DEVICE inline int32_t BinarySearchGEQ(
    REAL *arr, int32_t n, const int32_t stride, const int32_t offset, real target)
{
    CHECK_REAL_T();
    int32_t low = -1;    // Low is excluded
    int32_t hi  = n - 1; // Hi is included
    while(low < hi) {
        int32_t t = (low + hi + 1) / 2; // Round up
        if((real)arr[t * stride + offset] < target) {
            low = t;
        } else if(t <= 0) {
            return 0;
        } else if((real)arr[(t - 1) * stride + offset] < target) {
            return t;
        } else {
            hi = t - 1;
        }
    }
    return hi;
}


template<typename REAL> HOST_DEVICE inline int32_t BinarySearchGT(
    REAL *arr, int32_t n, const int32_t stride, const int32_t offset, real target)
{
    CHECK_REAL_T();
    int32_t low = -1;    // Low is excluded
    int32_t hi  = n - 1; // Hi is included
    while(low < hi) {
        int32_t t = (low + hi + 1) / 2; // Round up
        if((real)arr[t * stride + offset] <= target) {
            low = t;
        } else if(t <= 0) {
            return 0;
        } else if((real)arr[(t - 1) * stride + offset] <= target) {
            return t;
        } else {
            hi = t - 1;
        }
    }
    return hi;
}

////////////////////////////////////////////////////////////////////////////////
// Ray normals
////////////////////////////////////////////////////////////////////////////////

HOST_DEVICE inline void RayNormalImpl(
    const vec3 &t, real phi, bool ignorephi0, real c, vec3 &e1, vec3 &e2)
{
    real rl = glm::length(vec2(t.x, t.y));

    if(phi != RL(0.0) || ignorephi0) {
        real cosphi = STD::cos(phi), sinphi = STD::sin(phi);

        // e1
        e1.x = (c * t.x * t.z * cosphi + t.y * sinphi) / rl;
        e1.y = (c * t.y * t.z * cosphi - t.x * sinphi) / rl;
        e1.z = -c * rl * cosphi;

        // e2
        e2.x = (c * t.x * t.z * sinphi - t.y * cosphi) / rl;
        e2.y = (c * t.y * t.z * sinphi + t.x * cosphi) / rl;
        e2.z = -c * rl * sinphi;
        
    } else {
        e1 = vec3(c * t.x * t.z / rl, c * t.y * t.z / rl, -c * rl);
        e2 = vec3(-t.y / rl, t.x / rl, RL(0.0));
    }
}
/**
 * Computes the ray normals
 *
 * t: tangent vector (NOT) normalized
 * phi: torsion
 * c: sound speed
 * e1, e2: ray unit normals
 */
HOST_DEVICE inline void RayNormal(const vec3 &t, real phi, real c, vec3 &e1, vec3 &e2)
{
    RayNormalImpl(t, phi, false, c, e1, e2);
}
/**
 * Computes the ray normals
 * Same as routine RayNormal except this version assumes t is already normalized
 */
HOST_DEVICE inline void RayNormal_unit(const vec3 &t, real phi, vec3 &e1, vec3 &e2)
{
    RayNormalImpl(t, phi, true, RL(1.0), e1, e2);
}

////////////////////////////////////////////////////////////////////////////////
// Nx2D conversions
////////////////////////////////////////////////////////////////////////////////

template<bool O3D, bool R3D> HOST_DEVICE inline VEC23<O3D> RayToOceanX(
    const VEC23<R3D> &x, const Origin<O3D, R3D> &org)
{
    static_assert(O3D || !R3D, "2D ocean but 3D rays not allowed!");
    if constexpr(O3D && !R3D) {
        return vec3(org.xs.x + x.x * org.tradial.x, org.xs.y + x.x * org.tradial.y, x.y);
    } else {
        return x;
    }
}

template<bool O3D, bool R3D> HOST_DEVICE inline VEC23<O3D> RayToOceanT(
    const VEC23<R3D> &t, const Origin<O3D, R3D> &org)
{
    static_assert(O3D || !R3D, "2D ocean but 3D rays not allowed!");
    if constexpr(O3D && !R3D) {
        return vec3(t.x * org.tradial.x, t.x * org.tradial.y, t.y);
    } else {
        return t;
    }
}

template<bool O3D, bool R3D> HOST_DEVICE inline VEC23<R3D> OceanToRayX(
    const VEC23<O3D> &x, const Origin<O3D, R3D> &org, const VEC23<R3D> &t,
    [[maybe_unused]] const int32_t &snapDim, ErrState *errState)
{
    static_assert(O3D || !R3D, "2D ocean but 3D rays not allowed!");
    if constexpr(O3D && !R3D) {
        vec2 ret;
        // Depth always transfers perfectly--not changed.
        ret.y = x.z;
        // For range, use larger dimension--this avoids divide-by-zero or divide
        // by a small number causing accuracy problems.
        if(STD::abs(org.tradial.x) >= STD::abs(org.tradial.y)) {
            ret.x = (x.x - org.xs.x) / org.tradial.x;
        } else {
            ret.x = (x.y - org.xs.y) / org.tradial.y;
        }
        if(snapDim < -2 || snapDim == -1 || snapDim >= 2) {
            // Either:
            // snapDim out of range (won't happen, but want to help compiler)
            // No snap selected--this is the best estimate
            // Snap to Z--Z already perfect, this is the best estimate
            return ret;
        }
        // Only do this iteration a few times, then give up.
        for(int32_t i = 0; i < 4; ++i) {
            // Go back from 2D to 3D, compare to original x.
            vec3 x_back = RayToOceanX(ret, org);
            // If we can't be on the boundary, we want to be slightly forward of
            // the boundary, measured in terms of the ray tangent range. This also
            // encompasses cases where one component exactly matches (errdir.x_or_y
            // == RL(0.0)). For both of these values, only the sign matters.
            vec2 wantdir     = org.tradial * t.x;
            vec2 errdir      = XYCOMP(x_back) - XYCOMP(x);
            bool correctdirx = (wantdir.x * errdir.x) >= RL(0.0);
            bool correctdiry = (wantdir.y * errdir.y) >= RL(0.0);
            if((snapDim == 0 && correctdirx) || (snapDim == 1 && correctdiry)
               || (correctdirx && correctdiry)) {
                return ret;
            }
        
            ret.x = RealBitsAddInt(ret.x, (t.x > RL(0.0)) ? 1 : -1);
        }
        RunWarning(errState, BHC_WARN_OCEANTORAYX_GAVEUP);
        return ret;
    } else {
        return x;
    }
}

template<bool O3D, bool R3D> HOST_DEVICE inline VEC23<R3D> OceanToRayT(
    const VEC23<O3D> &t, const Origin<O3D, R3D> &org)
{
    static_assert(O3D || !R3D, "2D ocean but 3D rays not allowed!");
    if constexpr(O3D && !R3D) {
        return vec2(glm::dot(XYCOMP(t), org.tradial), DEP(t));
    } else {
        return t;
    }
}

} // namespace bhc
