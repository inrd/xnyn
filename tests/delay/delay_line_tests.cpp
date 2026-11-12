#include <catch2/catch_approx.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <random>
#include <type_traits>
#include <utility>
#include <vector>

#include "xnyn/delay/delay_line.hpp"

using Catch::Approx;

namespace {

// Straightforward reference: full history in a vector, delays in double.
template <typename T>
class HistoryDelay {
 public:
  void write(T x) { h_.push_back(x); }
  T at(long d) const {
    const long i = static_cast<long>(h_.size()) - d;
    return i >= 0 ? h_[static_cast<size_t>(i)] : T(0);
  }
  double linear(double d) const {
    const long i = static_cast<long>(std::floor(d));
    const double f = d - static_cast<double>(i);
    return static_cast<double>(at(i)) + f * (static_cast<double>(at(i + 1)) - static_cast<double>(at(i)));
  }

 private:
  std::vector<T> h_;
};

}  // namespace

TEMPLATE_TEST_CASE("DelayLine integer read delays a signal exactly", "[delay][delay_line]", float, double) {
  using T = TestType;
  xnyn::DelayLine<T> dl;
  dl.prepare(1000.0, 0.1);  // 100 samples, capacity 128: wraps several times below
  REQUIRE(dl.maxDelaySamples() == 100);
  HistoryDelay<T> ref;
  std::mt19937 rng(1);
  std::uniform_real_distribution<T> dist(T(-1), T(1));
  for (int n = 0; n < 1000; ++n) {
    for (const int d : {1, 2, 37, 99, 100}) REQUIRE(dl.read(d) == ref.at(d));
    const T x = dist(rng);
    dl.write(x);
    ref.write(x);
  }
}

TEMPLATE_TEST_CASE("DelayLine read then write gives y[n] = x[n - d]", "[delay][delay_line]", float, double) {
  using T = TestType;
  xnyn::DelayLine<T> dl;
  dl.prepare(48000.0, 0.01);
  constexpr int d = 5;
  for (int n = 0; n < 100; ++n) {
    const T y = dl.read(d);
    dl.write(static_cast<T>(n + 1));
    CHECK(y == (n < d ? T(0) : static_cast<T>(n + 1 - d)));
  }
}

TEMPLATE_TEST_CASE("DelayLine readLinear matches a reference interpolation", "[delay][delay_line]", float, double) {
  using T = TestType;
  xnyn::DelayLine<T> dl;
  dl.prepare(48000.0, 0.005);  // 240 samples
  HistoryDelay<T> ref;
  std::mt19937 rng(2);
  std::uniform_real_distribution<T> sig(T(-1), T(1));
  std::uniform_real_distribution<double> del(1.0, 240.0);
  const double tol = std::same_as<T, float> ? 2e-6 : 1e-14;
  for (int n = 0; n < 20000; ++n) {
    for (int k = 0; k < 4; ++k) {
      const auto d = static_cast<T>(del(rng));
      REQUIRE(static_cast<double>(dl.readLinear(d)) == Approx(ref.linear(static_cast<double>(d))).margin(tol));
    }
    const T x = sig(rng);
    dl.write(x);
    ref.write(x);
  }
}

TEMPLATE_TEST_CASE("DelayLine readLinear is exact at integer delays", "[delay][delay_line]", float, double) {
  using T = TestType;
  xnyn::DelayLine<T> dl;
  dl.prepare(48000.0, 0.01);
  std::mt19937 rng(3);
  std::uniform_real_distribution<T> dist(T(-1), T(1));
  for (int n = 0; n < 2000; ++n) {
    for (const int d : {1, 2, 100, 479, 480}) REQUIRE(dl.readLinear(static_cast<T>(d)) == dl.read(d));
    dl.write(dist(rng));
  }
}

