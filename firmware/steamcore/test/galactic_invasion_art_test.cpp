#include "galactic_invasion/galactic_invasion_art.h"

#include "fb_compare.h"
#include "galactic_invasion/galactic_invasion.h"
#include "steamcore/color.h"
#include "steamcore/framebuffer.h"
#include "steamcore/game_loop.h"
#include "steamcore/sprite.h"
#include "test_harness.h"

using steamcore::Color;
using steamcore::Framebuffer;
using steamcore::GameInput;
using steamcore::GameLoop;
using steamcore::Sprite;
using steamcore::games::GalacticInvasion;
using steamcore::games::kEnemyHeight;
using steamcore::games::kEnemyShotSprite;
using steamcore::games::kEnemySprite;
using steamcore::games::kEnemyWidth;
using steamcore::games::kLogoHeight;
using steamcore::games::kLogoWidth;
using steamcore::games::kPlayerHeight;
using steamcore::games::kPlayerSprite;
using steamcore::games::kPlayerWidth;
using steamcore::games::kProjectileHeight;
using steamcore::games::kProjectileSprite;
using steamcore::games::kProjectileWidth;
using steamcore::test::framebuffersEqual;

namespace {

// Restated independently of galactic_invasion_art.h's own
// detail::kPlayerRows/kEnemyRows/kProjectileRows (review precedent:
// title_screen_test.cpp's kExpectedWordmarkText) -- if this test instead
// imported and reused the production row strings, a drift introduced
// later would be invisible here, since "expected" and "actual" would
// silently agree on the same wrong value.
// galactic-invasion-artwork T9: generated from the concept sheet (was a
// hand-typed wedge before). Restated independently here from the
// rendered 1:1 dump, same as every other sprite in this file -- never
// imported from galactic_invasion_generated_art.h, so a drift there is
// still caught here.
constexpr const char* kExpectedPlayerRows[kPlayerHeight] = {
    "     ##     ",
    "     ##     ",
    "     ##     ",
    "    ###     ",
    "    ####    ",
    "    ####    ",
    "   ###### # ",
    " ########## ",
    "####### ####",
    "####### ### ",
    "##   ##  ###",
    " #         #",
};

// galactic-invasion-artwork T13: hand-authored, restated independently
// of galactic_invasion_art.h's own detail::kEnemyRows exactly like every
// other sprite in this file. '#' is the ORANGE outline, '+' the
// DARK_ORANGE interior core -- T10's generated FIGHTER-derived diamond
// was adopted, then reverted here after `/look-and-feel` judged it
// failed AC-2.4a's "opposite family from the player" read; this is the
// shipped invader silhouette (this project's original release) with
// T13's shading-language edit applied.
constexpr const char* kExpectedEnemyRows[kEnemyHeight] = {
    "  ##    ##  ",
    "   ######   ",
    "  ########  ",
    " ###++++### ",
    "####++++####",
    "####++++####",
    "## #++++# ##",
    "##  ++++  ##",
    "  ##    ##  ",
    " ##      ## ",
};

constexpr const char* kExpectedProjectileRows[kProjectileHeight] = {
    "##", "##", "##", "##", "##", "##",
};

// AC-3.10: deliberately not kExpectedProjectileRows -- the enemy shot's
// silhouette is its own, segmented shape, restated independently here
// exactly like every other sprite in this file.
constexpr const char* kExpectedEnemyShotRows[kProjectileHeight] = {
    "##", "##", "  ", "  ", "##", "##",
};

// Checks `sprite`'s Color array cell-by-cell against `rows`/`onColor`,
// never by calling any production build function.
void checkSpriteMatchesRows(const steamcore::Sprite& sprite,
                             const char* const* rows, int32_t width,
                             int32_t height, Color onColor) {
  for (int32_t row = 0; row < height; ++row) {
    for (int32_t col = 0; col < width; ++col) {
      const Color expected = rows[row][col] == '#' ? onColor : Color::BLACK;
      CHECK(sprite.pixels[row * sprite.stride + col] == expected);
    }
  }
}

// Two-ink variant of checkSpriteMatchesRows above (T13): '#' is the
// outline ink, '+' the interior one, matching galactic_invasion_art.h's
// own spritePixel2 convention -- needed only by kEnemySprite, the one
// sprite in this file with a shaded interior.
void checkSpriteMatchesRows2(const steamcore::Sprite& sprite,
                              const char* const* rows, int32_t width,
                              int32_t height, Color outlineOn,
                              Color interiorOn) {
  for (int32_t row = 0; row < height; ++row) {
    for (int32_t col = 0; col < width; ++col) {
      const char c = rows[row][col];
      const Color expected =
          c == '#' ? outlineOn : (c == '+' ? interiorOn : Color::BLACK);
      CHECK(sprite.pixels[row * sprite.stride + col] == expected);
    }
  }
}

}  // namespace

