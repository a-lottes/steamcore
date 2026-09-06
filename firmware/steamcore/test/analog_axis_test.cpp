#include "steamcore/analog_axis.h"

#include "fake_analog_source.h"
#include "steamcore/game_loop.h"
#include "steamcore/input.h"
#include "test_harness.h"

using steamcore::AxisLevel;
using steamcore::axisLevel;
using steamcore::directionActive;
using steamcore::GameInput;
using steamcore::InputReader;
using steamcore::InputSignal;
using steamcore::kAxisAdcCenter;
using steamcore::kAxisAdcFullScale;
using steamcore::kAxisDeadzoneCounts;
using steamcore::kAxisDeadzonePercent;
using steamcore::test::FakeAnalogSource;

// --- T3: direction battery, compile-time half ---

// AC-2.7's host-provable half: a failed ADC read maps to kNeutral
// upstream, and directionActive(signal, kNeutral, kNeutral) must be
// false for every one of the four directions -- proven here without any
// per-signal error branch.
static_assert(!directionActive(InputSignal::kUp, AxisLevel::kNeutral,
                                AxisLevel::kNeutral),
              "a neutral pair must never activate up");
static_assert(!directionActive(InputSignal::kDown, AxisLevel::kNeutral,
                                AxisLevel::kNeutral),
              "a neutral pair must never activate down");
static_assert(!directionActive(InputSignal::kLeft, AxisLevel::kNeutral,
                                AxisLevel::kNeutral),
              "a neutral pair must never activate left");
static_assert(!directionActive(InputSignal::kRight, AxisLevel::kNeutral,
                                AxisLevel::kNeutral),
              "a neutral pair must never activate right");

// A non-direction signal is never satisfied by axis state, regardless of
// what either axis reads.
static_assert(
    !directionActive(InputSignal::kStart, AxisLevel::kLow, AxisLevel::kLow),
    "start is never driven by axis state");
static_assert(
    !directionActive(InputSignal::kFire, AxisLevel::kHigh, AxisLevel::kHigh),
    "fire is never driven by axis state");
static_assert(!directionActive(InputSignal::kSelect, AxisLevel::kLow,
                                AxisLevel::kHigh),
              "select is never driven by axis state");

// --- T2: threshold battery, compile-time half ---
// Every assertion but the two explicitly-literal ones is written
// relative to the named constants, never to a bare number, so a T8
// retune of the deadzone does not invalidate this battery.

static_assert(axisLevel(kAxisAdcCenter) == AxisLevel::kNeutral,
              "the nominal midpoint itself must be neutral");
static_assert(
    axisLevel(kAxisAdcCenter - kAxisDeadzoneCounts) == AxisLevel::kNeutral,
    "the deadzone's low boundary is still neutral, not low");
static_assert(
    axisLevel(kAxisAdcCenter - kAxisDeadzoneCounts - 1) == AxisLevel::kLow,
    "one past the deadzone's low boundary must read low");
static_assert(
    axisLevel(kAxisAdcCenter + kAxisDeadzoneCounts) == AxisLevel::kNeutral,
    "the deadzone's high boundary is still neutral, not high");
static_assert(
    axisLevel(kAxisAdcCenter + kAxisDeadzoneCounts + 1) == AxisLevel::kHigh,
    "one past the deadzone's high boundary must read high");
static_assert(axisLevel(0) == AxisLevel::kLow,
              "the lowest representable ADC count must read low");
static_assert(axisLevel(kAxisAdcFullScale) == AxisLevel::kHigh,
              "the highest representable ADC count must read high");

// Deliberately concrete: pins today's tuned numbers so a retune after
// T8's on-device evidence is a visible, intentional edit to this row,
// not a silent drift.
static_assert(kAxisDeadzonePercent == 20,
              "today's tuned deadzone percentage (plan §1 Decision 3)");
static_assert(kAxisDeadzoneCounts == 819,
              "today's tuned deadzone count, derived from the percentage "
              "above");

