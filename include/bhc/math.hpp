
#pragma once

#ifndef _BHC_INCLUDED_
//#error "This file must be included via #include <bhc/bhc.hpp>!"
#endif

#include <cfloat>
#include <complex>

#define GLM_FORCE_EXPLICIT_CTOR 1
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat2x2.hpp>

namespace bhc {

#ifdef BHC_USE_FLOATS
using real = float;
#else
using real = double;
#endif

using vec2   = glm::vec<2, real, glm::defaultp>;
using vec3   = glm::vec<3, real, glm::defaultp>;
using int2   = glm::vec<2, int32_t, glm::defaultp>;
using mat2x2 = glm::mat<2, 2, real, glm::defaultp>;

using cpx  = STD::complex<real>;
using cpxf = STD::complex<float>;

} // namespace bhc