STEAMCORE_TEST(galactic_invasion_player_sprite_matches_art) {
  checkSpriteMatchesRows(kPlayerSprite, kExpectedPlayerRows, kPlayerWidth,
                          kPlayerHeight, Color::BRIGHT_ORANGE);
}

STEAMCORE_TEST(galactic_invasion_enemy_sprite_matches_art) {
  checkSpriteMatchesRows2(kEnemySprite, kExpectedEnemyRows, kEnemyWidth,
                           kEnemyHeight, Color::ORANGE, Color::DARK_ORANGE);
}

STEAMCORE_TEST(galactic_invasion_projectile_sprite_matches_art) {
  checkSpriteMatchesRows(kProjectileSprite, kExpectedProjectileRows,
                          kProjectileWidth, kProjectileHeight,
                          Color::BRIGHT_ORANGE);
}

// AC-3.10: the enemy shot's own silhouette and ink -- not a recolour of
// the player projectile.
STEAMCORE_TEST(galactic_invasion_enemy_shot_sprite_matches_art) {
  checkSpriteMatchesRows(kEnemyShotSprite, kExpectedEnemyShotRows,
                          kProjectileWidth, kProjectileHeight, Color::ORANGE);
}

// AC-12.2: blit's inherited transparency -- BLACK source cells leave the
// destination untouched, every '#' cell writes the sprite's own colour.
// Not reimplemented here: this proves composition, not blit's own
// contract (already proven elsewhere).
//
// Run over two different background fills per sprite, neither equal to
// the sprite's own ink (T7/galactic-invasion-artwork): a single fixed
// sentinel background (the shipped test used DARK_ORANGE for every
// sprite) stops being a safe choice once a sprite's *own* pixel data may
// legitimately contain more than one ink (spec A7) -- a background that
// happens to equal one of the sprite's real inks could mask a blit bug
// that overwrites an "off" cell. Two distinct backgrounds is what proves
// "left untouched" is genuine, not a coincidence of one chosen colour.
void checkBlitTransparency(const Sprite& sprite, const char* const* rows,
                            int32_t width, int32_t height, Color onColor,
                            Color background) {
  Framebuffer fb;
  fb.fillRect(0, 0, width, height, background);

  fb.blit(sprite, 0, 0);

  for (int32_t row = 0; row < height; ++row) {
    for (int32_t col = 0; col < width; ++col) {
      const bool onCell = rows[row][col] == '#';
      const Color expected = onCell ? onColor : background;
      CHECK(fb.pixel(col, row) == expected);
    }
  }
}

// Two-ink variant of checkBlitTransparency above (T13), matching
// checkSpriteMatchesRows2's '#'/'+' convention -- needed only by
// kEnemySprite.
void checkBlitTransparency2(const Sprite& sprite, const char* const* rows,
                             int32_t width, int32_t height,
                             Color outlineOn, Color interiorOn,
                             Color background) {
  Framebuffer fb;
  fb.fillRect(0, 0, width, height, background);

  fb.blit(sprite, 0, 0);

  for (int32_t row = 0; row < height; ++row) {
    for (int32_t col = 0; col < width; ++col) {
      const char c = rows[row][col];
      const Color expected =
          c == '#' ? outlineOn : (c == '+' ? interiorOn : background);
      CHECK(fb.pixel(col, row) == expected);
    }
  }
}

