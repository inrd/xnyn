#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <vector>

#include "xnyn/util/sanitize.hpp"

namespace {

constexpr int kBlock = 4096;

// Mostly finite signal with a sprinkling of NaN / inf, so nothing is trivially predictable.
std::vector<float> makeInput() {
  std::vector<float> in(kBlock);
  for (int i = 0; i < kBlock; ++i) {
    const auto k = static_cast<size_t>(i);
    in[k] = std::sin(0.013f * static_cast<float>(i));
    if (i % 97 == 0) in[k] = std::numeric_limits<float>::quiet_NaN();
    if (i % 131 == 0) in[k] = std::numeric_limits<float>::infinity();
  }
  return in;
}

}  // namespace

TEST_CASE("sanitize over a block", "[benchmark][util][sanitize]") {
  const auto in = makeInput();
  std::vector<float> out(kBlock);

  BENCHMARK("copy baseline (no sanitizing)") {
    for (size_t i = 0; i < kBlock; ++i) out[i] = in[i];
    return out[kBlock - 1];
  };

  BENCHMARK("xnyn::sanitize") {
    for (size_t i = 0; i < kBlock; ++i) out[i] = xnyn::sanitize(in[i]);
    return out[kBlock - 1];
  };

  BENCHMARK("std::isfinite baseline") {
    for (size_t i = 0; i < kBlock; ++i) out[i] = std::isfinite(in[i]) ? in[i] : 0.0f;
    return out[kBlock - 1];
  };
}
