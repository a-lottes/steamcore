#include <cstdio>

#include "galactic_invasion/galactic_invasion.h"

#include "fb_compare.h"
#include "steamcore/framebuffer.h"
#include "steamcore/game_loop.h"
#include "test_harness.h"

using steamcore::Framebuffer;
using steamcore::GameInput;
using steamcore::GameLoop;
using steamcore::games::GalacticInvasion;
using steamcore::test::framebuffersEqual;

namespace {

// A fixed, purely formulaic input sequence -- every field derived from the
// tick index alone, never randomness or a wall clock (NFR-3/A11) -- that
// exercises movement (a sweeping left/right pattern), firing (frequent
// pulses, enough to score real kills against the formation), and simply
// keeps running long enough to plausibly reach a life-lost contact and a
// subsequent restart, without needing to script either explicitly: this
// replay only asserts "the same fixed input produces the same output",
// not "this specific mix of events occurs" -- whatever the script's fixed
// pattern happens to trigger is fine, as long as both runs agree on it,
// tick for tick.
constexpr int32_t kReplayTicks = 6000;

GameInput scriptedInputAt(int32_t tick) {
  GameInput input{};
  const int32_t phase = tick % 240;
  if (phase < 120) {
    input.right = true;
  } else {
    input.left = true;
  }
  input.fire = (tick % 5) == 0;
  // A start pulse every 3000 ticks -- harmless while PLAYING (GameSession
  // only reacts to a *rising* edge, and this is a single-tick pulse every
  // 3000 ticks, not held), and is exactly what carries a GAME_OVER round
  // into a fresh restart if the script has reached one by then (AC-6.3).
  input.start = (tick % 3000) == 0;
  return input;
}

}  // namespace

// AC-10.7/NFR-3: the same fixed input sequence and starting state produce
// a byte-identical framebuffer on every single tick, not just the last --
// checked on two independently-constructed instances stepped in lockstep,
// mirroring game_state_determinism_test.cpp's own "compare after every
// step, report the first mismatch" convention.
STEAMCORE_TEST(galactic_invasion_replay_is_byte_identical_every_tick) {
  GalacticInvasion gameA;
  Framebuffer fbA;
  GameLoop<GalacticInvasion> loopA(gameA, fbA);

  GalacticInvasion gameB;
  Framebuffer fbB;
  GameLoop<GalacticInvasion> loopB(gameB, fbB);

  int32_t firstMismatchTick = -1;
  int32_t mismatchX = -1;
  int32_t mismatchY = -1;

  for (int32_t t = 0; t < kReplayTicks; ++t) {
    const GameInput input = scriptedInputAt(t);
    loopA.tick(input);
    loopB.tick(input);

    if (!framebuffersEqual(fbA, fbB, &mismatchX, &mismatchY)) {
      firstMismatchTick = t;
      break;
    }
  }

  if (firstMismatchTick != -1) {
    std::printf("determinism mismatch at tick %d, pixel (%d,%d)\n",
                firstMismatchTick, mismatchX, mismatchY);
  }
  CHECK_EQ(firstMismatchTick, -1);
}
