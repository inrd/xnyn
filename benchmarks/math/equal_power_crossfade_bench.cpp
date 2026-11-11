#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>
#include <vector>

#include "xnyn/math/equal_power_crossfade.hpp"

namespace {

constexpr int kBlock = 4096;

struct Inputs {
  std::vector<float> a, b, t;
};

Inputs makeInputs() {
  Inputs in{std::vector<float>(kBlock), std::vector<float>(kBlock), std::vector<float>(kBlock)};
  for (int i = 0; i < kBlock; ++i) {
    const auto k = static_cast<size_t>(i);
    in.a[k] = std::sin(0.013f * static_cast<float>(i));
    in.b[k] = std::cos(0.029f * static_cast<float>(i));
    in.t[k] = 0.5f + 0.5f * std::sin(0.007f * static_cast<float>(i));
  }
  return in;
}

// Naive baseline: both gains from std::cos in double.
float stdCrossfade(float a, float b, float t) {
  const auto c = static_cast<double>(t);
  const double gainA = std::cos(c * (std::numbers::pi / 2.0));
  const double gainB = std::cos((1.0 - c) * (std::numbers::pi / 2.0));
  return static_cast<float>(gainA * static_cast<double>(a) + gainB * static_cast<double>(b));
}

// Same gains through the general fastSin path, which pays for range reduction.
float fastSinCrossfade(float a, float b, float t) {
  return xnyn::fastSin(0.25f * (1.0f - t)) * a + xnyn::fastSin(0.25f * t) * b;
}

}  // namespace

TEST_CASE("equalPowerCrossfade over a block", "[benchmark][math][equal_power_crossfade]") {
  const auto in = makeInputs();
  std::vector<float> out(kBlock);

  BENCHMARK("xnyn::equalPowerCrossfade<float>") {
    for (size_t i = 0; i < kBlock; ++i) out[i] = xnyn::equalPowerCrossfade(in.a[i], in.b[i], in.t[i]);
    return out[kBlock - 1];
  };

  BENCHMARK("fastSin baseline") {
    for (size_t i = 0; i < kBlock; ++i) out[i] = fastSinCrossfade(in.a[i], in.b[i], in.t[i]);
    return out[kBlock - 1];
  };

  BENCHMARK("std::cos double baseline") {
    for (size_t i = 0; i < kBlock; ++i) out[i] = stdCrossfade(in.a[i], in.b[i], in.t[i]);
    return out[kBlock - 1];
  };
}
