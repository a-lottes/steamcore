#include "steamcore/game_loop.h"
#include "steamcore/title_screen.h"

#include "fb_compare.h"
#include "test_harness.h"
#include "title_screen_game.h"

using steamcore::Color;
using steamcore::Framebuffer;
using steamcore::GameInput;
using steamcore::GameLoop;
using steamcore::GameState;
using steamcore::kTitlePromptBounds;
using steamcore::kTitleWordmarkBounds;
using steamcore::test::TitleScreenGame;

namespace {

bool anyNonBlackInside(const Framebuffer& fb,
                        const steamcore::TitleBounds& bounds) {
  for (int32_t y = bounds.y; y < bounds.y + bounds.h; ++y) {
    for (int32_t x = bounds.x; x < bounds.x + bounds.w; ++x) {
      if (fb.pixel(x, y) != Color::BLACK) return true;
    }
  }
  return false;
}

}  // namespace

// AC-2.1/US-2: driven through an unmodified GameLoop<Game>, the title is
// present on every READY tick.
STEAMCORE_TEST(title_screen_session_present_every_ready_tick) {
  TitleScreenGame game;
  Framebuffer fb;
  GameLoop<TitleScreenGame> loop(game, fb);

  for (int32_t i = 0; i < 5; ++i) {
    loop.tick(GameInput{});
    CHECK(game.state() == GameState::READY);
    CHECK(anyNonBlackInside(fb, kTitleWordmarkBounds));
    CHECK(anyNonBlackInside(fb, kTitlePromptBounds));
  }
}

// AC-2.1: the tick right after a start rising edge shows PLAYING and a
// wholly black framebuffer -- neither element is drawn.
STEAMCORE_TEST(title_screen_session_absent_the_tick_after_start) {
  TitleScreenGame game;
  Framebuffer fb;
  GameLoop<TitleScreenGame> loop(game, fb);

  loop.tick(GameInput{});  // READY, title visible
  loop.tick(GameInput{true});  // start rising edge -> PLAYING

  CHECK(game.state() == GameState::PLAYING);

  Framebuffer untouched;
  untouched.clear(Color::BLACK);
  CHECK(steamcore::test::framebuffersEqual(fb, untouched));
}

// AC-2.2: no flicker back while start is merely held -- absent across
// many further ticks, not just the one right after the edge.
STEAMCORE_TEST(title_screen_session_stays_absent_while_start_held) {
  TitleScreenGame game;
  Framebuffer fb;
  GameLoop<TitleScreenGame> loop(game, fb);

  loop.tick(GameInput{});
  loop.tick(GameInput{true});

  Framebuffer untouched;
  untouched.clear(Color::BLACK);

  for (int32_t i = 0; i < 20; ++i) {
    loop.tick(GameInput{true});
    CHECK(game.state() == GameState::PLAYING);
    CHECK(steamcore::test::framebuffersEqual(fb, untouched));
  }
}
