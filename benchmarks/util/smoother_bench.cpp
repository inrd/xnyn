#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "xnyn/math/lerp.hpp"
#include "xnyn/util/smoother.hpp"

namespace {

constexpr int kBlock = 4096;
constexpr double kSR = 48000.0;

// Naive baseline: the common fixed-coefficient float smoother (R = 0.999 whatever the
// sample rate), float state.
class ReferenceSmoother {
 public:
  void setTarget(float x) { target_ = x; }
  float tick() {
    const float y = value_;
    value_ = xnyn::lerp(target_, value_, 0.999f);
    return y;
  }

 private:
  float value_ = 0.0f;
  float target_ = 0.0f;
};

// Gain smoothing over a block: the per-voice use case. The target changes once per
// block, as with a host parameter.
std::vector<float> makeInputs() {
  std::vector<float> in(kBlock);
  for (int i = 0; i < kBlock; ++i) in[static_cast<size_t>(i)] = std::sin(0.013f * static_cast<float>(i));
  return in;
}

}  // namespace

TEST_CASE("Smoother gain over a block", "[benchmark][util][smoother]") {
  const auto in = makeInputs();
  std::vector<float> out(kBlock);
  float target = 0.25f;

  xnyn::Smoother<float> s;
  s.prepare(kSR);
  BENCHMARK("xnyn::Smoother<float>") {
    target = 1.0f - target;
    s.setTarget(target);
    for (size_t i = 0; i < kBlock; ++i) out[i] = in[i] * s.tick();
    return out[kBlock - 1];
  };

  ReferenceSmoother ref;
  BENCHMARK("fixed-coefficient float-state baseline") {
    target = 1.0f - target;
    ref.setTarget(target);
    for (size_t i = 0; i < kBlock; ++i) out[i] = in[i] * ref.tick();
    return out[kBlock - 1];
  };
}
