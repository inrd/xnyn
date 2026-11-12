#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <limits>
#include <type_traits>

#include "xnyn/math/lerp.hpp"

namespace xnyn {

// One-pole parameter smoother: exponentially glides its value towards a target.
//
//   v[n+1] = target + R * (v[n] - target),   R = exp(-1 / (tau * fs))
//
// - Time constant tau set in milliseconds; default 20 ms. The value covers 63.2% of a step
//   after tau, and is within 1e-3 of it after ln(1000) tau (~6.9 tau). The time is the same
//   at every sample rate.
// - tau <= 0 or NaN gives R = 0: the value jumps to the target on the next tick. Before
//   prepare(), R = 0 as well (follows the target instantly). tau = +inf freezes the value.
// - Monotone, no overshoot: the value always stays between its start and the target.
// - Generator style: tick() returns the current value, then advances, so a new target shows
//   up from the second tick on.
// - A NaN / inf target poisons the value until reset(value).
template <std::floating_point T = float>
class Smoother {
 public:
  static constexpr T kDefaultTimeMs = T(20);

  void prepare(double sampleRate) noexcept {
    sampleRate_ = sampleRate;
    updateCoefficient();
    reset();
  }

  // Snaps the value to the current target (no glide).
  void reset() noexcept { value_ = target_; }

  // Sets value and target at once (no glide).
  void reset(T value) noexcept {
    target_ = static_cast<State>(value);
    value_ = target_;
  }

  // Configuration rather than modulation: pays one exact std::exp. A cheaper
  // R = 1 - 1 / (tau fs) misplaces short times (it reaches 0 at tau = 1 sample and goes
  // negative below, i.e. overshoots and rings).
  void setTimeMs(T timeMs) noexcept {
    timeMs_ = timeMs;
    updateCoefficient();
  }

  // Cheap enough to call every sample.
  void setTarget(T target) noexcept { target_ = static_cast<State>(target); }

  // Returns the current value, then advances.
  [[nodiscard]] T tick() noexcept {
    const T out = value();
    advance();
    return out;
  }

  // Advances without reading, e.g. when only every n-th value is used.
  void advance() noexcept { value_ = lerp(target_, value_, r_); }

  [[nodiscard]] T value() const noexcept { return static_cast<T>(value_); }

 private:
  // At least double precision, even for T = float. The update target + R * d rounds back
  // to the same value once R * d is within half an ulp of d, i.e. it stalls about
  // 0.5 / (1 - R) ulps away from the target. With float state at 20 ms / 48 kHz that is
  // ~480 ulps: gliding from 0 to 0.5 stalls ~3e-5 short. With double state it stalls ~5e-14
  // short, so the float output rounds to the target exactly. The extra cost for float
  // is one double-to-float conversion per tick.
  using State = std::common_type_t<T, double>;

  // max(min, NaN) keeps min: NaN, zero and negative times all give R = exp(-huge) = 0,
  // without dividing by zero (which -ffast-math builds may assume never happens).
  void updateCoefficient() noexcept {
    const State samples = static_cast<State>(timeMs_) * State(1e-3) * static_cast<State>(sampleRate_);
    r_ = std::exp(State(-1) / std::max(std::numeric_limits<State>::min(), samples));
  }

  State value_ = 0;            // current value
  State target_ = 0;           // as set
  State r_ = 0;                // pole, in [0, 1]; 0 = jump to target
  double sampleRate_ = 0.0;    // Hz; 0 before prepare -> R = 0
  T timeMs_ = kDefaultTimeMs;  // as set, unclamped
};

}  // namespace xnyn