STEAMCORE_TEST(galactic_invasion_player_sprite_blits_with_inherited_transparency) {
  for (Color background : {Color::DARK_ORANGE, Color::ORANGE}) {
    checkBlitTransparency(kPlayerSprite, kExpectedPlayerRows, kPlayerWidth,
                           kPlayerHeight, Color::BRIGHT_ORANGE, background);
  }
}

// Only one background remains safe here, not two (T13): the enemy now
// legitimately carries two of the palette's three non-BLACK inks
// (ORANGE outline, DARK_ORANGE interior), leaving only BRIGHT_ORANGE as
// a background that equals neither -- the same "neither equal to the
// sprite's own ink" rule the two-background comment above states, just
// with only one candidate left to satisfy it.
STEAMCORE_TEST(galactic_invasion_enemy_sprite_blits_with_inherited_transparency) {
  checkBlitTransparency2(kEnemySprite, kExpectedEnemyRows, kEnemyWidth,
                          kEnemyHeight, Color::ORANGE, Color::DARK_ORANGE,
                          Color::BRIGHT_ORANGE);
}

STEAMCORE_TEST(galactic_invasion_projectile_sprite_blits_with_inherited_transparency) {
  for (Color background : {Color::DARK_ORANGE, Color::ORANGE}) {
    checkBlitTransparency(kProjectileSprite, kExpectedProjectileRows,
                           kProjectileWidth, kProjectileHeight,
                           Color::BRIGHT_ORANGE, background);
  }
}

STEAMCORE_TEST(galactic_invasion_enemy_shot_sprite_blits_with_inherited_transparency) {
  for (Color background : {Color::DARK_ORANGE, Color::BRIGHT_ORANGE}) {
    checkBlitTransparency(kEnemyShotSprite, kExpectedEnemyShotRows,
                           kProjectileWidth, kProjectileHeight, Color::ORANGE,
                           background);
  }
}

// ---------------------------------------------------------------------
// AC-2.10: four structural assertions over every sprite's pixel data,
// covering the reduction risks §8 found (D1/D2/D11/D3). Expressed as
// constexpr predicates so every current sprite (still hand-authored at
// this point in the plan; T9/T10 later back kPlayerSprite/kEnemySprite
// with generated data without these predicates changing) is checked at
// compile time via static_assert below -- a violation is a build
// failure, not something that has to be run to be noticed. The
// dimensions/tight-packing/palette-membership/ink-rule/transparency
// guarantees AC-2.10 also names stay covered by the existing tests
// above (checkSpriteMatchesRows, checkBlitTransparency) and by
// galactic_invasion_art.h's own static_asserts -- unaffected, not
// restated here.
// ---------------------------------------------------------------------

