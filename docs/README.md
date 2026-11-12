# xnyn documentation

User-facing reference for every block: what it does, how to use it, and the
trade-offs it makes for speed. Implementation details live in the header comments.

For integration and build instructions see the [main README](../README.md); for the
rules every block follows see [CONVENTIONS.md](../CONVENTIONS.md).

## General notes

- Every block is header-only and lives in namespace `xnyn`. Include a single header,
  or `<xnyn/xnyn.hpp>` for everything.
- Everything that processes audio is `noexcept`, never allocates and never locks, so it is
  safe to call on the audio thread.
- Classes follow the same lifecycle: `prepare(sampleRate)` → setters → `tick()` per
  sample, with `reset()` to return to a known state. A default-constructed block
  produces neutral, finite output before `prepare` is called.
- Parameters are in physical units (Hz, seconds, cycles) and apply immediately:
  smoothing is up to the caller.
- Denormals are the caller's responsibility: enable FTZ/DAZ on the audio thread.

## Blocks

### filter

| Block | Header | Summary |
|---|---|---|
| [`DcBlocker`](filter/dc_blocker.md) | `xnyn/filter/dc_blocker.hpp` | One-pole DC blocker, cutoff in Hz, no overshoot |

### math

| Block | Header | Summary |
|---|---|---|
| [`bilerp`](math/bilerp.md) | `xnyn/math/bilerp.hpp` | Branchless bilinear interpolation between four corners |
| [`dbToGain` / `gainToDb`](math/decibels.md) | `xnyn/math/decibels.hpp` | Branchless dB ↔ gain conversions, optional dB floor |
| [`equalPowerCrossfade`](math/equal_power_crossfade.md) | `xnyn/math/equal_power_crossfade.hpp` | Sine / cosine crossfade with constant power, gains via `fastSin` |
| [`fastExp2` / `fastLog2` / `fastPow`](math/fast_pow.md) | `xnyn/math/fast_pow.hpp` | Branchless polynomial `2^x`, `log2` and `pow`, max rel error 2e-9 / 2.7e-8 (double) |
| [`fastSin`](math/fast_sin.md) | `xnyn/math/fast_sin.hpp` | Branchless polynomial `sin(2π·phase)`, phase in cycles, max abs error 6e-8 (double) |
| [`fastTanh`](math/fast_tanh.md) | `xnyn/math/fast_tanh.hpp` | Branchless rational tanh approximation, max abs error 0.0236 |
| [`lerp`](math/lerp.md) | `xnyn/math/lerp.hpp` | Branchless `a + t * (b - a)`, faster than `std::lerp` |

### osc

| Block | Header | Summary |
|---|---|---|
| [`Phasor`](osc/phasor.md) | `xnyn/osc/phasor.hpp` | Normalized phase accumulator in `[0, 1)`, drift-free, through-zero capable |

### util

| Block | Header | Summary |
|---|---|---|
| [`isFinite` / `sanitize`](util/sanitize.md) | `xnyn/util/sanitize.hpp` | NaN / inf guards that survive `-ffast-math` |
| [`Smoother`](util/smoother.md) | `xnyn/util/smoother.hpp` | One-pole parameter smoother, time constant in ms, no overshoot |
