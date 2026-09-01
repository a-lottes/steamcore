#include "steamcore/framebuffer.h"
#include "steamcore/sprite.h"
#include "test_harness.h"

using steamcore::Color;
using steamcore::Framebuffer;
using steamcore::Sprite;

namespace {

// An 8x8 pattern with no BLACK pixels, so every pixel is non-transparent
// under the default transparent colour — proves AC-3.1's "exactly those
// 64 pixels match" without the ambiguity a transparent pixel would add.
constexpr Color kOpaque8x8[8 * 8] = {
    Color::ORANGE,        Color::BRIGHT_ORANGE, Color::ORANGE,
    Color::BRIGHT_ORANGE, Color::ORANGE,        Color::BRIGHT_ORANGE,
    Color::ORANGE,        Color::BRIGHT_ORANGE, Color::DARK_ORANGE,
    Color::ORANGE,        Color::DARK_ORANGE,   Color::ORANGE,
    Color::DARK_ORANGE,   Color::ORANGE,        Color::DARK_ORANGE,
    Color::ORANGE,        Color::ORANGE,        Color::BRIGHT_ORANGE,
    Color::ORANGE,        Color::BRIGHT_ORANGE, Color::ORANGE,
    Color::BRIGHT_ORANGE, Color::ORANGE,        Color::BRIGHT_ORANGE,
    Color::DARK_ORANGE,   Color::ORANGE,        Color::DARK_ORANGE,
    Color::ORANGE,        Color::DARK_ORANGE,   Color::ORANGE,
    Color::DARK_ORANGE,   Color::ORANGE,        Color::ORANGE,
    Color::BRIGHT_ORANGE, Color::ORANGE,        Color::BRIGHT_ORANGE,
    Color::ORANGE,        Color::BRIGHT_ORANGE, Color::ORANGE,
    Color::BRIGHT_ORANGE, Color::DARK_ORANGE,   Color::ORANGE,
    Color::DARK_ORANGE,   Color::ORANGE,        Color::DARK_ORANGE,
    Color::ORANGE,        Color::DARK_ORANGE,   Color::ORANGE,
    Color::ORANGE,        Color::BRIGHT_ORANGE, Color::ORANGE,
    Color::BRIGHT_ORANGE, Color::ORANGE,        Color::BRIGHT_ORANGE,
    Color::ORANGE,        Color::BRIGHT_ORANGE, Color::DARK_ORANGE,
    Color::ORANGE,        Color::DARK_ORANGE,   Color::ORANGE,
    Color::DARK_ORANGE,   Color::ORANGE,        Color::DARK_ORANGE,
    Color::ORANGE,
};

Sprite opaque8x8() { return Sprite{kOpaque8x8, 8, 8, 8}; }

// Fills `out` (row-major, width*height entries, stride == width) with a
// non-BLACK pattern so every pixel is opaque under the default transparent
// colour, same reasoning as kOpaque8x8 above.
void fillOpaquePattern(Color* out, int32_t width, int32_t height) {
  constexpr Color kCycle[3] = {Color::ORANGE, Color::BRIGHT_ORANGE,
                                Color::DARK_ORANGE};
  for (int32_t y = 0; y < height; ++y) {
    for (int32_t x = 0; x < width; ++x) {
      out[y * width + x] = kCycle[(x + y) % 3];
    }
  }
}

bool onlyRegionMatchesSprite(const Framebuffer& fb, const Sprite& sprite,
                              int32_t originX, int32_t originY,
                              Color outside) {
  for (int32_t y = 0; y < Framebuffer::height(); ++y) {
    for (int32_t x = 0; x < Framebuffer::width(); ++x) {
      const bool within = x >= originX && x < originX + sprite.width &&
                           y >= originY && y < originY + sprite.height;
      if (!within) {
        if (fb.pixel(x, y) != outside) return false;
        continue;
      }
      const Color expected =
          sprite.pixels[(y - originY) * sprite.stride + (x - originX)];
      if (fb.pixel(x, y) != expected) return false;
    }
  }
  return true;
}

}  // namespace

