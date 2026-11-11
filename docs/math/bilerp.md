# bilerp

`#include <xnyn/math/bilerp.hpp>`

Bilinear interpolation between four corners, for 2D table lookups (wavetable morphing
between frames, XY pads, 2D waveshaper or filter-coefficient tables).

## API

```cpp
template <std::floating_point T>
constexpr T bilerp(T tl, T tr, T bl, T br, T x, T y) noexcept;
// tl / tr: top corners (y = 0), bl / br: bottom corners (y = 1); left at x = 0, right at x = 1
```

`x` and `y` are in `[0, 1]`; values outside extrapolate. Computed as
`lerp(lerp(tl, tr, x), lerp(bl, br, x), y)`.

## Usage

```cpp
// Wavetable with frame morphing: x = position within a frame, y = between two frames.
// Frames hold frameSize + 1 samples (wrap-around guard) and morph is in [0, 1).
const float pos = phase * frameSize;
const auto i = static_cast<std::size_t>(pos);
const float x = pos - static_cast<float>(i);
const float m = morph * static_cast<float>(numFrames - 1);
const auto frame = static_cast<std::size_t>(m);
const float y = m - static_cast<float>(frame);
const float s = xnyn::bilerp(table[frame][i], table[frame][i + 1],
                             table[frame + 1][i], table[frame + 1][i + 1], x, y);
```

## Accuracy and trade-offs

Same trade-offs as [`lerp`](lerp.md), accepted for speed:

- Exact at `(x, y) = (0, 0)`, and for a constant field (all corners equal, finite `x`, `y`),
  so flat table regions stay noise-free. The other corners and edges may miss by rounding.
- `|error| <= 5ε · max(|tl|, |tr|, |bl|, |br|)` for `x`, `y` in `[0, 1]`.
- Not guaranteed to be monotonic; corner differences can overflow near the type's max.

## Performance

Three `lerp`s: 3 subtractions and 3 multiply-adds (fusable into FMAs), no branches, so it
inlines and vectorizes. Fewer operations than the weighted-sum form
`tl(1-x)(1-y) + tr·x(1-y) + bl(1-x)y + br·xy` (8 multiplies), though over a vectorized
block both are close to memory-bound, so the gain is small; the main reason to prefer it
is exactness on constant fields. Benchmarked against the weighted-sum form
(`benchmarks/math/bilerp_bench.cpp`).
