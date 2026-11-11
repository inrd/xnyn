# isFinite / sanitize

`#include <xnyn/util/sanitize.hpp>`

NaN and infinity guards for `float` and `double` samples that keep working under
`-ffast-math`.

## API

```cpp
template <typename T>  // float or double
constexpr bool isFinite(T x) noexcept;  // false for any NaN and ±inf

template <typename T>  // float or double
constexpr T sanitize(T x) noexcept;     // x if finite, else +0
```

`long double` isn't supported, because it has no portable bit layout.

## Usage

```cpp
// Keep NaN / inf from a host or upstream code out of a filter.
for (auto& s : buffer) {
  s = filter.tick(xnyn::sanitize(s));
}

// Recover a block whose state has already been poisoned.
if (!xnyn::isFinite(out)) {
  filter.reset();
}
```

## Behaviour

- Any NaN (either sign, quiet or signalling, any payload) and ±inf count as non-finite.
- ±0, denormals and ±max are finite, so they pass through unchanged.
- Sanitizing a block's input stops new NaNs getting in, but can't repair state a NaN
  has already reached. Detect that with `isFinite` and call `reset()`.
- It doesn't flush denormals (use FTZ/DAZ for that) or clamp large values.

## Why not `std::isfinite`?

With `-ffast-math` (`-ffinite-math-only`), compilers assume NaN and inf never occur and
fold `std::isfinite`, `std::isnan` and `x != x` to constants, which silently disables
the guard. These functions test the bit pattern in a form that clang and GCC don't
recognise as a floating-point class check, so it survives optimization. This is
checked by `tests/util/sanitize_fast_math_tests.cpp`, which is built with
`-ffast-math`.

## Performance

It's branchless (shift, compare, select), so it vectorizes over buffers. Benchmarked
against a plain copy and `std::isfinite` (`benchmarks/util/sanitize_bench.cpp`).
