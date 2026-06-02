
#pragma once
#include "common_run.hpp"

namespace bhc {

HOST_DEVICE inline void cSpline(
    const real *tau, cpx *c1, cpx *c2, cpx *c3, cpx *c4, int32_t n, int32_t ibcbeg,
    int32_t ibcend, [[maybe_unused]] int32_t ndim)
{
    cpx g, dtau, divdf1, divdf3;

    int32_t l = n - 1;

    for(int32_t m = 1; m < n; ++m) {
        c3[m] = cpx(tau[m] - tau[m - 1], FL(0.0));
        c4[m] = (c1[m] - c1[m - 1]) / c3[m];
    }

    //  * BEGINNING BOUNDARY CONDITION SECTION *

    if(ibcbeg == 0) { // IBCBEG = 0
        if(n > 2) {   // N > 2
            c4[0] = c3[2];
            c3[0] = c3[1] + c3[2];
            c2[0] = ((c3[1] + FL(2.0) * c3[0]) * c4[1] * c3[2] + SQ(c3[1]) * c4[2])
                / c3[0];
        } else { // N = 2
            c4[0] = cpx(FL(1.0), FL(0.0));
            c3[0] = cpx(FL(1.0), FL(0.0));
            c2[0] = FL(2.0) * c4[1];
        }
    } else if(ibcbeg == 1) { // IBCBEG = 1
        c4[0] = cpx(FL(1.0), FL(0.0));
        c3[0] = cpx(FL(0.0), FL(0.0));
    } else if(ibcbeg == 2) { // IBCBEG = 2
        c4[0] = cpx(FL(2.0), FL(0.0));
        c3[0] = cpx(FL(1.0), FL(0.0));
        c2[0] = FL(3.0) * c4[1] - c2[0] * c3[1] * FL(0.5);
    }

    // * RUNNING CALCULATIONS TO N-1 - LOOP IS NOT EXECUTED IF N = 2 *

    for(int32_t m = 1; m < l; ++m) {
        g     = -c3[m + 1] / c4[m - 1];
        c2[m] = g * c2[m - 1] + FL(3.0) * (c3[m] * c4[m + 1] + c3[m + 1] * c4[m]);
        c4[m] = g * c3[m - 1] + FL(2.0) * (c3[m] + c3[m + 1]);
    }

    // * ENDING BOUNDARY CONDITION SECTION *

    if(ibcend != 1) {
        if(ibcend == 0) {
            if(n == 2 && ibcbeg == 0) {
                c2[n - 1] = c4[n - 1];
            } else if((n == 3 && ibcbeg == 0) || n == 2) {
                c2[n - 1] = FL(2.0) * c4[n - 1];
                c4[n - 1] = cpx(FL(1.0), FL(0.0));
                g         = FL(1.0) / -c4[n - 2];
            } else {
                g         = c3[n - 2] + c3[n - 1];
                c2[n - 1] = ((c3[n - 1] + FL(2.0) * g) * c4[n - 1] * c3[n - 2]
                             + SQ(c3[n - 1]) * (c1[n - 2] - c1[n - 3]) / c3[n - 2])
                    / g;
                g         = -g / c4[n - 2];
                c4[n - 1] = c3[n - 2];
            }
        } else if(ibcend == 2) {
            c2[n - 1] = FL(3.0) * c4[n - 1] + c2[n - 1] * c3[n - 1] * FL(0.5);
            c4[n - 1] = cpx(FL(2.0), FL(0.0));
            g         = FL(1.0) / -c4[n - 2];
        }

        if(ibcbeg > 0 || n > 2) {
            c4[n - 1] += g * c3[n - 2];
            c2[n - 1] = (g * c2[n - 2] + c2[n - 1]) / c4[n - 1];
        }
    }

    // * RUN THE ENDING BOUNDARY EFFECT BACK THROUGH THE EQUATIONS *

    for(int32_t j = l - 1; j >= 0; --j) { c2[j] = (c2[j] - c3[j] * c2[j + 1]) / c4[j]; }

    // * FINAL CALCULATIONS *

    for(int32_t i = 1; i < n; ++i) {
        dtau      = c3[i];
        divdf1    = (c1[i] - c1[i - 1]) / dtau;
        divdf3    = c2[i - 1] + c2[i] - FL(2.0) * divdf1;
        c3[i - 1] = FL(2.0) * (divdf1 - c2[i - 1] - divdf3) / dtau;
        c4[i - 1] = (divdf3 / dtau) * (FL(6.0) / dtau);
    }

    // * ADD THE CURVATURE AT THE LAST POINT IN THE THIRD POSITION OF THE
    //   LAST NODE *

    c3[n - 1] = c3[l - 1] + (tau[n - 1] - tau[l - 1]) * c4[l - 1];

    // * ADD THE MEAN VALUE OF THE ENTIRE INTERVAL IN THE FOURTH POSITION OF
    //   THE LAST NODE.  MEAN VALUE IS CALCULATED AS THE INTEGRAL OVER THE
    //   INTERVAL DIVIDED BY THE LENGTH OF THE INTERVAL. *

    c4[n - 1] = cpx(FL(0.0), FL(0.0));
    for(int32_t i = 0; i < l; ++i) { // INTEGRATE OVER THE INTERVAL
        dtau = tau[i + 1] - tau[i];
        c4[n - 1] += dtau
            * (c1[i]
               + dtau
                   * (c2[i] * FL(0.5)
                      + dtau * (c3[i] / FL(6.0) + dtau * c4[i] / FL(24.0))));
    }
    c4[n - 1] /= tau[n - 1] - tau[0]; // DIVIDE BY LENGTH OF INTERVAL
}

/**
 * THIS ROUTINE EVALUATES THE
 *        SPLINE,
 *        SPLINE DERIVATIVE, AND
 *        SPLINE 2ND DERIVATIVE AT THE POINT H
 */
HOST_DEVICE inline void SplineALL(
    const cpx &c1, const cpx &c2, const cpx &c3, const cpx &c4, real h, cpx &f, cpx &fx,
    cpx &fxx)
{
    constexpr float half = FL(0.5), sixth = FL(1.0) / FL(6.0);

    f   = c1 + h * (c2 + h * (half * c3 + sixth * h * c4));
    fx  = c2 + h * (c3 + h * half * c4);
    fxx = c3 + h * c4;
}


HOST_DEVICE inline void h_del(
    const real *x, const cpx *y, int32_t ix, real &h1, real &h2, cpx &del1, cpx &del2)
{
    h1 = x[ix] - x[ix - 1];
    h2 = x[ix + 1] - x[ix];

    del1 = (y[ix] - y[ix - 1]) / h1;
    del2 = (y[ix + 1] - y[ix]) / h2;
}

/**
 * check if derivative is within the trust region, project into it if not
 */
HOST_DEVICE inline real fprime_interior(real del1, real del2, real fprime)
{
    if(del1 * del2 > FL(0.0)) {
        // adjacent secant slopes have the same sign, enforce monotonicity
        if(del1 > FL(0.0)) {
            return bhc::min(bhc::max(fprime, RL(0.0)), RL(3.0) * bhc::min(del1, del2));
        } else {
            return bhc::max(bhc::min(fprime, RL(0.0)), RL(3.0) * bhc::max(del1, del2));
        }
    } else {
        // force the interpolant to have an extrema here
        return RL(0.0);
    }
}

HOST_DEVICE inline real fprime_left_end(real del1, real del2, real fprime)
{
    if(del1 * fprime <= RL(0.0)) {
        // set derivative to zero if the sign differs from sign of secant slope
        return FL(0.0);
    } else if((del1 * del2 <= RL(0.0)) && (STD::abs(fprime) > STD::abs(RL(3.0) * del1))) {
        // adjust derivative value to enforce monotonicity
        return RL(3.0) * del1;
    } else {
        return fprime;
    }
}

HOST_DEVICE inline real fprime_right_end(real del1, real del2, real fprime)
{
    return fprime_left_end(del2, del1, fprime);
}

HOST_DEVICE inline cpx fprime_interior_Cmplx(
    const cpx &del1, const cpx &del2, const cpx &fprime)
{
    return cpx(
        fprime_interior(del1.real(), del2.real(), fprime.real()),
        fprime_interior(del1.imag(), del2.imag(), fprime.imag()));
}

HOST_DEVICE inline cpx fprime_left_end_Cmplx(
    const cpx &del1, const cpx &del2, const cpx &fprime)
{
    return cpx(
        fprime_left_end(del1.real(), del2.real(), fprime.real()),
        fprime_left_end(del1.imag(), del2.imag(), fprime.imag()));
}

HOST_DEVICE inline cpx fprime_right_end_Cmplx(
    const cpx &del1, const cpx &del2, const cpx &fprime)
{
    return cpx(
        fprime_right_end(del1.real(), del2.real(), fprime.real()),
        fprime_right_end(del1.imag(), del2.imag(), fprime.imag()));
}


HOST_DEVICE inline void pchip(
    const real *x, const cpx *y, int32_t n, cpx *PolyCoef1, cpx *PolyCoef2,
    cpx *PolyCoef3, cpx *PolyCoef4, cpx *csWork1, cpx *csWork2, cpx *csWork3,
    cpx *csWork4)
{
    int32_t ix, iBCBeg, iBCEnd, i;
    real h1, h2;
    cpx del1, del2, f1, f2, f1prime, f2prime, fprimeT;

    // Precompute estimates of the derivatives at the nodes
    // The vector PolyCoef1[] holds the ordinate values at the nodes
    // The vector PolyCoef2[] holds the ordinate derivatives at the nodes

    if(n == 2) {
        // handle special case of two data points seperately (linear interpolation)

        PolyCoef1[0] = y[0];
        PolyCoef2[0] = (y[1] - y[0]) / (x[1] - x[0]);
        PolyCoef3[0] = RL(0.0);
        PolyCoef4[0] = RL(0.0);

    } else {
        // general case of more than two data points

        for(i = 0; i < n; ++i) PolyCoef1[i] = y[i];

        // left endpoint (non-centered 3-point difference formula)

        h_del(x, y, 1, h1, h2, del1, del2);
        fprimeT      = ((RL(2.0) * h1 + h2) * del1 - h1 * del2) / (h1 + h2);
        PolyCoef2[0] = fprime_left_end_Cmplx(del1, del2, fprimeT);

        // right endpoint (non-centered 3-point difference formula)

        h_del(x, y, n - 2, h1, h2, del1, del2);
        fprimeT          = (-h2 * del1 + (h1 + RL(2.0) * h2) * del2) / (h1 + h2);
        PolyCoef2[n - 1] = fprime_right_end_Cmplx(del1, del2, fprimeT);

        // compute coefficients of the cubic spline interpolating polynomial

        iBCBeg = 1; // specified derivatives at the end points
        iBCEnd = 1;
        for(i = 0; i < n; ++i) csWork1[i] = PolyCoef1[i];
        csWork2[0]     = PolyCoef2[0];
        csWork2[n - 1] = PolyCoef2[n - 1];
        cSpline(x, csWork1, csWork2, csWork3, csWork4, n, iBCBeg, iBCEnd, n);

        // interior nodes (use derivatives from the cubic spline as initial estimate)

        for(ix = 1; ix < n - 1; ++ix) {
            h_del(x, y, ix, h1, h2, del1, del2);
            // check if the derivative from the cubic spline satisfies monotonicity
            PolyCoef2[ix] = fprime_interior_Cmplx(del1, del2, csWork2[ix]);
        }

        //                                                               2      3
        // compute coefficients of std cubic polynomial: c0 + c1*x + c2*x + c3*x
        //

        for(ix = 0; ix < n - 1; ++ix) {
            real h = x[ix + 1] - x[ix];

            f1 = PolyCoef1[ix];
            f2 = PolyCoef1[ix + 1];

            f1prime = PolyCoef2[ix];
            f2prime = PolyCoef2[ix + 1];

            PolyCoef3[ix] = (RL(3.0) * (f2 - f1) - h * (RL(2.0) * f1prime + f2prime))
                / SQ(h);
            PolyCoef4[ix] = (h * (f1prime + f2prime) - RL(2.0) * (f2 - f1)) / CUBE(h);
        }
    }
}

} // namespace bhc
