#include "steamcore/input.h"

#include "fake_input_source.h"
#include "test_harness.h"

using steamcore::Debouncer;
using steamcore::GameInput;
using steamcore::InputReader;
using steamcore::InputSignal;
using steamcore::kInputSignalCount;
using steamcore::test::FakeInputSource;

namespace {

// All seven fields, independently redeclared here rather than reused
// from input.h's own steamcore::detail::kInputFields -- if that
// production table were ever wrong, a test that shared it could not
// catch it. In spec declaration order.
constexpr bool GameInput::* kAllFields[kInputSignalCount] = {
    &GameInput::start, &GameInput::fire,  &GameInput::select,
    &GameInput::up,    &GameInput::down,  &GameInput::left,
    &GameInput::right,
};

// Presses then releases exactly `signal` (via `field`, its GameInput
// member) and asserts: a clean cycle on that field, and every other of
// the seven fields reads false on every tick of the whole sequence --
// the "independently of the other six" half of AC-1.1.
void checkPressReleaseCyclesOnlyThatSignal(InputSignal signal,
                                            bool GameInput::* field) {
  FakeInputSource source;
  InputReader<FakeInputSource> reader;

  auto checkOthersFalse = [&](const GameInput& input) {
    for (bool GameInput::* other : kAllFields) {
      if (other == field) continue;
      CHECK_EQ(input.*other, false);
    }
  };

  checkOthersFalse(reader.read(source));
  checkOthersFalse(reader.read(source));

  source.setLevel(signal, true);
  checkOthersFalse(reader.read(source));  // 1st agreeing sample
  const GameInput pressed = reader.read(source);  // 2nd -- flips
  CHECK_EQ(pressed.*field, true);
  checkOthersFalse(pressed);

  source.setLevel(signal, false);
  checkOthersFalse(reader.read(source));  // 1st agreeing release sample
  const GameInput released = reader.read(source);  // 2nd -- flips back
  CHECK_EQ(released.*field, false);
  checkOthersFalse(released);
}

// T4: a scripted raw pattern for one signal, built so every disagreement
// with the current level is a single isolated sample (never two
// consecutive raw disagreements) except for the two genuine press/release
// events, each of which supplies exactly kDebounceSamples (2) consecutive
// agreeing disagreements. A correct debouncer absorbs every isolated blip
// and reports exactly two transitions (one press, one release); this is
// the fixture T4's tests apply per-signal and, phase-shifted, to all
// seven at once.
constexpr bool kBouncePattern[] = {
    false, false,                              // settled low
    true,  false, true,  false, true,  true,   // noisy press -> 1 transition
    true,  true,                               // held true
    false, true,  false, true,  false, false,  // noisy release -> 1 transition
};
constexpr int32_t kBouncePatternLength =
    sizeof(kBouncePattern) / sizeof(kBouncePattern[0]);

// Feeds `pattern` into `signal` through a fresh InputReader (all other
// signals held released throughout) and counts how many times `field`'s
// value changes across the *debounced* output -- the number of clean
// transitions InputReader actually reports, not a count of raw wiggle.
int32_t countTransitions(InputSignal signal, bool GameInput::* field,
                          const bool* pattern, int32_t length) {
  FakeInputSource source;
  InputReader<FakeInputSource> reader;
  bool previous = false;
  int32_t transitions = 0;
  for (int32_t i = 0; i < length; ++i) {
    source.setLevel(signal, pattern[i]);
    const bool current = reader.read(source).*field;
    if (current != previous) ++transitions;
    previous = current;
  }
  return transitions;
}

}  // namespace