namespace {

constexpr bool onOuterRing(int32_t x, int32_t y, int32_t w, int32_t h) {
  return x == 0 || x == w - 1 || y == 0 || y == h - 1;
}

// (i) D1: the outermost lit ring of every sprite contains no
// DARK_ORANGE -- a leftover halo/panel-fill pixel would show up here.
constexpr bool outerRingHasNoDarkOrange(const Color* px, int32_t w,
                                        int32_t h) {
  for (int32_t y = 0; y < h; ++y) {
    for (int32_t x = 0; x < w; ++x) {
      if (onOuterRing(x, y, w, h) && px[y * w + x] == Color::DARK_ORANGE) {
        return false;
      }
    }
  }
  return true;
}

// (ii) D1/D2: a non-BLACK pixel in the first row, last row, first column
// and last column (no sprite ships as a filled rectangle by *stopping
// short* of its box); and -- for sprites 3px or wider only (S1, the
// user's 2026-09-16 ruling: a 2-px-wide sprite is all outer ring, so
// this would forbid the deliberately solid player/enemy shots) -- at
// least one BLACK pixel in the outer ring, so no sprite ships as a
// filled rectangle by *covering* its whole box either.
constexpr bool touchesEveryEdgeAndHasBlackOnRingIfWideEnough(const Color* px,
                                                              int32_t w,
                                                              int32_t h) {
  bool firstRowLit = false, lastRowLit = false;
  bool firstColLit = false, lastColLit = false;
  for (int32_t x = 0; x < w; ++x) {
    if (px[0 * w + x] != Color::BLACK) firstRowLit = true;
    if (px[(h - 1) * w + x] != Color::BLACK) lastRowLit = true;
  }
  for (int32_t y = 0; y < h; ++y) {
    if (px[y * w + 0] != Color::BLACK) firstColLit = true;
    if (px[y * w + (w - 1)] != Color::BLACK) lastColLit = true;
  }
  if (!(firstRowLit && lastRowLit && firstColLit && lastColLit)) return false;

  if (w >= 3) {
    bool anyBlackOnRing = false;
    for (int32_t y = 0; y < h; ++y) {
      for (int32_t x = 0; x < w; ++x) {
        if (onOuterRing(x, y, w, h) && px[y * w + x] == Color::BLACK) {
          anyBlackOnRing = true;
        }
      }
    }
    if (!anyBlackOnRing) return false;
  }
  return true;
}

// (iii) D11: no lit pixel lacks a 4-neighbour of its own ink (rules out
// a stray, isolated single pixel -- indistinguishable from a stuck
// sub-pixel at this resolution), and no BLACK pixel is fully enclosed by
// lit pixels on all four sides (indistinguishable from a dead one) --
// except on the outer ring itself, where a 1-px notch open to the
// outline is silhouette, not noise, and stays allowed.
constexpr bool noIsolatedLitPixelsAndNoFullyEnclosedBlack(const Color* px,
                                                           int32_t w,
                                                           int32_t h) {
  for (int32_t y = 0; y < h; ++y) {
    for (int32_t x = 0; x < w; ++x) {
      const Color c = px[y * w + x];
      if (c != Color::BLACK) {
        bool hasSameInkNeighbour = false;
        if (x > 0 && px[y * w + (x - 1)] == c) hasSameInkNeighbour = true;
        if (x < w - 1 && px[y * w + (x + 1)] == c) hasSameInkNeighbour = true;
        if (y > 0 && px[(y - 1) * w + x] == c) hasSameInkNeighbour = true;
        if (y < h - 1 && px[(y + 1) * w + x] == c) hasSameInkNeighbour = true;
        if (!hasSameInkNeighbour) return false;
      } else if (!onOuterRing(x, y, w, h)) {
        const bool up = px[(y - 1) * w + x] != Color::BLACK;
        const bool down = px[(y + 1) * w + x] != Color::BLACK;
        const bool left = px[y * w + (x - 1)] != Color::BLACK;
        const bool right = px[y * w + (x + 1)] != Color::BLACK;
        if (up && down && left && right) return false;
      }
    }
  }
  return true;
}

// (iv) AC-2.3/D3: no BRIGHT_ORANGE pixel anywhere in enemy-side data --
// that ink is player-exclusive.
constexpr bool hasNoBrightOrange(const Color* px, int32_t w, int32_t h) {
  for (int32_t i = 0; i < w * h; ++i) {
    if (px[i] == Color::BRIGHT_ORANGE) return false;
  }
  return true;
}

}  // namespace

static_assert(outerRingHasNoDarkOrange(kPlayerSprite.pixels, kPlayerWidth,
                                        kPlayerHeight),
              "kPlayerSprite: DARK_ORANGE on the outer ring (D1)");
static_assert(outerRingHasNoDarkOrange(kEnemySprite.pixels, kEnemyWidth,
                                        kEnemyHeight),
              "kEnemySprite: DARK_ORANGE on the outer ring (D1)");
static_assert(outerRingHasNoDarkOrange(kProjectileSprite.pixels,
                                        kProjectileWidth, kProjectileHeight),
              "kProjectileSprite: DARK_ORANGE on the outer ring (D1)");
static_assert(outerRingHasNoDarkOrange(kEnemyShotSprite.pixels,
                                        kProjectileWidth, kProjectileHeight),
              "kEnemyShotSprite: DARK_ORANGE on the outer ring (D1)");

static_assert(
    touchesEveryEdgeAndHasBlackOnRingIfWideEnough(kPlayerSprite.pixels,
                                                   kPlayerWidth, kPlayerHeight),
    "kPlayerSprite: must touch every box edge and not fill it solid (D1/D2)");
