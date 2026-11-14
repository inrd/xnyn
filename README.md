# x[n]y[n]

Minimalist, performance-focused C++20 DSP building blocks.

**xnyn** is header-only and has no dependencies beyond the standard library.

## Blocks

| Block | Header | Description |
|---|---|---|
| [`DelayLine`](docs/delay/delay_line.md) | `xnyn/delay/delay_line.hpp` | Power-of-two circular buffer, branchless masked integer and linear reads, position-independent fractional resolution |
| [`DcBlocker`](docs/filter/dc_blocker.md) | `xnyn/filter/dc_blocker.hpp` | One-pole DC blocker, cutoff in Hz, no step overshoot, `double` state for low cutoffs |
| [`bilerp`](docs/math/bilerp.md) | `xnyn/math/bilerp.hpp` | Branchless bilinear interpolation between four corners, exact for constant fields |
| [`dbToGain` / `gainToDb`](docs/math/decibels.md) | `xnyn/math/decibels.hpp` | Branchless dB ↔ gain conversions via `fastExp2` / `fastLog2`, magnitude-based, optional dB floor |
| [`equalPowerCrossfade`](docs/math/equal_power_crossfade.md) | `xnyn/math/equal_power_crossfade.hpp` | Sine / cosine crossfade with constant power, gains via `fastSin`, exact endpoints |
| [`fastExp2` / `fastLog2` / `fastPow`](docs/math/fast_pow.md) | `xnyn/math/fast_pow.hpp` | Branchless polynomial `2^x`, `log2` and `pow`, max rel error 2e-9 / 2.7e-8 (double), 1e-7 / 2.4e-7 (float) |
| [`fastSin`](docs/math/fast_sin.md) | `xnyn/math/fast_sin.hpp` | Branchless polynomial `sin(2π·phase)`, phase in cycles, max abs error 6e-8 (double) / 2.1e-7 (float), FM-grade |
| [`fastTanPi`](docs/math/fast_tan.md) | `xnyn/math/fast_tan.hpp` | Branchless rational `tan(π·x)` for filter prewarping, x = fc / fs, max rel error 4.5e-8 (double) / 4e-7 (float) up to Nyquist |
| [`fastTanh`](docs/math/fast_tanh.md) | `xnyn/math/fast_tanh.hpp` | Branchless rational tanh approximation, max abs error 0.0236 |
| [`lerp`](docs/math/lerp.md) | `xnyn/math/lerp.hpp` | Branchless `a + t * (b - a)`, faster than `std::lerp` |
| [`Phasor`](docs/osc/phasor.md) | `xnyn/osc/phasor.hpp` | Normalized phase accumulator, branchless wrap, supports negative and through-zero frequencies |
| [`PinkNoise`](docs/osc/pink_noise.md) | `xnyn/osc/pink_noise.hpp` | 1/f noise, filter designed per sample rate, within 0.35 dB of -3 dB/octave over 20 Hz – 20 kHz, calibrated -18 dBFS band level |
| [`WhiteNoise`](docs/osc/white_noise.md) | `xnyn/osc/white_noise.hpp` | Uniform white noise in (-1, 1), 64-bit LCG with exponent-bit float conversion, zero-mean, seedable, period 2^64 |
| [`isFinite` / `sanitize`](docs/util/sanitize.md) | `xnyn/util/sanitize.hpp` | NaN / inf guards that survive `-ffast-math` |
| [`Smoother`](docs/util/smoother.md) | `xnyn/util/smoother.hpp` | One-pole parameter smoother, time constant in ms, no overshoot, `double` state so `float` lands on target |

See [docs/](docs/README.md) for per-block documentation.

## Usage

```cpp
#include <xnyn/osc/phasor.hpp>

xnyn::Phasor phasor;
phasor.prepare(sampleRate);
phasor.setFrequency(440.0);

for (auto& sample : buffer) {
  const double phase = phasor.tick();  // [0, 1)
  // ...
}
```

Processing is per-sample: blocks are header-only and non-virtual, so `tick()` inlines
into your loop with no per-call overhead. Speed-ups come from vectorizing across voices,
not across time.

Include individual headers, or `<xnyn/xnyn.hpp>` for every block.

## Integration

xnyn exposes the CMake target `xnyn::xnyn`.

**Git submodule**

```cmake
add_subdirectory(path/to/xnyn)
target_link_libraries(MyTarget PRIVATE xnyn::xnyn)
```

**FetchContent**

```cmake
include(FetchContent)
FetchContent_Declare(xnyn
    GIT_REPOSITORY https://github.com/inrd/xnyn.git
    GIT_TAG main)
FetchContent_MakeAvailable(xnyn)
target_link_libraries(MyTarget PRIVATE xnyn::xnyn)
```

Tests and benchmarks are only built when xnyn is the top-level project.

## Development

```sh
# tests
cmake --preset debug && cmake --build --preset debug && ctest --preset debug

# benchmarks
cmake --preset release && cmake --build --preset release
./build/release/benchmarks/xnyn_benchmarks
```

Or use the `Makefile` shortcuts: `make test`, `make test-release`, `make bench`,
`make format`, `make clean`. Filter with a Catch2 test spec, e.g.
`make test T="[fast_sin]"` (`make help` lists them).

Catch2 v3 is used if installed, otherwise fetched automatically.
See [CONVENTIONS.md](CONVENTIONS.md) for the rules every block follows.

## License

[MIT](LICENSE)
