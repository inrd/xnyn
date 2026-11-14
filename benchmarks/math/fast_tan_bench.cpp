#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>
#include <vector>

#include "xnyn/math/fast_sin.hpp"
#include "xnyn/math/fast_tan.hpp"

namespace {

constexpr int kBlock = 4096;

template <typename T>
std::vector<T> makeNormalizedFrequencies() {
  std::vector<T> in(kBlock);
  for (int i = 0; i < kBlock; ++i) in[static_cast<size_t>(i)] = static_cast<T>(0.49 * std::fmod(0.0137 * i, 1.0));
  return in;
}

template <typename T>
T stdTanPi(T x) {
  return std::tan(std::numbers::pi_v<T> * x);
}

template <typename T>
void benchBlock(const char* fastName, const char* stdName) {
  const auto in = makeNormalizedFrequencies<T>();
  std::vector<T> out(kBlock);

  BENCHMARK(fastName) {
    for (size_t i = 0; i < kBlock; ++i) out[i] = xnyn::fastTanPi(in[i]);
    return out[kBlock - 1];
  };

  BENCHMARK(stdName) {
    for (size_t i = 0; i < kBlock; ++i) out[i] = stdTanPi(in[i]);
    return out[kBlock - 1];
  };
}

// TPT state-variable filter with its cutoff swept by an LFO every sample, so the prewarp
// sits on the per-sample path as in a modulated synth filter.
template <typename T, typename TanPi>
T svfBlock(TanPi tanPi, T& s1, T& s2, double& lfoPhase) {
  constexpr T kInvSampleRate = T(1) / T(48000);
  constexpr T kTwoDamping = T(0.5);
  T acc = T(0);
  for (int i = 0; i < kBlock; ++i) {
    lfoPhase += 2.0 / 48000.0;
    lfoPhase -= std::floor(lfoPhase);
    const T cutoffHz = T(1000) + T(800) * static_cast<T>(xnyn::fastSin(lfoPhase));
    const T g = tanPi(cutoffHz * kInvSampleRate);
    const T a = T(1) / (T(1) + g * (g + kTwoDamping));
    const T x = (i & 64) != 0 ? T(1) : T(-1);
    const T hp = a * (x - (kTwoDamping + g) * s1 - s2);
    const T bp = g * hp + s1;
    const T lp = g * bp + s2;
    s1 = g * hp + bp;
    s2 = g * bp + lp;
    acc += lp;
  }
  return acc;
}

template <typename T>
void benchSvf(const char* fastName, const char* stdName) {
  T s1 = T(0);
  T s2 = T(0);
  double lfoPhase = 0.0;

  BENCHMARK(fastName) {
    return svfBlock<T>([](T x) { return xnyn::fastTanPi(x); }, s1, s2, lfoPhase);
  };

  BENCHMARK(stdName) {
    return svfBlock<T>([](T x) { return stdTanPi(x); }, s1, s2, lfoPhase);
  };
}

}  // namespace

TEST_CASE("fastTanPi over a block", "[benchmark][math][fast_tan]") {
  benchBlock<float>("xnyn::fastTanPi<float>", "std::tan float baseline");
  benchBlock<double>("xnyn::fastTanPi<double>", "std::tan double baseline");
}

TEST_CASE("fastTanPi in a cutoff-modulated SVF", "[benchmark][math][fast_tan]") {
  benchSvf<float>("xnyn::fastTanPi<float>", "std::tan float baseline");
  benchSvf<double>("xnyn::fastTanPi<double>", "std::tan double baseline");
}
