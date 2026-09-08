#include "galactic_invasion/galactic_invasion.h"

#include "galactic_invasion_fixture.h"
#include "steamcore/game_loop.h"
#include "steamcore/game_state.h"
#include "steamcore/title_screen.h"
#include "test_harness.h"

using steamcore::Framebuffer;
using steamcore::GameInput;
using steamcore::GameLoop;
using steamcore::GameState;
using steamcore::drawTitleScreen;
using steamcore::games::GalacticInvasion;
using steamcore::games::kPlayerHeight;
using steamcore::games::kPlayerSpeedX;
using steamcore::games::kPlayerStartX;
using steamcore::games::kPlayerWidth;
using steamcore::games::kPlayerY;
using steamcore::test::buildExpectedPlayerFrame;
using steamcore::test::findPlayerSpriteX;
using steamcore::test::runTicks;

namespace {

// Drives a fresh loop from READY into PLAYING, at kPlayerStartX, via the
// real rising-edge contract (AC-6.2) -- never a stub.
void enterPlaying(GameLoop<GalacticInvasion>& loop) {
  loop.tick(GameInput{});                // settle at READY
  loop.tick(GameInput{/*start=*/true});  // rising edge -> PLAYING
}

}  // namespace

// AC-6.1/NFR-5: GalacticInvasion satisfies GameLoop<Game>'s Game concept
// -- this line alone proves the kGameHasUpdate/kGameHasRender SFINAE
// conformance at compile time; it would be a named compile error
// otherwise (game_loop.h).
static_assert(sizeof(GameLoop<GalacticInvasion>) > 0,
              "GalacticInvasion must satisfy GameLoop<Game>'s Game concept");

// AC-6.1: the READY frame composes the unmodified drawTitleScreen exactly
// -- proven by comparing against a reference built by calling
// drawTitleScreen directly, the same composition-level check
// title_screen_harness_game.h's own pattern establishes. This is not
// re-proving drawTitleScreen's own pixel content (start-screen already
// did that); it proves GalacticInvasion delegates to it unmodified.
STEAMCORE_TEST(galactic_invasion_ready_frame_composes_title_screen) {
  GalacticInvasion game;
  Framebuffer fb;
  GameLoop<GalacticInvasion> loop(game, fb);

  loop.tick(GameInput{});

  Framebuffer expected;
  drawTitleScreen(expected, GameState::READY);

  for (int32_t y = 0; y < Framebuffer::height(); ++y) {
    for (int32_t x = 0; x < Framebuffer::width(); ++x) {
      CHECK(fb.pixel(x, y) == expected.pixel(x, y));
    }
  }
}

// AC-6.2: a rising edge of `start` at READY moves the round into PLAYING
// -- real, legitimate to test today (GameSession's own shipped
// rising-edge contract), unlike PLAYING -> GAME_OVER, which has no real
// trigger until later tasks build the game's own end conditions.
STEAMCORE_TEST(galactic_invasion_start_rising_edge_enters_playing) {
  GalacticInvasion game;
  Framebuffer fb;
  GameLoop<GalacticInvasion> loop(game, fb);

  loop.tick(GameInput{});                    // settle at READY
  loop.tick(GameInput{/*start=*/true});      // rising edge

  Framebuffer titleReference;
  drawTitleScreen(titleReference, GameState::READY);

  // PLAYING draws nothing yet (T1 stub) -- the frame must differ from
  // the READY frame, proving the state genuinely advanced rather than
  // the switch silently falling through to the same rendering.
  bool differsFromTitle = false;
  for (int32_t y = 0; y < Framebuffer::height() && !differsFromTitle; ++y) {
    for (int32_t x = 0; x < Framebuffer::width(); ++x) {
      if (fb.pixel(x, y) != titleReference.pixel(x, y)) {
        differsFromTitle = true;
        break;
      }
    }
  }
  CHECK(differsFromTitle);
}

