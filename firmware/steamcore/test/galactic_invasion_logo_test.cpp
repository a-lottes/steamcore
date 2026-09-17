#include "galactic_invasion/galactic_invasion_logo.h"

#include "fb_compare.h"
#include "galactic_invasion/galactic_invasion.h"
#include "galactic_invasion/galactic_invasion_generated_art.h"
#include "steamcore/color.h"
#include "steamcore/font.h"
#include "steamcore/game_loop.h"
#include "steamcore/title_screen.h"
#include "test_harness.h"

using steamcore::Color;
using steamcore::Framebuffer;
using steamcore::GameInput;
using steamcore::GameLoop;
using steamcore::GameState;
using steamcore::kTitlePromptBounds;
using steamcore::games::drawGalacticInvasionLogo;
using steamcore::games::GalacticInvasion;
using steamcore::games::GiRect;
using steamcore::games::kGiPromptBounds;
using steamcore::games::kLogoBounds;
using steamcore::games::kScoreBounds;
using steamcore::test::framebuffersEqual;

namespace {

// Templated: called with both this file's own GiRect and the engine's
// Entity (kScoreBounds) below -- both are plain x/y/w/h rectangles, just
// distinct types by convention (NFR-5: this game never depends on the
// other's rectangle type).
template <typename Rect>
bool anyNonBlackInside(const Framebuffer& fb, const Rect& r) {
  for (int32_t y = r.y; y < r.y + r.h; ++y) {
    for (int32_t x = r.x; x < r.x + r.w; ++x) {
      if (fb.pixel(x, y) != Color::BLACK) return true;
    }
  }
  return false;
}

// True iff `a` and `b` overlap at all -- an independent runtime check,
// separate from the header static_assert (CLAUDE.md's "prove it twice"
// layout convention: a general four-way separating-axis test, not "logo
// is above prompt").
bool rectsIntersect(const GiRect& a, const GiRect& b) {
  const bool separated = a.x + a.w <= b.x || b.x + b.w <= a.x ||
                          a.y + a.h <= b.y || b.y + b.h <= a.y;
  return !separated;
}

}  // namespace

// AC-1.1/AC-1.3: READY draws something inside both documented rects.
STEAMCORE_TEST(galactic_invasion_logo_ready_draws_inside_both_bounds) {
  Framebuffer fb;
  fb.clear(Color::BLACK);
  drawGalacticInvasionLogo(fb, GameState::READY);

  CHECK(anyNonBlackInside(fb, kLogoBounds));
  CHECK(anyNonBlackInside(fb, kGiPromptBounds));
}

// AC-1.8: PLAYING and GAME_OVER draw nothing at all -- proven at the
// level of the pure function itself (not just the composed game), so a
// blank framebuffer stays byte-identical.
STEAMCORE_TEST(galactic_invasion_logo_playing_and_game_over_draw_nothing) {
  for (GameState state : {GameState::PLAYING, GameState::GAME_OVER}) {
    Framebuffer drawn;
    drawn.clear(Color::BLACK);
    drawGalacticInvasionLogo(drawn, state);

    Framebuffer untouched;
    untouched.clear(Color::BLACK);
    CHECK(framebuffersEqual(drawn, untouched));
  }
}

// AC-1.1: every pixel outside both documented rects stays BLACK -- the
// minimalism guarantee, mirroring title_screen_test.cpp's own such test.
STEAMCORE_TEST(galactic_invasion_logo_every_pixel_outside_both_rects_is_black) {
  Framebuffer fb;
  fb.clear(Color::BLACK);
  drawGalacticInvasionLogo(fb, GameState::READY);

  auto insideBounds = [](int32_t x, int32_t y, const GiRect& r) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
  };

  for (int32_t y = 0; y < Framebuffer::height(); ++y) {
    for (int32_t x = 0; x < Framebuffer::width(); ++x) {
      if (insideBounds(x, y, kLogoBounds) ||
          insideBounds(x, y, kGiPromptBounds)) {
        continue;
      }
      CHECK(fb.pixel(x, y) == Color::BLACK);
    }
  }
}

