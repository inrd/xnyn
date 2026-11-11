# fastExp2 / fastLog2 / fastPow

`#include <xnyn/math/fast_pow.hpp>`

Branchless polynomial `2^x`, `log2(x)` and `pow(x, y)`, for gain, pitch and curve
conversions in per-sample code.

## API

```cpp
// float or double
template <typename T> constexpr T fastExp2(T x) noexcept;         // 2^x
template <typename T> constexpr T fastLog2(T x) noexcept;         // log2(x), x > 0
template <typename T> constexpr T fastPow(T x, T y) noexcept;     // x^y, x >= 0
```

## Usage

```cpp
// dB to gain, per sample (modulated level)
const float gain = xnyn::fastPow(10.0f, dB * 0.05f);   // or fastExp2(dB * (log2(10) / 20))

// Pitch: semitones to frequency ratio
const double hz = baseHz * xnyn::fastExp2(semitones * (1.0 / 12.0));

// Curve shaping, e.g. an envelope segment with exponent k
const float shaped = xnyn::fastPow(linear, k);

// exp(x)
const double e = xnyn::fastExp2(x * std::numbers::log2e);
```

Prefer `fastExp2` with a precomputed `log2` of a constant base over `fastPow` with that
base: it skips the log and is more accurate.

## Accuracy

Max relative error:

| Function | `double` | `float` | Notes |
|---|---|---|---|
| `fastExp2` | **2.0e-9** | **1.0e-7** | Polynomial bound 1.97e-9; float rounding dominates (9.8e-8 measured over every float). For normal results. |
| `fastLog2` | **2.7e-8** | **2.4e-7** | Relative to `log2(x)`. Polynomial bound 2.61e-8; 2.30e-7 measured over every positive float. |
| `fastPow` | **2.0e-9 + 1.9e-8·\|y·log2 x\|** | **1.0e-7 + 1.7e-7·\|y·log2 x\|** | The log error is scaled by the exponent. |

- `fastExp2` is exact for integer `x` (so octaves are exact), `2^0 == 1`. Results smaller
  than the smallest normal number have fewer significant bits (gradual underflow); below
  `-126.5` (float) / `-1022.5` (double) it returns `+0`. From `127.5` / `1023.5` up it
  returns `+inf`, half an octave before the true overflow. NaN propagates.
- `fastLog2` is exact at powers of two, `0` at 1, and its error is relative, so it stays
  accurate near 1. Denormal inputs are as accurate as normal ones. `log2(±0) = -inf`,
  `log2(+inf) = +inf`; negative inputs and NaN give NaN.
- `fastPow(x, 0) == 1` for every `x` (including 0, inf, NaN), `fastPow(1, y) == 1`, and
  powers of two to integer powers are exact. `fastPow(0, y > 0) = 0`,
  `fastPow(0, y < 0) = +inf`.
- `fastExp2` is monotonic across its reduction boundaries, `fastLog2` across its.

## When to use

- Per-sample or per-block gain, pitch, envelope-curve and coefficient conversions where
  ~1e-7 (float) / ~1e-8 (double) relative accuracy is enough. For reference, 1e-7 relative
  is 1.7e-4 cents or 9e-7 dB.
- For `fastPow` the error grows with `|y·log2 x|`, i.e. with the number of octaves in the
  result. A result spanning 20 octaves (120 dB) is still within ~3.5e-6 relative in float.
- Coefficients computed only on parameter changes can keep `std::pow` / `std::exp`.

## Trade-offs

- Negative bases give NaN even with integer exponents (unlike `std::pow`): take
  `std::abs` and apply the sign yourself. `fastPow(0, 0)` is 1, as with `std::pow`.
- `fastExp2` overflows to `+inf` half an octave early and flushes to `+0` instead of
  producing the smallest denormals.
- Not correctly rounded. Use the `std` functions when you need full precision.
- `float` and `double` only (the exponent is handled through their bit layouts).

## Performance

No branches: exponents are split off with integer conversions and bit manipulation, edge
cases are selects, and the polynomials (degree 6 for `2^x`, 9 for `log2`) are evaluated in
Estrin form for a shorter dependency chain. The win is throughput (buffers, many voices);
in a strict serial chain (each `pow` feeding the next) an optimized libm, e.g. Apple's
`powf`, can have lower latency. Benchmarked against `std::exp2`, `std::log2`
and `std::pow` for `float` and `double`, over a block and in a feedback chain
(`benchmarks/math/fast_pow_bench.cpp`).
