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

### math

| Block | Header | Summary |
|---|---|---|
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
