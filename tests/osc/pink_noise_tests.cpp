#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <numbers>
#include <vector>

#include "xnyn/osc/pink_noise.hpp"
#include "xnyn/osc/white_noise.hpp"

namespace {

using Complex = std::complex<double>;

void fft(std::vector<Complex>& a) {
  const size_t n = a.size();
  for (size_t i = 1, j = 0; i < n; ++i) {
    size_t bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) std::swap(a[i], a[j]);
  }
  for (size_t len = 2; len <= n; len <<= 1) {
    const Complex w = std::polar(1.0, -2.0 * std::numbers::pi / static_cast<double>(len));
    for (size_t i = 0; i < n; i += len) {
      Complex wk = 1.0;
      for (size_t k = 0; k < len / 2; ++k) {
        const Complex u = a[i + k];
        const Complex v = a[i + k + len / 2] * wk;
        a[i + k] = u + v;
        a[i + k + len / 2] = u - v;
        wk *= w;
      }
    }
  }
}

// Measures the shaping filter's power response |H(f)|^2 against a WhiteNoise with the same
// seed, which reproduces the pink generator's input exactly. Dividing the output spectrum by
// the known input spectrum removes the randomness: only windowing leakage remains.
struct Response {
  std::vector<Complex> cross, inPower;
  double binHz;

  // Mean |H|^2 * f over a third-octave band around hz: flat for an exact 1/f response.
  [[nodiscard]] double pinkDb(double hz) const {
    const double edge = std::cbrt(std::sqrt(2.0));
    const auto lo = static_cast<size_t>(hz / edge / binHz);
    const auto hi = static_cast<size_t>(hz * edge / binHz);
    double sum = 0.0;
    for (size_t i = lo; i <= hi; ++i) sum += std::norm(cross[i] / inPower[i]) * static_cast<double>(i) * binHz;
    return 10.0 * std::log10(sum / static_cast<double>(hi - lo + 1));
  }
};

Response measure(double sampleRate) {
  size_t n = 1;
  while (static_cast<double>(n) < sampleRate) n <<= 1;  // bins <= 1 Hz
  xnyn::PinkNoise pink;
  xnyn::WhiteNoise white;
  pink.prepare(sampleRate);
  for (size_t i = 0; i < n; ++i) {  // let the 2 Hz pole settle
    (void)pink.tick();
    (void)white.tick();
  }
  Response r{std::vector<Complex>(n / 2), std::vector<Complex>(n / 2), sampleRate / static_cast<double>(n)};
  for (int frame = 0; frame < 2; ++frame) {
    std::vector<Complex> x(n), y(n);
    for (size_t i = 0; i < n; ++i) {
      const double w = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * static_cast<double>(i) / static_cast<double>(n));
      x[i] = w * static_cast<double>(white.tick());
      y[i] = w * static_cast<double>(pink.tick());
    }
    fft(x);
    fft(y);
    for (size_t i = 0; i < n / 2; ++i) {
      r.cross[i] += y[i] * std::conj(x[i]);
      r.inPower[i] += std::norm(x[i]);
    }
  }
  return r;
}

double rms(xnyn::PinkNoise& pink, int n) {
  double sum = 0.0;
  for (int i = 0; i < n; ++i) {
    const auto x = static_cast<double>(pink.tick());
    sum += x * x;
  }
  return std::sqrt(sum / n);
}

}  // namespace

TEST_CASE("PinkNoise follows 1/f within 0.35 dB over 20 Hz .. 20 kHz", "[osc][pink_noise]") {
  for (const double sr : {8000.0, 22050.0, 44100.0, 48000.0, 96000.0, 192000.0, 384000.0}) {
    INFO("sr " << sr);
    const auto r = measure(sr);
    const double ref = r.pinkDb(1000.0);
    const double top = std::min(20000.0, 0.45 * sr) / std::cbrt(std::sqrt(2.0));
    // Within 0.05 dB from 88.2 kHz up.
    const double bound = sr >= 88200.0 ? 0.05 : 0.35;
    for (double hz = 20.0; hz <= top; hz *= std::cbrt(2.0)) {
      INFO("hz " << hz);
      CHECK(std::fabs(r.pinkDb(hz) - ref) < bound);
    }
  }
}

