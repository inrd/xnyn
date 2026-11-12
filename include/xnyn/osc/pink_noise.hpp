#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <numbers>

#include "xnyn/osc/white_noise.hpp"

namespace xnyn {

// Pink (1/f) noise generator: WhiteNoise shaped by a -3 dB / octave filter.
//
// - Spectrum within 0.35 dB of an exact 1/f slope over 20 Hz .. min(20 kHz, 0.45 fs), for
//   sample rates from 8 kHz to 384 kHz (within 0.05 dB from 88.2 kHz up; the error at 44.1 /
//   48 kHz is the top octave running slightly low). Below 20 Hz it keeps rising, then
//   flattens out under 2 Hz, so the output has no DC drift.
// - Calibrated level: by design the 20 Hz .. 20 kHz band holds an RMS of 1/8 (-18 dBFS) at
//   44.1 kHz and up; lower rates are quieter (no band above Nyquist). Total RMS is about
//   0.15 .. 0.165, rising slowly with the sample rate (content outside the band). The output
//   is near-Gaussian, not hard-bounded: |x| > 1 needs a > 6 sigma excursion, so in practice
//   it never clips.
// - Same seeding as WhiteNoise: deterministic per seed, neighbouring seeds uncorrelated.
// - Before prepare(), the filter is a pass-through: the output is the plain white noise.
class PinkNoise {
 public:
  static constexpr std::uint64_t kDefaultSeed = WhiteNoise::kDefaultSeed;

  // Designs the filter for this sample rate (std::exp / std::cos / std::sqrt, not meant
  // per sample), then reset().
  void prepare(double sampleRate) noexcept {
    design(sampleRate);
    reset();
  }

  // Restarts the sequence from the current seed and clears the filter.
  void reset() noexcept {
    white_.reset();
    clearState();
  }

  // Sets a new seed (any value) and restarts from it.
  void reset(std::uint64_t seed) noexcept {
    white_.reset(seed);
    clearState();
  }

  // Returns the next sample. Unlike other generators there is no separate value(): the
  // output is a filter's response, computed as the noise advances.
  [[nodiscard]] float tick() noexcept {
    double x = gain_ * static_cast<double>(white_.tick());
    // Cascade of first-order shelves. Sections chain within a sample, but the only
    // loop-carried dependency is each section's own state, so successive samples overlap
    // in the pipeline.
    for (std::size_t i = 0; i < kSections; ++i) {
      const double y = x - zeros_[i] * xPrev_[i] + poles_[i] * yPrev_[i];
      xPrev_[i] = x;
      yPrev_[i] = y;
      x = y;
    }
    const double out = x + tilt_ * xLast_;
    xLast_ = x;
    return static_cast<float>(out);
  }

 private:
  // Poles log-spaced from 2 Hz to 30 kHz (ratio ~3.9), each followed by a zero half a
  // spacing higher: alternating -6 / 0 dB per octave segments average to -3 dB per octave.
  // With 8 sections the ripple is ~0.02 dB; fewer sections grow it quickly (0.04 dB at 7).
  static constexpr std::size_t kSections = 8;
  static constexpr double kLowestPoleHz = 2.0;
  static constexpr double kHighestPoleHz = 30000.0;
  // Target RMS over 20 Hz .. 20 kHz: 1/8 (-18 dBFS).
  static constexpr double kBandRms = 0.125;

  void clearState() noexcept {
    xPrev_.fill(0.0);
    yPrev_.fill(0.0);
    xLast_ = 0.0;
  }

  [[nodiscard]] std::complex<double> response(double hz, double sampleRate) const noexcept {
    const auto z1 = std::polar(1.0, -2.0 * std::numbers::pi * hz / sampleRate);
    std::complex<double> h = 1.0 + tilt_ * z1;
    for (std::size_t i = 0; i < kSections; ++i) h *= (1.0 - zeros_[i] * z1) / (1.0 - poles_[i] * z1);
    return h;
  }

  void design(double sampleRate) noexcept {
    const double twoPiT = 2.0 * std::numbers::pi / sampleRate;
    const double ratio = std::pow(kHighestPoleHz / kLowestPoleHz, 1.0 / static_cast<double>(kSections - 1));
    const double zeroOffset = std::sqrt(ratio);
    double hz = kLowestPoleHz;
    for (std::size_t i = 0; i < kSections; ++i) {
      poles_[i] = std::exp(-twoPiT * hz);
      zeros_[i] = std::exp(-twoPiT * hz * zeroOffset);
      hz *= ratio;
    }

    // Matched z-transform poles near Nyquist fall short of the analog slope. A final
    // FIR zero (1 + c z^-1) restores the exact 1/f magnitude at the top of the band,
    // relative to a reference frequency. Solves |1 + c e^-jw|^2 = m2 for c: the two roots
    // multiply to 1, take the one inside (-1, 1).
    tilt_ = 0.0;
    gain_ = 1.0;
    const double refHz = std::min(1000.0, sampleRate / 8.0);
    const double topHz = std::min(20000.0, 0.45 * sampleRate);
    const double target = std::abs(response(refHz, sampleRate)) * std::sqrt(refHz / topHz);
    const double g = target / std::abs(response(topHz, sampleRate));
    const double g2 = g * g;
    const double a = 1.0 - g2;
    const double b = 2.0 * (std::cos(twoPiT * topHz) - g2);
    if (std::abs(a) > 1e-12) {
      const double disc = std::max(b * b - 4.0 * a * a, 0.0);
      double c = (-b - std::copysign(std::sqrt(disc), b)) / (2.0 * a);
      if (std::abs(c) > 1.0) c = 1.0 / c;
      tilt_ = std::clamp(c, -0.9, 0.9);
    }

    // Level: pink density S(f) = A / f with A ln(1000) = kBandRms^2 over 20 Hz .. 20 kHz.
    // Uniform white noise in (-1, 1) has variance 1/3, one-sided density (2/3) / fs.
    const double density = kBandRms * kBandRms / (std::log(1000.0) * refHz);
    gain_ = std::sqrt(density * 1.5 * sampleRate) / std::abs(response(refHz, sampleRate));
  }

  WhiteNoise white_;
  // Defaults make the filter a pass-through before prepare(): no poles, no zeros, unit gain.
  // State and coefficients are double: poles reach 1 - 7e-5 (2 Hz at 192 kHz), where a float
  // recursion would add audible rounding noise to the low end.
  std::array<double, kSections> poles_{};
  std::array<double, kSections> zeros_{};
  std::array<double, kSections> xPrev_{};
  std::array<double, kSections> yPrev_{};
  double xLast_ = 0.0;
  double tilt_ = 0.0;
  double gain_ = 1.0;
};

}  // namespace xnyn
