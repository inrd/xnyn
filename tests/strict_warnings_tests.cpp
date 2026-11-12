// Compiled with a strict warning set and -Werror (see tests/CMakeLists.txt): consumers often
// build plugins with flags like -Wfloat-equal or -Wdouble-promotion, and xnyn headers must
// stay clean under them. Instantiates every block for float and double so templates are
// actually checked.

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include "xnyn/xnyn.hpp"

TEMPLATE_TEST_CASE("Every block compiles cleanly under strict warnings", "[strict_warnings]", float, double) {
  using T = TestType;
  const T x = T(0.25);
  T acc = T(0);
  acc += xnyn::bilerp(x, x, x, x, x, x);
  acc += xnyn::dbToGain(x);
  acc += xnyn::gainToDb(x);
  acc += xnyn::gainToDb(x, T(-100));
  acc += xnyn::equalPowerCrossfade(x, x, x);
  acc += xnyn::fastExp2(x);
  acc += xnyn::fastLog2(x);
  acc += xnyn::fastPow(x, x);
  acc += xnyn::fastSin(x);
  acc += xnyn::fastTanh(x);
  acc += xnyn::lerp(x, x, x);
  acc += xnyn::sanitize(x);
  acc += xnyn::isFinite(x) ? T(1) : T(0);

  xnyn::DelayLine<T> delay;
  delay.prepare(48000.0, 0.01);
  delay.write(x);
  acc += delay.read(1);
  acc += delay.readLinear(T(1.5));
  acc += static_cast<T>(delay.maxDelaySamples());
  delay.reset();

  xnyn::DcBlocker<T> dc;
  dc.prepare(48000.0);
  dc.setCutoff(T(10));
  acc += dc.tick(x);
  dc.reset();

  xnyn::Phasor phasor;
  phasor.prepare(48000.0);
  phasor.setFrequency(440.0);
  acc += static_cast<T>(phasor.tick());
  phasor.advance();
  acc += static_cast<T>(phasor.phase());
  phasor.reset();

  xnyn::WhiteNoise noise;
  noise.prepare(48000.0);
  noise.reset(42);
  acc += static_cast<T>(noise.tick());
  noise.advance();
  acc += static_cast<T>(noise.value());
  noise.reset();

  xnyn::Smoother<T> smoother;
  smoother.prepare(48000.0);
  smoother.setTimeMs(T(10));
  smoother.setTarget(x);
  acc += smoother.tick();
  smoother.advance();
  acc += smoother.value();
  smoother.reset();
  smoother.reset(x);

  CHECK(xnyn::isFinite(acc));
}