// Walking skeleton (T2): one signal (start), press then release, through
// the real InputReader<FakeInputSource> pipeline end to end -- fake
// source -> Debouncer -> populated GameInput. T3/T4 add the exhaustive
// per-signal and bounce batteries; this is "does the shape of the
// pipeline work at all".
STEAMCORE_TEST(input_reader_start_press_then_release_is_one_clean_cycle) {
  FakeInputSource source;
  InputReader<FakeInputSource> reader;

  // Released.
  CHECK_EQ(reader.read(source).start, false);
  CHECK_EQ(reader.read(source).start, false);

  // Press: kDebounceSamples (2) agreeing samples needed before it flips.
  source.setLevel(InputSignal::kStart, true);
  CHECK_EQ(reader.read(source).start, false);  // 1st agreeing sample
  const GameInput afterFlip = reader.read(source);  // 2nd -- flips
  CHECK_EQ(afterFlip.start, true);
  CHECK_EQ(afterFlip.fire, false);  // untouched signal stays false
  CHECK_EQ(reader.read(source).start, true);  // stays true -- a level

  // Release: same two-sample confirmation to flip back.
  source.setLevel(InputSignal::kStart, false);
  CHECK_EQ(reader.read(source).start, true);   // 1st agreeing sample
  CHECK_EQ(reader.read(source).start, false);  // 2nd -- flips back
}

// F1 (peer-review): Debouncer is public on the recorded justification
// (plan §1 Decision 8, NFR-6) that AC-2.1 tests it in isolation -- this
// exercises it directly, not only through InputReader, so that
// justification is actually true. Same two-sample cycle as the walking
// skeleton above, but against the mechanism itself, and it also proves
// level() (otherwise uncalled anywhere) agrees with sample()'s return.
STEAMCORE_TEST(debouncer_sample_and_level_agree_through_a_full_cycle) {
  Debouncer<2> debouncer;
  CHECK_EQ(debouncer.level(), false);

  CHECK_EQ(debouncer.sample(true), false);  // 1st agreeing sample
  CHECK_EQ(debouncer.level(), false);
  CHECK_EQ(debouncer.sample(true), true);  // 2nd -- flips
  CHECK_EQ(debouncer.level(), true);

  CHECK_EQ(debouncer.sample(false), true);  // 1st agreeing release sample
  CHECK_EQ(debouncer.level(), true);
  CHECK_EQ(debouncer.sample(false), false);  // 2nd -- flips back
  CHECK_EQ(debouncer.level(), false);
}

// AC-1.1: one named test per signal -- pressed and released alone, the
// other six independently unaffected.
STEAMCORE_TEST(input_reader_start_alone_cycles_only_start) {
  checkPressReleaseCyclesOnlyThatSignal(InputSignal::kStart,
                                         &GameInput::start);
}

STEAMCORE_TEST(input_reader_fire_alone_cycles_only_fire) {
  checkPressReleaseCyclesOnlyThatSignal(InputSignal::kFire,
                                         &GameInput::fire);
}

STEAMCORE_TEST(input_reader_select_alone_cycles_only_select) {
  checkPressReleaseCyclesOnlyThatSignal(InputSignal::kSelect,
                                         &GameInput::select);
}

STEAMCORE_TEST(input_reader_up_alone_cycles_only_up) {
  checkPressReleaseCyclesOnlyThatSignal(InputSignal::kUp, &GameInput::up);
}

STEAMCORE_TEST(input_reader_down_alone_cycles_only_down) {
  checkPressReleaseCyclesOnlyThatSignal(InputSignal::kDown,
                                         &GameInput::down);
}

STEAMCORE_TEST(input_reader_left_alone_cycles_only_left) {
  checkPressReleaseCyclesOnlyThatSignal(InputSignal::kLeft,
                                         &GameInput::left);
}

STEAMCORE_TEST(input_reader_right_alone_cycles_only_right) {
  checkPressReleaseCyclesOnlyThatSignal(InputSignal::kRight,
                                         &GameInput::right);
}

