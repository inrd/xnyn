#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <random>

#include "xnyn/math/fast_pow.hpp"

namespace {

// Documented bounds (header and docs/math/fast_pow.md), relative.
template <typename T>
constexpr bool kIsFloat = sizeof(T) == sizeof(float);
template <typename T>
constexpr double kExp2Error = kIsFloat<T> ? 1.0e-7 : 2.0e-9;
template <typename T>
constexpr double kLog2Error = kIsFloat<T> ? 2.4e-7 : 2.7e-8;
// fastPow: kExp2Error + kPowSlope * |y log2 x|.
template <typename T>
constexpr double kPowSlope = kIsFloat<T> ? 1.7e-7 : 1.9e-8;

template <typename T>
double exp2RelError(T x) {
  const double ref = std::exp2(static_cast<double>(x));
  return std::abs(static_cast<double>(xnyn::fastExp2(x)) / ref - 1.0);
}

// Relative to log2(x); x = 1 is exact and checked separately.
template <typename T>
double log2RelError(T x) {
  const double ref = std::log2(static_cast<double>(x));
  return std::abs(static_cast<double>(xnyn::fastLog2(x)) - ref) / std::abs(ref);
}

}  // namespace

// Usable in constant expressions.
static_assert(xnyn::fastExp2(0.0) == 1.0);
static_assert(xnyn::fastExp2(10.0f) == 1024.0f);
static_assert(xnyn::fastLog2(1.0) == 0.0);
static_assert(xnyn::fastLog2(0.125f) == -3.0f);
static_assert(xnyn::fastPow(4.0, 1.5) == 8.0);

TEMPLATE_TEST_CASE("fastExp2 is exact for integers", "[math][fast_pow]", float, double) {
  using T = TestType;
  constexpr int kMaxExp = std::numeric_limits<T>::max_exponent - 1;
  constexpr int kMinExp = std::numeric_limits<T>::min_exponent - 1;
  for (int n = kMinExp; n <= kMaxExp; ++n) {
    INFO("n " << n);
    CHECK(xnyn::fastExp2(static_cast<T>(n)) == std::ldexp(T(1), n));
  }
}

TEST_CASE("fastExp2<float> stays within its error bound for every float in [-1/2, 1/2]", "[math][fast_pow]") {
  // 2^x = 2^n 2^f with f = x - n exact and the scaling exact: every reduced f of a larger x
  // is also a float in [-1/2, 1/2], so this covers the whole normal range.
  double worst = 0.0;
  for (float x = -0.5f; x <= 0.5f; x = std::nextafter(x, 1.0f)) worst = std::fmax(worst, exp2RelError(x));
  INFO("worst " << worst);
  CHECK(worst <= kExp2Error<float>);
}

TEMPLATE_TEST_CASE("fastExp2 stays within its error bound over its normal range", "[math][fast_pow]", float, double) {
  using T = TestType;
  constexpr int kSteps = 2000000;
  constexpr double kLow = std::numeric_limits<T>::min_exponent - 1;
  constexpr double kHigh = std::numeric_limits<T>::max_exponent - 0.5;
  double worst = 0.0;
  for (int i = 0; i < kSteps; ++i) {
    const T x = static_cast<T>(kLow + (kHigh - kLow) * static_cast<double>(i) / kSteps);
    worst = std::fmax(worst, exp2RelError(x));
  }
  INFO("worst " << worst);
  CHECK(worst <= kExp2Error<T>);
  if constexpr (!kIsFloat<T>) {
    // The polynomial bound is reached (1.97e-9), so the documented figure is not loose.
    CHECK(worst > 1.9e-9);
  }
}

TEMPLATE_TEST_CASE("fastExp2 handles out-of-range inputs", "[math][fast_pow]", float, double) {
  using T = TestType;
  constexpr T kInf = std::numeric_limits<T>::infinity();
  constexpr T kTop = T(std::numeric_limits<T>::max_exponent) - T(0.5);
  constexpr T kBottom = T(std::numeric_limits<T>::min_exponent) - T(1.5);
  // Finite up to the overflow threshold, including the last value below it (rounding there
  // must not produce an infinite scale).
  const T belowTop = std::nextafter(kTop, T(0));
  CHECK(std::isfinite(xnyn::fastExp2(belowTop)));
  CHECK(exp2RelError(belowTop) <= kExp2Error<T>);
  CHECK(xnyn::fastExp2(kTop) == kInf);
  CHECK(xnyn::fastExp2(T(1e6)) == kInf);
  CHECK(xnyn::fastExp2(kInf) == kInf);
  // Gradual underflow down to the threshold, then +0.
  const T denormal = xnyn::fastExp2(kBottom);
  CHECK(denormal > T(0));
  CHECK(denormal < std::numeric_limits<T>::min());
  CHECK(xnyn::fastExp2(std::nextafter(kBottom, T(-2000))) == T(0));
  CHECK(xnyn::fastExp2(T(-1e6)) == T(0));
  CHECK(xnyn::fastExp2(-kInf) == T(0));
  CHECK(std::isnan(xnyn::fastExp2(std::numeric_limits<T>::quiet_NaN())));
}

