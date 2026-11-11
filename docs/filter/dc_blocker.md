# DcBlocker

`#include <xnyn/filter/dc_blocker.hpp>`

One-pole DC blocker: removes DC offset (e.g. after asymmetric saturation) while leaving
the audible band untouched.

## API

```cpp
template <std::floating_point T = float>
class DcBlocker {
 public:
  static constexpr T kDefaultCutoffHz = T(5);

  void prepare(double sampleRate) noexcept;  // re-derives the cutoff, then reset()
  void reset() noexcept;                     // state as if the input had been 0 forever
  void setCutoff(T cutoffHz) noexcept;       // clamped to [0, sampleRate / 2], default 5 Hz
  T tick(T x) noexcept;                      // returns y
};
```

One instance processes one channel: use one per voice or per channel.

## Usage

```cpp
xnyn::DcBlocker<float> dc;  // per voice
dc.prepare(sampleRate);
dc.setCutoff(5.0f);  // Hz

for (auto& s : buffer) {
  s = dc.tick(xnyn::fastTanh(drive * s + bias));
}
```

## Behaviour

- `y[n] = x[n] - x[n-1] + R * y[n-1]`, with `R = exp(-2 pi fc / fs)`.
- Gain is exactly 0 at DC and -3 dB at `fc` (within 0.1% for DC-blocking cutoffs). Above
  `fc` it is transparent, rising slightly above unity towards `2 / (1 + R)` at Nyquist
  (+0.005 dB at 1 kHz with `fc` = 10 Hz).
- The cutoff is in Hz, so it is the same at every sample rate.
- Step response is `R^n`: no overshoot (useful on kick transients), a constant input
  decays to 1e-3 after `ln(1000) / (2 pi fc)` seconds (about 0.22 s at 5 Hz).
- 6 dB/octave slope. If you need a steeper high-pass, use a dedicated high-pass filter.
- The first sample after `prepare` / `reset` passes through unchanged.
- `fc <= 0` (and NaN) give `R = 1`: exact pass-through for `float`. Cutoffs above
  `fs / 2` are clamped to `fs / 2`.
- Before `prepare`, it is a pass-through whatever cutoff is set.
- NaN or inf input poisons the state until `reset()`. Guard the input with `sanitize` if
  that can happen.

## Precision

The state and coefficient are stored as at least `double`, even for `T = float`. Near
`R = 1` (low cutoff, high sample rate) a `float` recursion amplifies rounding noise to
about -100 dB, and at `R = 1` it random-walks away from pass-through. With `double`
state, a `float` filter stays within about -140 dB of an exact reference, even at
1 Hz and 192 kHz, and `R = 1` is an exact pass-through.

## Performance

`tick` is one multiply-add and a subtraction. The `double` state costs about the same as
a naive fixed-coefficient `double`-state one-pole, and is faster than an all-`float` state when
writing to a `float` buffer (no aliasing reloads).

`setCutoff` is a configuration call: it computes the coefficient with an exact
`std::exp` and is not meant to be modulated at audio rate. `tick` is benchmarked in
`benchmarks/filter/dc_blocker_bench.cpp`.
