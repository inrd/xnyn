# dbToGain / gainToDb

`#include <xnyn/math/decibels.hpp>`

Branchless decibel ↔ linear amplitude conversions, cheap enough for every sample.

## API

```cpp
// float or double
template <typename T> constexpr T dbToGain(T db) noexcept;   // 10^(dB / 20)
template <typename T> constexpr T gainToDb(T gain,           // 20 log10(|gain|)
                                           T floorDb = -inf) noexcept;
```

## Usage

```cpp
// Modulated output level
for (auto& s : buffer) s *= xnyn::dbToGain(levelDb + lfoDepthDb * lfo.tick());

// Peak meter, -100 dB floor so silence stays finite
const float peakDb = xnyn::gainToDb(peak, -100.0f);
```

## Accuracy

| Function | `double` | `float` | Notes |
|---|---|---|---|
| `dbToGain` | **2.0e-9** relative | **1.0e-7 + 8e-9·\|dB\|** relative | Float: the exponent's own rounding, 9e-6 dB at -120 dB |
| `gainToDb` | **2.8e-8** relative to the dB value | **3.0e-7** relative to the dB value | Accurate near 0 dB |

- `dbToGain(0) == 1` exactly. Negative dB (attenuation) is the normal case. `-inf` dB gives
  `0`, `+inf` gives `+inf`, NaN propagates. Below about -762 dB (float) / -6156 dB (double)
  the gain flushes to 0; above about +768 / +6162 dB it is `+inf`.
- `gainToDb` uses the magnitude, so negative gains and bipolar samples are valid:
  `gainToDb(-0.5) ≈ -6.02`. `gainToDb(±1) == 0` exactly.
- Silence is `-inf` dB by default. Pass `floorDb` (e.g. `-100`) to clamp the result from
  below so it stays finite for smoothing or display. `+inf` gives `+inf`; NaN propagates
  through the floor.

## Trade-offs

- Built on [`fastExp2` / `fastLog2`](fast_pow.md): not correctly rounded, but far below
  audibility. Use `std::pow` / `std::log10` when you need full precision.
- `dbToGain` multiplies by `log2(10) / 20` and calls `fastExp2`. That is cheaper and more
  accurate than `fastPow(10, dB / 20)`, which would recompute `log2(10)` every call.

## Performance

One multiply plus `fastExp2` (or `fastLog2`, a multiply and a `max`), no branches, so it
vectorizes over buffers. Benchmarked against `std::pow(10, dB / 20)` and
`20 * std::log10(...)` over a block (`benchmarks/math/decibels_bench.cpp`).
