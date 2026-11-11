#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "xnyn/math/lerp.hpp"

namespace {

constexpr int kBlock = 4096;

struct Inputs {
  std::vector<float> a, b, t;
};

// Mixed signs so std::lerp can't settle on one branch.
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

}  // namespace

TEST_CASE("lerp over a block", "[benchmark][math][lerp]") {
  const auto in = makeInputs();
  std::vector<float> out(kBlock);

  BENCHMARK("xnyn::lerp") {
    for (size_t i = 0; i < kBlock; ++i) out[i] = xnyn::lerp(in.a[i], in.b[i], in.t[i]);
    return out[kBlock - 1];
  };

  BENCHMARK("std::lerp baseline") {
    for (size_t i = 0; i < kBlock; ++i) out[i] = std::lerp(in.a[i], in.b[i], in.t[i]);
    return out[kBlock - 1];
  };
}

// Serial dependency (one-pole-like smoothing toward a target): measures latency, not
// throughput, which is what per-sample smoothing chains pay.
TEST_CASE("lerp in a feedback chain", "[benchmark][math][lerp]") {
  const auto in = makeInputs();

  BENCHMARK("xnyn::lerp") {
    float y = 0.0f;
    for (size_t i = 0; i < kBlock; ++i) y = xnyn::lerp(y, in.b[i], 0.01f);
    return y;
  };

  BENCHMARK("std::lerp baseline") {
    float y = 0.0f;
    for (size_t i = 0; i < kBlock; ++i) y = std::lerp(y, in.b[i], 0.01f);
    return y;
  };
}
