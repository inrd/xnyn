#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "xnyn/delay/delay_line.hpp"
#include "xnyn/math/lerp.hpp"

namespace {

constexpr int kBlock = 4096;
constexpr double kSR = 48000.0;
constexpr double kMaxSeconds = 1.0;

// Naive baseline: the common form, with a non-power-of-two buffer, a while loop to wrap the
// read position, a modulo for the second tap and a compare-and-reset write.
class NaiveDelayLine {
 public:
  explicit NaiveDelayLine(int size) : buf_(static_cast<size_t>(size), 0.0f), size_(size) {}

  float read(float timeInSamples) {
    auto rp = static_cast<float>(pos_) - timeInSamples;
    while (rp < 0.0f) rp += static_cast<float>(size_);
    const int p = static_cast<int>(rp);
    const int p1 = (p + 1) % size_;
    const float f = rp - static_cast<float>(p);
    return xnyn::lerp(buf_[static_cast<size_t>(p)], buf_[static_cast<size_t>(p1)], f);
  }

  void write(float x) {
    buf_[static_cast<size_t>(pos_)] = x;
    ++pos_;
    if (pos_ >= size_) pos_ = 0;
  }

 private:
  std::vector<float> buf_;
  int size_;
  int pos_ = 0;
};

std::vector<float> makeInputs() {
  std::vector<float> in(kBlock);
  for (int i = 0; i < kBlock; ++i) in[static_cast<size_t>(i)] = std::sin(0.013f * static_cast<float>(i));
  return in;
}

// Chorus-style modulated delay, 10 ms +- 3 ms, precomputed outside the timed loop.
std::vector<float> makeModulatedDelays() {
  std::vector<float> d(kBlock);
  for (int i = 0; i < kBlock; ++i)
    d[static_cast<size_t>(i)] = static_cast<float>(kSR * (0.010 + 0.003 * std::sin(0.002 * i)));
  return d;
}

}  // namespace

TEST_CASE("DelayLine modulated read", "[benchmark][delay][delay_line]") {
  const auto in = makeInputs();
  const auto delays = makeModulatedDelays();
  std::vector<float> out(kBlock);

  xnyn::DelayLine<float> dl;
  dl.prepare(kSR, kMaxSeconds);
  BENCHMARK("xnyn::DelayLine<float>::readLinear") {
    for (size_t i = 0; i < kBlock; ++i) {
      out[i] = dl.readLinear(delays[i]);
      dl.write(in[i]);
    }
    return out[kBlock - 1];
  };

  NaiveDelayLine naive(static_cast<int>(kSR * kMaxSeconds));
  BENCHMARK("naive while / modulo baseline") {
    for (size_t i = 0; i < kBlock; ++i) {
      out[i] = naive.read(delays[i]);
      naive.write(in[i]);
    }
    return out[kBlock - 1];
  };
}

TEST_CASE("DelayLine feedback echo", "[benchmark][delay][delay_line]") {
  const auto in = makeInputs();
  std::vector<float> out(kBlock);
  constexpr float kDelay = 12345.5f;

  xnyn::DelayLine<float> dl;
  dl.prepare(kSR, kMaxSeconds);
  BENCHMARK("xnyn::DelayLine<float> echo") {
    for (size_t i = 0; i < kBlock; ++i) {
      const float y = in[i] + 0.5f * dl.readLinear(kDelay);
      dl.write(y);
      out[i] = y;
    }
    return out[kBlock - 1];
  };

  NaiveDelayLine naive(static_cast<int>(kSR * kMaxSeconds));
  BENCHMARK("naive echo baseline") {
    for (size_t i = 0; i < kBlock; ++i) {
      const float y = in[i] + 0.5f * naive.read(kDelay);
      naive.write(y);
      out[i] = y;
    }
    return out[kBlock - 1];
  };
}
