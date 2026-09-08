#pragma once

#include <cstdint>

#include "steamcore/game_loop.h"
#include "steamcore/highscore.h"

// The arcade-style 3-letter initials-entry input state machine (spec
// US-2) -- pure logic, no rendering, 100% host-testable, the same
// posture galactic-invasion's own game logic takes despite eventually
// running on real hardware.
//
// Example:
//   steamcore::InitialsEntry entry;
//   entry.begin();
//   while (!entry.complete()) {
//     entry.update(readRealInput());
//   }
//   // entry.letter(0), entry.letter(1), entry.letter(2) are the result.
//
// Contract:
//  - Letters are held as indices in [0, kLetterCount), rendered only as
//    `kFirstLetter + index` -- never a character outside A-Z is
//    representable, structurally, not by a runtime check.
//  - `up`/`down` step the active position's letter by exactly one, with
//    wraparound (Z->A, A->Z), on a rising edge only -- holding the button
//    never repeats (AC-2.2).
//  - `fire`'s rising edge locks the active position's current letter and
//    advances the cursor; locking the third position completes entry
//    (AC-2.3) -- `update()` returns true on exactly that tick.
//  - `input.start` is never read by this type at all -- not ignored, not
//    checked and discarded, simply absent from every expression in this
//    header and its `.cpp` (AC-2.4; a lint rule, T13, makes this
//    structural rather than a reviewer's promise).
//  - `begin()` latches all three edge trackers `true`, not `false`: a
//    caller (galactic-invasion) whose player holds `fire` continuously at
//    the moment a round ends would otherwise have that already-held press
//    read as a fresh rising edge on this screen's very first tick,
//    silently confirming letter 1 before the player ever sees the screen.
//  - Deterministic: the exact same input sequence from a freshly-`begin()`
//    instance produces byte-identical letter/cursor history every time
//    (AC-2.5) -- no wall-clock read, no unseeded RNG anywhere in this type.
//  - Single-threaded, nothing throws, no error code, no dynamic
//    allocation -- same inherited contract as every other steamcore type.
namespace steamcore {

class InitialsEntry {
 public:
  // Resets to position 0, every letter at 'A' (spec A11), not yet
  // complete, and latches every edge tracker `true` (see this file's own
  // contract above).
  void begin();

  // Advances the state machine by one tick. Returns true on exactly the
  // tick the third position locks in -- false on every other tick,
  // including every tick after that one (calling `update()` again once
  // `complete()` is already true is a safe no-op, changing nothing).
  bool update(const GameInput& input);

  // The 0-based letter index in [0, kLetterCount) currently held at
  // `position` (in [0, kInitialsCount)). An out-of-range `position`
  // returns 0 rather than undefined behaviour.
  int32_t letterIndex(int32_t position) const;

  // The actual character (`kFirstLetter + letterIndex(position)`) held at
  // `position`.
  char letter(int32_t position) const;

  // The active position in [0, kInitialsCount) -- the one `up`/`down`
  // currently affects. Once `complete()` is true, this no longer names a
  // meaningful active position (every position is already locked); a
  // caller must stop rendering a cursor once `complete()` holds.
  int32_t cursor() const { return cursor_; }

  bool complete() const { return complete_; }

 private:
  int32_t letters_[kInitialsCount] = {};
  int32_t cursor_ = 0;
  bool complete_ = false;
  bool prevUp_ = false;
  bool prevDown_ = false;
  bool prevFire_ = false;
};

}  // namespace steamcore
