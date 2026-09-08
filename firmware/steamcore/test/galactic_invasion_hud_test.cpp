#include "galactic_invasion/galactic_invasion.h"

#include "galactic_invasion_fixture.h"
#include "steamcore/color.h"
#include "steamcore/font.h"
#include "steamcore/game_loop.h"
#include "steamcore/sprite.h"
#include "test_harness.h"

using steamcore::Color;
using steamcore::Entity;
using steamcore::Framebuffer;
using steamcore::GameInput;
using steamcore::GameLoop;
using steamcore::glyphFor;
using steamcore::kGlyphAdvance;
using steamcore::kGlyphHeight;
using steamcore::kGlyphWidth;
using steamcore::Sprite;
using steamcore::games::GalacticInvasion;
using steamcore::games::kFormationBandBounds;
using steamcore::games::kHudRowBounds;
using steamcore::games::kLivesBounds;
using steamcore::games::kPlayerBandBounds;
using steamcore::games::kScoreBounds;
using steamcore::test::runTicks;

namespace {

// Restated independently of galactic_invasion.cpp's own formatter
// (review precedent: title_screen_test.cpp's kExpectedWordmarkText) --
// never call the production formatScoreText/formatLivesText, or a drift
// in either would be invisible to this test.
constexpr char kExpectedFreshScoreText[] = "SCORE: 0000";
constexpr char kExpectedFreshLivesText[] = "LIVES: 3";

// Draws `text` into `fb` with the test's OWN placement loop -- never by
// calling drawText -- mirrors title_screen_test.cpp's placeExpected.
void placeExpected(Framebuffer& fb, int32_t x0, int32_t y0, const char* text,
                    Color ink) {
  int32_t gx = x0;
  for (const char* p = text; *p != '\0'; ++p, gx += kGlyphAdvance) {
    const Sprite glyph = glyphFor(*p);
    for (int32_t row = 0; row < kGlyphHeight; ++row) {
      for (int32_t col = 0; col < kGlyphWidth; ++col) {
        if (glyph.pixels[row * glyph.stride + col] != Color::BLACK) {
          fb.setPixel(gx + col, y0 + row, ink);
        }
      }
    }
  }
}

// An intersection helper written independently of steamcore::overlaps()
// (CLAUDE.md: prove disjointness twice, with two helpers that share no
// code) -- a different algebraic form (min/max spans) from overlaps()'s
// own four-strict-inequality expression, so a bug shared by both would
// have to be a coincidence, not a shared implementation.
bool rectsIntersect(const Entity& a, const Entity& b) {
  const int32_t aLeft = a.x, aRight = a.x + a.w;
  const int32_t aTop = a.y, aBottom = a.y + a.h;
  const int32_t bLeft = b.x, bRight = b.x + b.w;
  const int32_t bTop = b.y, bBottom = b.y + b.h;
  const int32_t overlapLeft = aLeft > bLeft ? aLeft : bLeft;
  const int32_t overlapRight = aRight < bRight ? aRight : bRight;
  const int32_t overlapTop = aTop > bTop ? aTop : bTop;
  const int32_t overlapBottom = aBottom < bBottom ? aBottom : bBottom;
  return overlapLeft < overlapRight && overlapTop < overlapBottom;
}

}  // namespace

// AC-7.1/AC-10.1: the HUD renders pixel-identically to an independently
// built reference at the documented coordinates, on a fresh round.
STEAMCORE_TEST(galactic_invasion_hud_matches_independent_reference) {
  GalacticInvasion game;
  Framebuffer fb;
  GameLoop<GalacticInvasion> loop(game, fb);
  loop.tick(GameInput{});
  loop.tick(GameInput{/*start=*/true});

  Framebuffer expected;
  placeExpected(expected, kScoreBounds.x, kScoreBounds.y,
                kExpectedFreshScoreText, Color::BRIGHT_ORANGE);
  placeExpected(expected, kLivesBounds.x, kLivesBounds.y,
                kExpectedFreshLivesText, Color::BRIGHT_ORANGE);

  for (int32_t y = kScoreBounds.y; y < kScoreBounds.y + kScoreBounds.h; ++y) {
    for (int32_t x = 0; x < Framebuffer::width(); ++x) {
      CHECK(fb.pixel(x, y) == expected.pixel(x, y));
    }
  }
}

// US-10/AC-10.1: a fresh round shows exactly 3 lives.
STEAMCORE_TEST(galactic_invasion_fresh_round_shows_three_lives) {
  GalacticInvasion game;
  Framebuffer fb;
  GameLoop<GalacticInvasion> loop(game, fb);
  loop.tick(GameInput{});
  loop.tick(GameInput{/*start=*/true});

  Framebuffer expected;
  placeExpected(expected, kLivesBounds.x, kLivesBounds.y, "LIVES: 3",
                Color::BRIGHT_ORANGE);

  for (int32_t y = kLivesBounds.y; y < kLivesBounds.y + kLivesBounds.h; ++y) {
    for (int32_t x = kLivesBounds.x; x < kLivesBounds.x + kLivesBounds.w;
         ++x) {
      CHECK(fb.pixel(x, y) == expected.pixel(x, y));
    }
  }
}

// AC-7.2: a runtime disjointness check over all four documented rects,
// using rectsIntersect (its own helper, sharing no code with overlaps())
// -- the compile-time static_assert(!overlaps(...)) proof in the header
// is the other, independent half.
STEAMCORE_TEST(galactic_invasion_hud_bounds_are_pairwise_disjoint_at_runtime) {
  CHECK(!rectsIntersect(kScoreBounds, kLivesBounds));
  CHECK(!rectsIntersect(kScoreBounds, kPlayerBandBounds));
  CHECK(!rectsIntersect(kLivesBounds, kPlayerBandBounds));
  CHECK(!rectsIntersect(kHudRowBounds, kFormationBandBounds));
}
