#include <cstdio>

#include "session_replay_fixture.h"
#include "steamcore/game_state.h"
#include "test_harness.h"

using steamcore::GameInput;
using steamcore::GameSession;
using steamcore::GameState;
using steamcore::test::kSessionReplaySteps;
using steamcore::test::ReplayStep;
using steamcore::test::sessionReplayStepAt;

// T6: the fixture is only useful if it actually exercises everything the
// determinism/sensitivity proof below depends on. Re-derived by
// replaying it with a real GameSession, not by reading its literals --
// without this, AC-5.1/5.2 could pass vacuously on a fixture that never
// reaches GAME_OVER or never holds `start` (the exact second half of
// `game-loop`'s own Round-1 review finding).
STEAMCORE_TEST(session_replay_fixture_covers_states_edges_and_fire_values) {
  GameSession session;
  bool prevStart = false;

  bool sawReady = false;
  bool sawPlaying = false;
  bool sawGameOver = false;
  bool sawStartRisingInReady = false;
  bool sawStartRisingInGameOver = false;
  bool sawSessionEnded = false;
  bool sawFireFalseInReady = false;
  bool sawFireTrueInReady = false;
  bool sawFireFalseInPlaying = false;
  bool sawFireTrueInPlaying = false;
  bool sawFireFalseInGameOver = false;
  bool sawFireTrueInGameOver = false;
  int32_t heldStartRun = 0;
  int32_t maxHeldStartRun = 0;

  for (int32_t i = 0; i < kSessionReplaySteps; ++i) {
    const ReplayStep step = sessionReplayStepAt(i);
    const GameState stateBefore = session.state();
    const bool startRising = step.input.start && !prevStart;

    switch (stateBefore) {
      case GameState::READY:
        sawReady = true;
        if (step.input.fire) sawFireTrueInReady = true;
        else sawFireFalseInReady = true;
        if (startRising) sawStartRisingInReady = true;
        break;
      case GameState::PLAYING:
        sawPlaying = true;
        if (step.input.fire) sawFireTrueInPlaying = true;
        else sawFireFalseInPlaying = true;
        break;
      case GameState::GAME_OVER:
        sawGameOver = true;
        if (step.input.fire) sawFireTrueInGameOver = true;
        else sawFireFalseInGameOver = true;
        if (startRising) sawStartRisingInGameOver = true;
        break;
    }
    if (step.sessionEnded) sawSessionEnded = true;

    if (step.input.start) {
      ++heldStartRun;
      if (heldStartRun > maxHeldStartRun) maxHeldStartRun = heldStartRun;
    } else {
      heldStartRun = 0;
    }

    prevStart = step.input.start;
    session.advance(step.input, step.sessionEnded);
  }

  CHECK(sawReady);
  CHECK(sawPlaying);
  CHECK(sawGameOver);
  CHECK(sawStartRisingInReady);
  CHECK(sawStartRisingInGameOver);
  CHECK(sawSessionEnded);
  CHECK(sawFireFalseInReady);
  CHECK(sawFireTrueInReady);
  CHECK(sawFireFalseInPlaying);
  CHECK(sawFireTrueInPlaying);
  CHECK(sawFireFalseInGameOver);
  CHECK(sawFireTrueInGameOver);
  CHECK(maxHeldStartRun >= 3);
}

// AC-5.1: two independently-constructed sessions, stepped in lockstep
// through the whole fixture, are compared after EVERY individual step --
// not just the last -- so a divergence introduced and later overwritten
// cannot hide behind the final state.
STEAMCORE_TEST(game_state_replay_is_deterministic_after_every_step) {
  GameSession sessionA;
  GameSession sessionB;

  int32_t firstMismatchStep = -1;

  for (int32_t i = 0; i < kSessionReplaySteps; ++i) {
    const ReplayStep step = sessionReplayStepAt(i);
    sessionA.advance(step.input, step.sessionEnded);
    sessionB.advance(step.input, step.sessionEnded);

    if (sessionA.state() != sessionB.state()) {
      firstMismatchStep = i;
      break;
    }
  }

  if (firstMismatchStep != -1) {
    std::printf("determinism mismatch at step %d\n", firstMismatchStep);
  }
  CHECK_EQ(firstMismatchStep, -1);
}

namespace {

enum class FlipField { kStart, kFire, kSessionEnded };

// Replays the fixture twice in lockstep, with run B having exactly one
// field flipped at exactly one step, and reports whether the two runs'
// states were ever observed to differ at that step or any later one (a
// state machine can legitimately re-converge -- e.g. run A gets its own
// rising edge later -- so "ever differ from here on", not "differ at
// every subsequent step", is what a real divergence looks like).
bool replayDivergesWhenFlipped(FlipField field, int32_t flipAtStep) {
  GameSession sessionA;
  GameSession sessionB;

  for (int32_t i = 0; i < kSessionReplaySteps; ++i) {
    const ReplayStep stepA = sessionReplayStepAt(i);
    ReplayStep stepB = stepA;
    if (i == flipAtStep) {
      switch (field) {
        case FlipField::kStart:
          stepB.input.start = !stepB.input.start;
          break;
        case FlipField::kFire:
          stepB.input.fire = !stepB.input.fire;
          break;
        case FlipField::kSessionEnded:
          stepB.sessionEnded = !stepB.sessionEnded;
          break;
      }
    }

    sessionA.advance(stepA.input, stepA.sessionEnded);
    sessionB.advance(stepB.input, stepB.sessionEnded);

    if (sessionA.state() != sessionB.state()) return true;
  }
  return false;
}

}  // namespace

// AC-5.2: the sensitivity sweep is deliberately asymmetric across the
// three input fields, not a blind copy of `game-loop`'s three-way sweep
// (which would be wrong here): `start` and `sessionEnded` must each be
// shown capable of producing a detected divergence somewhere in the
// fixture, while `fire` must be shown to NEVER produce one, at any step
// -- that inertness is AC-2.3/3.2/4.3's claim proven across the whole
// replay instead of at three hand-picked steps.
STEAMCORE_TEST(game_state_replay_sensitivity_sweep_is_asymmetric) {
  bool startEverDetected = false;
  bool sessionEndedEverDetected = false;
  bool fireEverDetected = false;

  for (int32_t i = 0; i < kSessionReplaySteps; ++i) {
    if (replayDivergesWhenFlipped(FlipField::kStart, i)) startEverDetected = true;
    if (replayDivergesWhenFlipped(FlipField::kSessionEnded, i)) sessionEndedEverDetected = true;
    if (replayDivergesWhenFlipped(FlipField::kFire, i)) fireEverDetected = true;
  }

  CHECK(startEverDetected);
  CHECK(sessionEndedEverDetected);
  CHECK(!fireEverDetected);
}
