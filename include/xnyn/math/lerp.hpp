#pragma once

#include <concepts>

namespace xnyn {

// Linear interpolation between a (t = 0) and b (t = 1); t outside [0, 1] extrapolates.
//
// Unlike std::lerp, this is the plain a + t * (b - a): one sub and one fma-able mul-add,
// no branches, so it inlines and vectorizes. std::lerp branches on the signs of a, b and t
// to guarantee exactness at t = 1, monotonicity and boundedness, which costs throughput in
// per-sample loops and blocks vectorization.
//
// Trade-offs, accepted for speed:
// - Exact at t = 0 and whenever a == b (finite t). At t = 1 it may miss b by rounding:
//   |lerp(a, b, 1) - b| <= 2 * epsilon * max(|a|, |b|) (b - a and the add round once each).
// - Not guaranteed monotonic in t.
// - b - a may overflow for finite a, b of opposite sign near the type's max.
template <std::floating_point T>
[[nodiscard]] constexpr T lerp(T a, T b, T t) noexcept {
  return a + t * (b - a);
}

}  // namespace xnyn
