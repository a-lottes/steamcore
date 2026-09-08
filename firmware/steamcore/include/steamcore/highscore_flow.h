#pragma once

#include <cstdint>

#include "steamcore/framebuffer.h"
#include "steamcore/game_loop.h"
#include "steamcore/highscore.h"
#include "steamcore/initials_entry.h"

// The real ENTRY -> TABLE -> INACTIVE flow (highscore-system spec US-2/
// US-3), composing the real `InitialsEntry` (T5) and the two real screens
// (T6) -- replaces T1's throwaway two-phase stub outright, not merely
// extends it.
//
// Example:
//   steamcore::HighscoreFlow flow;
//   flow.begin(12500);
//   while (flow.active()) {
//     const auto step = flow.update(readRealInput());
//     if (step.submitted) { /* record the entry, once */ }
//     flow.render(fb, table, "GALACTIC INVASION");
//   }
//
// Contract:
//  - `begin(score)` enters ENTRY, remembering `score` for the entry
//    screen's own header. `active()` is true in both ENTRY and TABLE,
//    false before the first `begin()` and again once `finished` fires.
//  - ENTRY: delegates every tick to the real `InitialsEntry`; the tick
//    its own `update()` returns true (the third letter locks in,
//    AC-2.3), this flow copies out the three confirmed letters, moves to
//    TABLE, and this call's own `Step.submitted` is true -- on no other
//    tick.
//  - TABLE: waits for a `start` rising edge (AC-3.3) -- its own edge
//    tracker, independently latched `true` the instant TABLE is entered
//    (the same R5 reasoning `InitialsEntry` already applies to its own
//    three trackers: a `start` already held at that instant must not
//    read as a fresh press). That edge moves to INACTIVE and this call's
//    own `Step.finished` is true -- on no other tick.
//  - `render()` draws the ENTRY screen while in ENTRY, the TABLE screen
//    (given the caller's own current `table`/`displayName`) while in
//    TABLE, and nothing at all while INACTIVE (before the first
//    `begin()`, or after `finished`) -- never clears the framebuffer
//    (the caller does, `HighscoreGame`'s own contract).
//  - Single-threaded, nothing throws, no error code, no dynamic
//    allocation -- same inherited contract as every other steamcore type.
namespace steamcore {

struct HighscoreFlowStep {
  bool active = false;
  bool submitted = false;
  bool finished = false;
};

class HighscoreFlow {
 public:
  void begin(int32_t score);

  bool active() const { return phase_ != Phase::INACTIVE; }

  HighscoreFlowStep update(const GameInput& input);

  // The three confirmed letters, valid from the tick `Step.submitted` was
  // true onward (garbage/stale before that, per the "caller checks
  // `submitted`" contract every steamcore type uses for "not ready yet").
  void initials(char (&out)[kInitialsCount]) const;

  int32_t score() const { return score_; }

  void render(Framebuffer& fb, const HighscoreTable& table,
              const char* displayName) const;

 private:
  enum class Phase { INACTIVE, ENTRY, TABLE };

  Phase phase_ = Phase::INACTIVE;
  int32_t score_ = 0;
  InitialsEntry entry_;
  char initials_[kInitialsCount] = {};
  bool prevStart_ = false;
};

}  // namespace steamcore
