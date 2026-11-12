# Smoother

`#include <xnyn/util/smoother.hpp>`

One-pole parameter smoother: glides a value exponentially towards a target, to remove
zipper noise from gain, cutoff or mix changes.

## API

```cpp
template <std::floating_point T = float>
class Smoother {
 public:
  static constexpr T kDefaultTimeMs = T(20);

  void prepare(double sampleRate) noexcept;  // re-derives the time constant, then reset()
  void reset() noexcept;                     // snaps the value to the target
  void reset(T value) noexcept;              // sets value and target, no glide
  void setTimeMs(T timeMs) noexcept;         // time constant in ms, default 20; <= 0 jumps
  void setTarget(T target) noexcept;         // cheap, call any time
  T tick() noexcept;                         // returns the current value, then advances
  void advance() noexcept;                   // advances without reading
  T value() const noexcept;                  // current value
};
```

One instance smooths one parameter.

## Usage

```cpp
xnyn::Smoother<float> gain;
gain.prepare(sampleRate);
gain.setTimeMs(10.0f);
gain.reset(initialGain);  // start at the current value, no glide

// per block
gain.setTarget(gainParam);
for (auto& s : buffer) {
  s *= gain.tick();
}
```

## Behaviour

- `v[n+1] = target + R * (v[n] - target)`, with `R = exp(-1 / (tau * fs))` and `tau` the
  time constant set with `setTimeMs`.
- After `tau` the value has covered 63.2% of a step, and it is within 1e-3 of the target
  after `ln(1000) * tau` (about 6.9 `tau`, 138 ms at the default 20 ms). The time is the
  same at every sample rate.
- Monotone, no overshoot: the value stays between where it started and the target.
- `tick` returns the current value before advancing, so a new target shows up from the
  second `tick` on.
- `prepare` and `reset()` snap the value to the current target; `reset(value)` sets both.
- `timeMs <= 0` (and NaN) jump to the target on the next tick; `+inf` freezes the value.
- Before `prepare`, the value follows the target instantly whatever time is set.
- A NaN or inf target poisons the value until `reset(value)`.

## Precision

The value is stored as at least `double`, even for `T = float`. A one-pole update stops
moving once `R * (v - target)` rounds back to `v - target`, about `0.5 / (1 - R)` ulps
from the target. With `float` state at 20 ms / 48 kHz that is ~480 ulps (a glide from 0 to
0.5 stalls ~3e-5 short); with `double` state it is far below `float` resolution, so a
`float` smoother lands exactly on its target.

## Performance

`tick` is one multiply-add on the value plus a `double` to `float` conversion. When writing
to a `float` buffer it is faster than a naive fixed-coefficient `float`-state smoother,
whose state may alias the output and gets reloaded after every store. Benchmarked in
`benchmarks/util/smoother_bench.cpp`.

`setTimeMs` is a configuration call: it computes the coefficient with an exact
`std::exp`. `setTarget` is a plain store and can be called every sample.
