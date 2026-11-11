// Compiled with -ffast-math -O2 (see tests/CMakeLists.txt): the guard must survive the
// optimizer assuming NaN and inf never occur, which is the reason sanitize inspects bits.
//
// The non-finite inputs are built from bit patterns behind volatile, so the compiler
// can't see they are NaN / inf and the checks run on real values at run time.

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <bit>
#include <cstdint>
#include <type_traits>

#include "xnyn/util/sanitize.hpp"

namespace {

template <typename T>
using UInt = std::conditional_t<std::same_as<T, float>, std::uint32_t, std::uint64_t>;

template <typename T>
T fromBits(UInt<T> bits) {
  volatile UInt<T> v = bits;
  return std::bit_cast<T>(static_cast<UInt<T>>(v));
}

template <typename T>
bool isZeroBits(T x) {
  return std::bit_cast<UInt<T>>(x) == UInt<T>(0);
}

}  // namespace

TEST_CASE("sanitize still catches NaN and inf under -ffast-math", "[util][sanitize]") {
  CHECK(isZeroBits(xnyn::sanitize(fromBits<float>(0x7FC0'0000u))));  // quiet NaN
  CHECK(isZeroBits(xnyn::sanitize(fromBits<float>(0xFF80'0001u))));  // -signalling NaN
  CHECK(isZeroBits(xnyn::sanitize(fromBits<float>(0x7F80'0000u))));  // +inf
  CHECK(isZeroBits(xnyn::sanitize(fromBits<float>(0xFF80'0000u))));  // -inf
  CHECK(xnyn::sanitize(fromBits<float>(0x3F80'0000u)) == 1.0f);

  CHECK(isZeroBits(xnyn::sanitize(fromBits<double>(0x7FF8'0000'0000'0000u))));
  CHECK(isZeroBits(xnyn::sanitize(fromBits<double>(0xFFF0'0000'0000'0001u))));
  CHECK(isZeroBits(xnyn::sanitize(fromBits<double>(0x7FF0'0000'0000'0000u))));
  CHECK(isZeroBits(xnyn::sanitize(fromBits<double>(0xFFF0'0000'0000'0000u))));
  CHECK(xnyn::sanitize(fromBits<double>(0x3FF0'0000'0000'0000u)) == 1.0);
}