TEMPLATE_TEST_CASE("fastExp2 is monotonic", "[math][fast_pow]", float, double) {
  using T = TestType;
  // Around the reduction boundaries x = n + 1/2, where the polynomial switches interval.
  for (const T b : {T(-10.5), T(-0.5), T(0.5), T(3.5), T(40.5)}) {
    T x = b;
    for (int i = 0; i < 64; ++i) x = std::nextafter(x, T(-1000));
    T prev = xnyn::fastExp2(x);
    for (int i = 0; i < 128; ++i) {
      x = std::nextafter(x, T(1000));
      const T y = xnyn::fastExp2(x);
      INFO("x " << x);
      CHECK(y >= prev);
      prev = y;
    }
  }
}

TEMPLATE_TEST_CASE("fastLog2 is exact at powers of two", "[math][fast_pow]", float, double) {
  using T = TestType;
  constexpr int kMinExp = std::numeric_limits<T>::min_exponent - std::numeric_limits<T>::digits;
  constexpr int kMaxExp = std::numeric_limits<T>::max_exponent - 1;
  for (int n = kMinExp; n <= kMaxExp; ++n) {
    INFO("n " << n);
    CHECK(xnyn::fastLog2(std::ldexp(T(1), n)) == static_cast<T>(n));
  }
}

TEST_CASE("fastLog2<float> stays within its error bound for every float in [1/2, 2)", "[math][fast_pow]") {
  // Covers every mantissa in both reduction intervals. The exponent is added exactly.
  double worst = 0.0;
  for (float x = 0.5f; x < 2.0f; x = std::nextafter(x, 4.0f)) {
    if (x != 1.0f) worst = std::fmax(worst, log2RelError(x));
  }
  INFO("worst " << worst);
  CHECK(worst <= kLog2Error<float>);
}

TEMPLATE_TEST_CASE("fastLog2 stays within its error bound over the positive range", "[math][fast_pow]", float, double) {
  using T = TestType;
  std::mt19937_64 rng(1);
  // Log-uniform over the whole range, denormals included, plus a dense band around 1 where
  // log2 x -> 0 and only a relative bound is meaningful.
  constexpr double kLow = std::numeric_limits<T>::min_exponent - std::numeric_limits<T>::digits;
  constexpr double kHigh = std::numeric_limits<T>::max_exponent;
  std::uniform_real_distribution<double> wide(kLow, kHigh);
  std::uniform_real_distribution<double> nearOne(-0.01, 0.01);
  double worst = 0.0;
  for (int i = 0; i < 2000000; ++i) {
    const T x = static_cast<T>(std::exp2((i & 1) != 0 ? wide(rng) : nearOne(rng)));
    if (x > T(0) && x != T(1) && std::isfinite(x)) worst = std::fmax(worst, log2RelError(x));
  }
  INFO("worst " << worst);
  CHECK(worst <= kLog2Error<T>);
  if constexpr (!kIsFloat<T>) {
    // The polynomial bound is reached (2.61e-8).
    CHECK(worst > 2.5e-8);
  }
}

TEMPLATE_TEST_CASE("fastLog2 handles zero, negative and non-finite inputs", "[math][fast_pow]", float, double) {
  using T = TestType;
  constexpr T kInf = std::numeric_limits<T>::infinity();
  CHECK(xnyn::fastLog2(T(0)) == -kInf);
  CHECK(xnyn::fastLog2(-T(0)) == -kInf);
  CHECK(xnyn::fastLog2(kInf) == kInf);
  CHECK(std::isnan(xnyn::fastLog2(T(-1))));
  CHECK(std::isnan(xnyn::fastLog2(-std::numeric_limits<T>::denorm_min())));
  CHECK(std::isnan(xnyn::fastLog2(-kInf)));
  CHECK(std::isnan(xnyn::fastLog2(std::numeric_limits<T>::quiet_NaN())));
  // Largest and smallest positive values stay finite and accurate.
  CHECK(log2RelError(std::numeric_limits<T>::max()) <= kLog2Error<T>);
  CHECK(log2RelError(std::numeric_limits<T>::min()) <= kLog2Error<T>);
  CHECK(log2RelError(std::numeric_limits<T>::denorm_min()) <= kLog2Error<T>);
}

