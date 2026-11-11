#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>

#include "xnyn/math/lerp.hpp"

// Usable in constant expressions.
static_assert(xnyn::lerp(2.0, 6.0, 0.25) == 3.0);
static_assert(xnyn::lerp(2.0f, 6.0f, 0.25f) == 3.0f);

TEMPLATE_TEST_CASE("lerp hits exact reference values", "[math][lerp]", float, double) {
  using T = TestType;
  CHECK(xnyn::lerp<T>(T(2), T(6), T(0)) == T(2));
  CHECK(xnyn::lerp<T>(T(2), T(6), T(1)) == T(6));
  CHECK(xnyn::lerp<T>(T(2), T(6), T(0.5)) == T(4));
  CHECK(xnyn::lerp<T>(T(2), T(6), T(0.25)) == T(3));
  CHECK(xnyn::lerp<T>(T(-1), T(1), T(0.75)) == T(0.5));
  // Decreasing range.
  CHECK(xnyn::lerp<T>(T(6), T(2), T(0.25)) == T(5));
}

TEMPLATE_TEST_CASE("lerp extrapolates outside [0, 1]", "[math][lerp]", float, double) {
  using T = TestType;
  CHECK(xnyn::lerp<T>(T(2), T(6), T(2)) == T(10));
  CHECK(xnyn::lerp<T>(T(2), T(6), T(-0.5)) == T(0));
}

TEMPLATE_TEST_CASE("lerp is exact at t = 0 and for a == b", "[math][lerp]", float, double) {
  using T = TestType;
  const T vals[] = {T(0), T(-0.1), T(0.3), T(1e-30), T(-7.77), std::numeric_limits<T>::max()};
  for (const T a : vals) {
    for (const T b : vals) {
      if (std::isfinite(b - a)) CHECK(xnyn::lerp(a, b, T(0)) == a);
    }
    for (const T t : {T(-3), T(0), T(0.37), T(1), T(5)}) CHECK(xnyn::lerp(a, a, t) == a);
  }
}

TEMPLATE_TEST_CASE("lerp stays within rounding of b at t = 1", "[math][lerp]", float, double) {
  using T = TestType;
  constexpr T eps = std::numeric_limits<T>::epsilon();
  const T vals[] = {T(0), T(-0.1), T(0.3), T(1e-3), T(-7.77), T(123.456)};
  for (const T a : vals) {
    for (const T b : vals) {
      const T y = xnyn::lerp(a, b, T(1));
      INFO("a " << a << " b " << b << " y " << y);
      // b - a and the final add each round once: error <= 2 ulp of max(|a|, |b|).
      CHECK(std::abs(y - b) <= T(2) * eps * std::fmax(std::abs(a), std::abs(b)));
    }
  }
}