// AC-1.2: held continuously true across many ticks reads true on every
// one of them -- a level, not a single pulse.
STEAMCORE_TEST(input_reader_held_signal_reads_true_every_tick) {
  FakeInputSource source;
  InputReader<FakeInputSource> reader;

  source.setLevel(InputSignal::kFire, true);
  reader.read(source);  // 1st agreeing sample
  reader.read(source);  // 2nd -- flips true

  int32_t trueCount = 0;
  for (int32_t i = 0; i < 10; ++i) {
    if (reader.read(source).fire) ++trueCount;
  }
  CHECK_EQ(trueCount, 10);
}

// AC-1.3: no signal asserted -- all seven read false on the very first
// read() and on every tick of an all-released run. No floating or
// undefined state.
STEAMCORE_TEST(input_reader_cold_start_all_seven_false) {
  FakeInputSource source;
  InputReader<FakeInputSource> reader;

  for (int32_t tick = 0; tick < 10; ++tick) {
    const GameInput input = reader.read(source);
    for (bool GameInput::* field : kAllFields) {
      CHECK_EQ(input.*field, false);
    }
  }
}

// AC-1.4: two direction signals held simultaneously both read true,
// independently -- no diagonal resolution, no collapsing.
STEAMCORE_TEST(input_reader_up_and_right_held_together_both_read_true) {
  FakeInputSource source;
  InputReader<FakeInputSource> reader;

  source.setLevel(InputSignal::kUp, true);
  source.setLevel(InputSignal::kRight, true);
  reader.read(source);  // 1st agreeing sample for both
  const GameInput input = reader.read(source);  // 2nd -- both flip

  CHECK_EQ(input.up, true);
  CHECK_EQ(input.right, true);
  CHECK_EQ(input.down, false);
  CHECK_EQ(input.left, false);
}

// AC-1.4: all four directions held at once -- every field reads true
// independently, still no interpretation applied.
STEAMCORE_TEST(input_reader_all_four_directions_held_together) {
  FakeInputSource source;
  InputReader<FakeInputSource> reader;

  source.setLevel(InputSignal::kUp, true);
  source.setLevel(InputSignal::kDown, true);
  source.setLevel(InputSignal::kLeft, true);
  source.setLevel(InputSignal::kRight, true);
  reader.read(source);
  const GameInput input = reader.read(source);

  CHECK_EQ(input.up, true);
  CHECK_EQ(input.down, true);
  CHECK_EQ(input.left, true);
  CHECK_EQ(input.right, true);
}

// A10: detail::kInputFields' seven entries are distinct and in exactly
// GameInput's/InputSignal's declaration order -- the production table
// itself, not a copy.
STEAMCORE_TEST(input_field_table_is_distinct_and_in_declaration_order) {
  using steamcore::detail::kInputFields;

  int32_t mismatches = 0;
  for (int32_t i = 0; i < kInputSignalCount; ++i) {
    if (kInputFields[i] != kAllFields[i]) ++mismatches;
  }
  CHECK_EQ(mismatches, 0);

  int32_t duplicates = 0;
  for (int32_t i = 0; i < kInputSignalCount; ++i) {
    for (int32_t j = i + 1; j < kInputSignalCount; ++j) {
      if (kInputFields[i] == kInputFields[j]) ++duplicates;
    }
  }
  CHECK_EQ(duplicates, 0);
}

// AC-2.1: the bounce fixture applied to each signal in turn -- despite the
// raw noise, exactly one transition is reported for the press and one for
// the release.
STEAMCORE_TEST(input_reader_start_bounce_yields_exactly_two_transitions) {
  CHECK_EQ(countTransitions(InputSignal::kStart, &GameInput::start,
                             kBouncePattern, kBouncePatternLength),
           2);
}

STEAMCORE_TEST(input_reader_fire_bounce_yields_exactly_two_transitions) {
  CHECK_EQ(countTransitions(InputSignal::kFire, &GameInput::fire,
                             kBouncePattern, kBouncePatternLength),
           2);
}

STEAMCORE_TEST(input_reader_select_bounce_yields_exactly_two_transitions) {
  CHECK_EQ(countTransitions(InputSignal::kSelect, &GameInput::select,
                             kBouncePattern, kBouncePatternLength),
           2);
}