TEST_CASE("PinkNoise has no DC drift", "[osc][pink_noise]") {
  // The response flattens below 2 Hz instead of growing to infinity, so the mean stays
  // near zero over a long run.
  xnyn::PinkNoise pink;
  pink.prepare(48000.0);
  double sum = 0.0;
  constexpr int n = 48000 * 30;
  for (int i = 0; i < n; ++i) sum += static_cast<double>(pink.tick());
  CHECK(std::fabs(sum / n) < 0.01);
}

TEST_CASE("PinkNoise level is calibrated across sample rates", "[osc][pink_noise]") {
  // Total RMS: 0.125 in 20 Hz .. 20 kHz plus the content outside it, which grows slowly
  // with the sample rate (0.153 at 44.1 kHz, 0.164 at 192 kHz).
  for (const double sr : {44100.0, 48000.0, 96000.0, 192000.0}) {
    INFO("sr " << sr);
    xnyn::PinkNoise pink;
    pink.prepare(sr);
    const double level = rms(pink, static_cast<int>(sr * 20));
    CHECK(level > 0.145);
    CHECK(level < 0.175);
  }
}

TEST_CASE("PinkNoise stays finite and in range over a long run", "[osc][pink_noise]") {
  xnyn::PinkNoise pink;
  pink.prepare(192000.0);  // poles closest to 1
  float peak = 0.0f;
  for (int i = 0; i < 192000 * 60; ++i) peak = std::max(peak, std::fabs(pink.tick()));
  CHECK(std::isfinite(peak));
  CHECK(peak < 1.0f);
}

TEST_CASE("PinkNoise is deterministic and reset restarts the sequence", "[osc][pink_noise]") {
  auto run = [](xnyn::PinkNoise& p) {
    std::vector<float> out(1000);
    for (auto& x : out) x = p.tick();
    return out;
  };
  xnyn::PinkNoise a, b;
  a.prepare(48000.0);
  b.prepare(48000.0);
  a.reset(7);
  b.reset(7);
  const auto first = run(a);
  CHECK(first == run(b));

  a.reset();  // same seed, filter cleared
  CHECK(first == run(a));

  a.prepare(48000.0);  // keeps the seed
  CHECK(first == run(a));
}

TEST_CASE("PinkNoise seeds give uncorrelated sequences", "[osc][pink_noise]") {
  xnyn::PinkNoise a, b;
  a.prepare(48000.0);
  b.prepare(48000.0);
  a.reset(3);
  b.reset(4);
  double ab = 0.0, aa = 0.0, bb = 0.0;
  constexpr int n = 1 << 20;
  for (int i = 0; i < n; ++i) {
    const auto x = static_cast<double>(a.tick());
    const auto y = static_cast<double>(b.tick());
    ab += x * y;
    aa += x * x;
    bb += y * y;
  }
  // Pink noise is strongly autocorrelated, so far fewer than n independent samples:
  // a looser bound than for white noise.
  CHECK(std::fabs(ab / std::sqrt(aa * bb)) < 0.05);
}

TEST_CASE("PinkNoise without prepare outputs the white noise", "[osc][pink_noise]") {
  xnyn::PinkNoise pink;
  xnyn::WhiteNoise white;
  for (int i = 0; i < 1000; ++i) REQUIRE(pink.tick() == white.tick());
  pink.reset(5);
  white.reset(5);
  for (int i = 0; i < 1000; ++i) REQUIRE(pink.tick() == white.tick());
}

TEST_CASE("PinkNoise first sample after reset is the scaled white sample", "[osc][pink_noise]") {
  // Zero filter state: the first output is the input times the filter's direct gain,
  // so it is small but non-zero and finite.
  xnyn::PinkNoise pink;
  pink.prepare(48000.0);
  const float x = pink.tick();
  CHECK(std::isfinite(x));
  CHECK(x != 0.0f);
  CHECK(std::fabs(x) < 1.0f);
}
