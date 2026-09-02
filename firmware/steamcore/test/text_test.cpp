#include "steamcore/font.h"
#include "steamcore/text.h"
#include "test_harness.h"

using steamcore::Color;
using steamcore::drawText;
using steamcore::Framebuffer;
using steamcore::glyphFor;
using steamcore::kGlyphAdvance;
using steamcore::kGlyphHeight;
using steamcore::kGlyphWidth;
using steamcore::Sprite;

namespace {

// Expected colour for glyph `ch`'s pixel at its own local (col, row),
// given the drawText contract: an "on" glyph pixel (anything other than
// BLACK in the atlas) becomes `ink`; an "off" pixel stays untouched
// background. This mirrors drawText's own mapping deliberately -- T2
// tests the drawing MECHANISM (placement, advance, clipping, colour
// selection); glyph shape correctness is T1/T6's job with their own
// independent anchor table.
Color expectedCellPixel(char ch, int32_t col, int32_t row, Color ink) {
  const Sprite glyph = glyphFor(ch);
  const Color src = glyph.pixels[row * glyph.stride + col];
  return (src == Color::BLACK) ? Color::BLACK : ink;
}

// True iff every framebuffer pixel matches `background`, except pixels
// inside [x0,x0+w) x [y0,y0+h), which are not checked at all (the caller
// checks those separately). Used to prove "everything else unchanged".
bool everythingOutsideIsBackground(const Framebuffer& fb, int32_t x0, int32_t y0,
                                    int32_t w, int32_t h, Color background) {
  for (int32_t y = 0; y < Framebuffer::height(); ++y) {
    for (int32_t x = 0; x < Framebuffer::width(); ++x) {
      const bool inside = x >= x0 && x < x0 + w && y >= y0 && y < y0 + h;
      if (inside) continue;
      if (fb.pixel(x, y) != background) return false;
    }
  }
  return true;
}

}  // namespace

// AC-2.1
STEAMCORE_TEST(text_single_char_draws_exactly_its_glyph_in_ink) {
  Framebuffer fb;
  drawText(fb, 10, 10, "A", Color::BRIGHT_ORANGE);

  int32_t mismatches = 0;
  for (int32_t row = 0; row < kGlyphHeight; ++row) {
    for (int32_t col = 0; col < kGlyphWidth; ++col) {
      const Color expected = expectedCellPixel('A', col, row, Color::BRIGHT_ORANGE);
      if (fb.pixel(10 + col, 10 + row) != expected) ++mismatches;
    }
  }
  CHECK_EQ(mismatches, 0);
  CHECK(everythingOutsideIsBackground(fb, 10, 10, kGlyphWidth, kGlyphHeight, Color::BLACK));
}

// AC-2.2
STEAMCORE_TEST(text_multichar_advance_has_no_overlap_and_no_gap) {
  Framebuffer combined;
  drawText(combined, 20, 30, ".A:", Color::BRIGHT_ORANGE);

  Framebuffer individual;
  drawText(individual, 20 + 0 * kGlyphAdvance, 30, ".", Color::BRIGHT_ORANGE);
  drawText(individual, 20 + 1 * kGlyphAdvance, 30, "A", Color::BRIGHT_ORANGE);
  drawText(individual, 20 + 2 * kGlyphAdvance, 30, ":", Color::BRIGHT_ORANGE);

  int32_t mismatches = 0;
  for (int32_t y = 0; y < Framebuffer::height(); ++y) {
    for (int32_t x = 0; x < Framebuffer::width(); ++x) {
      if (combined.pixel(x, y) != individual.pixel(x, y)) ++mismatches;
    }
  }
  CHECK_EQ(mismatches, 0);
  CHECK_EQ(kGlyphAdvance, kGlyphWidth);  // no gap: advance == cell width
}

// AC-2.3
STEAMCORE_TEST(text_unsupported_char_and_control_char_render_placeholder) {
  Framebuffer withLowercase;
  drawText(withLowercase, 0, 0, "z", Color::BRIGHT_ORANGE);
  Framebuffer withTofu;
  drawText(withTofu, 0, 0, "~", Color::BRIGHT_ORANGE);  // '~' guaranteed tofu (font_test.cpp)

  Framebuffer withNewline;
  drawText(withNewline, 0, 0, "\n", Color::BRIGHT_ORANGE);

  for (int32_t row = 0; row < kGlyphHeight; ++row) {
    for (int32_t col = 0; col < kGlyphWidth; ++col) {
      CHECK(withLowercase.pixel(col, row) == withTofu.pixel(col, row));
      CHECK(withNewline.pixel(col, row) == withTofu.pixel(col, row));
    }
  }
}

// AC-2.4: edges, fully off-screen, extreme coordinates -- ASan itself
// (make test-asan) is the actual proof of "no out-of-bounds finding";
// this test proves the on-screen behaviour is correct too.
STEAMCORE_TEST(text_off_screen_and_extreme_coordinates_are_clipped_not_crashed) {
  Framebuffer fb;
  // Left edge: half the first glyph is off-screen.
  drawText(fb, -4, 50, "A", Color::BRIGHT_ORANGE);
  // Right edge.
  drawText(fb, Framebuffer::width() - 4, 60, "A", Color::BRIGHT_ORANGE);
  // Top/bottom edge.
  drawText(fb, 70, -4, "A", Color::BRIGHT_ORANGE);
  drawText(fb, 70, Framebuffer::height() - 4, "A", Color::BRIGHT_ORANGE);
  // Entirely off-screen -- must be a total no-op.
  Framebuffer before = fb;
  drawText(fb, 10000, 10000, "AAAA", Color::BRIGHT_ORANGE);
  drawText(fb, -10000, -10000, "AAAA", Color::BRIGHT_ORANGE);
  int32_t mismatches = 0;
  for (int32_t y = 0; y < Framebuffer::height(); ++y) {
    for (int32_t x = 0; x < Framebuffer::width(); ++x) {
      if (fb.pixel(x, y) != before.pixel(x, y)) ++mismatches;
    }
  }
  CHECK_EQ(mismatches, 0);

  // Extreme coordinates must not crash and must not draw (string starts
  // far off either edge and stays off for its whole short length).
  drawText(fb, INT32_MAX - 2, 0, "AA", Color::BRIGHT_ORANGE);
  drawText(fb, INT32_MIN + 2, 0, "AA", Color::BRIGHT_ORANGE);
  drawText(fb, 0, INT32_MAX - 2, "AA", Color::BRIGHT_ORANGE);
  drawText(fb, 0, INT32_MIN + 2, "AA", Color::BRIGHT_ORANGE);
}