STEAMCORE_TEST(ac_3_1_blit_writes_exactly_sprite_pixels) {
  Framebuffer fb;
  const Sprite sprite = opaque8x8();
  fb.blit(sprite, 10, 10);
  CHECK(onlyRegionMatchesSprite(fb, sprite, 10, 10, Color::BLACK));
}

STEAMCORE_TEST(ac_3_2_default_transparent_is_black) {
  Framebuffer fb;
  fb.clear(Color::BRIGHT_ORANGE);

  Color pixels[4] = {Color::BLACK, Color::ORANGE, Color::BLACK,
                      Color::ORANGE};
  const Sprite sprite{pixels, 2, 2, 2};
  fb.blit(sprite, 5, 5);

  // BLACK source pixels (top-left, bottom-left) must leave the
  // BRIGHT_ORANGE background untouched; ORANGE pixels must overwrite it.
  CHECK(fb.pixel(5, 5) == Color::BRIGHT_ORANGE);
  CHECK(fb.pixel(6, 5) == Color::ORANGE);
  CHECK(fb.pixel(5, 6) == Color::BRIGHT_ORANGE);
  CHECK(fb.pixel(6, 6) == Color::ORANGE);
}

// --- AC-3.3: blit clipping at each of the four edges, and off-screen ---

STEAMCORE_TEST(ac_3_3_left_edge) {
  Framebuffer fb;
  const Sprite sprite = opaque8x8();
  fb.blit(sprite, -3, 50);
  CHECK(onlyRegionMatchesSprite(fb, sprite, -3, 50, Color::BLACK));
}

STEAMCORE_TEST(ac_3_3_top_edge) {
  Framebuffer fb;
  const Sprite sprite = opaque8x8();
  fb.blit(sprite, 50, -3);
  CHECK(onlyRegionMatchesSprite(fb, sprite, 50, -3, Color::BLACK));
}

STEAMCORE_TEST(ac_3_3_right_edge) {
  Framebuffer fb;
  const Sprite sprite = opaque8x8();
  fb.blit(sprite, Framebuffer::width() - 5, 50);  // 3 columns hang off
  CHECK(onlyRegionMatchesSprite(fb, sprite, Framebuffer::width() - 5, 50,
                                 Color::BLACK));
}

STEAMCORE_TEST(ac_3_3_bottom_edge) {
  Framebuffer fb;
  const Sprite sprite = opaque8x8();
  fb.blit(sprite, 50, Framebuffer::height() - 5);  // 3 rows hang off
  CHECK(onlyRegionMatchesSprite(fb, sprite, 50, Framebuffer::height() - 5,
                                 Color::BLACK));
}

STEAMCORE_TEST(ac_3_3_fully_offscreen_is_noop) {
  Framebuffer fb;
  const Sprite sprite = opaque8x8();
  fb.blit(sprite, 300, 300);
  CHECK(onlyRegionMatchesSprite(fb, sprite, 300, 300, Color::BLACK));
}

// --- AC-3.5: the same battery holds for other sprite sizes ---

STEAMCORE_TEST(ac_3_5_size_16x12_basic_and_clipped) {
  Color data[16 * 12];
  fillOpaquePattern(data, 16, 12);
  const Sprite sprite{data, 16, 12, 16};

  Framebuffer basic;
  basic.blit(sprite, 40, 40);
  CHECK(onlyRegionMatchesSprite(basic, sprite, 40, 40, Color::BLACK));

  Framebuffer clippedLeft;
  clippedLeft.blit(sprite, -5, 40);
  CHECK(onlyRegionMatchesSprite(clippedLeft, sprite, -5, 40, Color::BLACK));

  Framebuffer clippedBottomRight;
  clippedBottomRight.blit(sprite, Framebuffer::width() - 10,
                           Framebuffer::height() - 6);
  CHECK(onlyRegionMatchesSprite(clippedBottomRight, sprite,
                                 Framebuffer::width() - 10,
                                 Framebuffer::height() - 6, Color::BLACK));

  Framebuffer offscreen;
  offscreen.blit(sprite, -100, -100);
  CHECK(onlyRegionMatchesSprite(offscreen, sprite, -100, -100, Color::BLACK));
}

