#include <cstddef>

#include "steamcore/framebuffer.h"
#include "test_harness.h"

using steamcore::Color;
using steamcore::Framebuffer;

// AC-2.5's DoD note, checked at compile time: the type is exactly its
// pixel storage, nothing more (no static member — verified by reading
// framebuffer.h, since sizeof() cannot distinguish an added static from
// its absence).
static_assert(sizeof(Framebuffer) ==
              static_cast<std::size_t>(steamcore::kScreenWidth) *
                  static_cast<std::size_t>(steamcore::kScreenHeight));

namespace {

bool allPixelsEqual(const Framebuffer& fb, Color expected) {
  for (int32_t y = 0; y < Framebuffer::height(); ++y) {
    for (int32_t x = 0; x < Framebuffer::width(); ++x) {
      if (fb.pixel(x, y) != expected) return false;
    }
  }
  return true;
}

}  // namespace

STEAMCORE_TEST(framebuffer_fresh_instance_is_all_black) {
  Framebuffer fb;
  CHECK(allPixelsEqual(fb, Color::BLACK));
}

STEAMCORE_TEST(framebuffer_clear_sets_every_pixel) {
  Framebuffer fb;
  fb.clear(Color::ORANGE);
  CHECK(allPixelsEqual(fb, Color::ORANGE));
}

STEAMCORE_TEST(framebuffer_pixel_write_touches_only_that_pixel) {
  Framebuffer fb;
  const int32_t w = Framebuffer::width();
  const int32_t h = Framebuffer::height();

  fb.setPixel(0, 0, Color::ORANGE);
  fb.setPixel(w - 1, h - 1, Color::BRIGHT_ORANGE);

  CHECK(fb.pixel(0, 0) == Color::ORANGE);
  CHECK(fb.pixel(w - 1, h - 1) == Color::BRIGHT_ORANGE);

  int32_t unexpected = 0;
  for (int32_t y = 0; y < h; ++y) {
    for (int32_t x = 0; x < w; ++x) {
      const bool isCorner = (x == 0 && y == 0) || (x == w - 1 && y == h - 1);
      if (isCorner) continue;
      if (fb.pixel(x, y) != Color::BLACK) ++unexpected;
    }
  }
  CHECK_EQ(unexpected, 0);
}

STEAMCORE_TEST(framebuffer_two_instances_are_independent) {
  Framebuffer a;
  Framebuffer b;
  a.clear(Color::ORANGE);
  CHECK(allPixelsEqual(a, Color::ORANGE));
  CHECK(allPixelsEqual(b, Color::BLACK));
}
