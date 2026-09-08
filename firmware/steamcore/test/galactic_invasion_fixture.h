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
// [0, kPlayerY) -- the player's own row is excluded, and enemies render
// in the different Color::ORANGE (not BRIGHT_ORANGE), so this cannot
// mistake an enemy sprite for the projectile even if their x ranges
// happen to overlap. Returns -1 if no such pixel exists (no shot in
// flight in that column).
inline int32_t findProjectileTopY(const Framebuffer& fb, int32_t x) {
  for (int32_t y = 0; y < games::kPlayerY; ++y) {
    if (fb.pixel(x, y) == Color::BRIGHT_ORANGE) return y;
  }
  return -1;
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
