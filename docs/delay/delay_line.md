# DelayLine

`#include <xnyn/delay/delay_line.hpp>`

Single-channel delay line: the core of echoes, combs, chorus, flanger and
Karplus-Strong voices.

## API

```cpp
template <std::floating_point T = float>
class DelayLine {
 public:
  static constexpr int kMaxDelaySamples = 1 << 24;

  void prepare(double sampleRate, double maxDelaySeconds);  // allocates, then reset()
  void reset() noexcept;                       // zeroes the history
  void write(T x) noexcept;                    // pushes one sample
  T read(int delaySamples) const noexcept;     // x[n - d], d clamped to [1, maxDelaySamples()]
  T readLinear(T delaySamples) const noexcept; // linearly interpolated, same clamp
  int maxDelaySamples() const noexcept;        // ceil(maxDelaySeconds * sampleRate)
};
```

One instance is one channel. Copying is deleted (it would allocate); moving is allowed,
so lines can live in a `std::vector`, one per voice.

## Usage

```cpp
xnyn::DelayLine<float> delay;
delay.prepare(sampleRate, 2.0);  // up to 2 s, allocates: call off the audio thread

const float delaySamples = static_cast<float>(timeMs * 1e-3 * sampleRate);
for (auto& s : buffer) {
  const float echo = delay.readLinear(delaySamples);  // read first...
  delay.write(s + feedback * echo);                   // ...then write
  s += mix * echo;
}
```

Chain a `Smoother` on the delay time to modulate it without zipper noise.

## Behaviour

- Read, then write: `y = read(d); write(x)` gives `y[n] = x[n - d]`. For a feedback loop,
  the read value is the one fed back into `write`.
- Delays are in samples and clamped to `[1, maxDelaySamples()]`, without branches.
  NaN delays map to 1.
- `readLinear(d)` interpolates between `x[n - floor(d)]` and `x[n - floor(d) - 1]`, and
  is equal to `read(d)` at integer delays.
- `maxDelaySamples()` is `ceil(maxDelaySeconds * sampleRate)`, clamped to
  `[1, 2^24]` (about 5.8 min at 48 kHz). Non-positive or NaN sizes give 1.
- After `prepare` and `reset` the history is all zeros.
- Before `prepare`, reads return 0 and writes are discarded.
- A NaN or inf input only reaches the reads that touch it, until it is overwritten or
  `reset()` is called. A feedback loop around the line recirculates it, though: guard the
  input with `sanitize` if that can happen.

## Memory

`prepare` is the only function that allocates. It may throw `std::bad_alloc` (the line
is then unchanged) and, like a plugin's `prepareToPlay`, isn't real-time safe. When the
capacity is unchanged, it reuses the memory and only clears it.

The buffer is rounded up to a power of two, at least `maxDelaySamples() + 2` samples,
so memory use is up to twice the requested length. It is 64-byte aligned. `reset()`
zeroes the whole buffer, which costs time proportional to its size, but never
allocates.

## Precision

`readLinear` takes the fraction from the delay value itself (`d - floor(d)`) and does
the position math in integers. Its sub-sample resolution is therefore `ulp(d)`, however
long the line has been running. For `float`, that is 2^-8 sample or finer for delays
below 2^16 samples (1.4 s at 48 kHz). A naive `float(writePos) - d` loses all
fractional bits once the position passes 2^24.

## Performance

Wrapping is a bitmask on a power-of-two buffer, with no `while` loop, `%` or branch. Each
read is two masked loads and a `lerp`. The pointer and indices can't be aliased by stores
to a `float` output buffer, so they stay in registers.

Benchmarked against the naive form (`while` to wrap, `%` for the second tap, a
compare-and-reset write) in `benchmarks/delay/delay_line_bench.cpp`, for a modulated
chorus-style read and a feedback echo. It is faster in both.

The 32-bit indices and branchless reads leave room for a future multi-voice SIMD
version, which would do interpolated reads as gathers.
