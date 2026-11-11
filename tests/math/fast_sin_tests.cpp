#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <numbers>
#include <vector>

#include "xnyn/math/fast_sin.hpp"
#include "xnyn/osc/phasor.hpp"

namespace {

// Reference sin(2 pi x) in double, reduced to one cycle first so the reference itself
// stays accurate for large |x| (x - floor(x) is exact).
double refSin(double x) { return std::sin(2.0 * std::numbers::pi * (x - std::floor(x))); }

// Documented bounds (header and docs/math/fast_sin.md).
template <typename T>
constexpr double kMaxAbsError = sizeof(T) == sizeof(float) ? 2.1e-7 : 6.0e-8;

}  // namespace

// Usable in constant expressions.
static_assert(xnyn::fastSin(0.0) == 0.0);
static_assert(xnyn::fastSin(0.25) == 1.0);
static_assert(xnyn::fastSin(0.75f) == -1.0f);

TEMPLATE_TEST_CASE("fastSin is exact at the key points", "[math][fast_sin]", float, double) {
  using T = TestType;
  CHECK(xnyn::fastSin(T(0)) == T(0));
  CHECK(xnyn::fastSin(T(0.25)) == T(1));
  CHECK(xnyn::fastSin(T(0.5)) == T(0));
  CHECK(xnyn::fastSin(T(0.75)) == T(-1));
  // Whole cycles and negative phases reduce exactly.
  CHECK(xnyn::fastSin(T(1)) == T(0));
  CHECK(xnyn::fastSin(T(-0.25)) == T(-1));
  CHECK(xnyn::fastSin(T(-0.75)) == T(1));
  CHECK(xnyn::fastSin(T(1000.25)) == T(1));
  CHECK(xnyn::fastSin(T(-1000.75)) == T(1));
}

TEMPLATE_TEST_CASE("fastSin stays within its error bound over one cycle", "[math][fast_sin]", float, double) {
  using T = TestType;
  constexpr int kSteps = 2000000;
  double worst = 0.0;
  for (int i = 0; i < kSteps; ++i) {
    const T x = static_cast<T>(static_cast<double>(i) / kSteps);
    worst = std::fmax(worst, std::abs(static_cast<double>(xnyn::fastSin(x)) - refSin(static_cast<double>(x))));
  }
  INFO("worst " << worst);
  CHECK(worst <= kMaxAbsError<T>);
  // The polynomial bound is reached (5.98e-8), so the documented figure is not loose.
  CHECK(worst > 5.9e-8);
}

TEST_CASE("fastSin<float> stays within its error bound for every float in [0, 1)", "[math][fast_sin]") {
  double worst = 0.0;
  for (float x = 0.0f; x < 1.0f; x = std::nextafter(x, 2.0f)) {
    worst = std::fmax(worst, std::abs(static_cast<double>(xnyn::fastSin(x)) - refSin(static_cast<double>(x))));
  }
  INFO("worst " << worst);
  CHECK(worst <= kMaxAbsError<float>);
}

TEMPLATE_TEST_CASE("fastSin supports phases outside [0, 1)", "[math][fast_sin]", float, double) {
  using T = TestType;
  constexpr int kSteps = 2000000;
  double worst = 0.0;
  for (int i = 0; i <= kSteps; ++i) {
    const T x = static_cast<T>(-1000.0 + 2000.0 * static_cast<double>(i) / kSteps);
    worst = std::fmax(worst, std::abs(static_cast<double>(xnyn::fastSin(x)) - refSin(static_cast<double>(x))));
  }
  INFO("worst " << worst);
  CHECK(worst <= kMaxAbsError<T>);
}

TEST_CASE("fastSin<double> keeps its bound up to 2^61 cycles", "[math][fast_sin]") {
  // Range reduction is exact, so far from zero the error is still that of the polynomial
  // (the input's own resolution is what limits the phase).
  for (const double base : {1.0e6, 1.0e9, 1.0e12, 0x1p40, -0x1p40}) {
    double worst = 0.0;
    for (int i = 0; i < 4096; ++i) {
      const double x = base + static_cast<double>(i) / 4096.0;
      worst = std::fmax(worst, std::abs(xnyn::fastSin(x) - refSin(x)));
    }
    INFO("base " << base << " worst " << worst);
    CHECK(worst <= kMaxAbsError<double>);
  }
  // Exact multiples of a quarter cycle at the top of the range.
  CHECK(xnyn::fastSin(0x1p61) == 0.0);
  CHECK(xnyn::fastSin(-0x1p61) == 0.0);
  CHECK(xnyn::fastSin(0x1p50 + 0.25) == 1.0);
}

