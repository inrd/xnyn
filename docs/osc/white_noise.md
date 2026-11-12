# WhiteNoise

`#include <xnyn/osc/white_noise.hpp>`

Uniform white noise in `(-1, 1)`: for noise oscillators, excitation signals, dither and
random modulation sources.

## API

```cpp
class WhiteNoise {
 public:
  static constexpr std::uint64_t kDefaultSeed = 0;

  void prepare(double sampleRate) noexcept;  // sample rate unused, calls reset()
  void reset() noexcept;                     // restarts the sequence from the current seed
  void reset(std::uint64_t seed) noexcept;   // sets a new seed (any value), then restarts
  float tick() noexcept;                     // returns the current sample, then advances
  void advance() noexcept;                   // advances without reading
  float value() const noexcept;              // current sample in (-1, 1)
};
```

## Usage

```cpp
// One generator per voice, each with its own seed.
xnyn::WhiteNoise noise;
noise.prepare(sampleRate);
noise.reset(voiceIndex);

for (auto& s : buffer) {
  s = 0.25f * noise.tick();
}
```

## Behaviour

- Uniform distribution on `2^22` equally likely values: the odd multiples of `2^-22` in
  `[-1 + 2^-22, 1 - 2^-22]`. Never exactly `-1`, `0` or `1`.
- The grid is symmetric, so the mean is exactly zero. RMS is `1 / sqrt(3)` (-4.77 dBFS).
- Deterministic: the same seed always gives the same sequence. The period is `2^64`
  samples, so it never repeats in practice.
- Every seed is valid (including `0`). Neighbouring seeds (`0`, `1`, `2`...) give
  uncorrelated sequences, so the voice index is a fine seed.
- The sample rate doesn't matter: `prepare` only restarts the sequence. A
  default-constructed generator already produces the `kDefaultSeed` sequence.
- Outputs `float`: 22 bits of noise are plenty. Cast it for `double` pipelines.
- The generator is fast, not cryptographic.

## Algorithm

Based on [Íñigo Quílez's `sfrand`](https://iquilezles.org/articles/sfrand/): random bits
go into the mantissa of a float whose exponent is that of 2. That gives a float in `[2, 4)`,
and subtracting 3 maps it to `[-1, 1)` with no int-to-float conversion. Compared with the
original:

- A 64-bit LCG instead of the 32-bit `seed *= 16807`, whose period is `2^30` (6.2 hours at
  48 kHz). Only the high bits are used, since the low bits of an LCG are weak.
- The lowest mantissa bit is forced to 1, which removes the original's `-2^-23` mean bias
  and makes the output symmetric.
- Seeds are scrambled (SplitMix64) so that neighbouring seeds don't correlate.

## Performance

`tick` is one 64-bit multiply-add, a shift, an OR and a float subtraction. The 64-bit state
costs the same as the original 32-bit `sfrand` and an int-to-float conversion LCG,
because all three are bound by the multiply on the loop-carried chain. It is several
times faster than `std::uniform_real_distribution`. Benchmarked in
`benchmarks/osc/white_noise_bench.cpp`.

Per-voice generators keep 16 bytes of state each.
