#pragma once

#include <cstdint>

#include "steamcore/input.h"

// Pure ADC-count -> direction-boolean math (analog-joystick-input plan §1
// Decisions 2-5): the whole new decision surface this feature adds, kept
// entirely on the host-testable side of the ESP-IDF boundary. No ESP-IDF
// header, no allocation, no clock read -- a raw ADC sample in, a level or
// a boolean out, nothing else.
//
// One potentiometer axis cannot physically be at both extremes at once,
// so the result is a named three-state level, never a `{bool low; bool
// high;}` pair that would make that impossible state representable (same
// "a state is named, never numbered" convention game_state.h already
// established for GameState).

namespace steamcore {

enum class AxisLevel { kNeutral, kLow, kHigh };

// 12-bit ADC (0-4095), the ESP32-S3's default oneshot resolution.
inline constexpr int32_t kAxisAdcBitWidth = 12;
inline constexpr int32_t kAxisAdcFullScale = (1 << kAxisAdcBitWidth) - 1;
inline constexpr int32_t kAxisAdcCenter = kAxisAdcFullScale / 2;

// +/-20% of full scale around the nominal midpoint: roughly 3x a cheap
// module's typical rest scatter (~5%) plus ADC noise (~20-50 counts),
// while still only requiring ~40% of each half-travel to register a
// direction. One named constant -- a T8 on-device retune is a one-line
// change here, not a rewrite of the routine below.
inline constexpr int32_t kAxisDeadzonePercent = 20;
inline constexpr int32_t kAxisDeadzoneCounts =
    kAxisAdcFullScale * kAxisDeadzonePercent / 100;

// Classifies one raw ADC sample. Values outside [0, kAxisAdcFullScale]
// still classify correctly -- this is a plain comparison against fixed
// thresholds, never an addition that could overflow. The deadzone
// boundary itself (kAxisAdcCenter +/- kAxisDeadzoneCounts) reads as
// kNeutral; only strictly past it does a side become kLow/kHigh.
constexpr AxisLevel axisLevel(int32_t raw) {
  if (raw < kAxisAdcCenter - kAxisDeadzoneCounts) return AxisLevel::kLow;
  if (raw > kAxisAdcCenter + kAxisDeadzoneCounts) return AxisLevel::kHigh;
  return AxisLevel::kNeutral;
}

namespace detail {

// Which axis and which AxisLevel each direction signal requires to read
// `true` -- one table entry per direction, not a bespoke branch, the
// same shape input.h's own kInputFields already established. The
// axis-to-extreme polarity here is a placeholder pending T8's on-device
// confirmation (spec A2): a wrong guess is a one-line edit to this
// table, never a change to the port-level Source.
struct DirectionPolarity {
  InputSignal signal;
  bool isXAxis;      // true = reads x, false = reads y
  AxisLevel active;  // the level that axis must be at for this signal
};

inline constexpr DirectionPolarity kDirectionPolarities[4] = {
    {InputSignal::kUp, false, AxisLevel::kLow},
    {InputSignal::kDown, false, AxisLevel::kHigh},
    {InputSignal::kLeft, true, AxisLevel::kLow},
    {InputSignal::kRight, true, AxisLevel::kHigh},
};

}  // namespace detail

// Reports whether `signal` (one of the four directions) is active given
// both axes' already-classified levels. A non-direction signal (start,
// fire, select) is never satisfied by axis state and always reads
// `false` -- this is also the ADC-read-failure fallback: a failed read
// maps to AxisLevel::kNeutral upstream, and directionActive(signal,
// kNeutral, kNeutral) is false for all four directions by construction,
// proven on the host without a per-signal error branch.
constexpr bool directionActive(InputSignal signal, AxisLevel x, AxisLevel y) {
  for (const detail::DirectionPolarity& p : detail::kDirectionPolarities) {
    if (p.signal == signal) {
      return (p.isXAxis ? x : y) == p.active;
    }
  }
  return false;
}

}  // namespace steamcore