TEST_CASE("DelayLine fractional resolution doesn't depend on the write position", "[delay][delay_line]") {
  // Alternating 0 / 1 data makes readLinear return the fraction itself (lerp(0, 1, f) = f,
  // exact here), so any fraction lost to the write position would show. Millions of
  // writes put the position far beyond 2^24, where a float position - delay difference
  // would have no fractional bits left.
  xnyn::DelayLine<float> dl;
  dl.prepare(1.0, 65536.0);  // 2^16 samples
  constexpr int kWrites = 20'000'000;
  for (int n = 1; n <= kWrites; ++n) dl.write(static_cast<float>(n % 2));
  // Last written (n = kWrites, even) is 0, the one before 1: read(d) = (d + 1) % 2.
  for (const float f : {0.0f, 0.00390625f, 0.125f, 0.5f, 0.99609375f}) {
    INFO("f " << f);
    CHECK(dl.readLinear(1.0f + f) == f);
    CHECK(dl.readLinear(1001.0f + f) == f);
  }
  // Below 2^16 samples, float delays still resolve 2^-8 sample.
  for (const float f : {0.0f, 0.00390625f, 0.5f, 0.99609375f}) CHECK(dl.readLinear(65535.0f + f) == f);
}

TEMPLATE_TEST_CASE("DelayLine clamps delays to [1, maxDelaySamples]", "[delay][delay_line]", float, double) {
  using T = TestType;
  xnyn::DelayLine<T> dl;
  dl.prepare(1000.0, 0.064);  // 64 samples, capacity 128
  for (int n = 1; n <= 300; ++n) dl.write(static_cast<T>(n));
  // Last written is 300, so read(d) = 301 - d.
  CHECK(dl.read(64) == T(237));
  for (const int d : {65, 1000, std::numeric_limits<int>::max()}) CHECK(dl.read(d) == T(237));
  for (const int d : {0, -1, std::numeric_limits<int>::min()}) CHECK(dl.read(d) == T(300));
  for (const T d : {T(64), T(64.5), T(1e9), std::numeric_limits<T>::infinity(), std::numeric_limits<T>::max()})
    CHECK(dl.readLinear(d) == T(237));
  for (const T d :
       {T(1), T(0.5), T(0), T(-3), -std::numeric_limits<T>::infinity(), std::numeric_limits<T>::quiet_NaN()})
    CHECK(dl.readLinear(d) == T(300));
}

TEMPLATE_TEST_CASE("DelayLine capacity follows sample rate and max delay", "[delay][delay_line]", float, double) {
  using T = TestType;
  xnyn::DelayLine<T> dl;
  dl.prepare(48000.0, 1.0);
  CHECK(dl.maxDelaySamples() == 48000);
  dl.prepare(96000.0, 1.0);
  CHECK(dl.maxDelaySamples() == 96000);
  dl.prepare(44100.0, 0.0101);  // 445.41 -> ceil
  CHECK(dl.maxDelaySamples() == 446);
  for (const double s :
       {0.0, -1.0, std::numeric_limits<double>::quiet_NaN(), -std::numeric_limits<double>::infinity()}) {
    INFO("seconds " << s);
    dl.prepare(48000.0, s);
    CHECK(dl.maxDelaySamples() == 1);
    dl.write(T(0.5));
    CHECK(dl.read(1) == T(0.5));
    CHECK(dl.readLinear(T(1)) == T(0.5));
  }
  dl.prepare(48000.0, std::numeric_limits<double>::infinity());
  CHECK(dl.maxDelaySamples() == xnyn::DelayLine<T>::kMaxDelaySamples);
}

TEMPLATE_TEST_CASE("DelayLine reset and prepare clear the history", "[delay][delay_line]", float, double) {
  using T = TestType;
  xnyn::DelayLine<T> dl;
  dl.prepare(1000.0, 0.05);
  for (int n = 0; n < 200; ++n) dl.write(T(1));
  dl.reset();
  for (int d = 1; d <= 50; ++d) REQUIRE(dl.read(d) == T(0));
  for (int n = 0; n < 200; ++n) dl.write(T(1));
  dl.prepare(1000.0, 0.05);  // same capacity: memory reused, still cleared
  for (int d = 1; d <= 50; ++d) REQUIRE(dl.read(d) == T(0));
  for (int n = 0; n < 200; ++n) dl.write(T(1));
  dl.prepare(1000.0, 0.5);  // new capacity
  for (int d = 1; d <= 500; ++d) REQUIRE(dl.read(d) == T(0));
  // First sample after prepare: the previous write is visible at delay 1.
  dl.write(T(0.25));
  CHECK(dl.read(1) == T(0.25));
}

