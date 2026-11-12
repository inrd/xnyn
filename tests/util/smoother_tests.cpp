#include <catch2/catch_approx.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>

#include "xnyn/util/smoother.hpp"

using Catch::Approx;

TEMPLATE_TEST_CASE("Smoother step response matches target + R^n (start - target)", "[util][smoother]", float, double) {
  using T = TestType;
  constexpr double sr = 48000.0;
  constexpr double timeMs = 5.0;
  xnyn::Smoother<T> s;
  s.prepare(sr);
  s.setTimeMs(T(timeMs));
  s.reset(T(1));
  s.setTarget(T(-1));
  const double r = std::exp(-1.0 / (timeMs * 1e-3 * sr));
  for (int n = 0; n < 2000; ++n) {
    const double expected = -1.0 + 2.0 * std::pow(r, n);
    REQUIRE(static_cast<double>(s.tick()) == Approx(expected).margin(std::same_as<T, float> ? 1e-6 : 1e-12));
  }
}

TEMPLATE_TEST_CASE("Smoother time constant is the same at every sample rate", "[util][smoother]", float, double) {
  using T = TestType;
  for (const double sr : {44100.0, 48000.0, 96000.0, 192000.0}) {
    INFO("sr " << sr);
    xnyn::Smoother<T> s;
    s.prepare(sr);
    s.setTimeMs(T(10));
    s.setTarget(T(1));
    // After tau the value has covered 1 - 1/e of the step.
    const long n = static_cast<long>(std::lround(10e-3 * sr));
    for (long i = 0; i < n; ++i) s.advance();
    CHECK(static_cast<double>(s.value()) == Approx(1.0 - std::exp(-1.0)).margin(1e-4));
  }
}

TEMPLATE_TEST_CASE("Smoother default time is 20 ms", "[util][smoother]", float, double) {
  using T = TestType;
  CHECK(xnyn::Smoother<T>::kDefaultTimeMs == T(20));
  xnyn::Smoother<T> a;
  a.prepare(48000.0);
  xnyn::Smoother<T> b;
  b.prepare(48000.0);
  b.setTimeMs(T(20));
  a.setTarget(T(1));
  b.setTarget(T(1));
  for (int i = 0; i < 5000; ++i) REQUIRE(a.tick() == b.tick());
}

TEMPLATE_TEST_CASE("Smoother is monotone and reaches the target exactly", "[util][smoother]", float, double) {
  using T = TestType;
  for (const double timeMs : {0.1, 20.0, 200.0}) {
    for (const T target : {T(0.5), T(-3), T(1e-3), T(0)}) {
      INFO("timeMs " << timeMs << " target " << target);
      xnyn::Smoother<T> s;
      s.prepare(192000.0);
      s.setTimeMs(static_cast<T>(timeMs));
      s.reset(T(0.7));
      s.setTarget(target);
      const bool up = target > T(0.7);
      T prev = s.tick();
      const long n = static_cast<long>(timeMs * 1e-3 * 192000.0 * 60.0) + 10;
      for (long i = 0; i < n; ++i) {
        const T v = s.tick();
        if (up ? (v < prev || v > target) : (v > prev || v < target))
          FAIL("not monotone at sample " << i << ": " << prev << " -> " << v);
        prev = v;
      }
      // The double state stalls far below float resolution, so float lands on the target.
      // A zero target is approached geometrically (R^n v) instead of stalling.
      if (target == T(0))
        CHECK(std::fabs(s.value()) < T(1e-20));
      else if constexpr (std::same_as<T, float>)
        CHECK(s.value() == target);
      else
        CHECK(s.value() == Approx(target).margin(1e-10));
    }
  }
}

TEMPLATE_TEST_CASE("Smoother tick returns the value before advancing", "[util][smoother]", float, double) {
  using T = TestType;
  xnyn::Smoother<T> s;
  s.prepare(48000.0);
  s.reset(T(0.25));
  s.setTarget(T(1));
  CHECK(s.value() == T(0.25));
  CHECK(s.tick() == T(0.25));  // new target shows from the second tick
  const T v = s.value();
  CHECK(v > T(0.25));
  CHECK(s.tick() == v);

  xnyn::Smoother<T> a;
  xnyn::Smoother<T> b;
  a.prepare(48000.0);
  b.prepare(48000.0);
  a.setTarget(T(1));
  b.setTarget(T(1));
  for (int i = 0; i < 1000; ++i) {
    const T before = b.value();
    b.advance();
    REQUIRE(a.tick() == before);
  }
  CHECK(a.value() == b.value());
}

