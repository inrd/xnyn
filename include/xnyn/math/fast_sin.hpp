#pragma once

#include <algorithm>
#include <concepts>
#include <cstdint>

namespace xnyn {

namespace detail {

// cos(2 pi z) for z in [-1/4, 1/4]: the polynomial kernel of fastSin, without its range
// reduction. Shared so callers whose argument is already in range (e.g. crossfade gains)
// skip the reduction and stay bit-identical to fastSin. See fastSin for the derivation.
// Max absolute error 5.98e-8 (polynomial), exact 1 at z = 0 and 0 at z = +-1/4.
template <std::floating_point T>
[[nodiscard]] constexpr T cosQuarterCycle(T z) noexcept {
  const T s = z * z;
  const T s2 = s * s;
  return (T(0.0625) - s) * ((T(16) + s * T(-59.82686786041409471231193)) +
                            s2 * (T(81.72843751433724633433317) + s * T(-56.24296323750762340648717)));
}

}  // namespace detail

// Polynomial approximation of sin(2 pi x), with the phase x in cycles (e.g. Phasor output).
//
// Accurate enough for wide-deviation FM, where the approximation error is scaled by the
// deviation and becomes spurious modulation (see docs/math/fast_sin.md).
// - Max absolute error vs sin(2 pi x): 6.0e-8 for double (polynomial bound 5.98e-8 plus
//   rounding), 2.1e-7 for float (2.04e-7 measured over every float in [0, 1): float
//   rounding of the reduction and the evaluation dominates the polynomial error).
// - Valid for any phase with |x| <= 2^61 cycles, not just [0, 1), so phase offsets (PM)
//   need no wrapping. Range reduction is exact there, so whole cycles cost no accuracy;
//   only the input's own resolution (ulp(x)) limits the phase. Larger phases give
//   meaningless but defined (no UB) output. Cost of the full range over a [0, 1)-only
//   version: a min/max clamp and a parity select.
// - Odd: fastSin(-x) == -fastSin(x), and the half-cycle symmetry
//   sin(2 pi (x + 1/2)) = -sin(2 pi x) holds bit-exactly, so the error has zero mean over a
//   cycle (no DC: in FM a DC error would detune the carrier).
// - Exact at the key points: 0 at 0 and 0.5 (-0 at 0.5), 1 at 0.25, -1 at 0.75.
//   Output stays within [-1, 1].
// - NaN propagates. +-inf gives a non-finite output (inf), never a valid sample.
//
// Reduction: n = trunc(2x), h = x - n/2 in (-1/2, 1/2), so sin(2 pi x) = (-1)^n sin(2 pi h),
// and sin(2 pi h) = sign(h) cos(2 pi (|h| - 1/4)) with |h| - 1/4 in [-1/4, 1/4]. Folding onto
// cos, an even function, gives one polynomial in z^2 over the full quarter-cycle range and
// makes both symmetries exact. All steps are exact for |x| <= 2^61 except z = |h| - 1/4,
// which rounds by at most half an ulp of 1/4 near the zero crossings.
//
// Polynomial: cos(2 pi z) ~= (1/16 - z^2) (16 + z^2 (r0 + z^2 (r1 + z^2 r2))), degree 8.
// The factored form pins the zero at z = +-1/4 and the value 1 at z = 0 whatever the
// coefficient rounding, which is what makes the key points exact. r0..r2 are the minimax
// (equioscillating) absolute-error fit over z in [0, 1/4] with that form, computed with a
// Remez exchange in 40-digit arithmetic (mpmath). Equioscillation level: 5.98045e-8.
// Evaluated in Estrin form: same accuracy as Horner and shorter latency, which is what
// counts in FM feedback loops (benchmarks/math/fast_sin_bench.cpp).
template <std::floating_point T>
[[nodiscard]] constexpr T fastSin(T x) noexcept {
  // Clamped before the integer conversion: converting NaN or an out-of-range value is UB.
  // The argument order maps NaN to -limit (std::max returns its first argument when the
  // comparison is false); h below still uses x, so NaN propagates to the output.
  // Integer conversion rather than std::floor/trunc: constexpr in C++20, inline on baseline
  // x86-64 (no SSE4.1), and it survives -ffast-math, unlike the add/subtract 2^52 trick.
  constexpr T kLimit = T(std::int64_t{1} << 62);
  const auto n = static_cast<std::int64_t>(std::min(kLimit, std::max(-kLimit, x + x)));
  const T h = x - static_cast<T>(n) * T(0.5);
  const T a = h < T(0) ? -h : h;
  const T c = detail::cosQuarterCycle(a - T(0.25));
  // Selects, not branches.
  const bool negate = (h < T(0)) != ((n & 1) != 0);
  return negate ? -c : c;
}

}  // namespace xnyn