STEAMCORE_TEST(input_reader_up_bounce_yields_exactly_two_transitions) {
  CHECK_EQ(countTransitions(InputSignal::kUp, &GameInput::up, kBouncePattern,
                             kBouncePatternLength),
           2);
}

STEAMCORE_TEST(input_reader_down_bounce_yields_exactly_two_transitions) {
  CHECK_EQ(countTransitions(InputSignal::kDown, &GameInput::down,
                             kBouncePattern, kBouncePatternLength),
           2);
}

STEAMCORE_TEST(input_reader_left_bounce_yields_exactly_two_transitions) {
  CHECK_EQ(countTransitions(InputSignal::kLeft, &GameInput::left,
                             kBouncePattern, kBouncePatternLength),
           2);
}

STEAMCORE_TEST(input_reader_right_bounce_yields_exactly_two_transitions) {
  CHECK_EQ(countTransitions(InputSignal::kRight, &GameInput::right,
                             kBouncePattern, kBouncePatternLength),
           2);
}

// AC-2.2: the same fixture applied to all seven signals at once, each
// phase-shifted by a different number of ticks so no two settle in
// lockstep, still yields exactly two transitions per field -- debouncing
// runs independently per signal, not on shared state.
STEAMCORE_TEST(input_reader_seven_simultaneous_bounces_each_independent) {
  constexpr int32_t kOffsets[kInputSignalCount] = {0, 1, 2, 3, 4, 5, 6};
  constexpr int32_t kTotalTicks = kBouncePatternLength + 6;

  auto rawAt = [](int32_t signalIndex, int32_t tick) {
    const int32_t patternIndex = tick - kOffsets[signalIndex];
    if (patternIndex < 0 || patternIndex >= kBouncePatternLength) return false;
    return kBouncePattern[patternIndex];
  };

  FakeInputSource source;
  InputReader<FakeInputSource> reader;
  GameInput previous{};
  int32_t transitions[kInputSignalCount] = {};

  for (int32_t tick = 0; tick < kTotalTicks; ++tick) {
    for (int32_t s = 0; s < kInputSignalCount; ++s) {
      source.setLevel(static_cast<InputSignal>(s), rawAt(s, tick));
    }
    const GameInput current = reader.read(source);
    for (int32_t s = 0; s < kInputSignalCount; ++s) {
      if (current.*kAllFields[s] != previous.*kAllFields[s]) ++transitions[s];
    }
    previous = current;
  }

  for (int32_t s = 0; s < kInputSignalCount; ++s) {
    CHECK_EQ(transitions[s], 2);
  }
}

// NFR-1 (host half): a level flips on exactly the Samples-th agreeing
// sample and not before -- pinned as a sample count, run at Samples = 1,
// 2 and 5 via the template parameter so the ≤33 ms window at 60 Hz is
// proven as a threshold, not eyeballed from one hardcoded case.
template <int32_t Samples>
void checkFlipsOnExactlyTheNthAgreeingSample() {
  FakeInputSource source;
  InputReader<FakeInputSource, Samples> reader;

  source.setLevel(InputSignal::kStart, true);
  for (int32_t i = 1; i < Samples; ++i) {
    CHECK_EQ(reader.read(source).start, false);  // not yet -- sample i
  }
  CHECK_EQ(reader.read(source).start, true);  // exactly the Samples-th
}

STEAMCORE_TEST(input_reader_flips_on_exactly_the_first_sample_at_samples_1) {
  checkFlipsOnExactlyTheNthAgreeingSample<1>();
}

STEAMCORE_TEST(input_reader_flips_on_exactly_the_second_sample_at_samples_2) {
  checkFlipsOnExactlyTheNthAgreeingSample<2>();
}

STEAMCORE_TEST(input_reader_flips_on_exactly_the_fifth_sample_at_samples_5) {
  checkFlipsOnExactlyTheNthAgreeingSample<5>();
}