STEAMCORE_TEST(ac_3_5_size_1x1_basic_and_clipped) {
  Color pixel[1] = {Color::ORANGE};
  const Sprite sprite{pixel, 1, 1, 1};

  Framebuffer onScreen;
  onScreen.blit(sprite, 100, 80);
  CHECK(onlyRegionMatchesSprite(onScreen, sprite, 100, 80, Color::BLACK));

  Framebuffer offScreen;
  offScreen.blit(sprite, -1, 80);
  CHECK(onlyRegionMatchesSprite(offScreen, sprite, -1, 80, Color::BLACK));
}

// --- AC-3.6: a non-default transparent colour ---

STEAMCORE_TEST(ac_3_6_custom_transparent_colour) {
  Framebuffer fb;
  fb.clear(Color::BRIGHT_ORANGE);

  Color pixels[4] = {Color::ORANGE, Color::BLACK, Color::ORANGE,
                      Color::BLACK};
  const Sprite sprite{pixels, 2, 2, 2};
  fb.blit(sprite, 5, 5, Color::ORANGE);

  // ORANGE source pixels (top-left, bottom-left) are transparent here and
  // must leave BRIGHT_ORANGE untouched; BLACK pixels are opaque now and
  // must be written — proving a black silhouette over a lit area works.
  CHECK(fb.pixel(5, 5) == Color::BRIGHT_ORANGE);
  CHECK(fb.pixel(6, 5) == Color::BLACK);
  CHECK(fb.pixel(5, 6) == Color::BRIGHT_ORANGE);
  CHECK(fb.pixel(6, 6) == Color::BLACK);
}

// --- AC-3.7: a Sprite selecting a sub-rectangle via stride ---

namespace {

// A 32x32 atlas, BRIGHT_ORANGE everywhere except the 8x8 window at (8,8).
// Review F3: a uniform window (plain ORANGE) cannot distinguish "read the
// right pixels" from "read some *other* window row that happens to be
// the same colour" — a stride bug that stays inside the window's bounds
// is invisible to a uniform-colour check. The window instead carries a
// DIAGONAL marker (local row == local col -> DARK_ORANGE, else ORANGE),
// so the verification below can check that the diagonal lands at the
// exact *relative* position a correct stride/offset would put it at, for
// whichever slice of the window a given clip makes visible. A row/column
// swap, a wrong stride, or an off-by-a-few-rows shift all move the
// diagonal to a different relative position and are caught.
struct MarkedAtlas {
  Color data[32 * 32];

  MarkedAtlas() {
    for (int32_t i = 0; i < 32 * 32; ++i) data[i] = Color::BRIGHT_ORANGE;
    for (int32_t r = 0; r < 8; ++r) {
      for (int32_t c = 0; c < 8; ++c) {
        data[(8 + r) * 32 + (8 + c)] =
            (r == c) ? Color::DARK_ORANGE : Color::ORANGE;
      }
    }
  }