// AC-1.1/AC-1.5: a raw ADC value pushed past the left threshold reaches
// GameInput.left through the completely unmodified InputReader<Source> --
// the walking skeleton, end to end, before any battery exists. The
// centred starting point reads no direction at all.
STEAMCORE_TEST(analog_axis_walking_skeleton_left_reaches_game_input) {
  FakeAnalogSource source;
  InputReader<FakeAnalogSource> reader;

  const GameInput centered = reader.read(source);
  CHECK_EQ(centered.left, false);
  CHECK_EQ(centered.right, false);
  CHECK_EQ(centered.up, false);
  CHECK_EQ(centered.down, false);

  source.setRawX(kAxisAdcCenter - kAxisDeadzoneCounts - 1);
  reader.read(source);                            // 1st agreeing sample
  const GameInput pushedLeft = reader.read(source);  // 2nd -- flips
  CHECK_EQ(pushedLeft.left, true);
  CHECK_EQ(pushedLeft.right, false);
  CHECK_EQ(pushedLeft.up, false);
  CHECK_EQ(pushedLeft.down, false);
}

// AC-1.1/AC-1.2/AC-1.6: a runtime mirror of the static_assert battery
// above, exercised via CHECK so a regression is caught even where the
// compile-time half alone would not distinguish "wrong" from "did not
// run" as clearly.
STEAMCORE_TEST(analog_axis_classifies_centre_and_both_thresholds) {
  CHECK(axisLevel(kAxisAdcCenter) == AxisLevel::kNeutral);
  CHECK(axisLevel(kAxisAdcCenter - kAxisDeadzoneCounts) == AxisLevel::kNeutral);
  CHECK(axisLevel(kAxisAdcCenter - kAxisDeadzoneCounts - 1) ==
        AxisLevel::kLow);
  CHECK(axisLevel(kAxisAdcCenter + kAxisDeadzoneCounts) == AxisLevel::kNeutral);
  CHECK(axisLevel(kAxisAdcCenter + kAxisDeadzoneCounts + 1) ==
        AxisLevel::kHigh);
  CHECK(axisLevel(0) == AxisLevel::kLow);
  CHECK(axisLevel(kAxisAdcFullScale) == AxisLevel::kHigh);
}

// AC-1.6: a value outside the representable ADC range still classifies
// -- a plain threshold comparison, never an addition that could
// overflow, so nothing here is undefined behaviour.
STEAMCORE_TEST(analog_axis_classifies_out_of_range_values_without_ub) {
  CHECK(axisLevel(-1000000) == AxisLevel::kLow);
  CHECK(axisLevel(1000000) == AxisLevel::kHigh);
}

// AC-1.4: the routine is pure -- the same input, fed repeatedly, always
// returns the identical result, with no hidden state to drift.
STEAMCORE_TEST(analog_axis_is_idempotent_over_repeated_calls) {
  const int32_t raw = kAxisAdcCenter - kAxisDeadzoneCounts - 1;
  const AxisLevel first = axisLevel(raw);
  for (int32_t i = 0; i < 100; ++i) {
    CHECK(axisLevel(raw) == first);
  }
}

namespace {

struct CardinalCase {
  int32_t rawX;
  int32_t rawY;
  InputSignal expected;
};

}  // namespace

// AC-1.3: each of the four cardinals, pushed alone, drives exactly its
// own GameInput field through the unmodified InputReader, with the
// other three staying false.
STEAMCORE_TEST(analog_axis_each_cardinal_drives_only_its_own_field) {
  const int32_t low = kAxisAdcCenter - kAxisDeadzoneCounts - 1;
  const int32_t high = kAxisAdcCenter + kAxisDeadzoneCounts + 1;
  const int32_t mid = kAxisAdcCenter;

  const CardinalCase cases[] = {
      {low, mid, InputSignal::kLeft},
      {high, mid, InputSignal::kRight},
      {mid, low, InputSignal::kUp},
      {mid, high, InputSignal::kDown},
  };

  for (const CardinalCase& c : cases) {
    FakeAnalogSource source;
    InputReader<FakeAnalogSource> reader;
    reader.read(source);
    reader.read(source);  // settle at the centred baseline

    source.setRawX(c.rawX);
    source.setRawY(c.rawY);
    reader.read(source);                              // 1st agreeing sample
    const GameInput input = reader.read(source);       // 2nd -- flips

    CHECK_EQ(input.left, c.expected == InputSignal::kLeft);
    CHECK_EQ(input.right, c.expected == InputSignal::kRight);
    CHECK_EQ(input.up, c.expected == InputSignal::kUp);
    CHECK_EQ(input.down, c.expected == InputSignal::kDown);
  }
}

