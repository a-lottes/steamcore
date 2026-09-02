#include "fb_compare.h"

#include "steamcore/framebuffer.h"
#include "test_harness.h"

using steamcore::Color;
using steamcore::Framebuffer;
using steamcore::test::framebuffersEqual;

STEAMCORE_TEST(fb_compare_two_identically_drawn_buffers_are_equal) {
  Framebuffer a;
  Framebuffer b;
  a.fillRect(3, 4, 10, 6, Color::ORANGE);
  b.fillRect(3, 4, 10, 6, Color::ORANGE);

  CHECK(framebuffersEqual(a, b));
}

// The oracle is only trusted once it has been observed failing on a real
// difference -- proven at three positions, not just one, since an
// off-by-one in the scan loop could pass the middle case while still
// missing an edge.
STEAMCORE_TEST(fb_compare_detects_single_pixel_difference_at_origin) {
  Framebuffer a;
  Framebuffer b;
  b.setPixel(0, 0, Color::ORANGE);

  int32_t mismatchX = -1;
  int32_t mismatchY = -1;
  CHECK(!framebuffersEqual(a, b, &mismatchX, &mismatchY));
  CHECK_EQ(mismatchX, 0);
  CHECK_EQ(mismatchY, 0);
}

STEAMCORE_TEST(fb_compare_detects_single_pixel_difference_at_last_pixel) {
  Framebuffer a;
  Framebuffer b;
  const int32_t lastX = Framebuffer::width() - 1;
  const int32_t lastY = Framebuffer::height() - 1;
  b.setPixel(lastX, lastY, Color::BRIGHT_ORANGE);

  int32_t mismatchX = -1;
  int32_t mismatchY = -1;
  CHECK(!framebuffersEqual(a, b, &mismatchX, &mismatchY));
  CHECK_EQ(mismatchX, lastX);
  CHECK_EQ(mismatchY, lastY);
}

STEAMCORE_TEST(fb_compare_detects_single_pixel_difference_at_interior_position) {
  Framebuffer a;
  Framebuffer b;
  b.setPixel(77, 41, Color::DARK_ORANGE);

  int32_t mismatchX = -1;
  int32_t mismatchY = -1;
  CHECK(!framebuffersEqual(a, b, &mismatchX, &mismatchY));
  CHECK_EQ(mismatchX, 77);
  CHECK_EQ(mismatchY, 41);
}

STEAMCORE_TEST(fb_compare_works_with_no_mismatch_output_pointers) {
  Framebuffer a;
  Framebuffer b;
  b.setPixel(1, 1, Color::ORANGE);

  CHECK(!framebuffersEqual(a, b));
}