// AC-1.1: one tick of `right` moves exactly +kPlayerSpeedX; one tick of
// `left` moves exactly -kPlayerSpeedX.
STEAMCORE_TEST(galactic_invasion_player_moves_exactly_one_step_per_tick) {
  {
    GalacticInvasion game;
    Framebuffer fb;
    GameLoop<GalacticInvasion> loop(game, fb);
    enterPlaying(loop);

    loop.tick(GameInput{/*start=*/false, /*fire=*/false, /*select=*/false,
                         /*up=*/false, /*down=*/false, /*left=*/false,
                         /*right=*/true});

    CHECK_EQ(findPlayerSpriteX(fb), kPlayerStartX + kPlayerSpeedX);
  }
  {
    GalacticInvasion game;
    Framebuffer fb;
    GameLoop<GalacticInvasion> loop(game, fb);
    enterPlaying(loop);

    GameInput leftInput{};
    leftInput.left = true;
    loop.tick(leftInput);

    CHECK_EQ(findPlayerSpriteX(fb), kPlayerStartX - kPlayerSpeedX);
  }
}

// AC-1.2: holding a direction into a screen edge clamps fully on-screen,
// never partially or fully off it, on both edges.
STEAMCORE_TEST(galactic_invasion_player_clamps_fully_on_screen_at_both_edges) {
  {
    GalacticInvasion game;
    Framebuffer fb;
    GameLoop<GalacticInvasion> loop(game, fb);
    enterPlaying(loop);

    GameInput rightInput{};
    rightInput.right = true;
    runTicks(loop, rightInput, 200);

    const int32_t x = findPlayerSpriteX(fb);
    CHECK_EQ(x, Framebuffer::width() - kPlayerWidth);
    CHECK(x >= 0);
  }
  {
    GalacticInvasion game;
    Framebuffer fb;
    GameLoop<GalacticInvasion> loop(game, fb);
    enterPlaying(loop);

    GameInput leftInput{};
    leftInput.left = true;
    runTicks(loop, leftInput, 200);

    CHECK_EQ(findPlayerSpriteX(fb), 0);
  }
}

// AC-1.3: up/down are read but never move the ship -- every rendered
// frame over a held up+down sequence shows the ship at the same,
// unchanged row.
STEAMCORE_TEST(galactic_invasion_vertical_input_never_moves_the_player) {
  GalacticInvasion game;
  Framebuffer fb;
  GameLoop<GalacticInvasion> loop(game, fb);
  enterPlaying(loop);

  GameInput verticalInput{};
  verticalInput.up = true;
  verticalInput.down = true;

  for (int32_t i = 0; i < 60; ++i) {
    loop.tick(verticalInput);
    // The ship's fixed row is the only row that can ever be lit by it;
    // finding it there at all (rather than -1) proves the y never moved.
    CHECK(findPlayerSpriteX(fb) != -1);
  }

  // The player's own row band is pixel-identical to a reference at the
  // unchanged start position -- horizontal position also never moved,
  // since neither left nor right was ever pressed. Scoped to the
  // player's band only (not the whole frame): since T4, PLAYING also
  // renders the HUD, which this test is not about.
  const Framebuffer expected = buildExpectedPlayerFrame(kPlayerStartX);
  for (int32_t y = kPlayerY; y < kPlayerY + kPlayerHeight; ++y) {
    for (int32_t x = 0; x < Framebuffer::width(); ++x) {
      CHECK(fb.pixel(x, y) == expected.pixel(x, y));
    }
  }
}

// Both directions held simultaneously: a stated, fixed outcome -- since
// update() applies -kPlayerSpeedX for left then +kPlayerSpeedX for right
// unconditionally when each is true, holding both cancels to no net
// movement.
STEAMCORE_TEST(galactic_invasion_both_directions_held_cancels_to_no_movement) {
  GalacticInvasion game;
  Framebuffer fb;
  GameLoop<GalacticInvasion> loop(game, fb);
  enterPlaying(loop);

  GameInput bothInput{};
  bothInput.left = true;
  bothInput.right = true;
  runTicks(loop, bothInput, 50);

  CHECK_EQ(findPlayerSpriteX(fb), kPlayerStartX);
}
