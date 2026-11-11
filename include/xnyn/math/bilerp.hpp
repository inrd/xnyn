#pragma once

#include <concepts>

#include "xnyn/math/lerp.hpp"

namespace xnyn {

// Bilinear interpolation between four corners: tl / tr (top, y = 0), bl / br (bottom, y = 1),
// left at x = 0, right at x = 1. x and y in [0, 1]; outside, it extrapolates.
//
// Two lerps along x, then one along y: 3 subs and 3 fma-able mul-adds, no branches. Fewer
// operations than the weighted-sum form tl (1-x)(1-y) + tr x(1-y) + bl (1-x) y + br x y
// (8 muls), though both are close to memory-bound over a block; more importantly, unlike
// it, exact for a constant field (all corners equal), so it adds no noise to flat
// regions of a table.
//
// Trade-offs (inherited from lerp, accepted for speed):
// - Exact at (x, y) = (0, 0) and whenever all corners are equal (finite x, y). The other
//   corners and edges may miss by rounding.
// - |error| <= 5 * epsilon * max(|tl|, |tr|, |bl|, |br|) for x, y in [0, 1]: each lerp
//   rounds its sub, mul and add once (<= 2.5 epsilon * max), and the outer one carries the
//   inner errors with weights 1 - y and y, which sum to 1.
// - Not guaranteed monotonic; differences of corners may overflow near the type's max.
template <std::floating_point T>
[[nodiscard]] constexpr T bilerp(T tl, T tr, T bl, T br, T x, T y) noexcept {
  return lerp(lerp(tl, tr, x), lerp(bl, br, x), y);
}

}  // namespace xnyn
