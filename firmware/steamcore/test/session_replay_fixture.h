#pragma once

#include <cstdint>

#include "steamcore/game_loop.h"

// Shared by the AC-5.1/5.2 determinism test and its coverage check.
// Test-only: not part of the engine's public surface.

namespace steamcore::test {

// One step of the replay: the tick's input, plus the caller's
// end-of-session signal for that same step.
struct ReplayStep {
  GameInput input;
  bool sessionEnded;
};

// >= 20 per AC-5.1. Fixed, hand-written -- no loop-generated pattern, no
// RNG, no clock -- and deliberately shaped to exercise every corner T6's
// coverage test checks for: all three states, a held-start run of >= 3
// steps, a start rising edge observed while in READY and one observed
// while in GAME_OVER, at least one sessionEnded=true step, and both
// values of `fire` observed in every state the sequence visits.
inline constexpr int32_t kSessionReplaySteps = 20;

inline ReplayStep sessionReplayStepAt(int32_t stepIndex) {
  static constexpr ReplayStep kSteps[kSessionReplaySteps] = {
      // READY: fire=false, then fire=true, neither moves the state.
      {GameInput{false, false}, false},
      {GameInput{false, true}, false},
      // Rising edge in READY -> PLAYING, then held for a 3-step run.
      {GameInput{true, false}, false},
      {GameInput{true, false}, false},
      {GameInput{true, false}, false},
      // PLAYING: start released, fire true then false.
      {GameInput{false, true}, false},
      {GameInput{false, false}, false},
      // A start press mid-PLAYING does nothing.
      {GameInput{true, false}, false},
      // Session ends -> GAME_OVER.
      {GameInput{false, false}, true},
      // Rising edge in GAME_OVER -> PLAYING (restart).
      {GameInput{true, false}, false},
      // Ends again immediately (start released first this time).
      {GameInput{false, false}, true},
      // GAME_OVER: fire=false, then fire=true, neither moves the state.
      {GameInput{false, false}, false},
      {GameInput{false, true}, false},
      // Restart again.
      {GameInput{true, false}, false},
      {GameInput{false, false}, false},
      // Ends a third time.
      {GameInput{false, false}, true},
      // Restart once more, held this time.
      {GameInput{true, false}, false},
      {GameInput{true, false}, false},
      {GameInput{false, false}, false},
      // Final end-of-session step.
      {GameInput{false, false}, true},
  };
  return kSteps[stepIndex];
}

}  // namespace steamcore::test