TEMPLATE_TEST_CASE("DelayLine without prepare outputs silence", "[delay][delay_line]", float, double) {
  using T = TestType;
  xnyn::DelayLine<T> dl;
  CHECK(dl.maxDelaySamples() == 1);
  for (int n = 0; n < 100; ++n) {
    dl.write(T(1) + static_cast<T>(n));
    for (const int d : {-1, 1, 2, 1000}) REQUIRE(dl.read(d) == T(0));
    for (const T d : {T(1), T(1.5), T(1000), std::numeric_limits<T>::quiet_NaN()}) REQUIRE(dl.readLinear(d) == T(0));
  }
  dl.reset();
  CHECK(dl.read(1) == T(0));
}

TEMPLATE_TEST_CASE("DelayLine moves leave the source unprepared", "[delay][delay_line]", float, double) {
  using T = TestType;
  xnyn::DelayLine<T> a;
  a.prepare(1000.0, 0.01);
  for (int n = 1; n <= 20; ++n) a.write(static_cast<T>(n));

  xnyn::DelayLine<T> b(std::move(a));
  CHECK(b.maxDelaySamples() == 10);
  CHECK(b.read(1) == T(20));
  CHECK(b.read(10) == T(11));
  CHECK(a.maxDelaySamples() == 1);  // NOLINT(bugprone-use-after-move): defined state
  a.write(T(5));
  CHECK(a.read(1) == T(0));

  xnyn::DelayLine<T> c;
  c = std::move(b);
  CHECK(c.read(10) == T(11));
  CHECK(b.read(1) == T(0));  // NOLINT(bugprone-use-after-move)

  // Moving an unprepared line keeps it silent.
  xnyn::DelayLine<T> d(std::move(a));  // NOLINT(bugprone-use-after-move)
  d.write(T(1));
  CHECK(d.readLinear(T(1)) == T(0));

  // A vector of lines (one per voice) can grow.
  std::vector<xnyn::DelayLine<T>> voices(2);
  for (auto& v : voices) v.prepare(1000.0, 0.01);
  voices[0].write(T(3));
  voices.emplace_back();
  CHECK(voices[0].read(1) == T(3));
}

TEMPLATE_TEST_CASE("DelayLine NaN input only affects reads that touch it", "[delay][delay_line]", float, double) {
  using T = TestType;
  xnyn::DelayLine<T> dl;
  dl.prepare(1000.0, 0.01);  // 10 samples, capacity 16
  dl.write(std::numeric_limits<T>::quiet_NaN());
  dl.write(T(1));
  CHECK(std::isnan(dl.read(2)));
  CHECK(dl.read(1) == T(1));
  CHECK(std::isnan(dl.readLinear(T(1.5))));
  for (int n = 0; n < 16; ++n) dl.write(T(1));  // overwritten
  for (int d = 1; d <= 10; ++d) CHECK(dl.readLinear(static_cast<T>(d) + T(0.5)) == T(1));
}

TEST_CASE("DelayLine only prepare may throw or allocate", "[delay][delay_line]") {
  xnyn::DelayLine<float> dl;
  STATIC_REQUIRE(!noexcept(dl.prepare(1.0, 1.0)));
  STATIC_REQUIRE(noexcept(dl.reset()));
  STATIC_REQUIRE(noexcept(dl.write(0.0f)));
  STATIC_REQUIRE(noexcept(dl.read(1)));
  STATIC_REQUIRE(noexcept(dl.readLinear(1.0f)));
  STATIC_REQUIRE(!std::is_copy_constructible_v<xnyn::DelayLine<float>>);
  STATIC_REQUIRE(std::is_nothrow_move_constructible_v<xnyn::DelayLine<float>>);
}

TEST_CASE("DelayLine feedback comb matches the analytic impulse response", "[delay][delay_line]") {
  // y[n] = x[n] + g y[n - D]: impulse response is g^k at n = kD.
  xnyn::DelayLine<double> dl;
  dl.prepare(48000.0, 0.01);
  constexpr int kD = 300;
  constexpr double kG = 0.7;
  for (int n = 0; n < 10 * kD; ++n) {
    const double y = (n == 0 ? 1.0 : 0.0) + kG * dl.read(kD);
    dl.write(y);
    const double expected = n % kD == 0 ? std::pow(kG, n / kD) : 0.0;
    REQUIRE(y == Approx(expected).margin(1e-15));
  }
}
