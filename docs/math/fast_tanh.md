# fastTanh

`#include <xnyn/math/fast_tanh.hpp>`

Branchless rational approximation of `tanh(x)`, for saturation and waveshaping.

## API

```cpp
template <std::floating_point T>
constexpr T fastTanh(T x) noexcept;
```

## Usage

```cpp
for (auto& s : buffer) {
  s = xnyn::fastTanh(drive * s);
}
```

## Accuracy

- Valid for any `x`. Output is in `[-1, 1]`, give or take 1 ulp (see below).
- Max absolute error against `std::tanh`: **0.0236**, peaking near `|x| = 1.57`.
- Odd function, exact at 0.
- Reaches exactly ±1 at `|x| = 3` with zero slope, and is clamped beyond that. The knee is
  smooth (C1), so it doesn't add the harsh harmonics of a hard clip.
- NaN propagates.

## Trade-offs

- Near `|x| = 3` rounding can overshoot ±1 by 1 ulp. Clamping the output would fix this,
  but costs about 20% latency in feedback loops. Clamp it yourself if you need a hard bound.
- Unlike `std::tanh`, this isn't a faithful tanh. It has its own, slightly softer
  saturation curve. Use `std::tanh` when you need accuracy, not speed.

## Performance

No branches, so it vectorizes over buffers. Benchmarked against `std::tanh` and a
branching implementation, over a block and in a feedback chain
(`benchmarks/math/fast_tanh_bench.cpp`).
