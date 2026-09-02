#include <cstdio>

#include "fb_compare.h"
#include "replay_fixture.h"
#include "steamcore/framebuffer.h"
#include "steamcore/game_loop.h"
#include "test_harness.h"

using steamcore::Framebuffer;
using steamcore::GameInput;
using steamcore::GameLoop;
using steamcore::test::framebuffersEqual;
using steamcore::test::kReplayTicks;
using steamcore::test::replayInputAt;
using steamcore::test::ReplayGame;

// AC-4.1/AC-4.3: two independently-constructed consumer+framebuffer pairs,
// stepped in lockstep with the exact same input sequence, are compared
// after EVERY individual tick -- not just the last -- so a divergence
// introduced and later overwritten cannot hide behind the final frame.
STEAMCORE_TEST(game_loop_replay_is_deterministic_after_every_tick) {
  ReplayGame gameA;
  ReplayGame gameB;
  Framebuffer fbA;
  Framebuffer fbB;
  GameLoop<ReplayGame> loopA(gameA, fbA);
  GameLoop<ReplayGame> loopB(gameB, fbB);

  int32_t firstMismatchTick = -1;
  int32_t mismatchX = -1;
  int32_t mismatchY = -1;

  for (int32_t i = 0; i < kReplayTicks; ++i) {
    const GameInput input = replayInputAt(i);
    loopA.tick(input);
    loopB.tick(input);

    int32_t x = -1;
    int32_t y = -1;
    if (!framebuffersEqual(fbA, fbB, &x, &y)) {
      firstMismatchTick = i;
      mismatchX = x;
      mismatchY = y;
      break;
    }
  }

  if (firstMismatchTick != -1) {
    std::printf(
        "determinism mismatch at tick %d, first differing pixel (%d,%d)\n",
        firstMismatchTick, mismatchX, mismatchY);
  }
  CHECK_EQ(firstMismatchTick, -1);
}

namespace {

// Diverges exactly one field (`start` or `fire`) at one tick between two
// otherwise-identical runs and returns whether a framebuffer difference
// was ever detected. Shared by both sensitivity tests below so a defect
// in the loop itself can't be a reason the two report different results.
// Internal linkage, like every other test-local helper here (review F13):
// all test translation units link into one binary, so a namespace-scope
// helper with external linkage is one name collision away from an ODR
// violation the linker need not diagnose.
bool replaySequenceDetectsDivergenceIn(bool divergeStart, bool divergeFire) {
  ReplayGame gameA;
  ReplayGame gameB;
  Framebuffer fbA;
  Framebuffer fbB;
  GameLoop<ReplayGame> loopA(gameA, fbA);
  GameLoop<ReplayGame> loopB(gameB, fbB);

  constexpr int32_t kDivergeAtTick = kReplayTicks / 2;

  bool detectedDifference = false;
  for (int32_t i = 0; i < kReplayTicks; ++i) {
    const GameInput inputA = replayInputAt(i);
    GameInput inputB = inputA;
    if (i == kDivergeAtTick) {
      if (divergeStart) inputB.start = !inputB.start;
      if (divergeFire) inputB.fire = !inputB.fire;
    }

    loopA.tick(inputA);
    loopB.tick(inputB);

    if (!framebuffersEqual(fbA, fbB)) {
      detectedDifference = true;
      break;
    }
  }

  return detectedDifference;
}

}  // namespace

// Proves the replay fixture is actually sensitive to its input: without
// this, a consumer that silently ignored GameInput entirely would pass
// the determinism test above just as cleanly as a correct one -- a green
// suite that verifies nothing, the exact false-green class the last two
// features' reviews kept finding. Two separate tests, one per field
// (review F1): a single test that only ever diverged `fire` left a
// consumer that dropped `start` handling entirely undetected -- deleting
// `if (input.start) ...` from ReplayGame::update left all twelve
// game-loop tests green. Each field must independently prove it can be
// diverged and detected.
STEAMCORE_TEST(game_loop_replay_fixture_is_sensitive_to_start_changes) {
  CHECK(replaySequenceDetectsDivergenceIn(/*divergeStart=*/true,
                                           /*divergeFire=*/false));
}

STEAMCORE_TEST(game_loop_replay_fixture_is_sensitive_to_fire_changes) {
  CHECK(replaySequenceDetectsDivergenceIn(/*divergeStart=*/false,
                                           /*divergeFire=*/true));
}

// The two sensitivity tests above only prove something if replayInputAt
// actually produces all four start/fire combinations across the replay
// -- collapsing it to one constant combination would leave both of them
// vacuously true (review F1: verified, this was the second half of the
// gap). Independently re-derived from the fixture, not from glancing at
// its switch statement.
STEAMCORE_TEST(game_loop_replay_sequence_covers_all_four_combinations) {
  bool sawFalseFalse = false;
  bool sawTrueFalse = false;
  bool sawFalseTrue = false;
  bool sawTrueTrue = false;

  for (int32_t i = 0; i < kReplayTicks; ++i) {
    const GameInput input = replayInputAt(i);
    if (!input.start && !input.fire) sawFalseFalse = true;
    if (input.start && !input.fire) sawTrueFalse = true;
    if (!input.start && input.fire) sawFalseTrue = true;
    if (input.start && input.fire) sawTrueTrue = true;
  }

  CHECK(sawFalseFalse);
  CHECK(sawTrueFalse);
  CHECK(sawFalseTrue);
  CHECK(sawTrueTrue);
}
