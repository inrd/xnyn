#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "xnyn/math/fast_tanh.hpp"

namespace {

constexpr int kBlock = 4096;

// Branching version this block was ported from: computes in double, returns early outside
// [-3, 3].
float branchingTanh(float x) {
  const auto xd = static_cast<double>(x);
  if (xd < -3.0) return -1.0f;
  if (xd > 3.0) return 1.0f;
  const auto x2 = xd * xd;
  return static_cast<float>(xd * ((27.0 + x2) / (27.0 + 9.0 * x2)));
}

// Driven signal spanning [-5, 5] so both the curve and the clamp are exercised.
std::vector<float> makeInputs() {
  std::vector<float> in(kBlock);
  for (int i = 0; i < kBlock; ++i)
    in[static_cast<size_t>(i)] =
        5.0f * std::sin(0.013f * static_cast<float>(i)) * std::cos(0.0071f * static_cast<float>(i));
  return in;
}

}  // namespace

TEST_CASE("fastTanh over a block", "[benchmark][math][fast_tanh]") {
  const auto in = makeInputs();
  std::vector<float> out(kBlock);

  BENCHMARK("xnyn::fastTanh") {
    for (size_t i = 0; i < kBlock; ++i) out[i] = xnyn::fastTanh(in[i]);
    return out[kBlock - 1];
  };

  BENCHMARK("branching double baseline") {
    for (size_t i = 0; i < kBlock; ++i) out[i] = branchingTanh(in[i]);
    return out[kBlock - 1];
  };

  BENCHMARK("std::tanh baseline") {
    for (size_t i = 0; i < kBlock; ++i) out[i] = std::tanh(in[i]);
    return out[kBlock - 1];
  };
}

// Serial dependency (saturating feedback loop): measures latency, not throughput.
TEST_CASE("fastTanh in a feedback chain", "[benchmark][math][fast_tanh]") {
  const auto in = makeInputs();

  BENCHMARK("xnyn::fastTanh") {
    float y = 0.0f;
    for (size_t i = 0; i < kBlock; ++i) y = xnyn::fastTanh(in[i] + 0.9f * y);
    return y;
  };

  BENCHMARK("branching double baseline") {
    float y = 0.0f;
    for (size_t i = 0; i < kBlock; ++i) y = branchingTanh(in[i] + 0.9f * y);
    return y;
  };

  BENCHMARK("std::tanh baseline") {
    float y = 0.0f;
    for (size_t i = 0; i < kBlock; ++i) y = std::tanh(in[i] + 0.9f * y);
    return y;
  };
}
