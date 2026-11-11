#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>

#include "xnyn/math/decibels.hpp"

namespace {

// Documented bounds (header and docs/math/decibels.md).
template <typename T>
constexpr bool kIsFloat = sizeof(T) == sizeof(float);
// dbToGain relative error: kGainError + kGainSlope * |dB|.
template <typename T>
constexpr double kGainError = kIsFloat<T> ? 1.0e-7 : 2.0e-9;
template <typename T>
constexpr double kGainSlope = kIsFloat<T> ? 8.0e-9 : 0.0;
// gainToDb error relative to the dB value.
template <typename T>
constexpr double kDbError = kIsFloat<T> ? 3.0e-7 : 2.8e-8;

double refGain(double db) { return std::pow(10.0, db / 20.0); }
double refDb(double gain) { return 20.0 * std::log10(std::abs(gain)); }

}  // namespace

// Usable in constant expressions.
static_assert(xnyn::dbToGain(0.0) == 1.0);
static_assert(xnyn::gainToDb(1.0f) == 0.0f);
static_assert(xnyn::gainToDb(0.0, -100.0) == -100.0);

TEMPLATE_TEST_CASE("dbToGain stays within its error bound", "[math][decibels]", float, double) {
  using T = TestType;
  // Covers attenuation far below any practical noise floor and generous boost.
  constexpr int kSteps = 2000000;
  double worst = 0.0;  // error relative to the documented bound
  for (int i = 0; i <= kSteps; ++i) {
    const T db = static_cast<T>(-200.0 + 260.0 * static_cast<double>(i) / kSteps);
    const double err = std::abs(static_cast<double>(xnyn::dbToGain(db)) / refGain(static_cast<double>(db)) - 1.0);
    worst = std::fmax(worst, err / (kGainError<T> + kGainSlope<T> * std::abs(static_cast<double>(db))));
  }
  INFO("worst error / bound " << worst);
  CHECK(worst <= 1.0);
}

TEMPLATE_TEST_CASE("dbToGain reference values", "[math][decibels]", float, double) {
  using T = TestType;
  CHECK(xnyn::dbToGain(T(0)) == T(1));
  // Whole multiples of 20 dB are powers of ten.
  for (int k = -6; k <= 3; ++k) {
    const T db = static_cast<T>(20 * k);
    INFO("dB " << db);
    const double err = std::abs(static_cast<double>(xnyn::dbToGain(db)) / std::pow(10.0, k) - 1.0);
    CHECK(err <= kGainError<T> + kGainSlope<T> * std::abs(static_cast<double>(db)));
  }
  // -6.0206 dB is half amplitude.
  CHECK(std::abs(static_cast<double>(xnyn::dbToGain(T(-6.020599913279624))) - 0.5) <= 1e-6);
}

TEMPLATE_TEST_CASE("dbToGain is monotonic and positive", "[math][decibels]", float, double) {
  using T = TestType;
  T prev = T(0);
  for (int i = 0; i <= 200000; ++i) {
    const T db = static_cast<T>(-150.0 + 0.001 * i);
    const T g = xnyn::dbToGain(db);
    INFO("dB " << db);
    CHECK(g > T(0));
    CHECK(g >= prev);
    prev = g;
  }
}

TEMPLATE_TEST_CASE("dbToGain handles extreme inputs", "[math][decibels]", float, double) {
  using T = TestType;
  constexpr T kInf = std::numeric_limits<T>::infinity();
  CHECK(xnyn::dbToGain(-kInf) == T(0));
  CHECK(xnyn::dbToGain(kInf) == kInf);
  CHECK(xnyn::dbToGain(T(-1e5)) == T(0));
  CHECK(xnyn::dbToGain(T(1e5)) == kInf);
  CHECK(std::isnan(xnyn::dbToGain(std::numeric_limits<T>::quiet_NaN())));
}

