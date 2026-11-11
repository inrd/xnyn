#pragma once

#include <bit>
#include <concepts>
#include <cstdint>

namespace xnyn {

// NaN / infinity guards for float and double samples.
//
// isFinite(x) is false for NaN (any sign, quiet or signalling, any payload) and ±inf.
// sanitize(x) returns x unchanged when finite (including ±0, denormals and ±max), else +0.
//
// Why not std::isfinite / std::isnan / x != x: under -ffast-math (-ffinite-math-only)
// compilers assume NaN and inf never occur and fold those checks to "always finite",
// silently disabling the guard. Even a bit test isn't automatically safe: clang
// recognises (bits & expMask) == expMask as an fp-class test and folds it too.
// Shifting the sign bit out and comparing, (bits << 1) < (expMask << 1), is not
// recognised by clang or GCC, stays branchless (shift + compare + select) and
// vectorizes. tests/util/sanitize_fast_math_tests.cpp guards this under -ffast-math.
//
// Denormals pass through: flushing them is the caller's job (FTZ/DAZ). Large finite
// values pass through too: clamp separately if downstream needs a bounded range.
// Sanitizing a block's input keeps new NaNs out, but can't repair state a NaN already
// reached: detect with isFinite and reset() the block for that.

namespace detail {

template <typename T>
struct FloatBits;

template <>
struct FloatBits<float> {
  using UInt = std::uint32_t;
  // Exponent bits all set, shifted left by one (sign bit dropped).
  static constexpr UInt kShiftedExponent = 0xFF00'0000u;
};

template <>
struct FloatBits<double> {
  using UInt = std::uint64_t;
  static constexpr UInt kShiftedExponent = 0xFFE0'0000'0000'0000u;
};

}  // namespace detail

// float and double only: long double has no portable bit layout.
template <typename T>
  requires std::same_as<T, float> || std::same_as<T, double>
[[nodiscard]] constexpr bool isFinite(T x) noexcept {
  using Bits = detail::FloatBits<T>;
  return static_cast<typename Bits::UInt>(std::bit_cast<typename Bits::UInt>(x) << 1) < Bits::kShiftedExponent;
}

template <typename T>
  requires std::same_as<T, float> || std::same_as<T, double>
[[nodiscard]] constexpr T sanitize(T x) noexcept {
  return isFinite(x) ? x : T(0);
}

}  // namespace xnyn
