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
using steamcore::games::kEnemyCols;
using steamcore::games::kEnemyHeight;
using steamcore::games::kEnemyPitchX;
using steamcore::games::kEnemyPitchY;
using steamcore::games::kEnemyWidth;
using steamcore::games::kFormationStartX;
using steamcore::games::kFormationStartY;
using steamcore::games::kFormationStepTicks;
using steamcore::games::kFormationStepX;
using steamcore::games::kPlayerStartX;
using steamcore::games::kPlayerWidth;
using steamcore::games::kPlayerY;
using steamcore::games::kProjectileHeight;
using steamcore::games::kProjectileWidth;
using steamcore::games::kScoreBounds;
using steamcore::test::EnemyShots;
using steamcore::test::findEnemyShots;
using steamcore::test::findPlayerSpriteX;
using steamcore::test::pointInsideAnyShot;

namespace {

void enterPlaying(GameLoop<GalacticInvasion>& loop) {
  loop.tick(GameInput{});
  loop.tick(GameInput{/*start=*/true});
}

// Restated independently of galactic_invasion.cpp's own formatter --
// mirrors galactic_invasion_hud_test.cpp's own precedent.
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

// The formation's horizontal offset after `ticks` PLAYING-gameplay ticks,
// restated independently of stepFormation()'s own counter -- valid only
// before the formation has ever reached a screen edge (it hasn't, at the
// tick counts these tests use: offsets stay under 20, the edge is at 54).
int32_t offsetAfter(int32_t ticks) {
  return (ticks / kFormationStepTicks) * kFormationStepX;
}

// The still-alive rectangle a formation slot (row, col) occupies after
// `ticks` PLAYING-gameplay ticks -- restated independently of
// updateEnemyPositions()'s own grid formula.
Entity slotBounds(int32_t row, int32_t col, int32_t ticks) {
  return Entity{kFormationStartX + col * kEnemyPitchX + offsetAfter(ticks),
                kFormationStartY + row * kEnemyPitchY, kEnemyWidth,
                kEnemyHeight};
}

// Checks specifically for Color::ORANGE (the enemy sprite's own colour,
// distinct from the player/projectile's BRIGHT_ORANGE) -- a plain
// != BLACK test would false-positive whenever the player's own projectile
// happens to be transiting the same rectangle on its way further up.
bool enemyPixelIn(const Framebuffer& fb, const Entity& r) {
  for (int32_t y = r.y; y < r.y + r.h; ++y) {
    for (int32_t x = r.x; x < r.x + r.w; ++x) {
      if (fb.pixel(x, y) == Color::ORANGE) return true;
    }
  }
  return false;
}

// Column 2's alignment with the stationary player's fixed spawn column, at
// the specific formation offsets this file's two combat tests run through
// (derived by hand against the production geometry constants and recorded
// here rather than re-derived at runtime, since GalacticInvasion exposes no
// accessor a test could use to confirm it structurally -- NFR-5): the
// player never moves, so its shot always spawns at
// kPlayerStartX + (kPlayerWidth - kProjectileWidth)/2 = 119. offsetAfter(t)
// = 8..16 for t in [36,89] places column 2's window
// [kFormationStartX + 2*kEnemyPitchX + offset, +kEnemyWidth) over x=119,
// while every other column's window misses it throughout. A shot fired
// continuously from a stationary player therefore always destroys column
// 2's own enemies, front row (row 2) first, then row 1 -- never any other
// slot.
constexpr int32_t kTargetCol = 2;
constexpr int32_t kFrontRow = 2;
constexpr int32_t kMiddleRow = 1;

GameInput fireInput() {
  GameInput input{};
  input.fire = true;
  return input;
}

}  // namespace

