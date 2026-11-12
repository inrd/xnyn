#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <bit>
#include <cstdint>
#include <random>
#include <vector>

#include "xnyn/osc/white_noise.hpp"

namespace {

constexpr int kBlock = 4096;

// Naive baseline: the standard library route.
struct StdNoise {
  std::minstd_rand rng{1};
  std::uniform_real_distribution<float> dist{-1.0f, 1.0f};
  float tick() { return dist(rng); }
};

// iquilezles.org/articles/sfrand as published: 32-bit LCG, same exponent-bit trick.
// Kept to show the 64-bit state costs nothing extra: both are bound by the multiply
// latency on the loop-carried chain. This one has a 2^30 period and a -2^-23 mean bias.
struct SfrandNoise {
  std::uint32_t seed = 1;
  float tick() {
    seed *= 16807u;
    return std::bit_cast<float>((seed >> 9) | 0x40000000u) - 3.0f;
  }
};

// Integer LCG output converted with an int -> float multiply instead of the bit trick.
struct ConvertNoise {
  std::uint64_t state = 1;
  float tick() {
    state = state * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<float>(static_cast<std::int32_t>(state >> 32)) * 0x1p-31f;
  }
};

template <class Noise>
float fill(Noise& noise, std::vector<float>& out) {
  for (auto& s : out) s = noise.tick();
  return out[0] + out[kBlock - 1];
}

}  // namespace

TEST_CASE("WhiteNoise fill a block", "[benchmark][osc][white_noise]") {
  std::vector<float> out(kBlock);

  xnyn::WhiteNoise noise;
  BENCHMARK("xnyn::WhiteNoise") { return fill(noise, out); };

  SfrandNoise sfrand;
  BENCHMARK("sfrand (32-bit LCG) baseline") { return fill(sfrand, out); };

  ConvertNoise convert;
  BENCHMARK("int to float conversion baseline") { return fill(convert, out); };

  StdNoise stdNoise;
  BENCHMARK("std::uniform_real_distribution baseline") { return fill(stdNoise, out); };
}
