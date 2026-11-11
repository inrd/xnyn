#include <catch2/catch_approx.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <complex>
#include <limits>
#include <numbers>
#include <random>
#include <vector>

#include "xnyn/filter/dc_blocker.hpp"

using Catch::Approx;

namespace {

constexpr double kPi = std::numbers::pi;

// Analytic magnitude of H(z) = (1 - z^-1) / (1 - R z^-1) at f Hz.
double analyticMagnitude(double fc, double f, double sr) {
  const double r = std::exp(-2.0 * kPi * fc / sr);
  const std::complex<double> z1 = std::polar(1.0, -2.0 * kPi * f / sr);
  return std::abs((1.0 - z1) / (1.0 - r * z1));
}

// Steady-state gain measured by driving a sine and projecting the output onto the input
// frequency after the transient has died out.
template <typename T>
double measureMagnitude(double fc, double f, double sr) {
  xnyn::DcBlocker<T> dc;
  dc.prepare(sr);
  dc.setCutoff(static_cast<T>(fc));
  const double w = 2.0 * kPi * f / sr;
  // Settle for 20 time constants (gain e^-20 on the transient), then measure whole cycles.
  const long settle = static_cast<long>(20.0 * sr / (2.0 * kPi * fc)) + 1;
  for (long i = 0; i < settle; ++i) (void)dc.tick(static_cast<T>(std::sin(w * static_cast<double>(i))));
  const long cycles = std::max(1L, static_cast<long>(f));  // ~1 second
  const long n = static_cast<long>(std::round(static_cast<double>(cycles) * sr / f));
  std::complex<double> acc = 0.0;
  for (long i = 0; i < n; ++i) {
    const double ph = w * static_cast<double>(settle + i);
    const double y = static_cast<double>(dc.tick(static_cast<T>(std::sin(ph))));
    acc += y * std::polar(1.0, -ph);
  }
  return 2.0 * std::abs(acc) / static_cast<double>(n);
}

// Reference: the common fixed-coefficient form, double state and R = 0.9997 regardless
// of the sample rate (cutoff ~2.1 Hz at 44.1 kHz).
template <typename T>
class ReferenceDcBlocker {
 public:
  T tick(T x) {
    const auto y = (double)x - mX1 + (mY1 * 0.9997);
    mX1 = (double)x;
    mY1 = y;
    return (T)y;
  }

 private:
  double mX1 = 0.0;
  double mY1 = 0.0;
};

}  // namespace

TEMPLATE_TEST_CASE("DcBlocker removes a constant input", "[filter][dc_blocker]", float, double) {
  using T = TestType;
  constexpr double sr = 48000.0;
  for (const double fc : {1.0, 5.0, 10.0, 100.0}) {
    INFO("fc " << fc);
    xnyn::DcBlocker<T> dc;
    dc.prepare(sr);
    dc.setCutoff(static_cast<T>(fc));
    // Step response is exactly R^n: it reaches 1e-3 after ln(1000) time constants.
    const double tau = sr / (2.0 * kPi * fc);
    const long bound = static_cast<long>(std::ceil(std::log(1000.0) * tau));
    T y = 0;
    T prev = 2;
    for (long i = 0; i <= bound; ++i) {
      y = dc.tick(T(1));
      if (!(y <= prev && y > T(0))) FAIL("not monotone at sample " << i << ": " << y);  // no overshoot
      prev = y;
    }
    CHECK(static_cast<double>(y) <= 1e-3);  // y = R^bound
    CHECK(static_cast<double>(y) > 0.98e-3);
    for (long i = 0; i < 40 * static_cast<long>(tau); ++i) y = dc.tick(T(1));
    CHECK(std::fabs(static_cast<double>(y)) < 1e-12);
  }
}

