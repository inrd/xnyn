# Phasor

`#include <xnyn/osc/phasor.hpp>`

Normalized phase accumulator: the core of oscillators, LFOs and wavetable readers.

## API

```cpp
class Phasor {
 public:
  void prepare(double sampleRate) noexcept;  // caches 1 / sampleRate, then reset()
  void reset(double startPhase = 0.0) noexcept;  // startPhase in cycles, wrapped to [0, 1)
  void setFrequency(double hz) noexcept;     // any sign or magnitude
  double tick() noexcept;                    // returns the current phase, then advances
  void advance() noexcept;                   // advances without reading
  double phase() const noexcept;             // current phase in [0, 1)
};
```

## Usage

```cpp
xnyn::Phasor phasor;
phasor.prepare(sampleRate);
phasor.setFrequency(440.0);

for (auto& s : buffer) {
  s = static_cast<float>(2.0 * phasor.tick() - 1.0);  // naive saw
}
```

## Behaviour

- Phase is in cycles and is always within `[0, 1)`, never exactly 1.
- Frequency can be negative (the phase runs backwards, which allows through-zero FM),
  or above the sample rate (it wraps).
- `setFrequency` is cheap enough to call every sample for FM.
- Before `prepare`, the phasor stays at its start phase.
- `reset` accepts start phases with `|startPhase| < 2^63` cycles.

## Precision

Phase is stored as 64-bit fixed point (2^64 per cycle), so:

- Accumulation is exact: no drift, however long it runs.
- Resolution is 2^-63 cycle, better than a `double` accumulator (2^-53 near 1).
- `phase()` converts to `double` exactly from the top 53 bits.

## Performance

Each sample costs one integer add, and wrapping is free (unsigned overflow). Benchmarked
against `fmod` and `floor`-based accumulators (`benchmarks/osc/phasor_bench.cpp`).

The output is a raw phase, not a band-limited waveform: shaping it naively into
saw or square waves will alias.