TEMPLATE_TEST_CASE("fastLog2 is monotonic across the reduction boundary", "[math][fast_pow]", float, double) {
  using T = TestType;
  // The mantissa interval switches at sqrt(2) * 2^k.
  for (const T b :
       {std::numbers::sqrt2_v<T>, std::numbers::sqrt2_v<T> / T(2), std::numbers::sqrt2_v<T> * T(1024), T(1)}) {
    T x = b;
    for (int i = 0; i < 64; ++i) x = std::nextafter(x, T(0));
    T prev = xnyn::fastLog2(x);
    for (int i = 0; i < 128; ++i) {
      x = std::nextafter(x, T(1e30));
      const T y = xnyn::fastLog2(x);
      INFO("x " << x);
      CHECK(y >= prev);
      prev = y;
    }
  }
}

TEMPLATE_TEST_CASE("fastPow stays within its error bound", "[math][fast_pow]", float, double) {
  using T = TestType;
  std::mt19937_64 rng(2);
  std::uniform_real_distribution<double> exponent(-20.0, 20.0);
  std::uniform_real_distribution<double> wideBase(-12.0, 12.0);
  std::uniform_real_distribution<double> nearOne(-0.01, 0.01);
  double worst = 0.0;  // excess error over the documented bound, relative to it
  for (int i = 0; i < 2000000; ++i) {
    const T y = static_cast<T>(exponent(rng));
    const T x = static_cast<T>(std::exp2((i & 1) != 0 ? wideBase(rng) : nearOne(rng)));
    const double ref = std::pow(static_cast<double>(x), static_cast<double>(y));
    const double yLog = std::abs(static_cast<double>(y) * std::log2(static_cast<double>(x)));
    // Keep the result a normal number, where the exp2 bound applies.
    if (yLog > -(std::numeric_limits<T>::min_exponent - 1)) continue;
    const double err = std::abs(static_cast<double>(xnyn::fastPow(x, y)) / ref - 1.0);
    worst = std::fmax(worst, err / (kExp2Error<T> + kPowSlope<T> * yLog));
  }
  INFO("worst error / bound " << worst);
  CHECK(worst <= 1.0);
}

TEMPLATE_TEST_CASE("fastPow typical DSP conversions", "[math][fast_pow]", float, double) {
  using T = TestType;
  // dB to gain over [-120, 24] dB: |y log2 x| <= 20 there.
  const double tol = kExp2Error<T> + kPowSlope<T> * 20.0;
  for (int db = -120; db <= 24; ++db) {
    const T gain = xnyn::fastPow(T(10), static_cast<T>(db) / T(20));
    INFO("dB " << db);
    CHECK(std::abs(static_cast<double>(gain) / std::pow(10.0, db / 20.0) - 1.0) <= tol);
  }
  // Equal-tempered semitones go through fastExp2 directly: octaves are exact.
  for (int semis = -48; semis <= 48; semis += 12) {
    CHECK(xnyn::fastExp2(static_cast<T>(semis) / T(12)) == std::ldexp(T(1), semis / 12));
  }
}

TEMPLATE_TEST_CASE("fastPow edge cases", "[math][fast_pow]", float, double) {
  using T = TestType;
  constexpr T kInf = std::numeric_limits<T>::infinity();
  constexpr T kNaN = std::numeric_limits<T>::quiet_NaN();
  // Exact cases.
  for (const T x : {T(0), T(1e-30), T(0.3), T(1), T(7), kInf, kNaN}) CHECK(xnyn::fastPow(x, T(0)) == T(1));
  for (const T y : {T(-100), T(-0.5), T(0.5), T(3), T(100)}) CHECK(xnyn::fastPow(T(1), y) == T(1));
  CHECK(xnyn::fastPow(T(2), T(10)) == T(1024));
  CHECK(xnyn::fastPow(T(0.25), T(-3)) == T(64));
  CHECK(xnyn::fastPow(T(64), T(0.5)) == T(8));
  // Zero and infinity.
  CHECK(xnyn::fastPow(T(0), T(2)) == T(0));
  CHECK(xnyn::fastPow(T(0), T(-2)) == kInf);
  CHECK(xnyn::fastPow(kInf, T(2)) == kInf);
  CHECK(xnyn::fastPow(kInf, T(-2)) == T(0));
  CHECK(xnyn::fastPow(T(10), T(1000)) == kInf);
  CHECK(xnyn::fastPow(T(10), T(-1000)) == T(0));
  // Negative bases are not supported, even with integer exponents.
  CHECK(std::isnan(xnyn::fastPow(T(-2), T(2))));
  CHECK(std::isnan(xnyn::fastPow(T(-2), T(0.5))));
  CHECK(std::isnan(xnyn::fastPow(kNaN, T(2))));
  CHECK(std::isnan(xnyn::fastPow(T(2), kNaN)));
}