static_assert(
    touchesEveryEdgeAndHasBlackOnRingIfWideEnough(kEnemySprite.pixels,
                                                   kEnemyWidth, kEnemyHeight),
    "kEnemySprite: must touch every box edge and not fill it solid (D1/D2)");
static_assert(touchesEveryEdgeAndHasBlackOnRingIfWideEnough(
                  kProjectileSprite.pixels, kProjectileWidth,
                  kProjectileHeight),
              "kProjectileSprite: must touch every box edge (D1/D2; "
              "2px wide, so the solid-fill check is exempt, S1)");
static_assert(touchesEveryEdgeAndHasBlackOnRingIfWideEnough(
                  kEnemyShotSprite.pixels, kProjectileWidth,
                  kProjectileHeight),
              "kEnemyShotSprite: must touch every box edge (D1/D2; "
              "2px wide, so the solid-fill check is exempt, S1)");

static_assert(noIsolatedLitPixelsAndNoFullyEnclosedBlack(
                  kPlayerSprite.pixels, kPlayerWidth, kPlayerHeight),
              "kPlayerSprite: isolated lit pixel or enclosed hole (D11)");
static_assert(noIsolatedLitPixelsAndNoFullyEnclosedBlack(
                  kEnemySprite.pixels, kEnemyWidth, kEnemyHeight),
              "kEnemySprite: isolated lit pixel or enclosed hole (D11)");
static_assert(noIsolatedLitPixelsAndNoFullyEnclosedBlack(
                  kProjectileSprite.pixels, kProjectileWidth,
                  kProjectileHeight),
              "kProjectileSprite: isolated lit pixel or enclosed hole (D11)");
static_assert(noIsolatedLitPixelsAndNoFullyEnclosedBlack(
                  kEnemyShotSprite.pixels, kProjectileWidth,
                  kProjectileHeight),
              "kEnemyShotSprite: isolated lit pixel or enclosed hole (D11)");

static_assert(hasNoBrightOrange(kEnemySprite.pixels, kEnemyWidth,
                                 kEnemyHeight),
              "kEnemySprite: BRIGHT_ORANGE is player-exclusive (AC-2.3/D3)");
static_assert(hasNoBrightOrange(kEnemyShotSprite.pixels, kProjectileWidth,
                                 kProjectileHeight),
              "kEnemyShotSprite: BRIGHT_ORANGE is player-exclusive "
              "(AC-2.3/D3)");

// The same four properties, restated as host tests rather than purely
// static_asserts: AC-2.10 asks for compile-time proof "where
// expressible" -- it is, above -- but a runtime CHECK failure also
// prints which sprite and which property broke, which a build error
// alone does not, so both forms are kept rather than only the terser
// one.
STEAMCORE_TEST(galactic_invasion_structural_no_dark_orange_on_outer_ring) {
  CHECK(outerRingHasNoDarkOrange(kPlayerSprite.pixels, kPlayerWidth,
                                  kPlayerHeight));
  CHECK(outerRingHasNoDarkOrange(kEnemySprite.pixels, kEnemyWidth,
                                  kEnemyHeight));
  CHECK(outerRingHasNoDarkOrange(kProjectileSprite.pixels, kProjectileWidth,
                                  kProjectileHeight));
  CHECK(outerRingHasNoDarkOrange(kEnemyShotSprite.pixels, kProjectileWidth,
                                  kProjectileHeight));
}

STEAMCORE_TEST(galactic_invasion_structural_touches_edges_not_filled_solid) {
  CHECK(touchesEveryEdgeAndHasBlackOnRingIfWideEnough(
      kPlayerSprite.pixels, kPlayerWidth, kPlayerHeight));
  CHECK(touchesEveryEdgeAndHasBlackOnRingIfWideEnough(
      kEnemySprite.pixels, kEnemyWidth, kEnemyHeight));
  CHECK(touchesEveryEdgeAndHasBlackOnRingIfWideEnough(
      kProjectileSprite.pixels, kProjectileWidth, kProjectileHeight));
  CHECK(touchesEveryEdgeAndHasBlackOnRingIfWideEnough(
      kEnemyShotSprite.pixels, kProjectileWidth, kProjectileHeight));
}

