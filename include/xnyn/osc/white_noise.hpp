#pragma once

#include <bit>
#include <cstdint>

namespace xnyn {

// Uniform white noise generator in (-1, 1).
//
// - Output is uniform and exactly symmetric around zero: odd multiples of 2^-22 in
//   [-1 + 2^-22, 1 - 2^-22] (22 random bits, 2^22 equally likely values). Mean 0,
//   RMS 1 / sqrt(3) (-4.77 dBFS). Never exactly -1, 0 or 1.
// - Deterministic: the same seed always gives the same sequence. Period 2^64 samples.
// - Sample rate independent: prepare() only resets, and a default-constructed generator
//   already produces noise from the default seed.
// - Outputs float: noise needs no more than 22 bits, and the bit trick below is cheapest
//   in float. Convert for double pipelines.
class WhiteNoise {
 public:
  static constexpr std::uint64_t kDefaultSeed = 0;

  // Noise does not depend on the sample rate; kept for a uniform lifecycle.
  void prepare(double /*sampleRate*/) noexcept { reset(); }

  // Restarts the sequence from the current seed.
  void reset() noexcept { state_ = mix(seed_); }

  // Sets a new seed and restarts from it. Any value is valid, including 0.
  void reset(std::uint64_t seed) noexcept {
    seed_ = seed;
    reset();
  }

  // Returns the current sample, then advances.
  [[nodiscard]] float tick() noexcept {
    const float out = value();
    advance();
    return out;
  }

  void advance() noexcept { state_ = state_ * kMul + kInc; }

  // Current sample, as in iquilezles.org/articles/sfrand: random mantissa bits under the
  // exponent of 2 give a float in [2, 4), and subtracting 3 maps it to [-1, 1) exactly, with
  // no int to float conversion. Forcing the lowest mantissa bit to 1 shifts the grid by half
  // a step, so it is symmetric and exactly zero-mean instead of biased by -2^-23.
  [[nodiscard]] float value() const noexcept {
    const auto bits = static_cast<std::uint32_t>(state_ >> 41) | 0x40000001u;
    return std::bit_cast<float>(bits) - 3.0f;
  }

 private:
  // 64-bit LCG (Knuth's MMIX constants): one multiply-add per sample, full 2^64 period.
  // Only the top 22 bits are used; the low bits of a power-of-two LCG are weak (bit k has
  // period 2^(k+1)), the top ones are not. The 32-bit `seed *= 16807` of the original
  // article has a 2^30 period (6.2 hours at 48 kHz) and a weak multiplier.
  static constexpr std::uint64_t kMul = 6364136223846793005ull;
  static constexpr std::uint64_t kInc = 1442695040888963407ull;

  // SplitMix64 finalizer: neighbouring seeds (one per voice: 0, 1, 2...) start at unrelated
  // states. Raw LCG states s and s + 1 would share their top bits, hence
  // their first sample, and stay a fixed, structured offset apart afterwards.
  [[nodiscard]] static constexpr std::uint64_t mix(std::uint64_t x) noexcept {
    x += 0x9e3779b97f4a7c15ull;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
    return x ^ (x >> 31);
  }

  std::uint64_t state_ = mix(kDefaultSeed);
  std::uint64_t seed_ = kDefaultSeed;
};

}  // namespace xnyn
