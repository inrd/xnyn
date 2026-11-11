// Tests use a 1024 Hz sample rate so that frequencies that are multiples of 256 Hz
// give dyadic increments (0.25, 1.25...), which keeps expected phases exact.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <random>

#include "xnyn/osc/phasor.hpp"

using Catch::Approx;

namespace {

constexpr double kSR = 1024.0;

void checkInRange(const xnyn::Phasor& p) {
  INFO("phase " << p.phase());
  CHECK(p.phase() >= 0.0);
  CHECK(p.phase() < 1.0);
}

}  // namespace

TEST_CASE("Phasor starts at zero and stays there without increment", "[osc][phasor]") {
  xnyn::Phasor p;
  p.prepare(kSR);
  CHECK(p.phase() == 0.0);
  for (int i = 0; i < 10; ++i) p.advance();
  CHECK(p.phase() == 0.0);
}

TEST_CASE("Phasor tick returns the current phase then advances", "[osc][phasor]") {
  xnyn::Phasor p;
  p.prepare(kSR);
  p.setFrequency(256.0);
  CHECK(p.tick() == 0.0);
  CHECK(p.tick() == 0.25);
  CHECK(p.phase() == 0.5);
}

TEST_CASE("Phasor positive frequency advances and wraps", "[osc][phasor]") {
  xnyn::Phasor p;
  p.prepare(kSR);
  p.setFrequency(256.0);  // 0.25 cycle per sample
  for (const double e : {0.25, 0.5, 0.75, 0.0, 0.25}) {
    p.advance();
    CHECK(p.phase() == e);
  }
}

TEST_CASE("Phasor negative frequency runs backwards and wraps", "[osc][phasor]") {
  xnyn::Phasor p;
  p.prepare(kSR);
  p.setFrequency(-256.0);  // -0.25 cycle per sample
  for (const double e : {0.75, 0.5, 0.25, 0.0, 0.75}) {
    p.advance();
    CHECK(p.phase() == e);
  }
}

TEST_CASE("Phasor handles increments larger than one cycle", "[osc][phasor]") {
  xnyn::Phasor p;
  p.prepare(kSR);

  SECTION("positive") {
    p.setFrequency(1280.0);  // 1.25
    p.advance();
    CHECK(p.phase() == 0.25);
    p.advance();
    CHECK(p.phase() == 0.5);
  }

  SECTION("negative") {
    p.setFrequency(-1280.0);  // -1.25
    p.advance();
    CHECK(p.phase() == 0.75);
    p.advance();
    CHECK(p.phase() == 0.5);
  }

  SECTION("huge increments keep only the fractional cycle") {
    p.setFrequency((0x1p40 + 0.25) * kSR);
    p.advance();
    CHECK(p.phase() == 0.25);
    p.setFrequency(-(0x1p40 + 0.25) * kSR);
    p.advance();
    p.advance();
    CHECK(p.phase() == 0.75);
  }

  SECTION("whole number of cycles leaves phase unchanged") {
    p.reset(0.3);
    p.setFrequency(-3.0 * 1024.0);  // -3 cycles
    p.advance();
    CHECK(p.phase() == Approx(0.3).margin(1e-12));
  }
}

TEST_CASE("Phasor reset wraps the start phase into [0, 1)", "[osc][phasor]") {
  xnyn::Phasor p;
  p.prepare(kSR);
  p.reset(1.25);
  CHECK(p.phase() == 0.25);
  p.reset(-0.25);
  CHECK(p.phase() == 0.75);
  p.reset(-3.0);
  CHECK(p.phase() == 0.0);
  p.reset();
  CHECK(p.phase() == 0.0);
}

TEST_CASE("Phasor prepare keeps the frequency in Hz", "[osc][phasor]") {
  xnyn::Phasor p;
  p.setFrequency(256.0);  // before prepare: no increment
  p.advance();
  CHECK(p.phase() == 0.0);

  p.prepare(kSR);  // 0.25 cycle per sample
  p.advance();
  CHECK(p.phase() == 0.25);

  p.prepare(2.0 * kSR);  // same Hz, half the increment
  p.advance();
  CHECK(p.phase() == 0.125);
}