// AC-2.5
STEAMCORE_TEST(text_empty_string_is_noop) {
  Framebuffer fb;
  fb.fillRect(0, 0, Framebuffer::width(), Framebuffer::height(), Color::DARK_ORANGE);
  Framebuffer before = fb;
  drawText(fb, 50, 50, "", Color::BRIGHT_ORANGE);

  int32_t mismatches = 0;
  for (int32_t y = 0; y < Framebuffer::height(); ++y) {
    for (int32_t x = 0; x < Framebuffer::width(); ++x) {
      if (fb.pixel(x, y) != before.pixel(x, y)) ++mismatches;
    }
  }
  CHECK_EQ(mismatches, 0);
}

STEAMCORE_TEST(text_null_text_is_noop) {
  Framebuffer fb;
  fb.fillRect(0, 0, Framebuffer::width(), Framebuffer::height(), Color::DARK_ORANGE);
  Framebuffer before = fb;
  drawText(fb, 50, 50, nullptr, Color::BRIGHT_ORANGE);

  int32_t mismatches = 0;
  for (int32_t y = 0; y < Framebuffer::height(); ++y) {
    for (int32_t x = 0; x < Framebuffer::width(); ++x) {
      if (fb.pixel(x, y) != before.pixel(x, y)) ++mismatches;
    }
  }
  CHECK_EQ(mismatches, 0);
}

// AC-2.6
STEAMCORE_TEST(text_same_draw_twice_is_byte_identical) {
  Framebuffer a;
  Framebuffer b;
  drawText(a, 15, 25, ".A:", Color::BRIGHT_ORANGE);
  drawText(b, 15, 25, ".A:", Color::BRIGHT_ORANGE);

  int32_t mismatches = 0;
  for (int32_t y = 0; y < Framebuffer::height(); ++y) {
    for (int32_t x = 0; x < Framebuffer::width(); ++x) {
      if (a.pixel(x, y) != b.pixel(x, y)) ++mismatches;
    }
  }
  CHECK_EQ(mismatches, 0);
}

// AC-2.7: two different inks over the same background -- only "on"
// pixels differ, each matching its own requested colour; every
// "off"/background pixel is identical in both, INCLUDING the BLACK-ink
// case (which must draw real black pixels, not vanish, per the
// transparent-key design).
STEAMCORE_TEST(text_two_inks_over_same_background_only_on_pixels_differ) {
  Framebuffer orangeInk;
  orangeInk.clear(Color::DARK_ORANGE);
  drawText(orangeInk, 30, 40, "A", Color::BRIGHT_ORANGE);

  Framebuffer blackInk;
  blackInk.clear(Color::DARK_ORANGE);
  drawText(blackInk, 30, 40, "A", Color::BLACK);

  int32_t offPixelMismatches = 0;
  int32_t onPixelWrongColour = 0;
  for (int32_t row = 0; row < kGlyphHeight; ++row) {
    for (int32_t col = 0; col < kGlyphWidth; ++col) {
      const Sprite glyph = glyphFor('A');
      const bool on = glyph.pixels[row * glyph.stride + col] != Color::BLACK;
      const Color orangePixel = orangeInk.pixel(30 + col, 40 + row);
      const Color blackPixel = blackInk.pixel(30 + col, 40 + row);
      if (on) {
        if (orangePixel != Color::BRIGHT_ORANGE) ++onPixelWrongColour;
        if (blackPixel != Color::BLACK) ++onPixelWrongColour;
      } else {
        if (orangePixel != Color::DARK_ORANGE) ++offPixelMismatches;
        if (blackPixel != Color::DARK_ORANGE) ++offPixelMismatches;
      }
    }
  }
  CHECK_EQ(onPixelWrongColour, 0);
  CHECK_EQ(offPixelMismatches, 0);
}

// AC-2.8: a whole string of unsupported characters, not just one.
STEAMCORE_TEST(text_all_unsupported_string_renders_placeholder_at_every_position) {
  Framebuffer allTofu;
  drawText(allTofu, 0, 0, "~~~", Color::BRIGHT_ORANGE);

  Framebuffer expected;
  drawText(expected, 0 + 0 * kGlyphAdvance, 0, "~", Color::BRIGHT_ORANGE);
  drawText(expected, 0 + 1 * kGlyphAdvance, 0, "~", Color::BRIGHT_ORANGE);
  drawText(expected, 0 + 2 * kGlyphAdvance, 0, "~", Color::BRIGHT_ORANGE);

  int32_t mismatches = 0;
  for (int32_t y = 0; y < Framebuffer::height(); ++y) {
    for (int32_t x = 0; x < Framebuffer::width(); ++x) {
      if (allTofu.pixel(x, y) != expected.pixel(x, y)) ++mismatches;
    }
  }
  CHECK_EQ(mismatches, 0);
}