// AC-4.1/AC-4.2: a scripted shot destroys a known, surviving enemy via the
// shipped checkCollision (no bespoke overlap math anywhere in this
// feature's own source -- confirmed by reading galactic_invasion.cpp) and
// the score increases by exactly kScorePerKill the same tick; every other
// enemy is unaffected.
STEAMCORE_TEST(galactic_invasion_shot_destroys_a_known_enemy_and_scores) {
  GalacticInvasion game;
  Framebuffer fb;
  GameLoop<GalacticInvasion> loop(game, fb);
  enterPlaying(loop);

  // Hold fire, never move: continuous re-fire from the fixed spawn column
  // reaches column 2's front-row (row 2) enemy around tick ~58-60 (the
  // second shot's flight, the first having missed and self-cleared). 70
  // ticks is comfortably past that with margin to spare, and comfortably
  // before the next shot could reach row 1 (~86-88).
  constexpr int32_t kTicksUntilFrontRowKill = 70;
  for (int32_t i = 0; i < kTicksUntilFrontRowKill; ++i) loop.tick(fireInput());

  Framebuffer expectedScore;
  placeExpected(expectedScore, kScoreBounds.x, kScoreBounds.y, "SCORE: 0010",
                Color::BRIGHT_ORANGE);
  for (int32_t y = kScoreBounds.y; y < kScoreBounds.y + kScoreBounds.h; ++y) {
    for (int32_t x = kScoreBounds.x; x < kScoreBounds.x + kScoreBounds.w; ++x) {
      CHECK(fb.pixel(x, y) == expectedScore.pixel(x, y));
    }
  }

  for (int32_t row = 0; row < 3; ++row) {
    for (int32_t col = 0; col < kEnemyCols; ++col) {
      const bool expectedAlive =
          !(row == kFrontRow && col == kTargetCol);
      CHECK_EQ(enemyPixelIn(fb, slotBounds(row, col, kTicksUntilFrontRowKill)),
               expectedAlive);
    }
  }
}

// AC-4.1: a shot's path continuing through a just-destroyed enemy's former
// slot registers no further collision there (a zero-size Entity overlaps
// nothing, collision.h's own contract) -- it keeps travelling and destroys
// the next surviving enemy in its path instead, which this same stationary
// firing script also reaches (column 2's middle row, row 1, next).
STEAMCORE_TEST(galactic_invasion_shot_passes_through_a_destroyed_slot) {
  GalacticInvasion game;
  Framebuffer fb;
  GameLoop<GalacticInvasion> loop(game, fb);
  enterPlaying(loop);

  constexpr int32_t kTicksUntilMiddleRowKill = 100;
  for (int32_t i = 0; i < kTicksUntilMiddleRowKill; ++i) loop.tick(fireInput());

  Framebuffer expectedScore;
  placeExpected(expectedScore, kScoreBounds.x, kScoreBounds.y, "SCORE: 0020",
                Color::BRIGHT_ORANGE);
  for (int32_t y = kScoreBounds.y; y < kScoreBounds.y + kScoreBounds.h; ++y) {
    for (int32_t x = kScoreBounds.x; x < kScoreBounds.x + kScoreBounds.w; ++x) {
      CHECK(fb.pixel(x, y) == expectedScore.pixel(x, y));
    }
  }

  for (int32_t row = 0; row < 3; ++row) {
    for (int32_t col = 0; col < kEnemyCols; ++col) {
      const bool destroyed =
          col == kTargetCol && (row == kFrontRow || row == kMiddleRow);
      CHECK_EQ(enemyPixelIn(fb, slotBounds(row, col, kTicksUntilMiddleRowKill)),
               !destroyed);
    }
  }
}