TEMPLATE_TEST_CASE("fastSin is odd and half-cycle antisymmetric", "[math][fast_sin]", float, double) {
  using T = TestType;
  constexpr int kSteps = 100000;
  for (int i = 0; i < kSteps; ++i) {
    // Multiples of 2^-17: x + 0.5 is exact in both types, so the symmetry must be bit-exact.
    const T x = static_cast<T>(i) * T(0x1p-17);
    const T y = xnyn::fastSin(x);
    INFO("x " << x);
    CHECK(xnyn::fastSin(-x) == -y);
    CHECK(xnyn::fastSin(x + T(0.5)) == -y);
    CHECK(xnyn::fastSin(T(0.5) - x) == y);
    CHECK(std::abs(y) <= T(1));
  }
}

TEMPLATE_TEST_CASE("fastSin is continuous across the quadrant boundaries", "[math][fast_sin]", float, double) {
  using T = TestType;
  // The slope is at most 2 pi per cycle: a step to the next representable phase can only
  // move the output by 2 pi ulp(x) plus rounding and the approximation error on each side.
  for (const T b : {T(0), T(0.25), T(0.5), T(0.75), T(1), T(-0.25), T(-0.5)}) {
    T x = b;
    for (int i = 0; i < 64; ++i) x = std::nextafter(x, -T(2));
    for (int i = 0; i < 128; ++i) {
      const T next = std::nextafter(x, T(2));
      const double jump = std::abs(static_cast<double>(xnyn::fastSin(next)) - static_cast<double>(xnyn::fastSin(x)));
      const double limit = 2.0 * std::numbers::pi * static_cast<double>(next - x) + 2.0 * kMaxAbsError<T> +
                           4.0 * static_cast<double>(std::numeric_limits<T>::epsilon());
      INFO("boundary " << b << " x " << x);
      CHECK(jump <= limit);
      x = next;
    }
  }
}

TEMPLATE_TEST_CASE("fastSin error has no DC over a cycle", "[math][fast_sin]", float, double) {
  using T = TestType;
  // Uniform grid symmetric under x -> x + 1/2: the half-cycle antisymmetry cancels the
  // error pairwise, so the mean error is zero up to summation rounding.
  constexpr int kSteps = 1 << 20;
  double sum = 0.0;
  for (int i = 0; i < kSteps; ++i) {
    const T x = static_cast<T>(i) / T(kSteps);
    sum += static_cast<double>(xnyn::fastSin(x)) - refSin(static_cast<double>(x));
  }
  const double mean = sum / kSteps;
  INFO("mean " << mean);
  CHECK(std::abs(mean) <= 1e-12);
}

TEMPLATE_TEST_CASE("fastSin propagates NaN and gives non-finite output for infinity", "[math][fast_sin]", float,
                   double) {
  using T = TestType;
  constexpr T kInf = std::numeric_limits<T>::infinity();
  CHECK(std::isnan(xnyn::fastSin(std::numeric_limits<T>::quiet_NaN())));
  CHECK(std::isnan(xnyn::fastSin(-std::numeric_limits<T>::quiet_NaN())));
  // Infinite phase has no meaning; it only has to be visibly non-finite, not a valid sample.
  CHECK_FALSE(std::isfinite(xnyn::fastSin(kInf)));
  CHECK_FALSE(std::isfinite(xnyn::fastSin(-kInf)));
}

