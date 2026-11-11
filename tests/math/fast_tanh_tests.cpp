#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>

#include "xnyn/math/fast_tanh.hpp"

// Usable in constant expressions.
static_assert(xnyn::fastTanh(0.0) == 0.0);
static_assert(xnyn::fastTanh(3.0) == 1.0);
static_assert(xnyn::fastTanh(-3.0f) == -1.0f);

TEMPLATE_TEST_CASE("fastTanh hits exact reference values", "[math][fast_tanh]", float, double) {
  using T = TestType;
  CHECK(xnyn::fastTanh(T(0)) == T(0));
  CHECK(xnyn::fastTanh(T(3)) == T(1));
  CHECK(xnyn::fastTanh(T(-3)) == T(-1));
  // 1 * 28 / 36.
  CHECK(xnyn::fastTanh(T(1)) == T(28) / T(36));
}

TEMPLATE_TEST_CASE("fastTanh saturates outside [-3, 3]", "[math][fast_tanh]", float, double) {
  using T = TestType;
  for (const T x : {T(3.0001), T(10), T(1e6), std::numeric_limits<T>::max(), std::numeric_limits<T>::infinity()}) {
    CHECK(xnyn::fastTanh(x) == T(1));
    CHECK(xnyn::fastTanh(-x) == T(-1));
  }
}

TEMPLATE_TEST_CASE("fastTanh propagates NaN", "[math][fast_tanh]", float, double) {
  using T = TestType;
  CHECK(std::isnan(xnyn::fastTanh(std::numeric_limits<T>::quiet_NaN())));
}

TEMPLATE_TEST_CASE("fastTanh is odd, monotonic and bounded", "[math][fast_tanh]", float, double) {
  using T = TestType;
  constexpr int kSteps = 100000;
  T prev = xnyn::fastTanh(T(-5));
  for (int i = 0; i <= kSteps; ++i) {
    const T x = T(-5) + T(10) * static_cast<T>(i) / T(kSteps);
    const T y = xnyn::fastTanh(x);
    INFO("x " << x << " y " << y);
    // Rounding may wobble by a few ulps where the curve flattens near |x| = 3.
    CHECK(y >= prev - T(4) * std::numeric_limits<T>::epsilon());
    CHECK(std::abs(y) <= T(1) + std::numeric_limits<T>::epsilon());
    CHECK(xnyn::fastTanh(-x) == -y);
    prev = y;
  }
}

TEMPLATE_TEST_CASE("fastTanh stays within its error bound", "[math][fast_tanh]", float, double) {
  using T = TestType;
  constexpr T kMaxAbsError = T(0.0236);
  constexpr int kSteps = 200000;
  double worst = 0.0;
  // Beyond +-3 the error only shrinks (1 - tanh(x)), so +-10 covers every regime.
  for (int i = 0; i <= kSteps; ++i) {
    const T x = T(-10) + T(20) * static_cast<T>(i) / T(kSteps);
    const double err = std::abs(static_cast<double>(xnyn::fastTanh(x)) - std::tanh(static_cast<double>(x)));
    worst = std::fmax(worst, err);
  }
  INFO("worst " << worst);
  CHECK(worst <= kMaxAbsError);
  // The bound is tight: the documented peak is actually reached.
  CHECK(worst > T(0.0235));
}
