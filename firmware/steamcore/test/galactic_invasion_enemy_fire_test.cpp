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
using steamcore::games::kProjectileHeight;
using steamcore::games::kProjectileWidth;
using steamcore::test::anyEnemyShotOnScreen;
using steamcore::test::EnemyShots;
using steamcore::test::enemyShotThreatensColumn;
using steamcore::test::findEnemyShots;
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

// The number of enemy shots on screen this tick, counted by matching
// kEnemyShotSprite's own pixel data (fixture findEnemyShots). This
// replaces a count of disjoint horizontal segments of one ink: that
// approach read the true count only while the shot was a solid bar in a
// shade nothing else used, and would silently miscount a segmented
// silhouette (one shot, two segments per column) or a shot sharing its
// ink with an enemy body. Counting whole template matches is exact in
// both worlds.
int32_t maxConcurrentEnemyShots(const Framebuffer& fb) {
  return findEnemyShots(fb).count;
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

    // A spawn this tick is a shot standing at exactly one of the 18
    // candidate spawn points that was not there the tick before -- pinned
    // to the exact coordinate a fresh spawn must use, so an
    // already-travelling shot passing back through the same column (a real
    // risk: all 6 columns are reused by every row) is never mistaken for a
    // new one. Matching the whole sprite rather than one pixel of one ink
    // makes "a shot is standing here" the actual claim, not a proxy for it.
    const EnemyShots shotsNow = findEnemyShots(fb);
    const EnemyShots shotsBefore = findEnemyShots(prevFb);
    for (int32_t row = 0; row < kEnemyRows; ++row) {
      for (int32_t col = 0; col < kEnemyCols; ++col) {
        const SpawnPoint p = enemySpawnPoint(row, col, sim);
        bool isShotNow = false;
        for (int32_t i = 0; i < shotsNow.count; ++i) {
          if (shotsNow.at[i].x == p.x && shotsNow.at[i].y == p.y) {
            isShotNow = true;
          }
        }
        bool wasShotBefore = false;
        for (int32_t i = 0; i < shotsBefore.count; ++i) {
          if (shotsBefore.at[i].x == p.x && shotsBefore.at[i].y == p.y) {
            wasShotBefore = true;
          }
        }
        if (isShotNow && !wasShotBefore) sawAnySpawn = true;
      }
    }
    // Every shot's x must be a column x this formation could ever produce.
    // A shot's x is fixed at spawn, but offsetX has moved on by the time
    // it's still travelling -- checking only *this* tick's offsetX would
    // wrongly reject a shot spawned under an earlier one. Rather than
    // track offsetX's history, this checks x against every column at every
    // offsetX multiple of kFormationStepX the formation could ever reach
    // (the screen is only 240px wide and the formation 132px, so +-100 is
    // a comfortable superset of the true +-52-ish range stepFormation()'s
    // own edge check permits).
    for (int32_t i = 0; i < shotsNow.count; ++i) {
      const int32_t x = shotsNow.at[i].x;
      bool matchesSomeColumn = false;
      for (int32_t phase = -100; phase <= 100 && !matchesSomeColumn;
           phase += kFormationStepX) {
        for (int32_t col = 0; col < kEnemyCols; ++col) {
          FormationSim phased;
          phased.offsetX = phase;
          if (x == enemySpawnPoint(0, col, phased).x) {
            matchesSomeColumn = true;
            break;
          }
        }
      }
      if (!matchesSomeColumn) everyOneMatched = false;
    }
    // A locator that silently overflowed would make both checks above
    // vacuous, so an overflow is itself a failure.
    if (shotsNow.overflow) everyOneMatched = false;
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
      // Look for a shot already low on screen (its own span reaching the
      // bottom row) to start tracking, rather than following one for its
      // whole descent. Such a shot is deliberately clipped -- only its
      // topmost rows are on screen -- which is exactly the case
      // findEnemyShots handles by comparing visible cells only.
      const EnemyShots shots = findEnemyShots(fb);
      for (int32_t i = 0; i < shots.count; ++i) {
        const int32_t bottomRow = shots.at[i].y + kProjectileHeight - 1;
        if (bottomRow >= Framebuffer::height() - 1) {
          trackedX = shots.at[i].x;
          sawShotNearBottom = true;
          break;
        }
      }
      if (trackedX != -1) {
        trackedColumnHadShotLastTick =
            enemyShotThreatensColumn(fb, trackedX, trackedX + 1);
      }
      continue;
    }

    const bool stillThere =
        enemyShotThreatensColumn(fb, trackedX, trackedX + 1);
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

    // A shot overlapping the player's own row band. Located by sprite
    // match, so the player's pixels behind a transparent cell of the shot
    // cannot hide it -- that overlap is the entire point of this test.
    int32_t overlapX = -1;
    {
      const EnemyShots shots = findEnemyShots(fb);
      for (int32_t i = 0; i < shots.count && overlapX == -1; ++i) {
        const int32_t top = shots.at[i].y;
        const int32_t bottom = top + kProjectileHeight - 1;
        if (bottom >= kPlayerY && top < kPlayerY + kPlayerHeight) {
          overlapX = shots.at[i].x;
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
      const EnemyShots shots = findEnemyShots(fb);
      for (int32_t i = 0; i < shots.count; ++i) {
        if (shots.at[i].x == overlapX &&
            shots.at[i].y + kProjectileHeight - 1 >= kPlayerY) {
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

// ---------------------------------------------------------------------
// AC-2.11: negative controls for the locator this feature migrated from
// an ink scan to a sprite-template match. A rewritten locator that would
// still pass with the entity missing is a broken locator, not a migrated
// one -- so each of these proves the *absence* of a result, which no
// amount of gameplay assertion above would ever catch.
// ---------------------------------------------------------------------

// Absent: a frame with nothing drawn at all yields no match.
STEAMCORE_TEST(galactic_invasion_find_enemy_shots_negative_control_empty_frame) {
  Framebuffer fb;
  fb.clear(Color::BLACK);
  CHECK(findEnemyShots(fb).count == 0);
  CHECK(!anyEnemyShotOnScreen(fb));
  CHECK(!enemyShotThreatensColumn(fb, 0, Framebuffer::width()));
}

// Absent, and the hard case: a *full enemy formation with no shots in
// flight* must yield zero matches. This is the frame most likely to
// produce a spurious hit, since enemy bodies are the other thing on
// screen that carries large blocks of ink.
STEAMCORE_TEST(galactic_invasion_find_enemy_shots_negative_control_formation_only) {
  Framebuffer fb;
  fb.clear(Color::BLACK);
  for (int32_t row = 0; row < kEnemyRows; ++row) {
    for (int32_t col = 0; col < kEnemyCols; ++col) {
      fb.blit(steamcore::games::kEnemySprite,
              kFormationStartX + col * kEnemyPitchX,
              kFormationStartY + row * kEnemyPitchY);
    }
  }
  CHECK(findEnemyShots(fb).count == 0);
  CHECK(!anyEnemyShotOnScreen(fb));

  // Also at sub-pitch offsets, so the result does not depend on the
  // formation happening to sit at one convenient alignment.
  for (int32_t dx = 1; dx <= 3; ++dx) {
    Framebuffer shifted;
    shifted.clear(Color::BLACK);
    for (int32_t row = 0; row < kEnemyRows; ++row) {
      for (int32_t col = 0; col < kEnemyCols; ++col) {
        shifted.blit(steamcore::games::kEnemySprite,
                     kFormationStartX + col * kEnemyPitchX + dx,
                     kFormationStartY + row * kEnemyPitchY);
      }
    }
    CHECK(findEnemyShots(shifted).count == 0);
  }
}

// Present: one shot blitted by the test itself is found at exactly its
// own position -- and nowhere else.
STEAMCORE_TEST(galactic_invasion_find_enemy_shots_finds_a_placed_shot) {
  Framebuffer fb;
  fb.clear(Color::BLACK);
  fb.blit(steamcore::games::kEnemyShotSprite, 40, 20);
  const EnemyShots shots = findEnemyShots(fb);
  CHECK(shots.count == 1);
  CHECK(!shots.overflow);
  CHECK(shots.at[0].x == 40);
  CHECK(shots.at[0].y == 20);
  CHECK(enemyShotThreatensColumn(fb, 40, 41));
  CHECK(enemyShotThreatensColumn(fb, 41, 42));
}

// Displaced by one pixel: the reported position moves with it. A locator
// that returned a fixed or rounded position would pass the "present"
// test above and fail here.
STEAMCORE_TEST(galactic_invasion_find_enemy_shots_tracks_displacement) {
  for (int32_t dx = -1; dx <= 1; ++dx) {
    for (int32_t dy = -1; dy <= 1; ++dy) {
      Framebuffer fb;
      fb.clear(Color::BLACK);
      fb.blit(steamcore::games::kEnemyShotSprite, 40 + dx, 20 + dy);
      const EnemyShots shots = findEnemyShots(fb);
      CHECK(shots.count == 1);
      CHECK(shots.at[0].x == 40 + dx);
      CHECK(shots.at[0].y == 20 + dy);
    }
  }
  // Displaced out of a column it used to threaten.
  Framebuffer fb;
  fb.clear(Color::BLACK);
  fb.blit(steamcore::games::kEnemyShotSprite, 40, 20);
  CHECK(!enemyShotThreatensColumn(fb, 42, 44));
  CHECK(!enemyShotThreatensColumn(fb, 38, 40));
}

// Clipped at the bottom edge: a shot whose lower rows are off screen is
// still found, exactly once, at its true origin. Counting it more than
// once would silently inflate maxConcurrentEnemyShots past the engine's
// own kMaxEnemyShots limit and make that assertion vacuous.
STEAMCORE_TEST(galactic_invasion_find_enemy_shots_counts_a_clipped_shot_once) {
  for (int32_t y = Framebuffer::height() - kProjectileHeight;
       y < Framebuffer::height(); ++y) {
    Framebuffer fb;
    fb.clear(Color::BLACK);
    fb.blit(steamcore::games::kEnemyShotSprite, 40, y);
    const EnemyShots shots = findEnemyShots(fb);
    CHECK(shots.count == 1);
    CHECK(!shots.overflow);
    CHECK(shots.at[0].y == y);
  }
}

// Three concurrent shots -- the engine's own limit -- are counted as
// three, and the capacity does not overflow at exactly that number.
STEAMCORE_TEST(galactic_invasion_find_enemy_shots_counts_three_concurrent) {
  Framebuffer fb;
  fb.clear(Color::BLACK);
  fb.blit(steamcore::games::kEnemyShotSprite, 20, 10);
  fb.blit(steamcore::games::kEnemyShotSprite, 60, 40);
  fb.blit(steamcore::games::kEnemyShotSprite, 100, 90);
  const EnemyShots shots = findEnemyShots(fb);
  CHECK(shots.count == kMaxEnemyShots);
  CHECK(!shots.overflow);
}

// The shot is found even where it overlaps the player ship, which is what
// the invulnerability-window test above depends on: cells the sprite
// leaves transparent legitimately show the ship behind them.
STEAMCORE_TEST(galactic_invasion_find_enemy_shots_finds_shot_over_player) {
  Framebuffer fb;
  fb.clear(Color::BLACK);
  fb.blit(steamcore::games::kPlayerSprite, 40, kPlayerY);
  fb.blit(steamcore::games::kEnemyShotSprite, 44, kPlayerY + 2);
  const EnemyShots shots = findEnemyShots(fb);
  CHECK(shots.count == 1);
  CHECK(shots.at[0].x == 44);
  CHECK(shots.at[0].y == kPlayerY + 2);
}
