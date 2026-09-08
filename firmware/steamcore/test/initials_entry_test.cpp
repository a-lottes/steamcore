#include "steamcore/initials_entry.h"

#include "test_harness.h"

using steamcore::GameInput;
using steamcore::InitialsEntry;

namespace {

GameInput held(bool up, bool down, bool fire, bool start = false) {
  GameInput input{};
  input.up = up;
  input.down = down;
  input.fire = fire;
  input.start = start;
  return input;
}

}  // namespace

// AC-2.2: holding `up` for many ticks advances exactly one letter, not
// once per tick -- no auto-repeat. One neutral tick separates `begin()`
// from the press: `up`/`down`/`fire` all latch `true` at `begin()` (R5),
// so a button read as already-held on the very first tick is correctly
// *not* a fresh edge -- a real press starts from a released state.
STEAMCORE_TEST(initials_entry_holding_up_advances_exactly_one_letter) {
  InitialsEntry entry;
  entry.begin();
  entry.update(held(false, false, false));
  CHECK_EQ(entry.letter(0), 'A');
  for (int32_t i = 0; i < 60; ++i) entry.update(held(true, false, false));
  CHECK_EQ(entry.letter(0), 'B');
}

// AC-2.2: a 26-press sweep in each direction (one rising edge per press,
// release in between) returns exactly to 'A' and never produces a
// character outside A-Z at any intermediate step.
STEAMCORE_TEST(initials_entry_26_press_sweep_wraps_back_to_a_and_stays_in_range) {
  InitialsEntry entry;
  entry.begin();
  entry.update(held(false, false, false));  // settle past begin()'s own latch
  for (int32_t i = 0; i < 26; ++i) {
    entry.update(held(true, false, false));  // rising edge
    CHECK(entry.letter(0) >= 'A' && entry.letter(0) <= 'Z');
    entry.update(held(false, false, false));  // release, so the next press is a fresh edge
  }
  CHECK_EQ(entry.letter(0), 'A');

  for (int32_t i = 0; i < 26; ++i) {
    entry.update(held(false, true, false));
    CHECK(entry.letter(0) >= 'A' && entry.letter(0) <= 'Z');
    entry.update(held(false, false, false));
  }
  CHECK_EQ(entry.letter(0), 'A');
}

// R5: `fire` already held at the moment `begin()` is called (galactic-
// invasion's own player holds fire continuously) confirms nothing until
// it is released and pressed again as a genuine fresh edge.
STEAMCORE_TEST(initials_entry_fire_already_held_at_begin_confirms_nothing_until_released) {
  InitialsEntry entry;
  entry.begin();
  for (int32_t i = 0; i < 30; ++i) {
    CHECK(!entry.update(held(false, false, true)));  // still "held", never a fresh edge
  }
  CHECK_EQ(entry.cursor(), 0);

  entry.update(held(false, false, false));  // release
  entry.update(held(false, false, true));   // the first genuine rising edge
  CHECK_EQ(entry.cursor(), 1);
}

// AC-2.3: three `fire` rising edges submit exactly on the third, not
// before -- `update()` returns true only on that tick.
STEAMCORE_TEST(initials_entry_three_fire_edges_submit_on_the_third) {
  InitialsEntry entry;
  entry.begin();
  entry.update(held(false, false, false));  // settle past begin()'s own latch

  auto pressAndRelease = [&entry]() {
    const bool result = entry.update(held(false, false, true));
    entry.update(held(false, false, false));
    return result;
  };

  CHECK(!pressAndRelease());
  CHECK(!entry.complete());
  CHECK_EQ(entry.cursor(), 1);

  CHECK(!pressAndRelease());
  CHECK(!entry.complete());
  CHECK_EQ(entry.cursor(), 2);

  CHECK(pressAndRelease());
  CHECK(entry.complete());
  CHECK_EQ(entry.cursor(), 3);
}

// AC-2.4: `input.start` held for the entire session changes nothing --
// this type does not read it at all.
STEAMCORE_TEST(initials_entry_start_held_throughout_changes_nothing) {
  InitialsEntry withStart;
  withStart.begin();
  InitialsEntry withoutStart;
  withoutStart.begin();

  for (int32_t i = 0; i < 50; ++i) {
    const bool up = (i % 7) == 0;
    const bool down = (i % 11) == 0;
    const bool fire = (i % 5) == 0;
    withStart.update(held(up, down, fire, /*start=*/true));
    withoutStart.update(held(up, down, fire, /*start=*/false));
    CHECK_EQ(withStart.cursor(), withoutStart.cursor());
    for (int32_t p = 0; p < 3; ++p) {
      CHECK_EQ(withStart.letterIndex(p), withoutStart.letterIndex(p));
    }
    if (withStart.complete()) break;
  }
}

// AC-2.5: the same fixed 200-tick input sequence, run on two freshly-
// `begin()`-ed instances, produces byte-identical letter/cursor history
// on every single tick.
STEAMCORE_TEST(initials_entry_replay_is_byte_identical_every_tick) {
  InitialsEntry a;
  InitialsEntry b;
  a.begin();
  b.begin();

  for (int32_t i = 0; i < 200; ++i) {
    const GameInput input =
        held((i % 3) == 0, (i % 4) == 0, (i % 9) == 0, (i % 2) == 0);
    const bool doneA = a.update(input);
    const bool doneB = b.update(input);
    CHECK_EQ(doneA, doneB);
    CHECK_EQ(a.cursor(), b.cursor());
    CHECK_EQ(a.complete(), b.complete());
    for (int32_t p = 0; p < 3; ++p) {
      CHECK_EQ(a.letterIndex(p), b.letterIndex(p));
    }
  }
}