TEST_CASE("DcBlocker step response matches R^n", "[filter][dc_blocker]") {
  constexpr double sr = 44100.0;
  constexpr double fc = 7.0;
  xnyn::DcBlocker<double> dc;
  dc.prepare(sr);
  dc.setCutoff(fc);
  const double r = std::exp(-2.0 * kPi * fc / sr);
  for (int n = 0; n < 1000; ++n) CHECK(dc.tick(1.0) == Approx(std::pow(r, n)).epsilon(1e-12));
}

TEMPLATE_TEST_CASE("DcBlocker magnitude matches the analytic one-pole response", "[filter][dc_blocker]", float,
                   double) {
  using T = TestType;
  constexpr double sr = 48000.0;
  constexpr double fc = 10.0;
  for (const double f : {2.0, 5.0, 10.0, 20.0, 100.0, 1000.0, 10000.0}) {
    INFO("f " << f);
    const double expected = analyticMagnitude(fc, f, sr);
    CHECK(measureMagnitude<T>(fc, f, sr) == Approx(expected).epsilon(1e-3));
  }
  // -3 dB within 0.1% of fc: the bilinear-free pole mapping is accurate at low fc.
  CHECK(20.0 * std::log10(analyticMagnitude(fc, fc, sr)) == Approx(-3.0103).margin(0.01));
  // Well above fc the filter is transparent; the one-pole rises slightly above unity
  // towards 2 / (1 + R) at Nyquist (+0.005 dB at 1 kHz here).
  CHECK(20.0 * std::log10(measureMagnitude<T>(fc, 1000.0, sr)) == Approx(0.0).margin(0.01));
}

TEST_CASE("DcBlocker cutoff is the same at every sample rate", "[filter][dc_blocker]") {
  constexpr double fc = 5.0;
  for (const double sr : {44100.0, 48000.0, 96000.0}) {
    INFO("sr " << sr);
    CHECK(20.0 * std::log10(measureMagnitude<float>(fc, fc, sr)) == Approx(-3.0103).margin(0.02));
    CHECK(measureMagnitude<float>(fc, 1.0, sr) == Approx(analyticMagnitude(fc, 1.0, sr)).epsilon(2e-3));
  }

  // A fixed 0.9997 coefficient, as in the reference, moves the cutoff with fs.
  const auto fixedCutoff = [](double sr) { return -std::log(0.9997) * sr / (2.0 * kPi); };
  CHECK(fixedCutoff(44100.0) == Approx(2.1056).epsilon(1e-3));
  CHECK(fixedCutoff(96000.0) == Approx(4.5836).epsilon(1e-3));
}

TEST_CASE("DcBlocker matches the fixed-coefficient reference", "[filter][dc_blocker]") {
  constexpr double sr = 44100.0;
  // Cutoff that maps back to R = 0.9997 at 44.1 kHz.
  const double fc = -std::log(0.9997) * sr / (2.0 * kPi);

  xnyn::DcBlocker<float> dc;
  dc.prepare(sr);
  dc.setCutoff(static_cast<float>(fc));
  ReferenceDcBlocker<float> ref;

  std::mt19937 rng(42);
  std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
  float maxErr = 0.0f;
  for (int i = 0; i < 441000; ++i) {
    // Asymmetric drive into a saturator: the case this block exists for.
    const float x = std::tanh(3.0f * dist(rng) + 0.4f);
    maxErr = std::max(maxErr, std::fabs(dc.tick(x) - ref.tick(x)));
  }
  // R differs only by rounding fc to float, and the state is double in both.
  CHECK(maxErr < 1e-5f);
}

