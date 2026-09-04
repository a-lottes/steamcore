#include "steamcore/panel_format.h"

#include "steamcore/color.h"
#include "steamcore/config.h"
#include "steamcore/framebuffer.h"
#include "test_harness.h"

using steamcore::Color;
using steamcore::expandTile;
using steamcore::Framebuffer;
using steamcore::kPanelTileBytes;
using steamcore::PanelPixel;
using steamcore::PanelWindow;
using steamcore::tileWindow;
using steamcore::toPanelPixel;

// Walking skeleton (T1): one colour converted, one tile mapped, one tile
// expanded end to end. The batteries proving every colour (T3) and every
// tile with full coverage (T4) come later -- this is the "does the shape
// of the pipeline work at all" test.
STEAMCORE_TEST(panel_format_walking_skeleton) {
  const PanelPixel bright = toPanelPixel(Color::BRIGHT_ORANGE);
  CHECK_EQ(bright.r, static_cast<uint8_t>(0xFF));
  CHECK_EQ(bright.g, static_cast<uint8_t>(0x99));
  CHECK_EQ(bright.b, static_cast<uint8_t>(0x33));

  const PanelWindow window = tileWindow(0, 0);
  CHECK_EQ(window.x0, 0);
  CHECK_EQ(window.y0, 0);
  CHECK_EQ(window.x1, 31);
  CHECK_EQ(window.y1, 31);

  Framebuffer fb;
  fb.clear(Color::BRIGHT_ORANGE);
  static uint8_t tile[kPanelTileBytes];
  expandTile(fb, 0, 0, tile);

  CHECK_EQ(tile[0], static_cast<uint8_t>(0xFF));
  CHECK_EQ(tile[1], static_cast<uint8_t>(0x99));
  CHECK_EQ(tile[2], static_cast<uint8_t>(0x33));
  CHECK_EQ(tile[kPanelTileBytes - 3], static_cast<uint8_t>(0xFF));
  CHECK_EQ(tile[kPanelTileBytes - 2], static_cast<uint8_t>(0x99));
  CHECK_EQ(tile[kPanelTileBytes - 1], static_cast<uint8_t>(0x33));
}

// AC-1.1: each of the four Color values converts to the exact 3-byte
// 18bpp value matching framebuffer-viewer's already-approved RGB hex
// palette (docs/dump-format.md): #000000, #4D2600, #B35900, #FF9933 --
// one named test per colour so a filter can isolate a single one.
STEAMCORE_TEST(panel_format_black_converts_to_hex_000000) {
  const PanelPixel px = toPanelPixel(Color::BLACK);
  CHECK_EQ(px.r, static_cast<uint8_t>(0x00));
  CHECK_EQ(px.g, static_cast<uint8_t>(0x00));
  CHECK_EQ(px.b, static_cast<uint8_t>(0x00));
}

STEAMCORE_TEST(panel_format_dark_orange_converts_to_hex_4d2600) {
  const PanelPixel px = toPanelPixel(Color::DARK_ORANGE);
  CHECK_EQ(px.r, static_cast<uint8_t>(0x4D));
  CHECK_EQ(px.g, static_cast<uint8_t>(0x26));
  CHECK_EQ(px.b, static_cast<uint8_t>(0x00));
}

STEAMCORE_TEST(panel_format_orange_converts_to_hex_b35900) {
  const PanelPixel px = toPanelPixel(Color::ORANGE);
  CHECK_EQ(px.r, static_cast<uint8_t>(0xB3));
  CHECK_EQ(px.g, static_cast<uint8_t>(0x59));
  CHECK_EQ(px.b, static_cast<uint8_t>(0x00));
}

STEAMCORE_TEST(panel_format_bright_orange_converts_to_hex_ff9933) {
  const PanelPixel px = toPanelPixel(Color::BRIGHT_ORANGE);
  CHECK_EQ(px.r, static_cast<uint8_t>(0xFF));
  CHECK_EQ(px.g, static_cast<uint8_t>(0x99));
  CHECK_EQ(px.b, static_cast<uint8_t>(0x33));
}

// AC-1.1 (distinctness half): a truncation bug that maps two palette
// steps onto the same wire value would pass four individual-colour tests
// but fail this one -- all six pairs must differ in at least one byte.
STEAMCORE_TEST(panel_format_all_four_colours_are_pairwise_distinct) {
  const PanelPixel colours[4] = {
      toPanelPixel(Color::BLACK), toPanelPixel(Color::DARK_ORANGE),
      toPanelPixel(Color::ORANGE), toPanelPixel(Color::BRIGHT_ORANGE)};

  int32_t collisions = 0;
  for (int32_t i = 0; i < 4; ++i) {
    for (int32_t j = i + 1; j < 4; ++j) {
      const bool identical = colours[i].r == colours[j].r &&
                              colours[i].g == colours[j].g &&
                              colours[i].b == colours[j].b;
      if (identical) ++collisions;
    }
  }
  CHECK_EQ(collisions, 0);
}

