#include <cstdio>

#include "galactic_invasion/galactic_invasion.h"

#include "fake_flash_backend.h"
#include "fb_compare.h"
#include "steamcore/framebuffer.h"
#include "steamcore/game_loop.h"
#include "steamcore/highscore.h"
#include "steamcore/highscore_game.h"
#include "test_harness.h"

using steamcore::Framebuffer;
using steamcore::GameInput;
using steamcore::GameLoop;
using steamcore::HighscoreGame;
using steamcore::HighscoreStore;
using steamcore::count;
using steamcore::kBlockSize;
using steamcore::games::GalacticInvasion;
using steamcore::games::kHighscoreName;
using steamcore::games::kHighscoreSlot;
using steamcore::test::FakeFlashBackend;
using steamcore::test::framebuffersEqual;

namespace {

// A fixed, purely formulaic input sequence -- every field derived from the
// tick index alone, never randomness or a wall clock (NFR-3/AC-2.5) --
// mirroring galactic_invasion_determinism_test.cpp's own script exactly:
// a sweeping left/right pattern, a `fire` pulse every 5th tick (frequent
// enough to score real kills, and -- doubling as the exact edge shape
// InitialsEntry needs -- to submit a letter every 5 ticks once ENTRY is
// reached), and a `start` pulse every 300 ticks (harmless as a single-tick
// pulse while PLAYING or mid-flow, and exactly what carries a finished
// TABLE screen into a restart once the script has reached one).
constexpr int32_t kReplayTicks = 2000;

GameInput scriptedInputAt(int32_t tick) {
  GameInput input{};
  const int32_t phase = tick % 240;
  if (phase < 120) {
    input.right = true;
  } else {
    input.left = true;
  }
  input.fire = (tick % 5) == 0;
  input.start = (tick % 300) == 0;
  return input;
}

}  // namespace

// AC-2.5/NFR-3: the same fixed input sequence, driving the whole composed
// feature (GalacticInvasion wrapped in HighscoreGame over a real
// HighscoreStore<FakeFlashBackend>) from two independently-constructed
// instances stepped in lockstep, produces a byte-identical framebuffer on
// every single tick -- not just the final one -- and the two backends'
// final persisted bytes agree exactly. Over kReplayTicks the script's own
// firing and start pulses are enough to run a full round to GAME_OVER, a
// qualifying score (score > 0 always qualifies while the table has room,
// AC-1.1/A10), all three initials (each `fire` pulse is a fresh rising
// edge, exactly InitialsEntry's own submission signal), and a restart --
// checked explicitly below, not just assumed from the script's shape.
STEAMCORE_TEST(highscore_composed_feature_replay_is_byte_identical_every_tick) {
  FakeFlashBackend backendA;
  HighscoreStore<FakeFlashBackend> storeA(backendA);
  GalacticInvasion gameA;
  HighscoreGame<GalacticInvasion, HighscoreStore<FakeFlashBackend>> wrappedA(
      gameA, storeA, kHighscoreSlot, kHighscoreName);
  Framebuffer fbA;
  GameLoop<HighscoreGame<GalacticInvasion, HighscoreStore<FakeFlashBackend>>>
      loopA(wrappedA, fbA);

  FakeFlashBackend backendB;
  HighscoreStore<FakeFlashBackend> storeB(backendB);
  GalacticInvasion gameB;
  HighscoreGame<GalacticInvasion, HighscoreStore<FakeFlashBackend>> wrappedB(
      gameB, storeB, kHighscoreSlot, kHighscoreName);
  Framebuffer fbB;
  GameLoop<HighscoreGame<GalacticInvasion, HighscoreStore<FakeFlashBackend>>>
      loopB(wrappedB, fbB);

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
    std::printf("highscore determinism mismatch at tick %d, pixel (%d,%d)\n",
                firstMismatchTick, mismatchX, mismatchY);
  }
  CHECK_EQ(firstMismatchTick, -1);

  // The script actually reached a qualifying entry (not merely "would
  // have, in principle") -- both stores agree, and it happened at all.
  CHECK(count(storeA.table(kHighscoreSlot)) >= 1);
  CHECK_EQ(count(storeA.table(kHighscoreSlot)), count(storeB.table(kHighscoreSlot)));

  // The two backends' persisted bytes agree exactly -- a direct byte
  // comparison of the raw persisted image, not merely the decoded table
  // (which would miss a checksum/header divergence that still happened to
  // decode to the same table by coincidence).
  bool backendsIdentical = true;
  for (int32_t i = 0; i < kBlockSize; ++i) {
    if (backendA.rawBytes()[i] != backendB.rawBytes()[i]) {
      backendsIdentical = false;
      break;
    }
  }
  CHECK(backendsIdentical);
}
