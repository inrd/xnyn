#pragma once

#include <algorithm>
#include <bit>
#include <concepts>
#include <cstdint>
#include <limits>

namespace xnyn {

// Polynomial approximations of 2^x, log2(x) and pow(x, y) = 2^(y log2 x), for float and
// double. For the usual DSP conversions: dB to gain, pitch (2^(semitones / 12)), curve
// shaping (x^k) and exp(x) as fastExp2(x * std::numbers::log2e).
//
// Max relative error (tests/math/fast_pow_tests.cpp enforces these, see
// docs/math/fast_pow.md):
// - fastExp2: 2.0e-9 (double: polynomial bound 1.97e-9), 1.0e-7 (float: 9.8e-8 measured
//   over every float in range, rounding dominates), wherever the result is a normal
//   number (x >= -126 / -1022); denormal results have fewer significant bits.
// - fastLog2: 2.7e-8 (double: polynomial bound 2.61e-8), 2.4e-7 (float: 2.30e-7 measured
//   over every positive finite float), relative to log2(x), so it stays accurate near x = 1.
// - fastPow: exp2 bound + k |y log2 x|, k = 1.9e-8 (double), 1.7e-7 (float): the log2
//   error is scaled by the exponent y log2 x. In float the rounding of y log2 x alone costs
//   up to 4e-8 |y log2 x|, whatever the log2 accuracy.
//
// Branchless: exponents are handled with integer conversions and bit manipulation, edge
// cases with selects. All three are constexpr.

namespace detail {

template <typename T>
struct PowTraits;

template <>
struct PowTraits<float> {
  using Int = std::int32_t;
  using UInt = std::uint32_t;
  static constexpr int kMantissaBits = 23;
  static constexpr int kBias = 127;
  static constexpr UInt kSqrtHalfBits = 0x3F35'04F3u;  // sqrt(1/2), rounded down
  static constexpr int kDenormalShift = 24;
  static constexpr float kDenormalScale = 0x1p24f;
};

template <>
struct PowTraits<double> {
  using Int = std::int64_t;
  using UInt = std::uint64_t;
  static constexpr int kMantissaBits = 52;
  static constexpr int kBias = 1023;
  static constexpr UInt kSqrtHalfBits = 0x3FE6'A09E'667F'3BCCu;  // sqrt(1/2), rounded down
  static constexpr int kDenormalShift = 54;
  static constexpr double kDenormalScale = 0x1p54;
};

}  // namespace detail

// 2^x. Exact for integer x, 2^0 = 1. Returns +0 for x < -126.5 (float) / -1022.5 (double),
// instead of going further into denormals, and +inf for x >= 127.5 / 1023.5, half an
// octave before the true overflow. NaN propagates.
//
// Reduction: n = round(x), f = x - n in [-1/2, 1/2] (exact), 2^x = 2^n * 2^f with 2^n built
// from the exponent bits. Rounding rather than flooring halves the polynomial's interval.
// 2^f ~= 1 + f P(f), P of degree 5: minimax relative-error fit over [-1/2, 1/2] (Remez
// exchange in 50-digit arithmetic, mpmath), equioscillation level 1.97e-9. The 1 + f P form
// makes 2^0 exactly 1. Estrin evaluation for shorter latency.
template <typename T>
  requires std::same_as<T, float> || std::same_as<T, double>
[[nodiscard]] constexpr T fastExp2(T x) noexcept {
  using Traits = detail::PowTraits<T>;
  using Int = typename Traits::Int;
  using UInt = typename Traits::UInt;
  // n in [-bias + 1, bias]: 2^n stays a normal number, and 2^n * 2^f covers the rest.
  constexpr T kLow = T(-Traits::kBias) + T(0.5);
  constexpr T kHigh = T(Traits::kBias) + T(0.5);
  // Clamped before the integer conversion (NaN or out-of-range conversion is UB). The
  // argument order maps NaN to kLow; f below uses x, so NaN still propagates. The upper
  // clamp is bias, not kHigh: bias + (bias + 1/2) is exact, so n can't round up to bias + 1
  // and overflow the scale. Adding bias + 1/2 (exact, one add) makes the clamped value
  // positive, so truncation is floor(x + 1/2): round to nearest.
  const T xc = std::min(T(Traits::kBias), std::max(kLow, x));
  const Int n = static_cast<Int>(xc + kHigh) - Traits::kBias;
  const T f = x - static_cast<T>(n);
  const T f2 = f * f;
  const T p = (T(0.6931472028550471682868709) + f * T(0.2402264791363321305695171)) +
              f2 * ((T(0.05550332471158506282746489) + f * T(0.00961843735733012687781502)) +
                    f2 * (T(0.001339887440377462705901899) + f * T(0.000153533619735756652316191)));
  const T scale = std::bit_cast<T>(static_cast<UInt>(static_cast<UInt>(n + Traits::kBias) << Traits::kMantissaBits));
  // scale * (1 + f p) as one FMA: f * scale is exact and computed off the critical path.
  const T y = (f * scale) * p + scale;
  // Out-of-range inputs, selects rather than branches. Comparisons are false for NaN.
  return x < kLow ? T(0) : (x >= kHigh ? std::numeric_limits<T>::infinity() : y);
}

// log2(x). Exact at powers of two (in particular 0 at 1). log2(+-0) = -inf, log2(+inf) =
// +inf, negative inputs and NaN give NaN. Denormal inputs are rescaled, so they keep the
// accuracy of normal ones.
//
// Reduction: x = 2^e * m with m in [sqrt(1/2), sqrt(2)), found with one integer subtract
// on the bits, so |log2(m)| <= 1/2 and log2(x) = e + log2(m) with e exact.
// log2(1 + u) ~= u P(u), P of degree 8: minimax relative-error fit over
// u in [sqrt(1/2) - 1, sqrt(2) - 1] (Remez, mpmath), equioscillation level 2.61e-8.
// Relative error keeps log2 accurate near 1 (pow(1 + eps, y)) and exact at 1.
template <typename T>
  requires std::same_as<T, float> || std::same_as<T, double>
[[nodiscard]] constexpr T fastLog2(T x) noexcept {
  using Traits = detail::PowTraits<T>;
  using Int = typename Traits::Int;
  using UInt = typename Traits::UInt;
  constexpr T kInf = std::numeric_limits<T>::infinity();
  const bool denormal = x < std::numeric_limits<T>::min();
  const T xs = denormal ? x * Traits::kDenormalScale : x;
  // Unsigned arithmetic, so non-positive / non-finite inputs (replaced below) are not UB.
  const UInt bits = std::bit_cast<UInt>(xs);
  const UInt offset = bits - Traits::kSqrtHalfBits;
  const Int e = static_cast<Int>(offset) >> Traits::kMantissaBits;  // arithmetic shift (C++20)
  const T m = std::bit_cast<T>(static_cast<UInt>(bits - (static_cast<UInt>(e) << Traits::kMantissaBits)));
  const T u = m - T(1);
  const T u2 = u * u;
  const T u4 = u2 * u2;
  const T p = ((T(1.442695003652456371730321) + u * T(-0.7213473468015070343037636)) +
               u2 * (T(0.4809106429357173535177143) + u * T(-0.3607036829505153811136192))) +
              u4 * ((T(0.2879162485305828315339902) + u * T(-0.2389448186001926809159297)) +
                    u2 * (T(0.2157155990076021308434846) + u * T(-0.2072697626722062067350802)) +
                    u4 * T(0.1258370588531479042629182));
  const T exponent = static_cast<T>(e) - (denormal ? T(Traits::kDenormalShift) : T(0));
  const T y = exponent + u * p;
  // Edge cases as selects. NaN fails both comparisons and falls through to NaN.
  const T edge = x == T(0) ? -kInf : (x > T(0) ? x : std::numeric_limits<T>::quiet_NaN());
  return (x > T(0) && x < kInf) ? y : edge;
}

// pow(x, y) = 2^(y log2 x) for x >= 0. Exact for pow(x, 0) = 1 (any x, including 0, inf and
// NaN, as std::pow), pow(1, y) = 1 (finite y) and powers of two to integer powers in range.
// pow(0, y > 0) = 0, pow(0, y < 0) = +inf, pow(inf, y > 0) = inf. Negative x gives NaN,
// even for integer y (unlike std::pow): take std::abs and handle the sign separately.
template <typename T>
  requires std::same_as<T, float> || std::same_as<T, double>
[[nodiscard]] constexpr T fastPow(T x, T y) noexcept {
  // Select so that pow(0, 0) and pow(inf, 0) are 1 as with std::pow (0 * -inf is NaN).
  return y == T(0) ? T(1) : fastExp2(y * fastLog2(x));
}

}  // namespace xnyn