// AC-1.3: calling the conversion twice with the same Color returns
// byte-identical output both times.
STEAMCORE_TEST(panel_format_conversion_is_deterministic) {
  const Color colours[4] = {Color::BLACK, Color::DARK_ORANGE, Color::ORANGE,
                             Color::BRIGHT_ORANGE};
  int32_t mismatches = 0;
  for (const Color color : colours) {
    const PanelPixel first = toPanelPixel(color);
    const PanelPixel second = toPanelPixel(color);
    if (first.r != second.r || first.g != second.g || first.b != second.b) {
      ++mismatches;
    }
  }
  CHECK_EQ(mismatches, 0);
}

// AC-2.1: every one of the 150 tile positions maps to exactly the window
// the spec names -- a per-tile assertion, not a sample.
STEAMCORE_TEST(panel_format_tile_window_is_exact_at_every_position) {
  int32_t mismatches = 0;
  for (int32_t row = 0; row < steamcore::kTileRows; ++row) {
    for (int32_t col = 0; col < steamcore::kTileCols; ++col) {
      const PanelWindow w = tileWindow(col, row);
      const int32_t expectedX0 = col * steamcore::kPanelTileSize;
      const int32_t expectedY0 = row * steamcore::kPanelTileSize;
      const bool exact =
          w.x0 == expectedX0 && w.y0 == expectedY0 &&
          w.x1 == expectedX0 + steamcore::kPanelTileSize - 1 &&
          w.y1 == expectedY0 + steamcore::kPanelTileSize - 1;
      if (!exact) ++mismatches;
    }
  }
  CHECK_EQ(mismatches, 0);
}

// AC-2.1 (coverage half): a per-tile assertion alone would pass for a
// mapping that overlaps or leaves gaps between tiles -- this claims every
// one of the 480x320 panel pixels from all 150 windows and asserts each
// was claimed exactly once.
STEAMCORE_TEST(panel_format_tile_windows_cover_panel_with_no_gap_or_overlap) {
  static uint8_t claims[steamcore::kPanelWidth * steamcore::kPanelHeight] =
      {};
  for (int32_t row = 0; row < steamcore::kTileRows; ++row) {
    for (int32_t col = 0; col < steamcore::kTileCols; ++col) {
      const PanelWindow w = tileWindow(col, row);
      for (int32_t y = w.y0; y <= w.y1; ++y) {
        for (int32_t x = w.x0; x <= w.x1; ++x) {
          ++claims[y * steamcore::kPanelWidth + x];
        }
      }
    }
  }

  int32_t notExactlyOnce = 0;
  for (const uint8_t claimCount : claims) {
    if (claimCount != 1) ++notExactlyOnce;
  }
  CHECK_EQ(notExactlyOnce, 0);
}

// US-2: expandTile emits each source pixel as a kPanelScale x kPanelScale
// block in row-major panel order, with no bleed across source-pixel
// boundaries; first and last byte of the output checked explicitly.
STEAMCORE_TEST(panel_format_expand_tile_replicates_each_pixel_as_a_block) {
  Framebuffer fb;
  fb.clear(Color::BLACK);

  constexpr int32_t kTileCol = 2;
  constexpr int32_t kTileRow = 3;
  const int32_t baseX = kTileCol * steamcore::kTileSize;
  const int32_t baseY = kTileRow * steamcore::kTileSize;
  fb.setPixel(baseX, baseY, Color::BRIGHT_ORANGE);   // tile-local (0,0)
  fb.setPixel(baseX + 5, baseY + 7, Color::ORANGE);  // tile-local (5,7)

  static uint8_t tile[kPanelTileBytes];
  expandTile(fb, kTileCol, kTileRow, tile);

  const PanelPixel bright = toPanelPixel(Color::BRIGHT_ORANGE);
  const PanelPixel orange = toPanelPixel(Color::ORANGE);
  const PanelPixel black = toPanelPixel(Color::BLACK);

  auto byteAt = [&](int32_t panelX, int32_t panelY, int32_t channel) {
    return tile[(panelY * steamcore::kPanelTileSize + panelX) *
                    steamcore::kPanelBytesPerPixel +
                channel];
  };
  auto blockMatches = [&](int32_t px0, int32_t py0, const PanelPixel& want) {
    for (int32_t dy = 0; dy < steamcore::kPanelScale; ++dy) {
      for (int32_t dx = 0; dx < steamcore::kPanelScale; ++dx) {
        if (byteAt(px0 + dx, py0 + dy, 0) != want.r ||
            byteAt(px0 + dx, py0 + dy, 1) != want.g ||
            byteAt(px0 + dx, py0 + dy, 2) != want.b) {
          return false;
        }
      }
    }
    return true;
  };

  int32_t mismatches = 0;
  if (!blockMatches(0, 0, bright)) ++mismatches;    // source (0,0)
  if (!blockMatches(10, 14, orange)) ++mismatches;  // source (5,7)
  // Neighbouring, still-untouched source pixel (1,0) -- proves the
  // replication doesn't bleed across source-pixel boundaries.
  if (!blockMatches(2, 0, black)) ++mismatches;
  CHECK_EQ(mismatches, 0);

  // First byte is source (0,0)'s R channel; last byte is the bottom-right
  // source pixel's B channel, still BLACK (only two pixels were painted).
  CHECK_EQ(tile[0], bright.r);
  CHECK_EQ(tile[kPanelTileBytes - 1], black.b);
}