TEMPLATE_TEST_CASE("Smoother reset and prepare snap without glide", "[util][smoother]", float, double) {
  using T = TestType;
  xnyn::Smoother<T> s;
  s.prepare(48000.0);
  s.setTarget(T(0.8));
  s.reset();  // snaps to the target
  for (int i = 0; i < 10; ++i) CHECK(s.tick() == T(0.8));

  s.reset(T(-0.3));  // sets value and target
  for (int i = 0; i < 10; ++i) CHECK(s.tick() == T(-0.3));

  s.setTarget(T(2));
  (void)s.tick();
  (void)s.tick();
  s.prepare(96000.0);  // prepare resets to the target
  CHECK(s.tick() == T(2));
}

TEMPLATE_TEST_CASE("Smoother time is kept in ms across prepare", "[util][smoother]", float, double) {
  using T = TestType;
  xnyn::Smoother<T> a;
  a.setTimeMs(T(3));
  a.prepare(44100.0);
  xnyn::Smoother<T> b;
  b.prepare(44100.0);
  b.setTimeMs(T(3));
  a.setTarget(T(1));
  b.setTarget(T(1));
  for (int i = 0; i < 1000; ++i) REQUIRE(a.tick() == b.tick());
}

TEMPLATE_TEST_CASE("Smoother time edge cases", "[util][smoother]", float, double) {
  using T = TestType;
  SECTION("zero, negative and NaN times jump to the target") {
    for (const T t : {T(0), T(-5), -std::numeric_limits<T>::infinity(), std::numeric_limits<T>::quiet_NaN(),
                      std::numeric_limits<T>::denorm_min()}) {
      INFO("timeMs " << t);
      xnyn::Smoother<T> s;
      s.prepare(48000.0);
      s.setTimeMs(t);
      s.setTarget(T(0.6));
      CHECK(s.tick() == T(0));
      CHECK(s.tick() == T(0.6));
      s.setTarget(T(-0.1));
      CHECK(s.tick() == T(0.6));
      CHECK(s.tick() == T(-0.1));
    }
  }

  SECTION("infinite time freezes the value") {
    xnyn::Smoother<T> s;
    s.prepare(48000.0);
    s.setTimeMs(std::numeric_limits<T>::infinity());
    s.reset(T(0.4));
    s.setTarget(T(1));
    for (int i = 0; i < 10000; ++i) REQUIRE(s.tick() == Approx(T(0.4)).margin(1e-15));
  }

  SECTION("very long times stay finite and move slowly") {
    xnyn::Smoother<T> s;
    s.prepare(48000.0);
    s.setTimeMs(std::numeric_limits<T>::max());
    s.setTarget(T(1));
    for (int i = 0; i < 10000; ++i) {
      const T v = s.tick();
      REQUIRE(std::isfinite(v));
      REQUIRE(v >= T(0));
      REQUIRE(v < T(1e-6));
    }
  }
}

TEMPLATE_TEST_CASE("Smoother without prepare follows the target instantly", "[util][smoother]", float, double) {
  using T = TestType;
  xnyn::Smoother<T> s;
  CHECK(s.tick() == T(0));
  for (const T t : {T(20), T(0), T(1e30)}) {
    s.setTimeMs(t);
    s.setTarget(T(0.5));
    (void)s.tick();
    CHECK(s.tick() == T(0.5));
    s.setTarget(T(-2));
    (void)s.tick();
    CHECK(s.tick() == T(-2));
  }
}

TEMPLATE_TEST_CASE("Smoother NaN target poisons the value until reset", "[util][smoother]", float, double) {
  using T = TestType;
  xnyn::Smoother<T> s;
  s.prepare(48000.0);
  s.setTarget(std::numeric_limits<T>::quiet_NaN());
  (void)s.tick();
  CHECK(std::isnan(s.tick()));
  s.setTarget(T(1));
  CHECK(std::isnan(s.tick()));
  s.reset(T(0.5));
  CHECK(s.tick() == T(0.5));
}
