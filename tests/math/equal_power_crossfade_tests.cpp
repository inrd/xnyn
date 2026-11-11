#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <numbers>

#include "xnyn/math/equal_power_crossfade.hpp"

namespace {

// Documented gain error bound (header and docs/math/equal_power_crossfade.md).
template <typename T>
constexpr double kMaxGainError = sizeof(T) == sizeof(float) ? 2.1e-7 : 6.0e-8;
template <typename T>
constexpr double kMaxPowerError = sizeof(T) == sizeof(float) ? 6.0e-7 : 1.7e-7;

}  // namespace

// Usable in constant expressions.
static_assert(xnyn::equalPowerCrossfade(2.0, 6.0, 0.0) == 2.0);
static_assert(xnyn::equalPowerCrossfade(2.0f, 6.0f, 1.0f) == 6.0f);

TEMPLATE_TEST_CASE("equalPowerCrossfade is exact at the endpoints", "[math][equal_power_crossfade]", float, double) {
  using T = TestType;
  for (const T a : {T(0), T(-0.7), T(0.3), T(123.456)}) {
    for (const T b : {T(0), T(1), T(-5.5)}) {
      CHECK(xnyn::equalPowerCrossfade(a, b, T(0)) == a);
      CHECK(xnyn::equalPowerCrossfade(a, b, T(1)) == b);
    }
  }
}

TEMPLATE_TEST_CASE("equalPowerCrossfade gains stay within their error bound", "[math][equal_power_crossfade]", float,
                   double) {
  using T = TestType;
  constexpr int kSteps = 1000000;
  double worstGain = 0.0;
  double worstPower = 0.0;
  for (int i = 0; i <= kSteps; ++i) {
    const T t = static_cast<T>(static_cast<double>(i) / kSteps);
    const double td = static_cast<double>(t);
    const auto gainA = static_cast<double>(xnyn::equalPowerCrossfade(T(1), T(0), t));
    const auto gainB = static_cast<double>(xnyn::equalPowerCrossfade(T(0), T(1), t));
    worstGain = std::fmax(worstGain, std::abs(gainA - std::cos(td * std::numbers::pi / 2.0)));
    worstGain = std::fmax(worstGain, std::abs(gainB - std::sin(td * std::numbers::pi / 2.0)));
    worstPower = std::fmax(worstPower, std::abs(gainA * gainA + gainB * gainB - 1.0));
    REQUIRE(gainA >= 0.0);
    REQUIRE(gainB >= 0.0);
  }
  INFO("worst gain " << worstGain << " worst power " << worstPower);
  CHECK(worstGain <= kMaxGainError<T>);
  CHECK(worstPower <= kMaxPowerError<T>);
}

TEMPLATE_TEST_CASE("equalPowerCrossfade is symmetric", "[math][equal_power_crossfade]", float, double) {
  using T = TestType;
  // Multiples of 2^-16: 1 - t is exact, so the symmetry must be bit-exact.
  for (int i = 0; i <= 1 << 16; ++i) {
    const T t = static_cast<T>(i) * T(0x1p-16);
    INFO("t " << t);
    CHECK(xnyn::equalPowerCrossfade(T(1), T(0), t) == xnyn::equalPowerCrossfade(T(0), T(1), T(1) - t));
  }
  // Both gains equal at the midpoint: -3 dB each.
  const T mid = xnyn::equalPowerCrossfade(T(1), T(0), T(0.5));
  CHECK(xnyn::equalPowerCrossfade(T(0), T(1), T(0.5)) == mid);
  CHECK(std::abs(static_cast<double>(mid) - std::numbers::sqrt2 / 2.0) <= kMaxGainError<T>);
}

TEMPLATE_TEST_CASE("equalPowerCrossfade keeps a == b at unit power gain", "[math][equal_power_crossfade]", float,
                   double) {
  using T = TestType;
  // For identical inputs the output is a * (cos + sin): up to +3 dB at the midpoint, never
  // below a. This is the documented reason to use lerp for correlated signals.
  for (int i = 0; i <= 100; ++i) {
    const T t = static_cast<T>(i) / T(100);
    const T y = xnyn::equalPowerCrossfade(T(1), T(1), t);
    CHECK(y >= T(1) - T(kMaxGainError<T>));
    CHECK(y <= T(std::numbers::sqrt2) + T(2 * kMaxGainError<T>));
  }
}

TEMPLATE_TEST_CASE("equalPowerCrossfade clamps t to [0, 1]", "[math][equal_power_crossfade]", float, double) {
  using T = TestType;
  constexpr T kInf = std::numeric_limits<T>::infinity();
  for (const T t : {T(-0.001), T(-1), T(-3.5), T(-1e30), -kInf}) {
    INFO("t " << t);
    CHECK(xnyn::equalPowerCrossfade(T(2), T(6), t) == T(2));
  }
  for (const T t : {T(1.001), T(2), T(4.5), T(1e30), kInf}) {
    INFO("t " << t);
    CHECK(xnyn::equalPowerCrossfade(T(2), T(6), t) == T(6));
  }
}

TEMPLATE_TEST_CASE("equalPowerCrossfade propagates NaN", "[math][equal_power_crossfade]", float, double) {
  using T = TestType;
  constexpr T kNaN = std::numeric_limits<T>::quiet_NaN();
  CHECK(std::isnan(xnyn::equalPowerCrossfade(T(1), T(1), kNaN)));
  CHECK(std::isnan(xnyn::equalPowerCrossfade(kNaN, T(1), T(0.5))));
  CHECK(std::isnan(xnyn::equalPowerCrossfade(T(1), kNaN, T(0.5))));
}
