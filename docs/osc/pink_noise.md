# PinkNoise

`#include <xnyn/osc/pink_noise.hpp>`

Pink (1/f, -3 dB per octave) noise: equal energy per octave, for noise oscillators,
test signals and natural-sounding random modulation.

## API

```cpp
class PinkNoise {
 public:
  static constexpr std::uint64_t kDefaultSeed = 0;

  void prepare(double sampleRate) noexcept;  // designs the filter, then reset()
  void reset() noexcept;                     // restarts the sequence, clears the filter
  void reset(std::uint64_t seed) noexcept;   // sets a new seed (any value), then restarts
  float tick() noexcept;                     // returns the next sample
};
```

## Usage

```cpp
xnyn::PinkNoise noise;
noise.prepare(sampleRate);
noise.reset(voiceIndex);

for (auto& s : buffer) {
  s = noise.tick();  // about -18 dBFS in the audio band
}
```

## Behaviour

- `WhiteNoise` shaped by a -3 dB per octave filter, designed for the sample rate in
  `prepare`, so the spectrum is the same at every sample rate.
- Below 20 Hz the slope continues, then flattens out under 2 Hz, so the output has no DC
  drift.
- Same seeding as `WhiteNoise`: the same seed gives the same sequence, and neighbouring
  seeds give uncorrelated sequences.
- Unlike `WhiteNoise`, there is no `value()` / `advance()`: each output comes from the
  filter as the noise advances.
- Before `prepare`, the filter is a pass-through and the output is the plain white noise.

## Accuracy

- Spectrum within 0.35 dB of an exact 1/f slope from 20 Hz to `min(20 kHz, 0.45 fs)`,
  for sample rates from 8 kHz to 384 kHz. From 88.2 kHz up, it's within 0.05 dB. At
  44.1 / 48 kHz the error is the top octave running slightly low.
- Level: by design, the 20 Hz to 20 kHz band holds an RMS of 1/8 (-18 dBFS) at every
  sample rate from 44.1 kHz up. Total RMS is 0.15 to 0.165, rising slowly with the sample rate (more content
  outside the band). Lower sample rates are quieter, since the band above Nyquist is gone.
- The output is near-Gaussian, not hard-bounded: exceeding ±1 needs a > 6 sigma peak,
  which doesn't happen in practice.

The tests measure the filter's response against the exact white input and enforce the spectrum
bounds at 8, 22.05, 44.1, 48, 96, 192 and 384 kHz, and check the total RMS stays within
0.145 to 0.175 from 44.1 to 192 kHz.

## Design

A cascade of 8 first-order sections. The poles are spaced evenly in log frequency from
2 Hz to 30 kHz, and each one has a zero half a spacing above it. The response alternates
between -6 and 0 dB per octave, which averages to -3 dB per octave with about 0.02 dB of
ripple. Near Nyquist the digital poles and zeros fall short of the analog slope, so a final
`1 + c z^-1` term, solved in `prepare`, puts the top of the band back on the 1/f line.

State and coefficients are `double`: at 192 kHz the 2 Hz pole is `1 - 7e-5`, where a
`float` recursion would add rounding noise to the low end.

Compared with Paul Kellet's well-known filter, whose coefficients are fixed for 44.1 kHz,
this design is accurate at any sample rate.

## Performance

`tick` is one `WhiteNoise` sample plus 8 multiply-add sections and the final term. The
sections chain within a sample, but each section's only loop-carried dependency is its own
state, so successive samples overlap in the pipeline. It is faster than Kellet's
`float` filter (`benchmarks/osc/pink_noise_bench.cpp`).

`prepare` designs the filter with `std::exp`, `std::pow`, `std::cos` and complex
evaluation: it's a configuration call, not for per-sample use.
