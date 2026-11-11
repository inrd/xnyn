#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <numbers>
#include <type_traits>

namespace xnyn {

// One-pole DC blocker (first-order high-pass with a zero at DC):
//
//   y[n] = x[n] - x[n-1] + R * y[n-1],   R = exp(-2 pi fc / fs)
//
// - Cutoff fc in Hz, clamped to [0, fs / 2]; default 5 Hz (-0.26 dB at 20 Hz, DC down to
//   1e-3 within ~0.22 s). fc = 0 (and any NaN cutoff)
//   gives R = 1: exact pass-through. Before prepare(), R = 1 as well.
// - Removes DC exactly (gain is 0 at 0 Hz), no overshoot on a step, stable for every fc in
//   range (0 < R <= 1, and R = 1 only telescopes to y = x).
// - One instance is one channel.
// - NaN / inf input poisons the state until reset().
template <std::floating_point T = float>
class DcBlocker {
 public:
  static constexpr T kDefaultCutoffHz = T(5);

  void prepare(double sampleRate) noexcept {
    invSampleRate_ = 1.0 / sampleRate;
    updateCoefficient();
    reset();
  }

  // State as if the input had been 0 forever: the first sample after reset passes
  // through unchanged, and a DC offset present from the start decays with time
  // constant 1 / (2 pi fc).
  void reset() noexcept {
    x1_ = State(0);
    y1_ = State(0);
  }

  // Configuration, not modulation: a DC blocker's cutoff is set once (or on rare user
  // changes), so this pays one exact std::exp rather than approximating. The cheaper
  // R = 1 - 2 pi fc / fs is accurate at DC-blocking cutoffs (relative cutoff error
  // ~pi fc / fs, 0.07% at 10 Hz / 44.1 kHz) but turns unstable once fc > fs / pi, so
  // it can't cover the whole range.
  void setCutoff(T cutoffHz) noexcept {
    cutoffHz_ = cutoffHz;
    updateCoefficient();
  }

  [[nodiscard]] T tick(T x) noexcept {
    const State xs = static_cast<State>(x);
    const State y = xs - x1_ + r_ * y1_;
    x1_ = xs;
    y1_ = y;
    return static_cast<T>(y);
  }

 private:
  // At least double precision, even for T = float. Near R = 1 the recursion gains
  // rounding noise by ~1 / sqrt(2 (1 - R)): with float state the error is about -100 dB
  // at 1-10 Hz, and at R = 1 (fc = 0, or before prepare) it random-walks without bound
  // (~5e-5 after 20M samples). With double state the error is about -150 dB, and R = 1 is
  // an exact pass-through for float input whenever x - x1 is exact in double (operands
  // within 2^29 of each other, i.e. any audio signal): y = (x - x1) + x1 = x, so y1 == x1
  // again and nothing accumulates. With T = double, R = 1 is pass-through up to a random
  // walk of 1e-16 roundings. The extra cost for float is two float/double conversions.
  using State = std::common_type_t<T, double>;

  // NaN fails both comparisons: min keeps it, max then maps it to 0 (pass-through).
  void updateCoefficient() noexcept {
    const State normalized = std::max(State(0), std::min(static_cast<State>(cutoffHz_) * invSampleRate_, State(0.5)));
    r_ = std::exp(-State(2) * std::numbers::pi_v<State> * normalized);
  }

  State x1_ = 0;                   // previous input
  State y1_ = 0;                   // previous output
  State r_ = 1;                    // pole, in (0, 1]; 1 = pass-through
  State invSampleRate_ = 0;        // 1 / Hz; 0 before prepare -> R = 1
  T cutoffHz_ = kDefaultCutoffHz;  // as set, unclamped
};

}  // namespace xnyn
