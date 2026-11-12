#pragma once

#include <algorithm>
#include <bit>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <utility>

#include "xnyn/math/lerp.hpp"

namespace xnyn {

// Single-channel delay line: a circular buffer with integer and linearly interpolated reads.
//
// - Read, then write: with y = read(d); write(x), y[n] = x[n - d]. Delays are in samples
//   (converting from seconds is the caller's job, once per block or per sample) and are
//   clamped to [1, maxDelaySamples()]; NaN maps to 1.
// - prepare(sampleRate, maxDelaySeconds) allocates: maxDelaySamples() =
//   ceil(maxDelaySeconds * sampleRate), clamped to [1, 2^24] (2^24 samples: ~5.8 min at
//   48 kHz, 128 MB for float). Not real-time safe. Everything else is.
// - Before prepare(), every read returns 0 and writes are discarded.
// - No recursion inside the block: a NaN / inf input only reaches the reads whose taps
//   touch it, until it is overwritten or reset(). A feedback loop built around the line
//   recirculates it, though.
// - Copying is deleted (it would allocate); moving leaves the source unprepared.
template <std::floating_point T = float>
class DelayLine {
 public:
  // Every delay up to 2^24 is exact in float, so clamping in T needs no rounding care.
  static constexpr int kMaxDelaySamples = 1 << 24;

  DelayLine() noexcept = default;
  DelayLine(const DelayLine&) = delete;
  DelayLine& operator=(const DelayLine&) = delete;
  DelayLine(DelayLine&& other) noexcept { moveFrom(other); }
  DelayLine& operator=(DelayLine&& other) noexcept {
    if (this != &other) moveFrom(other);
    return *this;
  }
  ~DelayLine() = default;

  // Allocates unless the capacity is unchanged (then the memory is reused), then reset().
  // May throw std::bad_alloc, leaving the line as it was.
  void prepare(double sampleRate, double maxDelaySeconds) {
    // NaN fails both comparisons: min keeps it, max then maps it to 1.
    const double samples =
        std::max(1.0, std::min(std::ceil(maxDelaySeconds * sampleRate), static_cast<double>(kMaxDelaySamples)));
    const auto maxDelay = static_cast<std::uint32_t>(samples);
    // readLinear reaches maxDelay + 1 samples back, and must not see the slot about to be
    // written: capacity >= maxDelay + 2, rounded up to a power of two so wrapping is a mask.
    const std::uint32_t capacity = std::bit_ceil(maxDelay + 2);
    if (!buffer_ || capacity != mask_ + 1) {
      buffer_ = allocate(capacity);  // nothing has changed yet if this throws
      data_ = buffer_.get();
      mask_ = capacity - 1;
    }
    maxDelay_ = static_cast<int>(maxDelay);
    maxDelayT_ = static_cast<T>(maxDelay);
    step_ = 1;
    reset();
  }

  // Zeroes the whole buffer: O(capacity), but no allocation.
  void reset() noexcept {
    std::fill_n(data_, static_cast<std::size_t>(mask_) + 1, T(0));
    writePos_ = 0;
  }

  void write(T x) noexcept {
    data_[writePos_ & mask_] = x;
    writePos_ += step_;
  }

  // x[n - delaySamples], exact.
  [[nodiscard]] T read(int delaySamples) const noexcept {
    const auto d = static_cast<std::uint32_t>(std::max(1, std::min(delaySamples, maxDelay_)));
    return data_[(writePos_ - d) & mask_];
  }

  // Linear interpolation between x[n - floor(d)] and x[n - floor(d) - 1]; equal to read()
  // at integer delays (for finite neighbours: lerp gives NaN when the unused tap is). The
  // fraction is d - floor(d), taken from the delay itself instead of from a
  // position - delay difference, so its resolution is ulp(d) whatever the write position:
  // for float, 2^-8 sample or finer below 2^16 samples of delay.
  [[nodiscard]] T readLinear(T delaySamples) const noexcept {
    const T d = std::max(T(1), std::min(delaySamples, maxDelayT_));  // NaN -> 1
    // d >= 1, so truncation is floor, and the int32 conversion is a single instruction.
    const auto i = static_cast<std::uint32_t>(static_cast<std::int32_t>(d));
    const T frac = d - static_cast<T>(i);
    const std::uint32_t p = writePos_ - i;  // wraps modulo 2^32, a multiple of the capacity
    return lerp(data_[p & mask_], data_[(p - 1) & mask_], frac);
  }

  [[nodiscard]] int maxDelaySamples() const noexcept { return maxDelay_; }

 private:
  // Cache-line aligned: free, and lets whole lines load as full-width vectors.
  static constexpr std::align_val_t kAlignment{64};

  struct AlignedDelete {
    void operator()(T* p) const noexcept { ::operator delete(p, kAlignment); }
  };
  using Buffer = std::unique_ptr<T[], AlignedDelete>;

  // Uninitialized (floating-point types are implicit-lifetime); reset() zeroes it.
  [[nodiscard]] static Buffer allocate(std::uint32_t capacity) {
    return Buffer(static_cast<T*>(::operator new(std::size_t{capacity} * sizeof(T), kAlignment)));
  }

  void moveFrom(DelayLine& other) noexcept {
    buffer_ = std::move(other.buffer_);
    std::copy_n(other.fallback_, kFallbackSize, fallback_);
    data_ = buffer_ ? buffer_.get() : fallback_;
    mask_ = other.mask_;
    writePos_ = other.writePos_;
    step_ = other.step_;
    maxDelay_ = other.maxDelay_;
    maxDelayT_ = other.maxDelayT_;
    other.unprepare();
  }

  void unprepare() noexcept {
    buffer_.reset();
    std::fill_n(fallback_, kFallbackSize, T(0));
    data_ = fallback_;
    mask_ = kFallbackSize - 1;
    writePos_ = 0;
    step_ = 0;
    maxDelay_ = 1;
    maxDelayT_ = T(1);
  }

  // Unprepared state without a branch in the per-sample code: a 4-slot in-object buffer and
  // a frozen write position (step_ = 0). Writes land in slot 0; every delay clamps to 1, so
  // reads touch slots 3 and 2 only, which stay 0.
  static constexpr std::uint32_t kFallbackSize = 4;

  // Raw pointer and 32-bit indices: a store to a float output buffer can't alias them, so
  // they stay in registers across a loop, and 32-bit indices match SIMD gather widths for
  // future multi-voice processing.
  T* data_ = fallback_;                     // buffer_.get(), or fallback_ before prepare
  std::uint32_t mask_ = kFallbackSize - 1;  // capacity - 1, capacity a power of two
  std::uint32_t writePos_ = 0;              // next write slot, unmasked
  std::uint32_t step_ = 0;                  // 1 once prepared
  int maxDelay_ = 1;                        // samples
  T maxDelayT_ = T(1);                      // maxDelay_, exact in T
  T fallback_[kFallbackSize] = {};
  Buffer buffer_;
};

}  // namespace xnyn
