#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "xnyn/math/decibels.hpp"

namespace {

constexpr int kBlock = 4096;

// Levels in [-90, 6] dB, e.g. a modulated gain parameter.
std::vector<float> makeDb() {
  std::vector<float> in(kBlock);
  for (int i = 0; i < kBlock; ++i)
    in[static_cast<size_t>(i)] = -42.0f + 48.0f * std::sin(0.013f * static_cast<float>(i));
  return in;
}

// Bipolar audio for metering.
std::vector<float> makeSamples() {
  std::vector<float> in(kBlock);
  for (int i = 0; i < kBlock; ++i)
    in[static_cast<size_t>(i)] =
        0.8f * std::sin(0.031f * static_cast<float>(i)) * std::cos(0.0017f * static_cast<float>(i));
  return in;
}

}  // namespace

TEST_CASE("dbToGain over a block", "[benchmark][math][decibels]") {
  const auto in = makeDb();
  std::vector<float> out(kBlock);

  BENCHMARK("xnyn::dbToGain<float>") {
    for (size_t i = 0; i < kBlock; ++i) out[i] = xnyn::dbToGain(in[i]);
    return out[kBlock - 1];
  };

  BENCHMARK("std::pow(10, dB / 20) float baseline") {
    for (size_t i = 0; i < kBlock; ++i) out[i] = std::pow(10.0f, in[i] / 20.0f);
    return out[kBlock - 1];
  };
}

TEST_CASE("gainToDb over a block", "[benchmark][math][decibels]") {
  const auto in = makeSamples();
  std::vector<float> out(kBlock);

  BENCHMARK("xnyn::gainToDb<float> with -100 dB floor") {
    for (size_t i = 0; i < kBlock; ++i) out[i] = xnyn::gainToDb(in[i], -100.0f);
    return out[kBlock - 1];
  };

  BENCHMARK("20 * std::log10(max(|x|, 1e-5)) float baseline") {
    for (size_t i = 0; i < kBlock; ++i) out[i] = 20.0f * std::log10(std::fmax(std::abs(in[i]), 1e-5f));
    return out[kBlock - 1];
  };
}
