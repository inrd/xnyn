#pragma once

#include <algorithm>
#include <concepts>

namespace xnyn {

// Rational approximation of tanh(x), valid for any x; output in [-1, 1] (up to 1 ulp).
//
// Sources: https://www.musicdsp.org/en/latest/Other/238-rational-tanh-approximation.html
// and https://github.com/pichenettes/stmlib.
//
// y = x * (27 + x^2) / (27 + 9 x^2), with x clamped to [-3, 3].
// - Max absolute error vs std::tanh: 0.0236 (peak near |x| = 1.57).
// - Odd, exact at 0. Near |x| = 3, where the curve is flat, rounding can wobble by a few
//   ulps and overshoot +-1 by 1 ulp; clamping the output would fix that but costs ~20%
//   latency in feedback loops, so it is left to callers that need a hard bound. Reaches exactly +-1 at |x| = 3 with
//   zero slope (d/dx = 9 (x^2 - 9)^2 / (27 + 9 x^2)^2), so clamping the input instead of branching on it gives a smooth
//   (C1) knee and keeps the function branchless (min/max selects).
// - NaN propagates.
template <std::floating_point T>
[[nodiscard]] constexpr T fastTanh(T x) noexcept {
  const T xc = std::clamp(x, T(-3), T(3));
  const T x2 = xc * xc;
  return xc * (T(27) + x2) / (T(27) + T(9) * x2);
}

}  // namespace xnyn
