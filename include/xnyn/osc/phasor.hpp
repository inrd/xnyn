#pragma once

#include <cstdint>

namespace xnyn {

// Normalized phase accumulator.
//
// Phase is in cycles and always within [0, 1). The increment may be negative or larger
// than one cycle (through-zero FM, frequencies above the sample rate).
class Phasor {
 public:
  void prepare(double sampleRate) noexcept {
    invSampleRate_ = 1.0 / sampleRate;
    updateDelta();
    reset();
  }

  void reset(double startPhase = 0.0) noexcept { phase_ = toFixed(startPhase); }

  // Cheap enough to call every sample: the conversion stays off the per-sample
  // dependency chain, which is only the integer add in advance().
  void setFrequency(double hz) noexcept {
    hz_ = hz;
    updateDelta();
  }

  // Returns the current phase, then advances.
  [[nodiscard]] double tick() noexcept {
    const double out = phase();
    advance();
    return out;
  }

  void advance() noexcept { phase_ += delta_; }

  // The top 53 bits convert exactly, so the result is always within [0, 1).
  [[nodiscard]] double phase() const noexcept { return static_cast<double>(phase_ >> 11) * 0x1p-53; }

 private:
  // Converts cycles to fixed point, wrapped to one cycle. x - trunc(x) is exact and lies
  // in (-1, 1), so scaling by 2^63 fits int64; the shift restores 2^64 per cycle
  // (resolution 2^-63 cycle) and drops the whole cycles. Negative values wrap through
  // the unsigned cast. Truncating through an int64 cast instead of std::round/trunc
  // keeps this inline on baseline x86-64 (no SSE4.1), where those are libm calls.
  // Requires |x| < 2^63 cycles.
  [[nodiscard]] static std::uint64_t toFixed(double x) noexcept {
    const double t = x - static_cast<double>(static_cast<std::int64_t>(x));
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(t * 0x1p63)) << 1;
  }

  void updateDelta() noexcept { delta_ = toFixed(hz_ * invSampleRate_); }

  // Fixed point, 2^64 per cycle: wrapping is the free unsigned overflow, so the
  // loop-carried chain is one integer add instead of add + floor + sub + select on a
  // double. It also accumulates exactly: no drift, and resolution beats a double
  // accumulator (2^-53 cycle near 1).
  std::uint64_t phase_ = 0;     // cycles * 2^64
  std::uint64_t delta_ = 0;     // cycles per sample * 2^64, any sign (two's complement)
  double hz_ = 0.0;             // any sign / magnitude
  double invSampleRate_ = 0.0;  // 1 / Hz
};

}  // namespace xnyn
