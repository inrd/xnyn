#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include "xnyn/osc/white_noise.hpp"

namespace {

constexpr int kN = 1 << 20;

std::vector<double> generate(xnyn::WhiteNoise& noise, int n) {
  std::vector<double> out(static_cast<size_t>(n));
  for (auto& x : out) x = static_cast<double>(noise.tick());
  return out;
}

double correlation(const std::vector<double>& a, const std::vector<double>& b, size_t lag = 0) {
  double ab = 0.0, aa = 0.0, bb = 0.0;
  for (size_t i = 0; i + lag < a.size(); ++i) {
    ab += a[i] * b[i + lag];
    aa += a[i] * a[i];
    bb += b[i + lag] * b[i + lag];
  }
  return ab / std::sqrt(aa * bb);
}

// For n independent samples the correlation estimate has a standard deviation of
// 1 / sqrt(n); 5 sigma keeps these deterministic checks far from flaky.
const double kCorrBound = 5.0 / std::sqrt(static_cast<double>(kN));

}  // namespace

TEST_CASE("WhiteNoise output lies on a symmetric grid strictly inside (-1, 1)", "[osc][white_noise]") {
  xnyn::WhiteNoise noise;
  noise.prepare(48000.0);
  for (int i = 0; i < kN; ++i) {
    const double x = static_cast<double>(noise.tick());
    // Odd multiples of 2^-22 in [-1 + 2^-22, 1 - 2^-22]: never -1, 0 or 1.
    const double k = x * 0x1p22;
    if (!(x > -1.0 && x < 1.0) || k != std::floor(k) || std::fmod(std::fabs(k), 2.0) != 1.0)
      FAIL("off-grid sample " << x << " at " << i);
  }
  SUCCEED();
}

TEST_CASE("WhiteNoise reaches both ends of its range", "[osc][white_noise]") {
  xnyn::WhiteNoise noise;
  double minV = 1.0, maxV = -1.0;
  for (int i = 0; i < kN; ++i) {
    const double x = static_cast<double>(noise.tick());
    minV = std::min(minV, x);
    maxV = std::max(maxV, x);
  }
  CHECK(minV < -0.999);
  CHECK(maxV > 0.999);
}

TEST_CASE("WhiteNoise has uniform statistics", "[osc][white_noise]") {
  xnyn::WhiteNoise noise;
  noise.prepare(48000.0);
  const auto x = generate(noise, kN);

  double sum = 0.0, sumSq = 0.0;
  for (const double v : x) {
    sum += v;
    sumSq += v * v;
  }
  const double n = static_cast<double>(kN);
  // Uniform on (-1, 1): mean 0 (std 1 / sqrt(3n)), variance 1/3 (std ~0.3 / sqrt(n)).
  CHECK(std::fabs(sum / n) < 5.0 / std::sqrt(3.0 * n));
  CHECK(std::fabs(sumSq / n - 1.0 / 3.0) < 5.0 * 0.3 / std::sqrt(n));

  // Histogram: chi-square over 64 bins, 63 degrees of freedom. 120 is beyond p = 1e-5.
  constexpr int kBins = 64;
  std::array<double, kBins> hist{};
  for (const double v : x) hist[static_cast<size_t>((v + 1.0) * 0.5 * kBins)] += 1.0;
  const double expected = n / kBins;
  double chi2 = 0.0;
  for (const double h : hist) chi2 += (h - expected) * (h - expected) / expected;
  CHECK(chi2 < 120.0);
}

TEST_CASE("WhiteNoise is white: no autocorrelation", "[osc][white_noise]") {
  xnyn::WhiteNoise noise;
  const auto x = generate(noise, kN);
  for (const size_t lag : {1u, 2u, 3u, 4u, 7u, 8u, 16u, 64u, 100u, 1024u, 4096u}) {
    INFO("lag " << lag);
    CHECK(std::fabs(correlation(x, x, lag)) < kCorrBound);
  }
}

TEST_CASE("WhiteNoise seeds give uncorrelated sequences", "[osc][white_noise]") {
  // Neighbouring seeds, as used for one generator per voice.
  for (const std::uint64_t seed : {0ull, 1ull, 41ull}) {
    INFO("seed " << seed);
    xnyn::WhiteNoise a, b;
    a.reset(seed);
    b.reset(seed + 1);
    const auto xa = generate(a, kN);
    const auto xb = generate(b, kN);
    CHECK(std::fabs(correlation(xa, xb)) < kCorrBound);
    CHECK(xa[0] != xb[0]);
  }
}

TEST_CASE("WhiteNoise is deterministic and reset restarts the sequence", "[osc][white_noise]") {
  xnyn::WhiteNoise a, b;
  a.reset(1234);
  b.reset(1234);
  const auto first = generate(a, 1000);
  CHECK(first == generate(b, 1000));

  a.reset();  // same seed
  CHECK(first == generate(a, 1000));

  a.reset(1234);
  a.prepare(96000.0);  // keeps the seed, restarts
  CHECK(first == generate(a, 1000));
}

TEST_CASE("WhiteNoise tick returns the current value then advances", "[osc][white_noise]") {
  xnyn::WhiteNoise noise;
  const float v = noise.value();
  CHECK(noise.tick() == v);
  CHECK(noise.value() != v);
  const float next = noise.value();
  noise.advance();
  CHECK(noise.value() != next);
}

TEST_CASE("WhiteNoise without prepare already produces the default sequence", "[osc][white_noise]") {
  xnyn::WhiteNoise fresh, seeded;
  seeded.prepare(44100.0);
  seeded.reset(xnyn::WhiteNoise::kDefaultSeed);
  CHECK(generate(fresh, 1000) == generate(seeded, 1000));
}

TEST_CASE("WhiteNoise reference values", "[osc][white_noise]") {
  // Pins the sequence: changing the generator changes every user's noise.
  xnyn::WhiteNoise noise;
  const auto x = generate(noise, 4);
  // Exact: every output is an odd integer times 2^-22.
  constexpr std::array<std::int64_t, 4> ref{3215445, -1773565, -2491797, -195713};
  for (size_t i = 0; i < ref.size(); ++i) {
    INFO("sample " << i);
    CHECK(static_cast<std::int64_t>(x[i] * 0x1p22) == ref[i]);
  }
}
