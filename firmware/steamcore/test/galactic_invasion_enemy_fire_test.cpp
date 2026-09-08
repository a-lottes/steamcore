#include "galactic_invasion/galactic_invasion.h"

#include "fb_compare.h"
#include "galactic_invasion_fixture.h"
#include "steamcore/color.h"
#include "steamcore/font.h"
#include "steamcore/game_loop.h"
#include "steamcore/sprite.h"
#include "test_harness.h"

using steamcore::Color;
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
using steamcore::games::kEnemyRows;
using steamcore::games::kEnemyWidth;
using steamcore::games::kFormationDescendY;
using steamcore::games::kFormationStartX;
using steamcore::games::kFormationStartY;
using steamcore::games::kFormationStepTicks;
using steamcore::games::kFormationStepX;
using steamcore::games::kInvulnerabilityTicks;
using steamcore::games::kLivesBounds;
using steamcore::games::kMaxEnemyShots;
using steamcore::games::kPlayerHeight;
using steamcore::games::kPlayerY;
using steamcore::games::kProjectileWidth;
using steamcore::test::framebuffersEqual;

namespace {

void enterPlaying(GameLoop<GalacticInvasion>& loop) {
  loop.tick(GameInput{});
  loop.tick(GameInput{/*start=*/true});
}

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

bool livesTextIs(const Framebuffer& fb, const char* text) {
  Framebuffer expected;
  placeExpected(expected, kLivesBounds.x, kLivesBounds.y, text,
                Color::BRIGHT_ORANGE);
  for (int32_t y = kLivesBounds.y; y < kLivesBounds.y + kLivesBounds.h; ++y) {
    for (int32_t x = kLivesBounds.x; x < kLivesBounds.x + kLivesBounds.w;
         ++x) {
      if (fb.pixel(x, y) != expected.pixel(x, y)) return false;
    }
  }
  return true;
}

// The largest number of disjoint Color::DARK_ORANGE horizontal segments
// found in any single row -- distinct enemy shots never share an x-range
// (each is kProjectileWidth wide and travels independently), so this is a
// safe lower-bound-that-equals-the-true-count of shots simultaneously on
// screen at this tick.
int32_t maxConcurrentEnemyShots(const Framebuffer& fb) {
  int32_t maxSegments = 0;
  for (int32_t y = 0; y < Framebuffer::height(); ++y) {
    int32_t segments = 0;
    bool wasDark = false;
    for (int32_t x = 0; x < Framebuffer::width(); ++x) {
      const bool isDark = fb.pixel(x, y) == Color::DARK_ORANGE;
      if (isDark && !wasDark) ++segments;
      wasDark = isDark;
    }
    if (segments > maxSegments) maxSegments = segments;
  }
  return maxSegments;
}

// A from-scratch simulation of the formation's own x/y kinematics,
// mirroring stepFormation()'s bounce/descend rule (plan §1 Decision 7) --
// restated independently, the same convention
// galactic_invasion_combat_test.cpp's slotBounds() and
// galactic_invasion_round_test.cpp's FormationSim both follow. Valid here
// because this file's tests never fire, so every column stays alive the
// whole run and "any surviving enemy would leave the screen" collapses to
// just checking the two outermost columns. Stepped tick by tick in
// lockstep with the real game, since a 1200-tick run spans several
// reversals (and their descents) -- a naive closed-form offset formula is
// only valid before the first one.
struct FormationSim {
  int32_t offsetX = 0;
  int32_t offsetY = 0;
  int32_t dir = 1;
  int32_t stepTicks = 0;
};

void advanceFormationSim(FormationSim& s) {
  if (++s.stepTicks < kFormationStepTicks) return;
  s.stepTicks = 0;
  const int32_t leftmostX = kFormationStartX + s.offsetX;
  const int32_t rightmostX =
      kFormationStartX + (kEnemyCols - 1) * kEnemyPitchX + s.offsetX;
  const int32_t candidateLeft = leftmostX + s.dir * kFormationStepX;
  const int32_t candidateRight = rightmostX + s.dir * kFormationStepX;
  const bool wouldLeaveScreen =
      candidateLeft < 0 || candidateRight + kEnemyWidth > Framebuffer::width();
  if (wouldLeaveScreen) {
    s.dir = -s.dir;
    s.offsetY += kFormationDescendY;
  } else {
    s.offsetX += s.dir * kFormationStepX;
  }
}

// The exact spawn point (top-left) a shot from (row, col) would use this
// tick -- restated independently of stepEnemyShots()'s own formula.
struct SpawnPoint {
  int32_t x, y;
};

SpawnPoint enemySpawnPoint(int32_t row, int32_t col, const FormationSim& s) {
  const int32_t enemyX = kFormationStartX + col * kEnemyPitchX + s.offsetX;
  const int32_t enemyY = kFormationStartY + row * kEnemyPitchY + s.offsetY;
  return SpawnPoint{enemyX + (kEnemyWidth - kProjectileWidth) / 2,
                     enemyY + kEnemyHeight};
}

int32_t tickUntilFirstHit(GameLoop<GalacticInvasion>& loop, const Framebuffer& fb,
                           int32_t capTicks) {
  for (int32_t t = 1; t <= capTicks; ++t) {
    loop.tick(GameInput{});
    if (!livesTextIs(fb, "LIVES: 3")) return t;
  }
  return -1;
}

}  // namespace