// AC-1.4: pairwise disjointness, proven again at runtime -- the header
// static_assert already proves it at compile time; this is the
// independent second proof CLAUDE.md's layout convention calls for.
STEAMCORE_TEST(galactic_invasion_logo_bounds_are_disjoint_at_runtime) {
  CHECK(!rectsIntersect(kLogoBounds, kGiPromptBounds));
}

// AC-1.12: the prompt's bounds are identical to the engine's own
// kTitlePromptBounds, field-by-field -- derived independently (this game
// never includes title_screen.h from production code), so this is what
// actually keeps the two from drifting apart rather than merely hoping
// the same formula was copied correctly.
STEAMCORE_TEST(galactic_invasion_logo_prompt_bounds_match_engine_title_prompt) {
  CHECK_EQ(kGiPromptBounds.x, kTitlePromptBounds.x);
  CHECK_EQ(kGiPromptBounds.y, kTitlePromptBounds.y);
  CHECK_EQ(kGiPromptBounds.w, kTitlePromptBounds.w);
  CHECK_EQ(kGiPromptBounds.h, kTitlePromptBounds.h);
}

// AC-1.5/AC-1.6: driven through the real, composed GalacticInvasion via
// an unmodified GameLoop -- the logo is visible on the READY tick, gone
// the tick after a START rising edge (PLAYING's own HUD appears
// instead), and stays gone across many further held-start ticks, never
// redrawn. No private accessor is used (NFR-5): "we are in PLAYING" is
// observed the same way every other galactic_invasion_*_test.cpp
// observes it -- the score HUD, which only PLAYING's render branch ever
// draws.
STEAMCORE_TEST(galactic_invasion_logo_session_gone_after_start_and_never_redrawn) {
  GalacticInvasion game;
  Framebuffer fb;
  GameLoop<GalacticInvasion> loop(game, fb);

  loop.tick(GameInput{});  // READY
  CHECK(anyNonBlackInside(fb, kLogoBounds));
  CHECK(anyNonBlackInside(fb, kGiPromptBounds));
  CHECK(!anyNonBlackInside(fb, kScoreBounds));  // PLAYING's HUD, not yet

  loop.tick(GameInput{/*start=*/true});  // rising edge -> PLAYING
  CHECK(anyNonBlackInside(fb, kScoreBounds));  // PLAYING's own HUD signal

  for (int32_t i = 0; i < 20; ++i) {
    loop.tick(GameInput{/*start=*/true});  // held, never a fresh edge
    CHECK(anyNonBlackInside(fb, kScoreBounds));
  }
}

// AC-1.9: a freshly constructed game whose very first tick already has
// start == true begins PLAYING immediately (game_state.h's own
// documented rule: the first tick of a fresh instance counts as a rising
// edge) -- the logo is never rendered at all, not even for one tick.
STEAMCORE_TEST(galactic_invasion_logo_held_start_at_boot_skips_the_logo) {
  GalacticInvasion game;
  Framebuffer fb;
  GameLoop<GalacticInvasion> loop(game, fb);

  loop.tick(GameInput{/*start=*/true});
  CHECK(anyNonBlackInside(fb, kScoreBounds));  // already PLAYING
}

// AC-1.6/NFR-3: the same fixed input sequence, replayed from two fresh
// instances, produces byte-identical frames tick for tick across the
// READY-to-PLAYING transition -- scoped to this screen rather than
// duplicating galactic_invasion_determinism_test.cpp's own full-game,
// 6000-tick replay.
STEAMCORE_TEST(galactic_invasion_logo_ready_to_playing_replay_is_deterministic) {
  GalacticInvasion gameA;
  Framebuffer fbA;
  GameLoop<GalacticInvasion> loopA(gameA, fbA);

  GalacticInvasion gameB;
  Framebuffer fbB;
  GameLoop<GalacticInvasion> loopB(gameB, fbB);

  constexpr int32_t kTicks = 30;
  for (int32_t t = 0; t < kTicks; ++t) {
    GameInput input{};
    input.start = (t >= 5);  // READY for 5 ticks, then start held
    loopA.tick(input);
    loopB.tick(input);
    CHECK(framebuffersEqual(fbA, fbB));
  }
}
