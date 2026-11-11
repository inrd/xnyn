#pragma once

#include <cmath>

namespace xnyn {

// Normalized phase accumulator.
//
// Phase is in cycles and always within [0, 1). The increment may be negative or larger
// than one cycle (through-zero FM, frequencies above the sample rate): wrapping is a
// single branchless floor, no fmod.
class Phasor {
 public:
  void prepare(double sampleRate) noexcept {
    invSampleRate_ = 1.0 / sampleRate;
    updateDelta();
    reset();
  }

  void reset(double startPhase = 0.0) noexcept { phase_ = wrap(startPhase); }

  void setFrequency(double hz) noexcept {
    hz_ = hz;
    updateDelta();
  }

  // Returns the current phase, then advances.
  [[nodiscard]] double tick() noexcept {
    const double out = phase_;
    advance();
    return out;
  }

  void advance() noexcept { phase_ = wrap(phase_ + delta_); }

  [[nodiscard]] double phase() const noexcept { return phase_; }

 private:
  // x - floor(x) rounds to exactly 1.0 for tiny negative x: fold that back to 0.
  // The ternary compiles to a select (fcsel / blendv), not a branch.
  [[nodiscard]] static double wrap(double x) noexcept {
    const double w = x - std::floor(x);
    return w < 1.0 ? w : 0.0;
  }

  void updateDelta() noexcept { delta_ = hz_ * invSampleRate_; }

  // double: a float accumulator drifts audibly within seconds at audio rates.
  double phase_ = 0.0;          // cycles, [0, 1)
  double delta_ = 0.0;          // cycles per sample, any sign / magnitude
  double hz_ = 0.0;             // any sign / magnitude
  double invSampleRate_ = 0.0;  // 1 / Hz
};

}  // namespace xnyn
