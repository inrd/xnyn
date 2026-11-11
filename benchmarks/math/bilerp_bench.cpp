#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "xnyn/math/bilerp.hpp"

namespace {

constexpr int kBlock = 4096;

struct Inputs {
  std::vector<float> tl, tr, bl, br, x, y;
};

Inputs makeInputs() {
  Inputs in;
  for (auto* v : {&in.tl, &in.tr, &in.bl, &in.br, &in.x, &in.y}) v->resize(kBlock);
  for (int i = 0; i < kBlock; ++i) {
    const auto k = static_cast<size_t>(i);
    const auto f = static_cast<float>(i);
    in.tl[k] = std::sin(0.013f * f);
    in.tr[k] = std::cos(0.029f * f);
    in.bl[k] = std::sin(0.041f * f);
    in.br[k] = std::cos(0.017f * f);
    in.x[k] = 0.5f + 0.5f * std::sin(0.007f * f);
    in.y[k] = 0.5f + 0.5f * std::cos(0.011f * f);
  }
  return in;
}

// Weighted-sum form: each corner times its area weight.
float weightedSum(float tl, float tr, float bl, float br, float x, float y) {
  const float ix = 1.0f - x;
  const float iy = 1.0f - y;
  return (tl * ix * iy) + (tr * x * iy) + (bl * y * ix) + (br * x * y);
}

}  // namespace

TEST_CASE("bilerp over a block", "[benchmark][math][bilerp]") {
  const auto in = makeInputs();
  std::vector<float> out(kBlock);

  BENCHMARK("xnyn::bilerp") {
    for (size_t i = 0; i < kBlock; ++i) out[i] = xnyn::bilerp(in.tl[i], in.tr[i], in.bl[i], in.br[i], in.x[i], in.y[i]);
    return out[kBlock - 1];
  };

  BENCHMARK("weighted-sum baseline") {
    for (size_t i = 0; i < kBlock; ++i) out[i] = weightedSum(in.tl[i], in.tr[i], in.bl[i], in.br[i], in.x[i], in.y[i]);
    return out[kBlock - 1];
  };
}
