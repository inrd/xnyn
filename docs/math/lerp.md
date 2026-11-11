# lerp

`#include <xnyn/math/lerp.hpp>`

Linear interpolation between `a` (at `t = 0`) and `b` (at `t = 1`), for crossfades,
interpolated table lookups and fractional delays.

## API

```cpp
template <std::floating_point T>
constexpr T lerp(T a, T b, T t) noexcept;  // a + t * (b - a)
```

Values of `t` outside `[0, 1]` extrapolate.

## Usage

```cpp
// Linear-interpolated wavetable lookup.
const float pos = phase * tableSize;
const auto i = static_cast<std::size_t>(pos);
const float y = xnyn::lerp(table[i], table[i + 1], pos - static_cast<float>(i));
```

## Trade-offs compared with `std::lerp`

`std::lerp` branches on the signs of its arguments to guarantee exactness, monotonicity
and boundedness. `xnyn::lerp` gives those guarantees up for speed:

- Exact at `t = 0`, and whenever `a == b` (for finite `t`).
- At `t = 1` it may miss `b` by rounding: `|lerp(a, b, 1) - b| <= 2ε · max(|a|, |b|)`.
- Not guaranteed to be monotonic in `t`.
- `b - a` can overflow for finite `a` and `b` of opposite sign near the type's max.

None of these matter for audio-range values. Use `std::lerp` when they do.

## Performance

One subtraction and one multiply-add (which can be fused into an FMA), with no branches, so it inlines and
vectorizes. Benchmarked against `std::lerp` (`benchmarks/math/lerp_bench.cpp`).