TEST_CASE("Phasor without prepare is neutral and finite", "[osc][phasor]") {
  xnyn::Phasor p;
  for (const double hz : {0.0, 440.0, -1e9, 1e300}) {
    INFO("freq " << hz);
    p.setFrequency(hz);
    CHECK(p.tick() == 0.0);
    CHECK(p.phase() == 0.0);
  }
  p.reset(0.5);
  p.setFrequency(440.0);
  p.advance();
  CHECK(p.phase() == 0.5);
}

TEST_CASE("Phasor prepare resets the phase", "[osc][phasor]") {
  xnyn::Phasor p;
  p.prepare(kSR);
  p.reset(0.5);
  p.prepare(kSR);
  CHECK(p.phase() == 0.0);
}

TEST_CASE("Phasor tiny negative step from zero stays below one", "[osc][phasor]") {
  // x - floor(x) rounds to exactly 1.0 for tiny negative x
  xnyn::Phasor p;
  p.prepare(kSR);
  p.setFrequency(-1e-20 * kSR);
  p.advance();
  checkInRange(p);

  p.reset(-1e-20);
  checkInRange(p);
}

TEST_CASE("Phasor stays in range under audio rate through-zero FM", "[osc][phasor]") {
  std::mt19937 rng(1234);
  std::uniform_real_distribution<double> freqDist(-60000.0, 60000.0);
  xnyn::Phasor p;
  p.prepare(48000.0);
  for (int i = 0; i < 200000; ++i) {
    p.setFrequency(freqDist(rng));
    p.advance();
    if (p.phase() < 0.0 || p.phase() >= 1.0) FAIL("phase out of range at sample " << i << ": " << p.phase());
  }
  SUCCEED();
}

TEST_CASE("Phasor does not drift over a long run", "[osc][phasor]") {
  constexpr double sr = 48000.0;
  constexpr long n = 48000L * 60L;  // one minute

  for (const double freq : {440.0, -440.0, 27.5, -12345.6}) {
    INFO("freq " << freq);
    xnyn::Phasor p;
    p.prepare(sr);
    p.setFrequency(freq);
    for (long i = 0; i < n; ++i) p.advance();

    const long double total = static_cast<long double>(freq * (1.0 / sr)) * n;
    const auto expected = static_cast<double>(total - std::floor(total));
    // circular distance so 0.9999.. vs 0.0000.. counts as close
    double err = std::fabs(p.phase() - expected);
    err = std::min(err, 1.0 - err);
    CHECK(err < 1e-9);
  }
}

TEST_CASE("Phasor largest phase below one stays below one", "[osc][phasor]") {
  xnyn::Phasor p;
  p.prepare(kSR);
  const double justBelowOne = std::nextafter(1.0, 0.0);
  p.reset(justBelowOne);
  CHECK(p.phase() == justBelowOne);
  p.reset(-std::nextafter(0.0, 1.0));  // smallest negative: rounds to zero, not one
  checkInRange(p);
}

TEST_CASE("Phasor half-cycle increments are exact in both directions", "[osc][phasor]") {
  for (const double hz : {512.0, -512.0}) {  // Nyquist: +/- 0.5 cycle
    INFO("freq " << hz);
    xnyn::Phasor p;
    p.prepare(kSR);
    p.setFrequency(hz);
    for (const double e : {0.5, 0.0, 0.5, 0.0}) {
      p.advance();
      CHECK(p.phase() == e);
    }
  }
}

TEST_CASE("Phasor is exactly periodic for rational frequencies", "[osc][phasor]") {
  // 441 Hz at 44100 Hz: 1/100 cycle per sample isn't representable, but the error of the
  // quantized increment is below 2^-63 cycle, so 100 * 1000 steps land on zero.
  xnyn::Phasor p;
  p.prepare(44100.0);
  p.setFrequency(441.0);
  for (int i = 0; i < 100000; ++i) p.advance();
  double err = p.phase();
  err = std::min(err, 1.0 - err);
  CHECK(err < 1e-12);
}