// AC-1.3: a diagonal (both axes past threshold) drives both adjacent
// fields true simultaneously -- no cardinal-snapping to a single
// direction.
STEAMCORE_TEST(analog_axis_diagonal_drives_both_adjacent_fields) {
  FakeAnalogSource source;
  InputReader<FakeAnalogSource> reader;
  reader.read(source);
  reader.read(source);

  source.setRawX(kAxisAdcCenter - kAxisDeadzoneCounts - 1);  // left
  source.setRawY(kAxisAdcCenter - kAxisDeadzoneCounts - 1);  // up

  reader.read(source);
  const GameInput input = reader.read(source);

  CHECK_EQ(input.left, true);
  CHECK_EQ(input.up, true);
  CHECK_EQ(input.right, false);
  CHECK_EQ(input.down, false);
}

// AC-1.4: the two opposite directions on one axis can never both be
// active at once, for either axis, at any sampled raw value -- a
// structural consequence of axisLevel() returning exactly one level,
// checked here rather than only argued.
STEAMCORE_TEST(analog_axis_opposite_directions_never_both_active) {
  for (int32_t raw = -1000; raw <= kAxisAdcFullScale + 1000; raw += 37) {
    const AxisLevel level = axisLevel(raw);
    const bool left = directionActive(InputSignal::kLeft, level, AxisLevel::kNeutral);
    const bool right = directionActive(InputSignal::kRight, level, AxisLevel::kNeutral);
    CHECK(!(left && right));

    const bool up = directionActive(InputSignal::kUp, AxisLevel::kNeutral, level);
    const bool down = directionActive(InputSignal::kDown, AxisLevel::kNeutral, level);
    CHECK(!(up && down));
  }
}

// AC-3.3's host analogue: a sustained centred hold reports all four
// directions false on every single tick, with zero transitions.
STEAMCORE_TEST(analog_axis_sustained_centre_hold_never_fires) {
  FakeAnalogSource source;
  InputReader<FakeAnalogSource> reader;

  for (int32_t i = 0; i < 60; ++i) {
    const GameInput input = reader.read(source);
    CHECK_EQ(input.left, false);
    CHECK_EQ(input.right, false);
    CHECK_EQ(input.up, false);
    CHECK_EQ(input.down, false);
  }
}

// The fake's three scripted button levels pass through readSignal
// unchanged and independently of each other -- proven directly against
// the fake (bypassing InputReader's debounce, which is already covered
// by input_test.cpp for the mechanism itself).
STEAMCORE_TEST(analog_axis_fake_button_levels_pass_through_unchanged) {
  FakeAnalogSource source;
  CHECK_EQ(source.readSignal(InputSignal::kStart), false);
  CHECK_EQ(source.readSignal(InputSignal::kFire), false);
  CHECK_EQ(source.readSignal(InputSignal::kSelect), false);

  source.setButtonLevel(InputSignal::kStart, true);
  CHECK_EQ(source.readSignal(InputSignal::kStart), true);
  CHECK_EQ(source.readSignal(InputSignal::kFire), false);
  CHECK_EQ(source.readSignal(InputSignal::kSelect), false);

  source.setButtonLevel(InputSignal::kFire, true);
  source.setButtonLevel(InputSignal::kSelect, true);
  CHECK_EQ(source.readSignal(InputSignal::kStart), true);
  CHECK_EQ(source.readSignal(InputSignal::kFire), true);
  CHECK_EQ(source.readSignal(InputSignal::kSelect), true);
}