TEMPLATE_TEST_CASE("DcBlocker is stable and drift-free over long runs", "[filter][dc_blocker]", float, double) {
  using T = TestType;
  struct Case {
    double sr;
    double fc;
  };
  for (const auto [sr, fc] : {Case{192000.0, 1.0}, Case{96000.0, 5.0}, Case{44100.0, 10.0}, Case{48000.0, 0.0}}) {
    INFO("sr " << sr << " fc " << fc);
    xnyn::DcBlocker<T> dc;
    dc.prepare(sr);
    dc.setCutoff(static_cast<T>(fc));
    std::mt19937 rng(7);
    std::uniform_real_distribution<T> dist(T(-0.5), T(0.5));
    const long n = 4'000'000;
    double sum = 0.0;
    double maxAbs = 0.0;
    double maxPassErr = 0.0;
    for (long i = 0; i < n; ++i) {
      const T x = T(0.25) + dist(rng);  // DC offset plus noise
      const T y = dc.tick(x);
      if (i >= n / 2) sum += static_cast<double>(y);
      maxAbs = std::max(maxAbs, std::fabs(static_cast<double>(y)));
      maxPassErr = std::max(maxPassErr, std::fabs(static_cast<double>(y - x)));
    }
    CHECK(std::isfinite(maxAbs));
    CHECK(maxAbs < 2.0);
    if (fc == 0.0) {
      // R = 1 telescopes to pass-through and must not random-walk away from it.
      if constexpr (std::same_as<T, float>)
        CHECK(maxPassErr == 0.0);
      else
        CHECK(maxPassErr < 1e-12);
    } else {
      // Residual offset of the second half: noise mean (~3e-4 for 2M samples) plus
      // filter rounding, far below what a float-state recursion would leave.
      CHECK(std::fabs(sum / static_cast<double>(n / 2)) < 2e-3);
    }
  }
}

TEST_CASE("DcBlocker float output tracks a double reference at low cutoffs", "[filter][dc_blocker]") {
  // Measures the precision decision: with double state, float I/O only adds output
  // rounding (~-140 dB), even at 1 Hz / 192 kHz where R is 3e-5 below 1.
  for (const double sr : {44100.0, 192000.0}) {
    INFO("sr " << sr);
    xnyn::DcBlocker<float> f;
    xnyn::DcBlocker<long double> ref;
    f.prepare(sr);
    ref.prepare(sr);
    f.setCutoff(1.0f);
    ref.setCutoff(1.0L);
    std::mt19937 rng(3);
    std::uniform_real_distribution<float> dist(-0.5f, 0.5f);
    double errSq = 0.0;
    double refSq = 0.0;
    for (int i = 0; i < 1'000'000; ++i) {
      const float x = 0.3f + dist(rng);
      const long double yr = ref.tick(x);
      const double e = static_cast<double>(f.tick(x)) - static_cast<double>(yr);
      errSq += e * e;
      refSq += static_cast<double>(yr * yr);
    }
    CHECK(10.0 * std::log10(errSq / refSq) < -130.0);
  }
}

TEMPLATE_TEST_CASE("DcBlocker first sample after prepare and reset passes through", "[filter][dc_blocker]", float,
                   double) {
  using T = TestType;
  xnyn::DcBlocker<T> dc;
  dc.prepare(48000.0);
  CHECK(dc.tick(T(0.75)) == T(0.75));
  for (int i = 0; i < 100; ++i) (void)dc.tick(T(0.3));
  dc.reset();
  CHECK(dc.tick(T(-0.5)) == T(-0.5));
  for (int i = 0; i < 100; ++i) (void)dc.tick(T(0.3));
  dc.prepare(48000.0);  // prepare resets too
  CHECK(dc.tick(T(0.125)) == T(0.125));
}

TEMPLATE_TEST_CASE("DcBlocker zero input gives exact zero output", "[filter][dc_blocker]", float, double) {
  using T = TestType;
  xnyn::DcBlocker<T> dc;
  dc.prepare(48000.0);
  for (int i = 0; i < 1000; ++i) CHECK(dc.tick(T(0)) == T(0));
}

