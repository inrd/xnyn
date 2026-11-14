#pragma once

#include <concepts>

namespace xnyn {

// Rational approximation of tan(pi x) over one period, |x| < 1/2.
//
// Typical use: bilinear (TPT / SVF) prewarping, g = tan(pi fc / fs) with x a normalized
// frequency, cheap enough to recompute every sample under cutoff modulation
// (see docs/math/fast_tan.md).
// - Valid for |x| < 1/2 (below Nyquist). Max relative error vs tan(pi x): 4.5e-8 for double
//   (polynomial bound 4.40e-8 plus rounding), 4.0e-7 for float (3.91e-7 measured over every
//   float in [0, 1/2), near the pole: float rounding dominates). The relative cutoff error is
//   at most the relative error of g (d fc / fc = (dg / g) sin(2 pi x) / (2 pi x)): 8e-5 cents
//   in double, 7e-4 cents in float.
// - The bound holds right up to the pole: the pole is a factor of the formula, not fitted.
//   x = +-1/2 gives +-inf. Outside (-1/2, 1/2) the output is meaningless (finite except at
//   x = +-3/2).
// - The relative bound covers normal outputs: subnormal ones lose precision with the format
//   (and are flushed under FTZ).
// - Odd (fastTanPi(-x) == -fastTanPi(x) exactly), exact 0 at 0. Increasing over (-1/2, 1/2)
//   up to rounding: in float, adjacent inputs can step back by a few ulps (less than twice
//   the error bound), far below anything audible in a cutoff sweep.
// - NaN propagates.
//
// Form: tan(pi x) ~= x P(x^2) / ((1/4 - x^2)(9/4 - x^2)). Dividing out the poles at +-1/2 and
// +-3/2 leaves a smooth, even function whose nearest singularity (the pole at +-5/2) is far
// from [0, 1/4] in x^2, so a degree-3 polynomial P in x^2 reaches 4.4e-8. It costs one
// division and no branches or range reduction. 1/4 - x^2 is computed as (1/2 - x)(1/2 + x):
// 1/2 - x is exact for x in [1/4, 1/2] (Sterbenz), so there is no cancellation near the
// pole, where a relative error in 1/4 - x^2 would be one in the output.
//
// P: minimax (equioscillating) relative-error fit of tan(pi x) over x in [0, 1/2] with that
// form, computed with a Remez exchange in 50-digit arithmetic (mpmath). Equioscillation level
// 4.40087e-8. Evaluated in Estrin form for a shorter dependency chain.
template <std::floating_point T>
[[nodiscard]] constexpr T fastTanPi(T x) noexcept {
  const T s = x * x;
  const T s2 = s * s;
  const T p = (T(1.767145789874520345931021) + s * T(-2.040295548773288044025973)) +
              s2 * (T(0.2543304003204935884048126) + s * T(0.01740390402273619678225962));
  return x * p / (((T(0.5) - x) * (T(0.5) + x)) * (T(2.25) - s));
}

}  // namespace xnyn
