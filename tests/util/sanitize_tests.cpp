#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

#include "xnyn/util/sanitize.hpp"

// Usable in constant expressions.
static_assert(xnyn::sanitize(1.5f) == 1.5f);
static_assert(xnyn::sanitize(std::numeric_limits<double>::infinity()) == 0.0);
static_assert(xnyn::sanitize(std::numeric_limits<float>::quiet_NaN()) == 0.0f);
static_assert(!xnyn::isFinite(-std::numeric_limits<double>::infinity()));

namespace {

template <typename T>
using UInt = std::conditional_t<std::same_as<T, float>, std::uint32_t, std::uint64_t>;

// Every non-finite flavour: ±inf, quiet / signalling NaNs of both signs, custom payloads.
template <typename T>
std::vector<T> nonFinite() {
  using U = UInt<T>;
  constexpr int kMantissaBits = std::numeric_limits<T>::digits - 1;
  constexpr U kSign = U(1) << (sizeof(T) * 8 - 1);
  constexpr U kExp = std::bit_cast<U>(std::numeric_limits<T>::infinity());
  constexpr U kQuiet = U(1) << (kMantissaBits - 1);
  const U patterns[] = {
      kExp,                                  // +inf
      kExp | kQuiet,                         // canonical quiet NaN
      kExp | 1,                              // signalling NaN, smallest payload
      kExp | (kQuiet - 1),                   // signalling NaN, largest payload
      kExp | kQuiet | 0x1234,                // quiet NaN with payload
      kExp | ((U(1) << kMantissaBits) - 1),  // all mantissa bits set
  };
  std::vector<T> out;
  for (const U p : patterns) {
    out.push_back(std::bit_cast<T>(p));
    out.push_back(std::bit_cast<T>(p | kSign));
  }
  out.push_back(std::numeric_limits<T>::signaling_NaN());
  return out;
}

template <typename T>
std::vector<T> finite() {
  using L = std::numeric_limits<T>;
  return {T(0),
          -T(0),
          T(1),
          T(-0.5),
          T(123.456),
          L::max(),
          L::lowest(),
          L::min(),
          -L::min(),
          L::denorm_min(),
          -L::denorm_min(),
          L::min() / T(3),
          L::epsilon(),
          std::nextafter(L::max(), T(0))};
}

}  // namespace

TEMPLATE_TEST_CASE("sanitize zeroes NaN and infinity", "[util][sanitize]", float, double) {
  using T = TestType;
  for (const T x : nonFinite<T>()) {
    INFO("bits " << std::bit_cast<UInt<T>>(x));
    CHECK_FALSE(xnyn::isFinite(x));
    const T y = xnyn::sanitize(x);
    CHECK(std::bit_cast<UInt<T>>(y) == UInt<T>(0));  // +0 exactly, not -0
  }
}

TEMPLATE_TEST_CASE("sanitize passes finite values through bit-exactly", "[util][sanitize]", float, double) {
  using T = TestType;
  for (const T x : finite<T>()) {
    INFO("x " << x);
    CHECK(xnyn::isFinite(x));
    // Bit comparison: keeps the sign of zero and denormals untouched.
    CHECK(std::bit_cast<UInt<T>>(xnyn::sanitize(x)) == std::bit_cast<UInt<T>>(x));
  }
}

TEMPLATE_TEST_CASE("sanitize agrees with std::isfinite on arithmetic results", "[util][sanitize]", float, double) {
  using T = TestType;
  // Values produced at run time, as in a real signal chain.
  volatile T zero = T(0);
  volatile T big = std::numeric_limits<T>::max();
  const T produced[] = {zero / zero,
                        T(1) / zero,
                        T(-1) / zero,
                        big * T(2),
                        std::sqrt(T(-1) + zero),
                        std::log(zero),
                        (T(1) / zero) - (T(1) / zero)};
  for (const T x : produced) {
    CHECK(xnyn::isFinite(x) == std::isfinite(x));
    CHECK(xnyn::sanitize(x) == (std::isfinite(x) ? x : T(0)));
  }
}
