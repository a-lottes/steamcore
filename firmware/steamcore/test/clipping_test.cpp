#include <cstdint>
#include <limits>

#include "steamcore/framebuffer.h"
#include "test_harness.h"

using steamcore::Color;
using steamcore::Framebuffer;

namespace {

// Scans the whole buffer: every pixel inside [x0,x1) x [y0,y1) must equal
// `inside`, every pixel outside that rect must equal `outside`. Used to
// prove clipping fills exactly the on-screen part and touches nothing
// else (AC-2.5) rather than trusting a handful of spot checks.
bool onlyRegionEquals(const Framebuffer& fb, int32_t x0, int32_t y0,
                       int32_t x1, int32_t y1, Color inside, Color outside) {
  for (int32_t y = 0; y < Framebuffer::height(); ++y) {
    for (int32_t x = 0; x < Framebuffer::width(); ++x) {
      const bool within = x >= x0 && x < x1 && y >= y0 && y < y1;
      const Color expected = within ? inside : outside;
      if (fb.pixel(x, y) != expected) return false;
    }
  }
  return true;
}

bool allPixelsEqual(const Framebuffer& fb, Color expected) {
  return onlyRegionEquals(fb, 0, 0, 0, 0, expected, expected);
}

}  // namespace

// --- AC-2.4: entirely off-screen pixel/rect is a no-op, no crash ---

STEAMCORE_TEST(ac_2_4_offscreen_pixel_is_noop) {
  Framebuffer fb;
  fb.setPixel(-5, -5, Color::ORANGE);
  fb.setPixel(1000, 1000, Color::ORANGE);
  fb.setPixel(Framebuffer::width(), 0, Color::ORANGE);   // one past edge
  fb.setPixel(0, Framebuffer::height(), Color::ORANGE);  // one past edge
  CHECK(allPixelsEqual(fb, Color::BLACK));
}

STEAMCORE_TEST(ac_2_4_offscreen_rect_is_noop) {
  Framebuffer fb;
  fb.fillRect(300, 300, 10, 10, Color::ORANGE);
  fb.fillRect(-50, -50, 10, 10, Color::ORANGE);
  CHECK(allPixelsEqual(fb, Color::BLACK));
}

// --- AC-2.5: a rect crossing each edge fills exactly the on-screen part ---

STEAMCORE_TEST(ac_2_5_rect_crossing_left_edge) {
  Framebuffer fb;
  fb.fillRect(-5, 50, 20, 10, Color::ORANGE);
  // on-screen: x in [0, 15), y in [50, 60)
  CHECK(onlyRegionEquals(fb, 0, 50, 15, 60, Color::ORANGE, Color::BLACK));
}

STEAMCORE_TEST(ac_2_5_rect_crossing_top_edge) {
  Framebuffer fb;
  fb.fillRect(50, -5, 10, 20, Color::ORANGE);
  // on-screen: x in [50, 60), y in [0, 15)
  CHECK(onlyRegionEquals(fb, 50, 0, 60, 15, Color::ORANGE, Color::BLACK));
}

STEAMCORE_TEST(ac_2_5_rect_crossing_right_edge) {
  Framebuffer fb;
  fb.fillRect(230, 50, 20, 10, Color::ORANGE);
  // on-screen: x in [230, 240), y in [50, 60)
  CHECK(onlyRegionEquals(fb, 230, 50, 240, 60, Color::ORANGE, Color::BLACK));
}

STEAMCORE_TEST(ac_2_5_rect_crossing_bottom_edge) {
  Framebuffer fb;
  fb.fillRect(50, 150, 10, 20, Color::ORANGE);
  // on-screen: x in [50, 60), y in [150, 160)
  CHECK(onlyRegionEquals(fb, 50, 150, 60, 160, Color::ORANGE, Color::BLACK));
}

// --- AC-2.6: zero or negative width/height changes nothing ---

STEAMCORE_TEST(ac_2_6_degenerate_rect_is_noop) {
  Framebuffer fb;
  fb.fillRect(10, 10, 0, 5, Color::ORANGE);
  fb.fillRect(10, 10, 5, 0, Color::ORANGE);
  fb.fillRect(10, 10, -5, 5, Color::ORANGE);
  fb.fillRect(10, 10, 5, -5, Color::ORANGE);
  fb.fillRect(10, 10, 0, 0, Color::ORANGE);
  CHECK(allPixelsEqual(fb, Color::BLACK));
}

// --- AC-2.9: extreme coordinates never overflow, never go out of bounds ---

STEAMCORE_TEST(ac_2_9_pixel_at_int32_extremes) {
  Framebuffer fb;
  constexpr int32_t kMin = std::numeric_limits<int32_t>::min();
  constexpr int32_t kMax = std::numeric_limits<int32_t>::max();

  fb.setPixel(kMin, kMin, Color::ORANGE);
  fb.setPixel(kMax, kMax, Color::ORANGE);
  fb.setPixel(kMin, 10, Color::ORANGE);
  fb.setPixel(10, kMax, Color::ORANGE);

  CHECK(allPixelsEqual(fb, Color::BLACK));
}

STEAMCORE_TEST(ac_2_9_rect_at_int32_min) {
  Framebuffer fb;
  constexpr int32_t kMin = std::numeric_limits<int32_t>::min();

  fb.fillRect(kMin, kMin, 100, 100, Color::ORANGE);
  fb.fillRect(kMin, 10, 100, 20, Color::ORANGE);

  CHECK(allPixelsEqual(fb, Color::BLACK));
}

STEAMCORE_TEST(ac_2_9_rect_width_overflows_signed_32bit) {
  Framebuffer fb;
  constexpr int32_t kMax = std::numeric_limits<int32_t>::max();

  // x + width would overflow a plain int32_t sum; clip.h computes in
  // int64_t, so this must clip to the on-screen strip [50, 240) only.
  fb.fillRect(50, 10, kMax, 5, Color::ORANGE);
  CHECK(onlyRegionEquals(fb, 50, 10, Framebuffer::width(), 15, Color::ORANGE,
                          Color::BLACK));
}

STEAMCORE_TEST(ac_2_9_rect_height_overflows_signed_32bit) {
  Framebuffer fb;
  constexpr int32_t kMax = std::numeric_limits<int32_t>::max();

  // y + height would overflow a plain int32_t sum; must clip to the
  // on-screen strip [20, 160) only.
  fb.fillRect(10, 20, 5, kMax, Color::ORANGE);
  CHECK(onlyRegionEquals(fb, 10, 20, 15, Framebuffer::height(), Color::ORANGE,
                          Color::BLACK));
}

STEAMCORE_TEST(ac_2_9_rect_x_plus_width_overflows_at_max) {
  Framebuffer fb;
  constexpr int32_t kMax = std::numeric_limits<int32_t>::max();

  // x itself is already INT32_MAX; x + width overflows a 32-bit sum many
  // times over. The whole rect is off-screen.
  fb.fillRect(kMax, 10, kMax, 5, Color::ORANGE);
  CHECK(allPixelsEqual(fb, Color::BLACK));
}