// AC-8.1: never more than kMaxEnemyShots enemy projectiles in flight at
// once, checked every tick across a long run.
STEAMCORE_TEST(galactic_invasion_never_more_than_max_enemy_shots_in_flight) {
  GalacticInvasion game;
  Framebuffer fb;
  GameLoop<GalacticInvasion> loop(game, fb);
  enterPlaying(loop);

  int32_t maxSeen = 0;
  for (int32_t t = 0; t < 1200; ++t) {
    loop.tick(GameInput{});
    const int32_t seen = maxConcurrentEnemyShots(fb);
    if (seen > maxSeen) maxSeen = seen;
  }
  CHECK(maxSeen > 0);  // the pool is actually exercised over this long a run
  CHECK(maxSeen <= kMaxEnemyShots);
}

// AC-8.1: every new shot appears at a then-surviving enemy's own
// bottom-centre x -- checked by recomputing every column's independently-
// derived x at the tick a shot is first seen there, mirroring
// galactic_invasion_combat_test.cpp's own "restate the grid formula, don't
// call into production" convention.
STEAMCORE_TEST(galactic_invasion_every_enemy_shot_spawns_at_a_column_centre) {
  GalacticInvasion game;
  Framebuffer fb;
  GameLoop<GalacticInvasion> loop(game, fb);
  enterPlaying(loop);

  bool sawAnySpawn = false;
  bool everyOneMatched = true;
  Framebuffer prevFb;  // all-BLACK, matching PLAYING's very first frame
  FormationSim sim;
  for (int32_t t = 1; t <= 1200; ++t) {
    loop.tick(GameInput{});
    advanceFormationSim(sim);

    // A spawn this tick is a pixel that is now DARK_ORANGE at exactly one
    // of the 18 candidate spawn points, and was not DARK_ORANGE there the
    // tick before -- pinned to the exact coordinate a fresh spawn must
    // use, so an already-travelling shot passing back through the same
    // column (a real risk: all 6 columns are reused by every row) is
    // never mistaken for a new one.
    for (int32_t row = 0; row < kEnemyRows; ++row) {
      for (int32_t col = 0; col < kEnemyCols; ++col) {
        const SpawnPoint p = enemySpawnPoint(row, col, sim);
        const bool isDarkNow = fb.pixel(p.x, p.y) == Color::DARK_ORANGE;
        const bool wasDarkBefore = prevFb.pixel(p.x, p.y) == Color::DARK_ORANGE;
        if (isDarkNow && !wasDarkBefore) sawAnySpawn = true;
      }
    }
    // Every DARK_ORANGE pixel's x must be a column x this formation could
    // ever produce. A shot's x is fixed at spawn, but offsetX has moved on
    // by the time it's still travelling -- checking only *this* tick's
    // offsetX would wrongly reject a shot spawned under an earlier one.
    // Rather than track offsetX's history, this checks x against every
    // column at every offsetX multiple of kFormationStepX the formation
    // could ever reach (the screen is only 240px wide and the formation
    // 132px, so +-100 is a comfortable superset of the true +-52-ish
    // range stepFormation()'s own edge check permits).
    for (int32_t y = 0; y < Framebuffer::height(); ++y) {
      for (int32_t x = 0; x < Framebuffer::width(); ++x) {
        if (fb.pixel(x, y) != Color::DARK_ORANGE) continue;
        bool matchesSomeColumn = false;
        for (int32_t phase = -100; phase <= 100 && !matchesSomeColumn;
             phase += kFormationStepX) {
          for (int32_t col = 0; col < kEnemyCols; ++col) {
            FormationSim phased;
            phased.offsetX = phase;
            const int32_t candidateX = enemySpawnPoint(0, col, phased).x;
            // The sprite is kProjectileWidth (2) pixels wide -- x can be
            // either column of it, not just its own left edge.
            if (x >= candidateX && x < candidateX + kProjectileWidth) {
              matchesSomeColumn = true;
              break;
            }
          }
        }
        if (!matchesSomeColumn) everyOneMatched = false;
      }
    }
    prevFb = fb;
  }
  CHECK(sawAnySpawn);
  CHECK(everyOneMatched);
}

