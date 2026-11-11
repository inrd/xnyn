#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <random>

#include "xnyn/math/bilerp.hpp"

namespace {

// Double-double reference (~1e-30 relative), so the double bound is checked against
// something more precise than double on every target (long double isn't wider on MSVC or
// Apple arm64).
struct Dd {
  double hi = 0.0;
  double lo = 0.0;
};

Dd twoSum(double a, double b) {
  const double s = a + b;
  const double bb = s - a;
  return {s, (a - (s - bb)) + (b - bb)};
}

Dd add(Dd a, Dd b) {
  const Dd s = twoSum(a.hi, b.hi);
  return twoSum(s.hi, s.lo + a.lo + b.lo);
}

Dd mul(double x, Dd d) {
  const double p = x * d.hi;
  return twoSum(p, std::fma(x, d.hi, -p) + x * d.lo);
}

Dd lerpDd(Dd a, Dd b, double t) { return add(a, mul(t, add(b, {-a.hi, -a.lo}))); }

double refBilerp(double tl, double tr, double bl, double br, double x, double y) {
  return lerpDd(lerpDd({tl, 0.0}, {tr, 0.0}, x), lerpDd({bl, 0.0}, {br, 0.0}, x), y).hi;
}

}  // namespace

// Usable in constant expressions.
static_assert(xnyn::bilerp(0.0, 4.0, 8.0, 12.0, 0.5, 0.5) == 6.0);
static_assert(xnyn::bilerp(0.0f, 4.0f, 8.0f, 12.0f, 0.25f, 0.75f) == 7.0f);

TEMPLATE_TEST_CASE("bilerp hits exact reference values", "[math][bilerp]", float, double) {
  using T = TestType;
  // tl 0, tr 4, bl 8, br 12: f(x, y) = 4x + 8y, exact on dyadic grids.
  CHECK(xnyn::bilerp<T>(T(0), T(4), T(8), T(12), T(0), T(0)) == T(0));
  CHECK(xnyn::bilerp<T>(T(0), T(4), T(8), T(12), T(1), T(0)) == T(4));
  CHECK(xnyn::bilerp<T>(T(0), T(4), T(8), T(12), T(0), T(1)) == T(8));
  CHECK(xnyn::bilerp<T>(T(0), T(4), T(8), T(12), T(1), T(1)) == T(12));
  CHECK(xnyn::bilerp<T>(T(0), T(4), T(8), T(12), T(0.5), T(0.5)) == T(6));
  CHECK(xnyn::bilerp<T>(T(0), T(4), T(8), T(12), T(0.25), T(0.75)) == T(7));
  // Non-planar corners: the saddle term x y (tl - tr - bl + br) shows at the centre.
  CHECK(xnyn::bilerp<T>(T(1), T(0), T(0), T(1), T(0.5), T(0.5)) == T(0.5));
  CHECK(xnyn::bilerp<T>(T(1), T(0), T(0), T(1), T(0.25), T(0.25)) == T(0.625));
}

TEMPLATE_TEST_CASE("bilerp reduces to lerp along edges", "[math][bilerp]", float, double) {
  using T = TestType;
  for (int i = 0; i <= 64; ++i) {
    const T t = static_cast<T>(i) / T(64);
    CHECK(xnyn::bilerp<T>(T(-0.3), T(0.9), T(2.5), T(-7), t, T(0)) == xnyn::lerp<T>(T(-0.3), T(0.9), t));
    CHECK(xnyn::bilerp<T>(T(-0.3), T(0.9), T(2.5), T(-7), T(0), t) == xnyn::lerp<T>(T(-0.3), T(2.5), t));
  }
}

TEMPLATE_TEST_CASE("bilerp is exact for a constant field", "[math][bilerp]", float, double) {
  using T = TestType;
  for (const T v : {T(0), T(-0.1), T(0.3), T(1e-30), T(-7.77), T(123.456)}) {
    for (int i = -4; i <= 8; ++i) {
      for (int j = -4; j <= 8; ++j) {
        const T x = static_cast<T>(i) * T(0.37);
        const T y = static_cast<T>(j) * T(0.23);
        CHECK(xnyn::bilerp(v, v, v, v, x, y) == v);
      }
    }
  }
}

TEMPLATE_TEST_CASE("bilerp stays within its error bound", "[math][bilerp]", float, double) {
  using T = TestType;
  constexpr double eps = std::numeric_limits<T>::epsilon();
  std::mt19937 rng(42);
  std::uniform_real_distribution<double> corner(-100.0, 100.0);
  std::uniform_real_distribution<double> ratio(0.0, 1.0);
  double worst = 0.0;
  for (int i = 0; i < 1000000; ++i) {
    const auto tl = static_cast<T>(corner(rng)), tr = static_cast<T>(corner(rng));
    const auto bl = static_cast<T>(corner(rng)), br = static_cast<T>(corner(rng));
    const auto x = static_cast<T>(ratio(rng)), y = static_cast<T>(ratio(rng));
    const double ref = refBilerp(static_cast<double>(tl), static_cast<double>(tr), static_cast<double>(bl),
                                 static_cast<double>(br), static_cast<double>(x), static_cast<double>(y));
    const double m = std::fmax(std::fmax(std::abs(static_cast<double>(tl)), std::abs(static_cast<double>(tr))),
                               std::fmax(std::abs(static_cast<double>(bl)), std::abs(static_cast<double>(br))));
    const double err = std::abs(static_cast<double>(xnyn::bilerp(tl, tr, bl, br, x, y)) - ref);
    worst = std::fmax(worst, err / (eps * m));
  }
  INFO("worst error in eps * max|corner| " << worst);
  CHECK(worst <= 5.0);
}

TEMPLATE_TEST_CASE("bilerp extrapolates outside [0, 1]", "[math][bilerp]", float, double) {
  using T = TestType;
  // f(x, y) = 4x + 8y continues linearly.
  CHECK(xnyn::bilerp<T>(T(0), T(4), T(8), T(12), T(2), T(0)) == T(8));
  CHECK(xnyn::bilerp<T>(T(0), T(4), T(8), T(12), T(-0.5), T(-0.5)) == T(-6));
}

TEMPLATE_TEST_CASE("bilerp propagates NaN", "[math][bilerp]", float, double) {
  using T = TestType;
  constexpr T kNaN = std::numeric_limits<T>::quiet_NaN();
  CHECK(std::isnan(xnyn::bilerp<T>(kNaN, T(1), T(2), T(3), T(0.5), T(0.5))));
  CHECK(std::isnan(xnyn::bilerp<T>(T(0), T(1), T(2), kNaN, T(0.5), T(0.5))));
  CHECK(std::isnan(xnyn::bilerp<T>(T(0), T(1), T(2), T(3), kNaN, T(0.5))));
  CHECK(std::isnan(xnyn::bilerp<T>(T(0), T(1), T(2), T(3), T(0.5), kNaN)));
}