STEAMCORE_TEST(galactic_invasion_structural_no_isolated_pixels_or_holes) {
  CHECK(noIsolatedLitPixelsAndNoFullyEnclosedBlack(kPlayerSprite.pixels,
                                                    kPlayerWidth,
                                                    kPlayerHeight));
  CHECK(noIsolatedLitPixelsAndNoFullyEnclosedBlack(kEnemySprite.pixels,
                                                    kEnemyWidth,
                                                    kEnemyHeight));
  CHECK(noIsolatedLitPixelsAndNoFullyEnclosedBlack(
      kProjectileSprite.pixels, kProjectileWidth, kProjectileHeight));
  CHECK(noIsolatedLitPixelsAndNoFullyEnclosedBlack(
      kEnemyShotSprite.pixels, kProjectileWidth, kProjectileHeight));
}

STEAMCORE_TEST(galactic_invasion_structural_enemy_side_has_no_bright_orange) {
  CHECK(hasNoBrightOrange(kEnemySprite.pixels, kEnemyWidth, kEnemyHeight));
  CHECK(hasNoBrightOrange(kEnemyShotSprite.pixels, kProjectileWidth,
                           kProjectileHeight));
}

// AC-2.9: the six dimension constants stay put -- checked here in the
// sense that this file still compiles against them unchanged; the real
// proof is `git diff` showing no change to their declarations, done as
// part of this task's own verification, not restated as a runtime check.

// ---------------------------------------------------------------------
// T11/NFR-2: total art footprint <= 12,288 B (12 KB), computed at
// compile time from the frozen dimensions (A6) so it cannot silently
// drift as art content changes without the box it lives in also
// changing -- exactly the invariant NFR-2's own text names. Logo:
// 11,200 B (200x56 at 1 B/pixel, Color's underlying type); four cast
// sprites: 144 + 120 + 12 + 12 = 288 B; total 11,488 B, 800 B under
// budget.
// ---------------------------------------------------------------------

static_assert(sizeof(Color) == 1,
              "the footprint budget below assumes Color is 1 B/pixel");

// AC-1.2 caps each dimension ("at most 200x56 px overall"), not just the
// area: an area-only check would pass a 240x46 logo that is wider than
// the framebuffer's own documented ceiling allows (review F5).
static_assert(kLogoWidth <= 200 && kLogoHeight <= 56,
              "the logo must not exceed 200x56 px, its own documented "
              "ceiling (AC-1.2)");

// T11/NFR-1: the READY screen is static (no per-tick animation, no
// invulnerability-style flicker) -- consecutive PLAYING-untouched READY
// frames must be pixel-for-pixel identical, which is also what makes
// "no new dirty tiles after the first frame" (the other half of NFR-1)
// possible in the first place.
STEAMCORE_TEST(galactic_invasion_consecutive_ready_frames_are_identical) {
  GalacticInvasion game;
  Framebuffer fb;
  GameLoop<GalacticInvasion> loop(game, fb);

  loop.tick(GameInput{});
  Framebuffer first = fb;
  loop.tick(GameInput{});
  Framebuffer second = fb;
  loop.tick(GameInput{});

  CHECK(framebuffersEqual(first, second));
  CHECK(framebuffersEqual(second, fb));
}

static_assert(kPlayerWidth * kPlayerHeight * static_cast<int32_t>(sizeof(Color)) +
                      kEnemyWidth * kEnemyHeight *
                          static_cast<int32_t>(sizeof(Color)) +
                      kProjectileWidth * kProjectileHeight *
                          static_cast<int32_t>(sizeof(Color)) +
                      // The enemy shot shares kProjectileWidth/Height
                      // (both 2x6 shots do) -- counted a second time,
                      // once per sprite, not folded into the line above.
                      kProjectileWidth * kProjectileHeight *
                          static_cast<int32_t>(sizeof(Color)) +
                      kLogoWidth * kLogoHeight *
                          static_cast<int32_t>(sizeof(Color)) <=
                  12288,
              "total art footprint (player+enemy+projectile+enemy shot+"
              "logo) must not exceed 12,288 B (12 KB, NFR-2)");