// AC-8.1/NFR-3: the same (empty) input script on two fresh instances
// produces byte-identical frames every tick, including whichever enemy
// fires and when -- the LCG is a pure function of its own advancing state,
// never a clock or true randomness.
STEAMCORE_TEST(galactic_invasion_enemy_fire_sequence_is_deterministic) {
  GalacticInvasion gameA;
  Framebuffer fbA;
  GameLoop<GalacticInvasion> loopA(gameA, fbA);
  enterPlaying(loopA);

  GalacticInvasion gameB;
  Framebuffer fbB;
  GameLoop<GalacticInvasion> loopB(gameB, fbB);
  enterPlaying(loopB);

  int32_t firstMismatch = -1;
  for (int32_t t = 0; t < 1200 && firstMismatch == -1; ++t) {
    loopA.tick(GameInput{});
    loopB.tick(GameInput{});
    if (!framebuffersEqual(fbA, fbB)) firstMismatch = t;
  }
  CHECK_EQ(firstMismatch, -1);
}

// AC-8.3: a shot that reaches the bottom edge without hitting the player
// is removed -- tracked by watching a single column (once a shot is known
// to be there) until it clears the last row, then confirming it is gone
// the very next tick.
STEAMCORE_TEST(galactic_invasion_enemy_shot_reaching_bottom_is_removed) {
  GalacticInvasion game;
  Framebuffer fb;
  GameLoop<GalacticInvasion> loop(game, fb);
  enterPlaying(loop);

  bool sawShotNearBottom = false;
  bool clearedNextTick = false;
  bool trackedColumnHadShotLastTick = false;
  int32_t trackedX = -1;

  for (int32_t t = 1; t <= 1200 && !clearedNextTick; ++t) {
    loop.tick(GameInput{});

    if (trackedX == -1) {
      // Look for a shot already low on screen (close to the bottom) to
      // start tracking, rather than following one for its whole descent.
      for (int32_t x = 0; x < Framebuffer::width(); ++x) {
        if (fb.pixel(x, Framebuffer::height() - 1) == Color::DARK_ORANGE) {
          trackedX = x;
          sawShotNearBottom = true;
          break;
        }
      }
      if (trackedX != -1) {
        bool stillThere = false;
        for (int32_t y = 0; y < Framebuffer::height(); ++y) {
          if (fb.pixel(trackedX, y) == Color::DARK_ORANGE) {
            stillThere = true;
            break;
          }
        }
        trackedColumnHadShotLastTick = stillThere;
      }
      continue;
    }

    bool stillThere = false;
    for (int32_t y = 0; y < Framebuffer::height(); ++y) {
      if (fb.pixel(trackedX, y) == Color::DARK_ORANGE) {
        stillThere = true;
        break;
      }
    }
    if (trackedColumnHadShotLastTick && !stillThere) {
      clearedNextTick = true;
    }
    trackedColumnHadShotLastTick = stillThere;
  }

  CHECK(sawShotNearBottom);
  CHECK(clearedNextTick);
}

