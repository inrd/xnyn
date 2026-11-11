# equalPowerCrossfade

`#include <xnyn/math/equal_power_crossfade.hpp>`

Equal-power (constant-power) crossfade from `a` (at `t = 0`) to `b` (at `t = 1`), with
sine / cosine gains, for crossfading uncorrelated signals without a level dip.

## API

```cpp
template <std::floating_point T>
constexpr T equalPowerCrossfade(T a, T b, T t) noexcept;  // a cos(t π/2) + b sin(t π/2), t clamped to [0, 1]
```

## Usage

```cpp
// Dry / wet mix of a reverb (uncorrelated with the dry signal).
for (std::size_t i = 0; i < n; ++i) out[i] = xnyn::equalPowerCrossfade(dry[i], wet[i], mix);
```

## Equal power vs equal gain

- Uncorrelated signals (different sources, reverb, noise): powers add, so
  `gainA² + gainB² = 1` keeps the perceived level constant. Use `equalPowerCrossfade`.
  `lerp` would dip by 3 dB at the midpoint.
- Correlated or identical signals (two takes of the same material, a filter's input and
  output at low settings): amplitudes add, so `equalPowerCrossfade` bumps the level by up
  to +3 dB at the midpoint. Use [`lerp`](lerp.md) (equal gain) there.

## Accuracy

- Gains use [`fastSin`](fast_sin.md)'s polynomial: max absolute error against the exact
  `cos` / `sin` gains **6.0e-8** for `double`, **2.1e-7** for `float`. Power error
  `|gainA² + gainB² - 1|` <= **1.7e-7** (`double`), **6.0e-7** (`float`).
- Exact at the endpoints: `t = 0` returns `a`, `t = 1` returns `b`.
- Symmetric: the gain of `a` at `t` equals the gain of `b` at `1 - t` (bit-exact when
  `1 - t` is exact), so both gains are equal at `t = 0.5`.
- `t` is clamped to `[0, 1]`: below 0 returns `a`, above 1 returns `b`. Gains never go
  negative or above 1, so modulating `t` past its range is safe.
- NaN in any argument propagates.

## Performance

A clamp, two degree-8 polynomials and a multiply-add, with no branches and no `std::cos`
calls, so it inlines and vectorizes. Both gain arguments are already within a quarter
cycle, so it calls `fastSin`'s polynomial directly and skips its range reduction, with
the same error bounds. When `t` is constant over a block and many channels share it, compute
the gains once with `fastSin(0.25 * (1 - t))` and `fastSin(0.25 * t)` instead. Benchmarked
against the general `fastSin` path and the naive `std::cos` in `double` version
(`benchmarks/math/equal_power_crossfade_bench.cpp`).
