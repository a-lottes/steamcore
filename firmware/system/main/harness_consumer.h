#pragma once

#include "steamcore/color.h"
#include "steamcore/config.h"
#include "steamcore/framebuffer.h"
#include "steamcore/game_loop.h"
#include "steamcore/test/fixture_pattern.h"

// Throwaway, non-public consumer for the display-driver on-device proof
// (spec US-3, A12/A13): its first tick's render() draws the exact same
// shared fixture pattern T7 already proved (steamcore::test::
// drawFixturePattern), then every following tick moves a single
// full-tile BRIGHT_ORANGE marker one tile along a fixed, deterministic
// path -- a pure function of its own tick counter, no clock, no RNG
// (NFR-5, reusing game-loop's already-proven determinism guarantee
// rather than re-deriving one here). Not a game: no GameSession, no
// win/lose rules, no real input (spec §6). Never a new steamcore public
// header -- this lives in firmware/system/main/, outside
// firmware/steamcore/'s public surface (A13, NFR-7).
namespace steamcore::test {

class HarnessConsumer {
 public:
  void update(const GameInput&) { ++tick_; }

  void render(Framebuffer& fb) {
    if (tick_ == 1) {
      drawFixturePattern(fb);
      return;
    }

    const int32_t markerIndex = tick_ - 2;
    if (markerIndex > 0) {
      const TileCoord previous = tileAt(markerIndex - 1);
      restoreFixtureTile(fb, previous.col, previous.row);
    }

    const TileCoord current = tileAt(markerIndex);
    fb.fillRect(current.col * kTileSize, current.row * kTileSize, kTileSize,
                kTileSize, Color::BRIGHT_ORANGE);
  }

 private:
  struct TileCoord {
    int32_t col;
    int32_t row;
  };

  // A fixed, deterministic path -- sweeps left to right along row 0,
  // then the next row -- pure function of `step`, nothing else. 20
  // steps never reach kTileCols*kTileRows, so no wraparound is needed.
  static TileCoord tileAt(int32_t step) {
    return TileCoord{step % kTileCols, step / kTileCols};
  }

  // Repaints exactly the pixels drawFixturePattern would have left at
  // this tile, by drawing the pattern once into a reference buffer and
  // copying from it -- keeps this file from re-deriving the fixture's
  // geometry by hand, a second source of truth that could drift from
  // the shared one (spec §6).
  static void restoreFixtureTile(Framebuffer& fb, int32_t col, int32_t row) {
    static Framebuffer reference;
    static bool initialized = false;
    if (!initialized) {
      drawFixturePattern(reference);
      initialized = true;
    }
    for (int32_t y = row * kTileSize; y < (row + 1) * kTileSize; ++y) {
      for (int32_t x = col * kTileSize; x < (col + 1) * kTileSize; ++x) {
        fb.setPixel(x, y, reference.pixel(x, y));
      }
    }
  }

  int32_t tick_ = 0;
};

}  // namespace steamcore::test
