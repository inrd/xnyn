#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "xnyn/osc/pink_noise.hpp"
#include "xnyn/osc/white_noise.hpp"

namespace {

constexpr int kBlock = 4096;

// Baseline: Paul Kellet's refined pink filter, 7 parallel one-poles with float state.
// Coefficients are fixed for 44.1 kHz.
struct KelletPink {
  xnyn::WhiteNoise white;
  float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
  float tick() {
    const float w = white.tick();
    b0 = 0.99886f * b0 + w * 0.0555179f;
    b1 = 0.99332f * b1 + w * 0.0750759f;
    b2 = 0.96900f * b2 + w * 0.1538520f;
    b3 = 0.86650f * b3 + w * 0.3104856f;
    b4 = 0.55000f * b4 + w * 0.5329522f;
    b5 = -0.7616f * b5 - w * 0.0168980f;
    const float out = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f;
    b6 = w * 0.115926f;
    return out * 0.11f;
  }
};

template <class Noise>
float fill(Noise& noise, std::vector<float>& out) {
  for (auto& s : out) s = noise.tick();
  return out[0] + out[kBlock - 1];
}

}  // namespace

TEST_CASE("PinkNoise fill a block", "[benchmark][osc][pink_noise]") {
  std::vector<float> out(kBlock);

  xnyn::PinkNoise pink;
  pink.prepare(48000.0);
  BENCHMARK("xnyn::PinkNoise") { return fill(pink, out); };

  KelletPink kellet;
  BENCHMARK("Kellet (float, 44.1 kHz only) baseline") { return fill(kellet, out); };

  xnyn::WhiteNoise white;
  BENCHMARK("xnyn::WhiteNoise (reference)") { return fill(white, out); };
}
