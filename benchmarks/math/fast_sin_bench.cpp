#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>
#include <vector>

#include "xnyn/math/fast_sin.hpp"
#include "xnyn/osc/phasor.hpp"

namespace {

constexpr int kBlock = 4096;

template <typename T>
std::vector<T> makePhases() {
  std::vector<T> in(kBlock);
  for (int i = 0; i < kBlock; ++i) in[static_cast<size_t>(i)] = static_cast<T>(std::fmod(0.0137 * i, 1.0));
  return in;
}

template <typename T>
T stdSin(T phase) {
  return std::sin(T(2) * std::numbers::pi_v<T> * phase);
}

template <typename T>
void benchBlock(const char* fastName, const char* stdName) {
  const auto in = makePhases<T>();
  std::vector<T> out(kBlock);

  BENCHMARK(fastName) {
    for (size_t i = 0; i < kBlock; ++i) out[i] = xnyn::fastSin(in[i]);
    return out[kBlock - 1];
  };

  BENCHMARK(stdName) {
    for (size_t i = 0; i < kBlock; ++i) out[i] = stdSin(in[i]);
    return out[kBlock - 1];
  };
}

// 2-op FM with modulator feedback through the frequency: the loop-carried dependency
// (sin -> setFrequency -> phase) measures latency, as in a synth voice.
template <typename Sin>
double fmBlock(Sin sinCycles, xnyn::Phasor& modulator, xnyn::Phasor& carrier, double& fb) {
  double acc = 0.0;
  for (int i = 0; i < kBlock; ++i) {
    modulator.setFrequency(50.0 + fb * (0.5 * 50.0));
    const double m = sinCycles(modulator.tick());
    carrier.setFrequency(220.0 + m * 300000.0);
    const double c = sinCycles(carrier.tick());
    fb = m;
    acc += c;
  }
  return acc;
}

}  // namespace

TEST_CASE("fastSin over a block", "[benchmark][math][fast_sin]") {
  benchBlock<float>("xnyn::fastSin<float>", "std::sin float baseline");
  benchBlock<double>("xnyn::fastSin<double>", "std::sin double baseline");
}

TEST_CASE("fastSin in a phasor-driven 2-op FM loop", "[benchmark][math][fast_sin]") {
  xnyn::Phasor modulator;
  xnyn::Phasor carrier;
  modulator.prepare(48000.0);
  carrier.prepare(48000.0);
  double fb = 0.0;

  BENCHMARK("xnyn::fastSin<double>") {
    return fmBlock([](double p) { return xnyn::fastSin(p); }, modulator, carrier, fb);
  };

  BENCHMARK("xnyn::fastSin<float>") {
    return fmBlock([](double p) { return static_cast<double>(xnyn::fastSin(static_cast<float>(p))); }, modulator,
                   carrier, fb);
  };

  BENCHMARK("std::sin double baseline") {
    return fmBlock([](double p) { return stdSin(p); }, modulator, carrier, fb);
  };

  BENCHMARK("std::sin float baseline") {
    return fmBlock([](double p) { return static_cast<double>(stdSin(static_cast<float>(p))); }, modulator, carrier, fb);
  };
}
