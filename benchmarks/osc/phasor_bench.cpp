#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "xnyn/osc/phasor.hpp"

namespace {

constexpr int kBlock = 4096;
constexpr double kSR = 48000.0;
constexpr double kInvSR = 1.0 / kSR;

// Naive baseline: same normalized phase, wrapped with fmod plus a branch for negatives.
struct FmodPhasor {
  double phase = 0.0;
  double delta = 0.0;
  void advance() {
    phase = std::fmod(phase + delta, 1.0);
    if (phase < 0.0) phase += 1.0;
  }
};

// Audio-rate frequency modulation crossing zero, precomputed outside the timed loop.
std::vector<double> makeFmFreqs() {
  std::vector<double> f(kBlock);
  for (int i = 0; i < kBlock; ++i) f[(size_t)i] = 2000.0 * std::sin(0.05 * i);
  return f;
}

}  // namespace

TEST_CASE("Phasor through-zero FM", "[benchmark][osc][phasor]") {
  const auto freqs = makeFmFreqs();

  BENCHMARK("xnyn::Phasor") {
    xnyn::Phasor p;
    p.prepare(kSR);
    double acc = 0.0;
    for (const double f : freqs) {
      p.setFrequency(f);
      acc += p.tick();
    }
    return acc;
  };

  BENCHMARK("fmod baseline") {
    FmodPhasor p;
    double acc = 0.0;
    for (const double f : freqs) {
      p.delta = f * kInvSR;
      p.advance();
      acc += p.phase;
    }
    return acc;
  };
}