namespace {

constexpr double kFmSampleRate = 48000.0;

struct FmSettings {
  double modHz = 50.0;
  double carrierHz = 220.0;
  double deviationHz = 300000.0;  // peak deviation
  double feedback = 0.0;          // modulator self-feedback, scales modHz
  bool carrierFeedback = false;   // feed the carrier output back instead of the modulator's
};

struct FmRun {
  std::vector<double> out;
  double meanModHz = 0.0;
};

// 2-op FM with feedback through the modulator frequency, as used in a synth voice.
template <typename Sin>
FmRun runFm(Sin sinCycles, const FmSettings& s, int samples) {
  xnyn::Phasor modulator;
  xnyn::Phasor carrier;
  modulator.prepare(kFmSampleRate);
  carrier.prepare(kFmSampleRate);
  FmRun r;
  r.out.resize(static_cast<size_t>(samples));
  double fb = 0.0;
  double modHzSum = 0.0;
  for (int i = 0; i < samples; ++i) {
    const double modHz = s.modHz + fb * (s.feedback * s.modHz);
    modulator.setFrequency(modHz);
    const double m = sinCycles(modulator.tick());
    carrier.setFrequency(s.carrierHz + m * s.deviationHz);
    const double c = sinCycles(carrier.tick());
    fb = s.carrierFeedback ? c : m;
    modHzSum += modHz;
    r.out[static_cast<size_t>(i)] = c;
  }
  r.meanModHz = modHzSum / samples;
  return r;
}

double maxDiff(const std::vector<double>& a, const std::vector<double>& b, size_t from, size_t to) {
  double d = 0.0;
  for (size_t i = from; i < to; ++i) d = std::fmax(d, std::abs(a[i] - b[i]));
  return d;
}

const auto kFast = [](double p) { return xnyn::fastSin(p); };
const auto kStd = [](double p) { return std::sin(2.0 * std::numbers::pi * p); };

}  // namespace

TEST_CASE("fastSin matches std::sin in extreme 2-op FM", "[math][fast_sin]") {
  constexpr int kSamples = 10 * 48000;
  constexpr auto kSecond = static_cast<size_t>(kFmSampleRate);
  for (const double modHz : {50.0, 51.3, 200.0}) {
    FmSettings s;
    s.modHz = modHz;
    const auto fast = runFm(kFast, s, kSamples);
    const auto ref = runFm(kStd, s, kSamples);
    // The carrier phase error is the integral of deviation * e(modulator phase): with
    // e <= 6e-8 its first harmonic is ~ 6e-8 * 3e5 / (2 pi 50) = 5.7e-5 cycles, a spurious
    // modulation index of ~3.6e-4 rad. That bounds the output difference too.
    const double first = maxDiff(fast.out, ref.out, 0, kSecond);
    const double last = maxDiff(fast.out, ref.out, kSamples - kSecond, kSamples);
    INFO("modHz " << modHz << " first second " << first << " last second " << last);
    CHECK(first <= 5e-4);
    CHECK(last <= 5e-4);
    // No DC in the error, so no carrier detuning: the difference doesn't grow over time.
    CHECK(last <= 1.5 * first);
  }
}

TEST_CASE("fastSin keeps the modulator pitch in self-feedback FM", "[math][fast_sin]") {
  // A modulator feeding back into its own frequency is an autonomous oscillator: its pitch
  // depends on the waveform shape, so any approximation (even 1-ulp differences in
  // std::sin) makes the waveforms drift apart in phase. What matters is that the pitch,
  // set by the fed-back error, matches.
  constexpr int kSamples = 10 * 48000;
  for (const double feedback : {0.5, 0.9, 1.0}) {
    FmSettings s;
    s.feedback = feedback;
    const auto fast = runFm(kFast, s, kSamples);
    const auto ref = runFm(kStd, s, kSamples);
    const double rel = std::abs(fast.meanModHz / ref.meanModHz - 1.0);
    INFO("feedback " << feedback << " mean modulator Hz " << fast.meanModHz << " vs " << ref.meanModHz);
    // 1e-6 relative is 0.002 cents.
    CHECK(rel <= 1e-6);
  }
}

TEST_CASE("fastSin stays bounded in chaotic carrier-feedback FM", "[math][fast_sin]") {
  // Carrier-to-modulator feedback at this depth is chaotic: trajectories diverge whatever
  // the sine implementation, so only boundedness and long-run statistics are comparable.
  constexpr int kSamples = 10 * 48000;
  FmSettings s;
  s.feedback = 0.1;
  s.carrierFeedback = true;
  const auto fast = runFm(kFast, s, kSamples);
  const auto ref = runFm(kStd, s, kSamples);
  double dc = 0.0;
  for (const double y : fast.out) {
    REQUIRE(std::isfinite(y));
    REQUIRE(std::abs(y) <= 1.0);
    dc += y;
  }
  dc /= kSamples;
  INFO("dc " << dc << " mean modulator Hz " << fast.meanModHz << " vs " << ref.meanModHz);
  CHECK(std::abs(dc) <= 0.01);
  // No drift or offset in the fed-back frequency.
  CHECK(std::abs(fast.meanModHz / ref.meanModHz - 1.0) <= 1e-3);
}
