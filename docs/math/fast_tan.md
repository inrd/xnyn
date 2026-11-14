# fastTanPi

`#include <xnyn/math/fast_tan.hpp>`

Branchless rational `tan(π·x)` over the whole period `|x| < 1/2`, with a relative error bound
that holds right up to the poles. Its typical use is bilinear prewarping (`x = cutoffHz / sampleRate`),
cheap enough to recompute every sample under cutoff modulation.

## API

```cpp
template <std::floating_point T>
constexpr T fastTanPi(T x) noexcept;  // tan(pi x), |x| < 1/2 (below Nyquist)
```

The argument is `fc / fs`, not radians: code ported from `std::tan(w)` needs `w * (1 / π)`.
The name follows `tanpi` (C23 / IEEE 754).

## Usage

Prewarped integrator gain of a TPT state-variable filter, with a modulated cutoff:

```cpp
const float invSampleRate = 1.0f / static_cast<float>(sampleRate);

for (auto& s : buffer) {
  const float cutoffHz = std::min(baseHz * xnyn::fastExp2(lfo * octaves), 0.49f * sampleRate);
  const float g = xnyn::fastTanPi(cutoffHz * invSampleRate);  // tan(pi fc / fs)
  const float a = 1.0f / (1.0f + g * (g + 2.0f * damping));
  // ... SVF update with g and a
}
```

Clamp the cutoff below Nyquist: `g` grows without bound as `fc → fs / 2`.

## Accuracy

Max relative error against `tan(π·x)`, for `|x| < 1/2`:

| Type | Bound | Notes |
|---|---|---|
| `double` | **4.5e-8** | Polynomial minimax error 4.40e-8, plus rounding |
| `float` | **4.0e-7** | 3.91e-7 measured over every normal float in `[0, 1/2)`; float rounding dominates |

- The bound holds right up to Nyquist: the pole at `x = ±1/2` is a factor of the formula,
  not something the fit has to approximate. `x = ±1/2` gives `±inf`.
- As a filter cutoff, the realized frequency `fs / π · atan(g)` is off by at most the
  relative error of `g`: **8e-5 cents** in `double`, **7e-4 cents** in `float`, at any
  cutoff and sample rate.
- The relative bound covers normal outputs. Subnormal outputs lose precision with the
  format itself (and are flushed under FTZ).
- Odd (`fastTanPi(-x) == -fastTanPi(x)` exactly), `0` at `0`. Increasing up to rounding:
  in `float`, adjacent inputs can step back by a few ulps (less than twice the error bound).
- NaN propagates. Outside `(-1/2, 1/2)` the output is meaningless.

## When to use

- Bilinear-transform coefficients that change per sample: modulated SVF / ladder / one-pole
  TPT filters, audio-rate cutoff FM.
- For coefficients computed only on parameter changes, `std::tan` is just as good: its
  cost is amortized over the block.

## Trade-offs

- Not full double precision: `4.5e-8` relative. Use `std::tan` if you need correctly
  rounded results.
- No range reduction: only one period, `(-1/2, 1/2)`, is supported. That is the whole
  audio range for prewarping.

## Performance

One division and a degree-3 polynomial in `x²` (Estrin form), with no branches, no range
reduction and no lookup tables. Benchmarked against `std::tan(π·x)` for `float` and
`double`, over a block and in a per-sample cutoff-modulated SVF
(`benchmarks/math/fast_tan_bench.cpp`).