TEMPLATE_TEST_CASE("gainToDb stays within its error bound", "[math][decibels]", float, double) {
  using T = TestType;
  // Gains from about -240 dB to +60 dB, both signs.
  constexpr int kSteps = 2000000;
  double worst = 0.0;
  for (int i = 0; i <= kSteps; ++i) {
    const double g = std::exp2(-40.0 + 50.0 * static_cast<double>(i) / kSteps);
    for (const T gain : {static_cast<T>(g), static_cast<T>(-g)}) {
      const double ref = refDb(static_cast<double>(gain));
      if (ref == 0.0) continue;
      worst = std::fmax(worst, std::abs(static_cast<double>(xnyn::gainToDb(gain)) - ref) / std::abs(ref));
    }
  }
  INFO("worst " << worst);
  CHECK(worst <= kDbError<T>);
}

TEMPLATE_TEST_CASE("gainToDb reference values and sign", "[math][decibels]", float, double) {
  using T = TestType;
  CHECK(xnyn::gainToDb(T(1)) == T(0));
  CHECK(xnyn::gainToDb(T(-1)) == T(0));
  CHECK(std::abs(static_cast<double>(xnyn::gainToDb(T(0.5))) - refDb(0.5)) <= 1e-5);
  CHECK(std::abs(static_cast<double>(xnyn::gainToDb(T(10))) - 20.0) <= 1e-5);
  CHECK(std::abs(static_cast<double>(xnyn::gainToDb(T(0.001))) + 60.0) <= 1e-4);
  // Magnitude: bipolar samples meter the same.
  for (const T g : {T(0.001), T(0.3), T(0.5), T(2), T(123.4)}) CHECK(xnyn::gainToDb(-g) == xnyn::gainToDb(g));
}

TEMPLATE_TEST_CASE("gainToDb handles silence, the floor and non-finite inputs", "[math][decibels]", float, double) {
  using T = TestType;
  constexpr T kInf = std::numeric_limits<T>::infinity();
  constexpr T kNaN = std::numeric_limits<T>::quiet_NaN();
  // Default: no floor, silence is -inf dB.
  CHECK(xnyn::gainToDb(T(0)) == -kInf);
  CHECK(xnyn::gainToDb(-T(0)) == -kInf);
  // With a floor, silence and anything quieter map to it; louder values are untouched.
  CHECK(xnyn::gainToDb(T(0), T(-100)) == T(-100));
  CHECK(xnyn::gainToDb(T(1e-7), T(-100)) == T(-100));
  CHECK(xnyn::gainToDb(std::numeric_limits<T>::denorm_min(), T(-100)) == T(-100));
  CHECK(xnyn::gainToDb(T(1), T(-100)) == T(0));
  CHECK(xnyn::gainToDb(T(0.5), T(-100)) == xnyn::gainToDb(T(0.5)));
  // Infinities and NaN.
  CHECK(xnyn::gainToDb(kInf) == kInf);
  CHECK(xnyn::gainToDb(-kInf) == kInf);
  CHECK(std::isnan(xnyn::gainToDb(kNaN)));
  CHECK(std::isnan(xnyn::gainToDb(kNaN, T(-100))));
}

TEMPLATE_TEST_CASE("dbToGain and gainToDb round-trip", "[math][decibels]", float, double) {
  using T = TestType;
  // Back to dB within the combined error: the gain error e maps to 20 / ln(10) * e dB.
  for (int i = 0; i <= 1440; ++i) {
    const T db = static_cast<T>(-120.0 + 0.1 * i);
    const double back = static_cast<double>(xnyn::gainToDb(xnyn::dbToGain(db)));
    const double tol = 8.686 * (kGainError<T> + kGainSlope<T> * std::abs(static_cast<double>(db))) +
                       kDbError<T> * std::abs(static_cast<double>(db)) + 1e-12;
    INFO("dB " << db);
    CHECK(std::abs(back - static_cast<double>(db)) <= tol);
  }
}
