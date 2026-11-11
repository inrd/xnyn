#pragma once

#include <algorithm>
#include <concepts>

#include "xnyn/math/fast_sin.hpp"

namespace xnyn {

// Equal-power (constant-power) crossfade from a (t = 0) to b (t = 1):
// a * cos(t pi / 2) + b * cos((1 - t) pi / 2), so gainA^2 + gainB^2 = 1 for t in [0, 1].
// Use it for uncorrelated signals (no -3 dB dip in the middle); for correlated or identical
// signals use lerp (equal gain), which keeps the amplitude constant instead.
//
// - t is clamped to [0, 1], so gains never go negative or exceed 1. NaN t gives NaN output
//   (the clamp keeps NaN: std::min / std::max return their first argument when the
//   comparison is false).
// - Gains use fastSin's cos polynomial: max absolute error vs the exact gains 6.0e-8 for
//   double, 2.1e-7 for float. Power error |gainA^2 + gainB^2 - 1| <= 2 sqrt(2) * that
//   (gainA + gainB <= sqrt 2): 1.7e-7 for double, 6.0e-7 for float.
// - Exact at the endpoints: t = 0 returns a, t = 1 returns b (gains exactly 1 and 0).
// - Symmetric: gainA(t) == gainB(1 - t) whenever 1 - t is exact (e.g. both gains are
//   equal at t = 0.5).
//
// Both gain arguments, t / 4 and (1 - t) / 4, are already within the polynomial's
// quarter-cycle range, so calling the kernel directly skips fastSin's range reduction
// (integer conversion, fold, sign select): about 4x faster than two fastSin calls, same error bounds. Both gains go
// through the same expression, which is what makes the endpoints and the symmetry exact.
template <std::floating_point T>
[[nodiscard]] constexpr T equalPowerCrossfade(T a, T b, T t) noexcept {
  const T tc = std::min(std::max(t, T(0)), T(1));
  const T gainA = detail::cosQuarterCycle(T(0.25) * tc);
  const T gainB = detail::cosQuarterCycle(T(0.25) * (T(1) - tc));
  return gainA * a + gainB * b;
}

}  // namespace xnyn