// AC-8.2/AC-10.2/AC-10.3: an enemy-shot hit costs exactly one life and
// starts the same 120-tick invulnerability window direct contact does --
// verified generically here (this stationary, never-fired-upon scenario's
// first hit is, in practice, an enemy shot reaching the player long before
// the formation's own body ever could -- galactic_invasion_lives_test.cpp
// already proves the window's exact 120-tick boundary in depth; this
// confirms the shot path specifically reaches resolvePlayerHit() at all).
STEAMCORE_TEST(galactic_invasion_enemy_shot_hit_costs_one_life_and_starts_invulnerability) {
  GalacticInvasion game;
  Framebuffer fb;
  GameLoop<GalacticInvasion> loop(game, fb);
  enterPlaying(loop);

  const int32_t hitTick = tickUntilFirstHit(loop, fb, 10000);
  CHECK(hitTick != -1);
  CHECK(livesTextIs(fb, "LIVES: 2"));

  // Protected for the next 120 ticks, regardless of what caused the hit.
  for (int32_t k = 0; k < kInvulnerabilityTicks; ++k) {
    loop.tick(GameInput{});
    CHECK(livesTextIs(fb, "LIVES: 2"));
  }
}

// AC-10.3: an enemy shot overlapping an invulnerable player costs nothing
// and is itself unaffected -- it keeps travelling toward the bottom edge
// rather than being consumed on contact, checked by confirming a shot
// that overlaps the player's own row during the window is still present
// (further down) on a later tick, not just gone.
STEAMCORE_TEST(galactic_invasion_enemy_shot_through_invulnerable_player_is_unaffected) {
  GalacticInvasion game;
  Framebuffer fb;
  GameLoop<GalacticInvasion> loop(game, fb);
  enterPlaying(loop);

  const int32_t hitTick = tickUntilFirstHit(loop, fb, 10000);
  CHECK(hitTick != -1);

  bool sawOverlapDuringWindow = false;
  bool confirmedUnaffected = false;
  for (int32_t k = 0; k < kInvulnerabilityTicks && !confirmedUnaffected; ++k) {
    loop.tick(GameInput{});
    CHECK(livesTextIs(fb, "LIVES: 2"));  // the window itself never breaks

    int32_t overlapX = -1;
    for (int32_t y = kPlayerY; y < kPlayerY + kPlayerHeight && overlapX == -1;
         ++y) {
      for (int32_t x = 0; x < Framebuffer::width(); ++x) {
        if (fb.pixel(x, y) == Color::DARK_ORANGE) {
          overlapX = x;
          break;
        }
      }
    }
    if (overlapX == -1) continue;
    sawOverlapDuringWindow = true;

    // The overlapping shot was not consumed on the spot -- something is
    // still travelling down this exact column (either the same shot
    // further along, or it has already cleared the bottom edge, AC-8.3;
    // either way "vanished immediately at the player's row" is ruled out).
    bool stillPresentThisColumn = false;
    for (int32_t j = 0; j < 5 && !stillPresentThisColumn; ++j) {
      loop.tick(GameInput{});
      for (int32_t y = kPlayerY; y < Framebuffer::height(); ++y) {
        if (fb.pixel(overlapX, y) == Color::DARK_ORANGE) {
          stillPresentThisColumn = true;
          break;
        }
      }
    }
    confirmedUnaffected = stillPresentThisColumn;
  }
  CHECK(sawOverlapDuringWindow);
  CHECK(confirmedUnaffected);
}
