#pragma once

#include <algorithm>
#include <concepts>
#include <limits>

#include "xnyn/math/fast_pow.hpp"

namespace xnyn {

// Decibel <-> linear amplitude conversions, built on fastExp2 / fastLog2. Cheap enough to
// call every sample (modulated levels, metering).
//
// dbToGain(dB) = 10^(dB / 20), computed as 2^(dB * log2(10) / 20): one multiply and
// fastExp2, no log of the constant base at run time (cheaper and more accurate than
// fastPow(10, dB / 20)).
// - Max relative error: 2.0e-9 (double), 1.0e-7 + 8e-9 |dB| (float; 7.0e-9 measured). In
//   float, rounding the exponent dB * log2(10) / 20 to 24 bits costs an error that grows
//   with |dB|: 1.06e-6 at -120 dB, i.e. 9e-6 dB, far below audibility.
// - Exact 1 at 0 dB. Negative dB is the normal case (attenuation). -inf dB gives 0, +inf
//   gives +inf, NaN propagates. Below about -762 dB (float) / -6156 dB (double) the result
//   flushes to 0; above +768 / +6162 dB it is +inf (see fastExp2).
//
// gainToDb(gain) = 20 log10(|gain|), computed as log2(|gain|) * 20 / log2(10).
// - Max relative error vs 20 log10(|gain|): 2.8e-8 (double), 3.0e-7 (float), relative to
//   the dB value, so it stays accurate near 0 dB. Exact 0 dB at |gain| = 1.
// - Takes the magnitude: negative gains and bipolar sample values (metering) are valid,
//   gainToDb(-0.5) is -6.02 dB.
// - Silence is -inf dB. floorDb clamps the result from below (e.g. -100 for a meter), so
//   0 maps to floorDb and the output stays finite for downstream smoothing or display.
//   +inf gives +inf, NaN propagates through the floor.
template <typename T>
  requires std::same_as<T, float> || std::same_as<T, double>
[[nodiscard]] constexpr T dbToGain(T db) noexcept {
  return fastExp2(db * T(0.1660964047443681173935159714744));  // log2(10) / 20
}

template <typename T>
  requires std::same_as<T, float> || std::same_as<T, double>
[[nodiscard]] constexpr T gainToDb(T gain, T floorDb = -std::numeric_limits<T>::infinity()) noexcept {
  const T magnitude = gain < T(0) ? -gain : gain;                          // constexpr std::abs is C++23
  const T db = fastLog2(magnitude) * T(6.020599913279623904274777894489);  // 20 / log2(10)
  // Argument order: std::max returns its first argument when the comparison is false, so
  // NaN propagates instead of being replaced by the floor.
  return std::max(db, floorDb);
}

}  // namespace xnyn
