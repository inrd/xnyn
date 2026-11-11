#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "xnyn/filter/dc_blocker.hpp"

namespace {

constexpr int kBlock = 4096;
constexpr double kSR = 48000.0;

// Naive baseline: the common fixed-coefficient form (double state, R = 0.9997 whatever
// the sample rate).
class ReferenceDcBlocker {
 public:
  float tick(float x) {
    const auto y = (double)x - mX1 + (mY1 * 0.9997);
    mX1 = (double)x;
    mY1 = y;
    return (float)y;
  }

 private:
  double mX1 = 0.0;
  double mY1 = 0.0;
};

// All-float version, to price the double state. Writing float output through a pointer
// may alias float state members, so the compiler reloads the state after every store and
// this runs several times slower than the double-state versions (whose state can't alias a
// float buffer). Arithmetic alone, with state in registers, costs about the same.
class FloatDcBlocker {
 public:
  float tick(float x) {
    const float y = x - x1_ + r_ * y1_;
    x1_ = x;
    y1_ = y;
    return y;
  }

 private:
  float x1_ = 0.0f;
  float y1_ = 0.0f;
  float r_ = 0.9997f;
};

// Saturated, offset signal: the per-voice use case.
std::vector<float> makeInputs() {
  std::vector<float> in(kBlock);
  for (int i = 0; i < kBlock; ++i)
    in[static_cast<size_t>(i)] = std::tanh(2.0f * std::sin(0.013f * static_cast<float>(i)) + 0.5f);
  return in;
}

}  // namespace

TEST_CASE("DcBlocker over a block", "[benchmark][filter][dc_blocker]") {
  const auto in = makeInputs();
  std::vector<float> out(kBlock);

  xnyn::DcBlocker<float> dc;
  dc.prepare(kSR);
  BENCHMARK("xnyn::DcBlocker<float>") {
    for (size_t i = 0; i < kBlock; ++i) out[i] = dc.tick(in[i]);
    return out[kBlock - 1];
  };

  ReferenceDcBlocker ref;
  BENCHMARK("fixed-coefficient double-state baseline") {
    for (size_t i = 0; i < kBlock; ++i) out[i] = ref.tick(in[i]);
    return out[kBlock - 1];
  };

  FloatDcBlocker flt;
  BENCHMARK("float-state baseline") {
    for (size_t i = 0; i < kBlock; ++i) out[i] = flt.tick(in[i]);
    return out[kBlock - 1];
  };
}
