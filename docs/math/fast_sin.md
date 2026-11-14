# fastSin

`#include <xnyn/math/fast_sin.hpp>`

Branchless polynomial `sin(2π·phase)`, with the phase in cycles. Accurate enough for
wide-deviation FM.

## API

```cpp
template <std::floating_point T>
constexpr T fastSin(T phase) noexcept;  // sin(2 pi phase), phase in cycles, |phase| <= 2^61
```

## Usage

```cpp
xnyn::Phasor modulator, carrier;
// prepare(sampleRate) both...

for (auto& s : buffer) {
  const double m = xnyn::fastSin(modulator.tick());
  carrier.setFrequency(carrierHz + m * deviationHz);
  s = static_cast<float>(xnyn::fastSin(carrier.tick()));
}
```

Phase offsets work without wrapping, e.g. phase modulation:
`xnyn::fastSin(phasor.tick() + index * m)`.

## Accuracy

Max absolute error against `sin(2π·phase)`:

| Type | Bound | Notes |
|---|---|---|
| `double` | **6.0e-8** | Polynomial minimax error 5.98e-8, plus rounding |
| `float` | **2.1e-7** | 2.04e-7 measured over every float in `[0, 1)`; float rounding dominates |

- Valid for any phase with `|phase| <= 2^61` cycles, not just `[0, 1)`. Whole cycles are
  removed exactly, so the bound holds over the full range; only the input's own resolution
  limits how precisely a large phase is known. Beyond `2^61` the output is meaningless
  (but defined).
- Odd, and `fastSin(x + 0.5) == -fastSin(x)` exactly. The error therefore has zero mean
  over a cycle: no DC term.
- Exact at the key points: `0` at 0 and 0.5, `1` at 0.25, `-1` at 0.75. Output stays within
  `[-1, 1]` (no overshoot).
- NaN propagates. `±inf` gives a non-finite output.

### Why this accuracy: FM with large deviation

The approximation error `e(phase)` is periodic, with harmonics of the modulator frequency.
In FM it is multiplied by the deviation `D`: an error harmonic `k` with amplitude `e`
adds a spurious modulation index of about `e·D / (k·fm)`. At `D = 300 kHz`, `fm = 50 Hz`,
an error of `1e-4` would give an index of ~0.6 (audible new sidebands), so this
approximation targets ~`1e-7`: the spurious index stays below ~`4e-4` at those extremes.

Any DC in the error would shift the carrier frequency (detune). The half-cycle symmetry
makes the error zero-mean, so there is no pitch offset, and no offset is fed back when
the output drives the modulator frequency.

The tests run a 2-op FM pair at `D = 300 kHz`, `fm = 50 Hz`, `fs = 48 kHz` against
`std::sin`: outputs differ by at most `5e-4`, without growing over 10 s, and with
modulator self-feedback the mean modulator frequency matches within `1e-6` relative.

## When to use

- Any per-sample sine or cosine: oscillators, LFOs, FM / PM, sine waveshaping, windows.
  For cos, use `fastSin(phase + 0.25)`; for equal-power panning (`pan` in `[0, 1]`),
  `fastSin(0.25 * pan)` and `fastSin(0.25 * (1 - pan))`.
- The input is in cycles: code ported from `std::sin(x)` in radians needs `x * (1 / 2π)`.
- The bound is absolute, not relative: near zero the relative error grows. For coefficients
  computed only on parameter changes (e.g. very low filter cutoffs in `float`), prefer
  `std::sin`, which is cheap there and fully precise.
- Sine only: for `tan` (bilinear prewarping) use [`fastTanPi`](fast_tan.md).

## Trade-offs

- Double accuracy is ~`6e-8`, not full double precision. If you need correctly rounded
  results, use `std::sin`.
- Phases beyond `2^61` cycles aren't supported. Keep accumulated phases wrapped
  (`Phasor` does).

## Performance

No branches: the quadrant fold uses an integer conversion, `min`/`max` and selects, and
the degree-8 polynomial is evaluated in Estrin form for a shorter dependency chain in
feedback loops. Benchmarked against `std::sin(2π·x)` for `float` and `double`, over a block
and in a `Phasor`-driven 2-op FM loop with feedback (`benchmarks/math/fast_sin_bench.cpp`).