TEMPLATE_TEST_CASE("DcBlocker cutoff edge cases", "[filter][dc_blocker]", float, double) {
  using T = TestType;
  constexpr double sr = 48000.0;
  std::vector<T> in;
  std::mt19937 rng(11);
  std::uniform_real_distribution<T> dist(T(-1), T(1));
  for (int i = 0; i < 10000; ++i) in.push_back(T(0.5) + dist(rng));

  SECTION("zero and negative cutoffs pass through") {
    for (const T fc : {T(0), T(-10), -std::numeric_limits<T>::infinity(), std::numeric_limits<T>::quiet_NaN()}) {
      INFO("fc " << fc);
      xnyn::DcBlocker<T> dc;
      dc.prepare(sr);
      dc.setCutoff(fc);
      for (const T x : in) {
        const T y = dc.tick(x);
        if constexpr (std::same_as<T, float>)
          REQUIRE(y == x);
        else
          REQUIRE(y == Approx(x).margin(1e-13));
      }
    }
  }

  SECTION("cutoffs at and above Nyquist clamp to fs / 2 and stay bounded") {
    for (const T fc : {T(24000), T(1e6), std::numeric_limits<T>::infinity(), std::numeric_limits<T>::max()}) {
      INFO("fc " << fc);
      xnyn::DcBlocker<T> dc;
      dc.prepare(sr);
      dc.setCutoff(fc);
      // h[0] = 1, h[n] = (R - 1) R^(n-1): sum |h| = 2, so |y| <= 2 max|x| = 3.
      for (const T x : in) REQUIRE(std::fabs(dc.tick(x)) <= T(3));
      // Same response as asking for exactly fs / 2.
      xnyn::DcBlocker<T> nyq;
      nyq.prepare(sr);
      nyq.setCutoff(T(24000));
      dc.reset();
      for (const T x : in) REQUIRE(dc.tick(x) == nyq.tick(x));
    }
  }

  SECTION("cutoff is kept in Hz across prepare") {
    xnyn::DcBlocker<T> a;
    a.setCutoff(T(20));
    a.prepare(sr);
    xnyn::DcBlocker<T> b;
    b.prepare(sr);
    b.setCutoff(T(20));
    for (const T x : in) REQUIRE(a.tick(x) == b.tick(x));
  }

  SECTION("default cutoff is 5 Hz") {
    CHECK(xnyn::DcBlocker<T>::kDefaultCutoffHz == T(5));
    xnyn::DcBlocker<T> a;
    a.prepare(sr);
    xnyn::DcBlocker<T> b;
    b.prepare(sr);
    b.setCutoff(T(5));
    for (const T x : in) REQUIRE(a.tick(x) == b.tick(x));
  }
}

TEMPLATE_TEST_CASE("DcBlocker without prepare is a finite pass-through", "[filter][dc_blocker]", float, double) {
  using T = TestType;
  xnyn::DcBlocker<T> dc;
  for (const T fc : {T(5), T(0), T(-1), T(1e30)}) {
    INFO("fc " << fc);
    dc.setCutoff(fc);
    std::mt19937 rng(5);
    std::uniform_real_distribution<T> dist(T(-1), T(1));
    for (int i = 0; i < 1'000'000; ++i) {
      const T x = T(0.5) + dist(rng);
      const T y = dc.tick(x);
      if constexpr (std::same_as<T, float>) {
        if (y != x) FAIL("sample " << i << ": " << y << " != " << x);
      } else {
        if (!(std::fabs(y - x) < 1e-12)) FAIL("sample " << i << ": " << y << " != " << x);
      }
    }
  }
}

TEMPLATE_TEST_CASE("DcBlocker NaN input poisons the state until reset", "[filter][dc_blocker]", float, double) {
  using T = TestType;
  xnyn::DcBlocker<T> dc;
  dc.prepare(48000.0);
  (void)dc.tick(T(0.5));
  CHECK(std::isnan(dc.tick(std::numeric_limits<T>::quiet_NaN())));
  CHECK(std::isnan(dc.tick(T(0.5))));
  CHECK(std::isnan(dc.tick(T(0.5))));
  dc.reset();
  CHECK(dc.tick(T(0.5)) == T(0.5));
}