// AC-4.3/AC-11.1: clearing every enemy ends the round via sessionEnded=true
// on the killing tick -- GameSession reaches GAME_OVER exactly as any other
// ending (A3), and no enemy, player or projectile pixel is left rendered
// (the GAME_OVER render branch draws none of them). A continuously-firing
// sweep across the whole screen width is used instead of hand-scripting 18
// individual shots. Since T8, player-vs-enemy contact is a second, real way
// to reach GAME_OVER (a loss), so reaching it is no longer proof by itself
// that this was a win -- this test additionally tracks the formation's own
// lowest surviving row every tick and asserts it never came anywhere near
// the player's row, which rules out a contact-loss having caused the
// ending: the only ending left possible is therefore the formation-clear
// win this test claims (AC-4.3/AC-11.1).
STEAMCORE_TEST(galactic_invasion_clearing_every_enemy_wins) {
  GalacticInvasion game;
  Framebuffer fb;
  GameLoop<GalacticInvasion> loop(game, fb);
  enterPlaying(loop);

  // Color::ORANGE is the enemy side's identity ink -- scanning for it
  // specifically survives the transition into GAME_OVER correctly,
  // unlike the shared findFormationBounds fixture helper, whose
  // `!= BLACK` scan over [kFormationStartY, kFormationThresholdY) would
  // otherwise pick up the "GAME OVER" text's own BRIGHT_ORANGE pixels,
  // which happen to fall inside that same y-band, and misread them as
  // "formation still there". Since AC-3.10/D4, the enemy *shot* is
  // ORANGE too (previously DARK_ORANGE, unique to it) -- a raw ORANGE
  // scan would misread a shot in flight as more enemy body, so any pixel
  // inside a located shot's bounding box is excluded (AC-2.8).
  auto anyEnemyPixelOnScreen = [&fb]() {
    const EnemyShots shots = findEnemyShots(fb);
    for (int32_t y = 0; y < Framebuffer::height(); ++y) {
      for (int32_t x = 0; x < Framebuffer::width(); ++x) {
        if (fb.pixel(x, y) == Color::ORANGE &&
            !pointInsideAnyShot(shots, x, y)) {
          return true;
        }
      }
    }
    return false;
  };
  // The lowest y at which any enemy-body Color::ORANGE pixel currently
  // renders, or -1 if none do -- tracks how close the formation's
  // leading edge has come to the player's row. Shot pixels excluded for
  // the same reason as anyEnemyPixelOnScreen above: a shot travelling
  // toward the player would otherwise be misread as the formation itself
  // having descended that far.
  auto lowestEnemyY = [&fb]() {
    const EnemyShots shots = findEnemyShots(fb);
    for (int32_t y = Framebuffer::height() - 1; y >= 0; --y) {
      for (int32_t x = 0; x < Framebuffer::width(); ++x) {
        if (fb.pixel(x, y) == Color::ORANGE &&
            !pointInsideAnyShot(shots, x, y)) {
          return y;
        }
      }
    }
    return -1;
  };

  constexpr int32_t kSweepHalfPeriod = 150;  // > (width - kPlayerWidth)/speed
  constexpr int32_t kMaxTicks = 20000;  // empirically clears by tick ~2585
  bool cleared = false;
  int32_t phase = 0;
  int32_t maxObservedEnemyY = -1;
  for (int32_t i = 0; i < kMaxTicks && !cleared; ++i) {
    GameInput input = fireInput();
    if ((phase / kSweepHalfPeriod) % 2 == 0) {
      input.right = true;
    } else {
      input.left = true;
    }
    ++phase;
    loop.tick(input);

    const int32_t y = lowestEnemyY();
    if (y > maxObservedEnemyY) maxObservedEnemyY = y;
    if (!anyEnemyPixelOnScreen()) cleared = true;
  }

  CHECK(cleared);
  // The formation's leading edge never came within one enemy-height of the
  // player's row -- contact (T8's AC-5.1) was structurally impossible
  // throughout this run, so the ending reached below can only be the
  // formation-clear win, not a contact-triggered loss.
  CHECK(maxObservedEnemyY + kEnemyHeight < kPlayerY);
  // The GAME_OVER render branch composes only the outcome text -- no
  // formation, no player, no projectile survives into this frame.
  CHECK(!anyEnemyPixelOnScreen());
  CHECK_EQ(findPlayerSpriteX(fb), -1);
  // Something was actually drawn (the outcome text), not a blank frame.
  bool anyLit = false;
  for (int32_t y = 0; y < Framebuffer::height() && !anyLit; ++y) {
    for (int32_t x = 0; x < Framebuffer::width(); ++x) {
      if (fb.pixel(x, y) != Color::BLACK) {
        anyLit = true;
        break;
      }
    }
  }
  CHECK(anyLit);
}
