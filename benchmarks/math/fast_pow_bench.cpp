#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <string>
#include <vector>

#include "xnyn/math/fast_pow.hpp"

namespace {

constexpr int kBlock = 4096;

// Exponents in [-20, 20] and bases in [1/64, 64], the range of gain, pitch and curve maths.
template <typename T>
std::vector<T> makeInputs(double scale, double offset) {
  std::vector<T> in(kBlock);
  for (int i = 0; i < kBlock; ++i)
    in[static_cast<size_t>(i)] = static_cast<T>(offset + scale * std::sin(0.0137 * i) * std::cos(0.0071 * i));
  return in;
}

template <typename T>
void benchBlock(const char* label) {
  const auto exponents = makeInputs<T>(20.0, 0.0);
  std::vector<T> bases(kBlock);
  for (size_t i = 0; i < kBlock; ++i) bases[i] = static_cast<T>(std::exp2(0.3 * static_cast<double>(exponents[i])));
  std::vector<T> out(kBlock);

  const std::string type = label;

  BENCHMARK("xnyn::fastExp2<" + type + ">") {
    for (size_t i = 0; i < kBlock; ++i) out[i] = xnyn::fastExp2(exponents[i]);
    return out[kBlock - 1];
  };
  BENCHMARK("std::exp2 " + type + " baseline") {
    for (size_t i = 0; i < kBlock; ++i) out[i] = std::exp2(exponents[i]);
    return out[kBlock - 1];
  };
  BENCHMARK("xnyn::fastLog2<" + type + ">") {
    for (size_t i = 0; i < kBlock; ++i) out[i] = xnyn::fastLog2(bases[i]);
    return out[kBlock - 1];
  };
  BENCHMARK("std::log2 " + type + " baseline") {
    for (size_t i = 0; i < kBlock; ++i) out[i] = std::log2(bases[i]);
    return out[kBlock - 1];
  };
  BENCHMARK("xnyn::fastPow<" + type + ">") {
    for (size_t i = 0; i < kBlock; ++i) out[i] = xnyn::fastPow(bases[i], exponents[i] * T(0.1));
    return out[kBlock - 1];
  };
  BENCHMARK("std::pow " + type + " baseline") {
    for (size_t i = 0; i < kBlock; ++i) out[i] = std::pow(bases[i], exponents[i] * T(0.1));
    return out[kBlock - 1];
  };
}

}  // namespace

TEST_CASE("fastPow over a block", "[benchmark][math][fast_pow]") {
  benchBlock<float>("float");
  benchBlock<double>("double");
}

// Serial dependency (pow feeding the next pow's exponent, e.g. curve-shaped envelopes or
// pitch modulation in a feedback loop): measures latency, not throughput.
TEST_CASE("fastPow in a feedback chain", "[benchmark][math][fast_pow]") {
  const auto mod = makeInputs<float>(0.5, 0.0);

  BENCHMARK("xnyn::fastPow<float>") {
    float y = 0.5f;
    for (size_t i = 0; i < kBlock; ++i) y = xnyn::fastPow(0.5f + y, mod[i]);
    return y;
  };

  BENCHMARK("std::pow float baseline") {
    float y = 0.5f;
    for (size_t i = 0; i < kBlock; ++i) y = std::pow(0.5f + y, mod[i]);
    return y;
  };

  BENCHMARK("xnyn::fastPow<double>") {
    double y = 0.5;
    for (size_t i = 0; i < kBlock; ++i) y = xnyn::fastPow(0.5 + y, static_cast<double>(mod[i]));
    return y;
  };

  BENCHMARK("std::pow double baseline") {
    double y = 0.5;
    for (size_t i = 0; i < kBlock; ++i) y = std::pow(0.5 + y, static_cast<double>(mod[i]));
    return y;
  };
}
