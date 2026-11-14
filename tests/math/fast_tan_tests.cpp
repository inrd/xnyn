#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <numbers>

#include "xnyn/math/fast_tan.hpp"

namespace {

// Reference tan(pi x) in double, accurate up to the pole: above 1/4 it uses
// tan(pi x) = 1 / tan(pi (1/2 - x)), where 1/2 - x is exact, instead of losing the distance
// to the pole in the rounding of pi x.
double refTanPi(double x) {
  const double a = std::abs(x);
  const double r = a <= 0.25 ? std::tan(std::numbers::pi * a) : 1.0 / std::tan(std::numbers::pi * (0.5 - a));
  return std::copysign(r, x);
}

double relError(double y, double x) { return std::abs(y / refTanPi(x) - 1.0); }

// Documented bounds (header and docs/math/fast_tan.md).
template <typename T>
constexpr double kMaxRelError = sizeof(T) == sizeof(float) ? 4.0e-7 : 4.5e-8;

}  // namespace

// Usable in constant expressions.
static_assert(xnyn::fastTanPi(0.0) == 0.0);
static_assert(xnyn::fastTanPi(-0.25f) < 0.0f);

TEMPLATE_TEST_CASE("fastTanPi is exact at 0 and infinite at the poles", "[math][fast_tan]", float, double) {
  using T = TestType;
  CHECK(xnyn::fastTanPi(T(0)) == T(0));
  CHECK(std::signbit(xnyn::fastTanPi(-T(0))));
  CHECK(xnyn::fastTanPi(T(0.5)) == std::numeric_limits<T>::infinity());
  CHECK(xnyn::fastTanPi(T(-0.5)) == -std::numeric_limits<T>::infinity());
}

TEMPLATE_TEST_CASE("fastTanPi stays within its relative error bound below Nyquist", "[math][fast_tan]", float, double) {
  using T = TestType;
  constexpr int kSteps = 2000000;
  double worst = 0.0;
  for (int i = 1; i < kSteps; ++i) {
    const T x = static_cast<T>(0.5 * static_cast<double>(i) / kSteps);
    worst = std::fmax(worst, relError(static_cast<double>(xnyn::fastTanPi(x)), static_cast<double>(x)));
  }
  INFO("worst " << worst);
  CHECK(worst <= kMaxRelError<T>);
  // The polynomial bound is reached (4.40e-8), so the documented figure is not loose.
  CHECK(worst > 4.3e-8);
}

TEST_CASE("fastTanPi<float> stays within its error bound for every normal float in [0, 1/2)", "[math][fast_tan]") {
  // Subnormal outputs lose relative precision with the format itself (and are flushed
  // under FTZ), so the bound starts at the smallest normal float.
  // Rounding can make adjacent inputs step back, but never by more than the error band
  // allows: a cutoff sweep cannot stutter audibly.
  double worst = 0.0;
  double worstStepBack = 0.0;
  float prev = 0.0f;
  for (float x = std::numeric_limits<float>::min(); x < 0.5f; x = std::nextafter(x, 1.0f)) {
    const float y = xnyn::fastTanPi(x);
    worst = std::fmax(worst, relError(static_cast<double>(y), static_cast<double>(x)));
    worstStepBack = std::fmax(worstStepBack, static_cast<double>(prev) / static_cast<double>(y) - 1.0);
    prev = y;
  }
  INFO("worst " << worst << " worst step back " << worstStepBack);
  CHECK(worst <= kMaxRelError<float>);
  CHECK(worstStepBack <= 2.0 * kMaxRelError<float>);
}

TEMPLATE_TEST_CASE("fastTanPi keeps its bound next to the pole and near zero", "[math][fast_tan]", float, double) {
  using T = TestType;
  // The pole is factored out, not fitted, so the relative error holds however close to it.
  double worst = 0.0;
  T x = T(0.5);
  for (int i = 0; i < 4096; ++i) {
    x = std::nextafter(x, T(0));
    worst = std::fmax(worst, relError(static_cast<double>(xnyn::fastTanPi(x)), static_cast<double>(x)));
  }
  // Tiny inputs: tan(pi x) ~= pi x, the underflow of x^2 to zero doesn't matter.
  for (const T t : {T(1e-3), T(1e-10), T(1e-30), std::numeric_limits<T>::min()}) {
    worst = std::fmax(worst, relError(static_cast<double>(xnyn::fastTanPi(t)), static_cast<double>(t)));
  }
  INFO("worst " << worst);
  CHECK(worst <= kMaxRelError<T>);
}

TEMPLATE_TEST_CASE("fastTanPi is odd and increasing over a grid below Nyquist", "[math][fast_tan]", float, double) {
  using T = TestType;
  constexpr int kSteps = 200000;
  T prev = -std::numeric_limits<T>::infinity();
  for (int i = -kSteps + 1; i < kSteps; ++i) {
    const T x = static_cast<T>(0.5 * static_cast<double>(i) / kSteps);
    const T y = xnyn::fastTanPi(x);
    INFO("x " << x);
    CHECK(xnyn::fastTanPi(-x) == -y);
    CHECK(y > prev);
    prev = y;
  }
}

TEMPLATE_TEST_CASE("fastTanPi propagates NaN", "[math][fast_tan]", float, double) {
  using T = TestType;
  CHECK(std::isnan(xnyn::fastTanPi(std::numeric_limits<T>::quiet_NaN())));
  CHECK(std::isnan(xnyn::fastTanPi(-std::numeric_limits<T>::quiet_NaN())));
}

TEMPLATE_TEST_CASE("fastTanPi prewarps audio cutoffs with negligible pitch error", "[math][fast_tan]", float, double) {
  using T = TestType;
  // A bilinear filter's cutoff is fc = fs / pi * atan(g): map g back to Hz and compare.
  for (const double fs : {44100.0, 48000.0, 96000.0, 192000.0}) {
    double worstCents = 0.0;
    for (double fc = 20.0; fc < 0.499 * fs; fc *= 1.01) {
      const T g = xnyn::fastTanPi(static_cast<T>(fc / fs));
      const double realized = fs / std::numbers::pi * std::atan(static_cast<double>(g));
      // The input's own rounding to T is not the approximation's error: compare against it.
      const double wanted = static_cast<double>(static_cast<T>(fc / fs)) * fs;
      worstCents = std::fmax(worstCents, std::abs(1200.0 * std::log2(realized / wanted)));
    }
    INFO("fs " << fs << " worst cents " << worstCents);
    CHECK(worstCents <= (sizeof(T) == sizeof(float) ? 7e-4 : 8e-5));
  }
}
