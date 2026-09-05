#include <cstdio>

#include "steamcore/game_loop.h"
#include "steamcore/title_screen.h"

#include "fb_compare.h"
#include "test_harness.h"
#include "title_screen_game.h"

using steamcore::Framebuffer;
using steamcore::GameInput;
using steamcore::GameLoop;
using steamcore::test::framebuffersEqual;
using steamcore::test::TitleScreenGame;

namespace {

// Covers READY, the rising edge, held start, release, and a second
// press -- fixed, hand-written, no loop-generated pattern (constitution
// §3/§4 determinism).
constexpr int32_t kStepCount = 8;
constexpr GameInput kSequence[kStepCount] = {
    GameInput{false},  // READY
    GameInput{false},  // READY
    GameInput{true},   // rising edge -> PLAYING
    GameInput{true},   // held
    GameInput{true},   // held
    GameInput{false},  // released
    GameInput{true},   // second press
    GameInput{false},  // released again
};

}  // namespace

// AC-2.3: two independently-constructed consumers, stepped in lockstep
// through the same fixed sequence, produce byte-identical framebuffers
// after EVERY step -- not only the last.
STEAMCORE_TEST(title_screen_replay_is_deterministic_after_every_step) {
  TitleScreenGame gameA;
  Framebuffer fbA;
  GameLoop<TitleScreenGame> loopA(gameA, fbA);

  TitleScreenGame gameB;
  Framebuffer fbB;
  GameLoop<TitleScreenGame> loopB(gameB, fbB);

  int32_t firstMismatchStep = -1;
  int32_t mismatchX = -1;
  int32_t mismatchY = -1;

  for (int32_t i = 0; i < kStepCount; ++i) {
    loopA.tick(kSequence[i]);
    loopB.tick(kSequence[i]);

    if (!framebuffersEqual(fbA, fbB, &mismatchX, &mismatchY)) {
      firstMismatchStep = i;
      break;
    }
  }

  if (firstMismatchStep != -1) {
    std::printf("determinism mismatch at step %d, pixel (%d,%d)\n",
                firstMismatchStep, mismatchX, mismatchY);
  }
  CHECK_EQ(firstMismatchStep, -1);
}

// Negative control: the comparison above is capable of failing. One run
// gets a deliberately perturbed step (start flipped at the rising edge),
// and the two framebuffers are asserted to differ at that step -- proving
// framebuffersEqual can detect a real divergence, not just always pass.
// Checked immediately after the perturbed step, not at the sequence's
// end: a state machine can legitimately re-converge later (run B reaches
// PLAYING one tick after run A instead of never), which would otherwise
// make a real, momentary divergence invisible.
STEAMCORE_TEST(title_screen_replay_comparison_can_detect_a_real_divergence) {
  constexpr int32_t kFlipAtStep = 2;  // the rising edge itself

  TitleScreenGame gameA;
  Framebuffer fbA;
  GameLoop<TitleScreenGame> loopA(gameA, fbA);

  TitleScreenGame gameB;
  Framebuffer fbB;
  GameLoop<TitleScreenGame> loopB(gameB, fbB);

  bool everDiverged = false;
  for (int32_t i = 0; i < kStepCount; ++i) {
    GameInput inputB = kSequence[i];
    if (i == kFlipAtStep) inputB.start = !inputB.start;

    loopA.tick(kSequence[i]);
    loopB.tick(inputB);

    if (!framebuffersEqual(fbA, fbB)) everDiverged = true;
  }

  CHECK(everDiverged);
}