  Sprite window() const { return Sprite{&data[8 * 32 + 8], 8, 8, 32}; }
};

// Checks a blit of MarkedAtlas::window() placed with its own (unclipped)
// origin at screen (screenX, screenY): the visible slice starts at local
// window coordinates (windowColStart, windowRowStart) and is
// (visibleCols x visibleRows) in size -- e.g. clipping 4 columns off the
// window's left edge means windowColStart=4, visibleCols=4. Every pixel
// in that slice must show the window's own diagonal marker at the
// correct *local* position; everything else on the framebuffer must be
// untouched BLACK.
bool onlyWindowSliceEquals(const Framebuffer& fb, int32_t screenX,
                            int32_t screenY, int32_t windowColStart,
                            int32_t windowRowStart, int32_t visibleCols,
                            int32_t visibleRows) {
  for (int32_t y = 0; y < Framebuffer::height(); ++y) {
    for (int32_t x = 0; x < Framebuffer::width(); ++x) {
      const int32_t dx = x - screenX;
      const int32_t dy = y - screenY;
      const bool within =
          dx >= 0 && dx < visibleCols && dy >= 0 && dy < visibleRows;
      if (!within) {
        if (fb.pixel(x, y) != Color::BLACK) return false;
        continue;
      }
      const int32_t windowCol = windowColStart + dx;
      const int32_t windowRow = windowRowStart + dy;
      const Color expected =
          (windowRow == windowCol) ? Color::DARK_ORANGE : Color::ORANGE;
      if (fb.pixel(x, y) != expected) return false;
    }
  }
  return true;
}

}  // namespace

STEAMCORE_TEST(ac_3_7_stride_sub_rectangle_no_leakage) {
  MarkedAtlas atlas;
  const Sprite sprite = atlas.window();

  Framebuffer onScreen;
  onScreen.blit(sprite, 60, 60);
  CHECK(onlyWindowSliceEquals(onScreen, 60, 60, /*windowColStart=*/0,
                               /*windowRowStart=*/0, /*visibleCols=*/8,
                               /*visibleRows=*/8));

  Framebuffer clipped;
  clipped.blit(sprite, Framebuffer::width() - 4, 60);  // right edge
  // Only the window's own first 4 columns (0..3) fit before the edge.
  CHECK(onlyWindowSliceEquals(clipped, Framebuffer::width() - 4, 60,
                               /*windowColStart=*/0, /*windowRowStart=*/0,
                               /*visibleCols=*/4, /*visibleRows=*/8));
}

// The right-edge case above clips only the destination's trailing edge:
// the source read still starts at the window's own (0,0), so srcOffsetX
// and srcOffsetY are both 0 and the stride arithmetic that advances
// between source rows is never exercised. Left/top clipping forces a
// non-zero source offset before the first row is even read — exactly the
// case a wrong stride (e.g. using sprite.width instead of sprite.stride)
// would get wrong (review F3; caught by mutation testing).
STEAMCORE_TEST(ac_3_7_stride_sub_rectangle_clipped_left) {
  MarkedAtlas atlas;
  const Sprite sprite = atlas.window();

  Framebuffer fb;
  fb.blit(sprite, -4, 60);
  // Sprite origin x=-4 clips off the window's first 4 columns (0..3);
  // what's visible on-screen (x in [0,4)) is window columns 4..7.
  CHECK(onlyWindowSliceEquals(fb, 0, 60, /*windowColStart=*/4,
                               /*windowRowStart=*/0, /*visibleCols=*/4,
                               /*visibleRows=*/8));
}

STEAMCORE_TEST(ac_3_7_stride_sub_rectangle_clipped_top) {
  MarkedAtlas atlas;
  const Sprite sprite = atlas.window();

  Framebuffer fb;
  fb.blit(sprite, 60, -4);
  // Sprite origin y=-4 clips off the window's first 4 rows (0..3);
  // what's visible on-screen (y in [0,4)) is window rows 4..7.
  CHECK(onlyWindowSliceEquals(fb, 60, 0, /*windowColStart=*/0,
                               /*windowRowStart=*/4, /*visibleCols=*/8,
                               /*visibleRows=*/4));
}

STEAMCORE_TEST(ac_3_4_blit_is_deterministic) {
  const Sprite sprite = opaque8x8();

  Framebuffer a;
  Framebuffer b;
  a.blit(sprite, 20, 30);
  b.blit(sprite, 20, 30);

  for (int32_t y = 0; y < Framebuffer::height(); ++y) {
    for (int32_t x = 0; x < Framebuffer::width(); ++x) {
      CHECK(a.pixel(x, y) == b.pixel(x, y));
    }
  }
}
