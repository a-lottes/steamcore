#pragma once

#include <cstdint>

#include "galactic_invasion/galactic_invasion.h"
#include "steamcore/color.h"
#include "steamcore/framebuffer.h"
#include "steamcore/game_loop.h"

// Shared test fixture for every galactic_invasion_*_test.cpp file (plan
// §1 Decision 1's stated deviation: this game's tests live in the
// existing host suite, not a second test root under games/). No
// accessor into GalacticInvasion's private state exists (NFR-5) -- every
// helper here observes only what a real player could see: rendered
// pixels.

namespace steamcore::test {

// Ticks `loop` `n` times with the same `input` every tick.
inline void runTicks(GameLoop<games::GalacticInvasion>& loop,
                      const GameInput& input, int32_t n) {
  for (int32_t i = 0; i < n; ++i) loop.tick(input);
}

// Locates the player ship's bounding-box left edge: the minimum lit x
// across its whole fixed row band (kPlayerY..kPlayerY+kPlayerHeight),
// not just its top row -- the ship's wedge shape is narrower at the top
// than at the base, so scanning only the topmost row would find the
// nose's offset, not the sprite's actual left edge. Returns -1 if no lit
// pixel exists in that band at all (e.g. PLAYING has not started).
inline int32_t findPlayerSpriteX(const Framebuffer& fb) {
  int32_t minX = -1;
  for (int32_t y = games::kPlayerY; y < games::kPlayerY + games::kPlayerHeight;
       ++y) {
    for (int32_t x = 0; x < Framebuffer::width(); ++x) {
      if (fb.pixel(x, y) != Color::BLACK) {
        if (minX == -1 || x < minX) minX = x;
        break;
      }
    }
  }
  return minX;
}

// The bounding box of every lit pixel within the formation's own y-band
// (kFormationStartY up to, but not including, kFormationThresholdY --
// the player's row), which never overlaps the HUD or the player row
// (proven at compile time in the header), so no extra filtering is
// needed to exclude them. All fields are -1 if nothing is lit in that
// band at all.
struct FormationBounds {
  int32_t minX = -1;
  int32_t maxX = -1;
  int32_t minY = -1;
  int32_t maxY = -1;
};

inline FormationBounds findFormationBounds(const Framebuffer& fb) {
  FormationBounds bounds;
  for (int32_t y = games::kFormationStartY; y < games::kFormationThresholdY;
       ++y) {
    for (int32_t x = 0; x < Framebuffer::width(); ++x) {
      if (fb.pixel(x, y) == Color::BLACK) continue;
      if (bounds.minX == -1 || x < bounds.minX) bounds.minX = x;
      if (x > bounds.maxX) bounds.maxX = x;
      if (bounds.minY == -1) bounds.minY = y;
      bounds.maxY = y;
    }
  }
  return bounds;
}

// The topmost row in column `x` that renders Color::BRIGHT_ORANGE within
// [0, kPlayerY) -- the player's own row is excluded, and BRIGHT_ORANGE is
// player-exclusive by spec (AC-2.3: the player ship and player shot carry
// it, enemy-side sprites may never contain it), so this cannot mistake an
// enemy sprite for the projectile even if their x ranges happen to
// overlap. That is a stated guarantee, not a coincidence of the current
// art: it is asserted over every sprite's pixel data, so a redraw cannot
// quietly invalidate this helper. Returns -1 if no such pixel exists (no
// shot in flight in that column).
inline int32_t findProjectileTopY(const Framebuffer& fb, int32_t x) {
  for (int32_t y = 0; y < games::kPlayerY; ++y) {
    if (fb.pixel(x, y) == Color::BRIGHT_ORANGE) return y;
  }
  return -1;
}

// Every enemy shot currently on screen, located by matching the rendered
// pixels against kEnemyShotSprite's own pixel data rather than against an
// ink. A locator written against a colour breaks the next time the art
// changes; one written against the sprite constant follows it for free
// (AC-2.8) -- and it discriminates the shot from enemy *bodies*, which a
// single-ink scan cannot once both sides may share a colour.
//
// Two subtleties the naive "does this 2x6 window equal the sprite" loop
// gets wrong, both load-bearing:
//
//  - **Clipping.** stepEnemyShots() only retires a shot once its y has
//    reached Framebuffer::height(), so a shot sitting at y = height - 1
//    is on screen with five of its six rows off the bottom edge. Cells
//    that fall outside the framebuffer are therefore skipped rather than
//    compared, exactly as blit's own clipping contract draws them; a
//    candidate with no visible cell at all is not a match.
//  - **Span claiming, not a "row above" anchor.** Clipping alone would
//    let a clipped shot match again at every row below its true origin.
//    A first attempt rejected a candidate whenever the row directly
//    above it already carried the shot's own ink -- but AC-3.10's
//    segmented silhouette (this file predates it: kEnemyShotRows is
//    "##"/"##"/"  "/"  "/"##"/"##") defeats that: the row above the
//    *second* block is the sprite's own internal gap, which is empty,
//    so a clipped tail showing only that second block looks like a
//    fresh top edge (caught by
//    galactic_invasion_find_enemy_shots_counts_a_clipped_shot_once,
//    which exists precisely to catch this -- plan risk R4). Scanning y
//    ascending, each accepted match instead claims x's column from its
//    own y through y + kProjectileHeight (its full, unclipped height,
//    not just what's visible) -- any later candidate at the same x
//    falling inside that span is a slice of the same physical shot, no
//    matter what pattern is inside it, and is skipped without
//    inspecting its pixels at all.
//
// Capacity is kMaxEnemyShots, the engine's own in-flight limit, so no
// allocation happens; `overflow` reports a match count beyond that limit,
// which means the locator has started firing spuriously and the test
// relying on it is no longer sound.
struct EnemyShotPosition {
  int32_t x = -1;
  int32_t y = -1;
};

struct EnemyShots {
  int32_t count = 0;
  bool overflow = false;
  EnemyShotPosition at[games::kMaxEnemyShots]{};
};

// The ink kEnemyShotSprite draws itself in: its first non-BLACK cell.
// Derived from the sprite data, never named as a colour here, so this
// file needs no update when the shot's ink changes.
inline Color enemyShotInk() {
  for (int32_t i = 0; i < games::kProjectileWidth * games::kProjectileHeight;
       ++i) {
    const Color c = games::detail::kEnemyShotPixels[static_cast<size_t>(i)];
    if (c != Color::BLACK) return c;
  }
  return Color::BLACK;
}

inline EnemyShots findEnemyShots(const Framebuffer& fb) {
  EnemyShots found;
  const Color ink = enemyShotInk();
  // claimedUntil[x]: the y a match already accepted in column x claims
  // through (exclusive); 0 means unclaimed, since y=0 is never itself an
  // exclusive upper bound for a real match. One entry per column the
  // sprite's left edge could ever occupy.
  int32_t claimedUntil[Framebuffer::width()] = {};

  for (int32_t y = 0; y < Framebuffer::height(); ++y) {
    for (int32_t x = 0; x + games::kProjectileWidth <= Framebuffer::width();
         ++x) {
      if (y < claimedUntil[x]) continue;  // inside an already-claimed span

      bool comparedAny = false;
      bool matches = true;
      for (int32_t row = 0; row < games::kProjectileHeight && matches; ++row) {
        const int32_t py = y + row;
        if (py >= Framebuffer::height()) break;  // clipped off the bottom
        for (int32_t col = 0; col < games::kProjectileWidth; ++col) {
          const Color expected = games::detail::kEnemyShotPixels[static_cast<
              size_t>(row * games::kProjectileWidth + col)];
          const Color actual = fb.pixel(x + col, py);
          if (expected == Color::BLACK) {
            // A transparent cell of the sprite: blit never wrote here, so
            // whatever is behind the shot legitimately shows through (a
            // shot overlapping the player's row is a real, tested case).
            // Requiring BLACK here would make the locator miss exactly
            // those overlaps. It must still not carry the shot's own ink,
            // or the "gap" isn't a gap and this is some larger mass of
            // that colour rather than the shot's silhouette.
            if (actual == ink) {
              matches = false;
              break;
            }
            continue;
          }
          if (actual != expected) {
            matches = false;
            break;
          }
          comparedAny = true;
          // Isolation check: a genuine shot is exactly kProjectileWidth
          // wide with background on both sides (the sprite's own
          // "different aspect ratio... reads as a shot at a glance"
          // contract) -- a 12-wide enemy body is mostly solid ORANGE,
          // so *some* 2-column slice through it can accidentally
          // satisfy the 6-row on/off pattern above by coincidence
          // (found the hard way: a phantom match inside the formation
          // claimed a column and hid a real shot travelling beneath
          // it). Rejecting a match whose immediate left/right neighbour
          // also carries the ink at an "on" row is what tells "a slim
          // 2px object" apart from "a slice through something wider".
          if (x > 0 && fb.pixel(x - 1, py) == ink) {
            matches = false;
            break;
          }
          if (x + games::kProjectileWidth < Framebuffer::width() &&
              fb.pixel(x + games::kProjectileWidth, py) == ink) {
            matches = false;
            break;
          }
        }
      }
      if (!matches || !comparedAny) continue;

      // Claim this column through the sprite's own full height from this
      // origin -- not just what was visible -- so a clipped lower slice
      // of this same shot (whatever its internal pattern) is skipped
      // without being re-examined.
      claimedUntil[x] = y + games::kProjectileHeight;

      if (found.count >= games::kMaxEnemyShots) {
        found.overflow = true;
        continue;
      }
      found.at[found.count++] = EnemyShotPosition{x, y};
    }
  }
  return found;
}

// True iff any enemy shot is on screen -- the template-matched successor
// to the "any pixel of the shot's ink" scans four test files used before
// the player and the enemy could share an ink.
inline bool anyEnemyShotOnScreen(const Framebuffer& fb) {
  return findEnemyShots(fb).count > 0;
}

// True iff (x, y) falls inside any located shot's bounding box (its own
// unclipped kProjectileWidth x kProjectileHeight extent from its origin,
// not just the cells that are lit). AC-2.8's other half of the enemy-
// shot recolour (D4/AC-3.10): several test files scan for Color::ORANGE
// to find the enemy *formation*, on the premise that ORANGE meant "enemy
// and nothing else" -- true before this feature, false now that the
// enemy shot is ORANGE too. Excluding a shot's own bounding box from an
// ORANGE scan is what keeps those tests reading "enemy body", not "enemy
// body or a shot that happens to share its ink".
inline bool pointInsideAnyShot(const EnemyShots& shots, int32_t x,
                                int32_t y) {
  for (int32_t i = 0; i < shots.count; ++i) {
    const int32_t sx = shots.at[i].x;
    const int32_t sy = shots.at[i].y;
    if (x >= sx && x < sx + games::kProjectileWidth && y >= sy &&
        y < sy + games::kProjectileHeight) {
      return true;
    }
  }
  return false;
}

// True iff an enemy shot overlaps the half-open column range [x0, x1).
// Previously three byte-identical copies, one per test file, each
// scanning for a single pixel of one ink; hoisted here as one
// template-matched definition so the evasion strategy these tests steer
// with cannot drift between files. A shot is kProjectileWidth wide, so it
// threatens the range whenever its own span overlaps it at all.
//
// KNOWN LIMITATION (see CLAUDE.md-style note in this file's own
// findEnemyShots doc comment, and the Deviations entry this produced in
// plan.md): findEnemyShots -- and therefore this function -- cannot
// detect a shot while it visually overlaps an enemy body. blit draws
// enemies before shots, so at the shot's own transparent "gap" rows
// (kEnemyShotRows' blank rows) whatever the enemy already drew (the same
// ORANGE) shows through unchanged -- the composited bytes are then
// bit-for-bit identical to "no shot here at all", which no pixel-only
// algorithm can recover. This is a genuine, provable information loss
// from AC-3.10/D4's enemy-shot recolour (to ORANGE, shared with enemy
// bodies), not a bug in the matching logic, and it was not caught until
// a test built on top of this function reached a real overlap case.
inline bool enemyShotThreatensColumn(const Framebuffer& fb, int32_t x0,
                                      int32_t x1) {
  const EnemyShots shots = findEnemyShots(fb);
  for (int32_t i = 0; i < shots.count; ++i) {
    const int32_t shotX0 = shots.at[i].x;
    const int32_t shotX1 = shotX0 + games::kProjectileWidth;
    if (shotX0 < x1 && shotX1 > x0) return true;
  }
  return false;
}

// Builds a reference frame containing only the player ship at `playerX`
// -- an independently-constructed expectation (calls blit directly, not
// GalacticInvasion::render), the same "build the expected frame with the
// test's own drawing call" convention title_screen_test.cpp's
// placeExpected establishes.
inline Framebuffer buildExpectedPlayerFrame(int32_t playerX) {
  Framebuffer fb;
  fb.blit(games::kPlayerSprite, playerX, games::kPlayerY);
  return fb;
}

}  // namespace steamcore::test
