#include <cstdio>
#include <limits>

#include "steamcore/title_screen.h"

#include "fb_compare.h"
#include "steamcore/color.h"
#include "steamcore/font.h"
#include "steamcore/text.h"
#include "test_harness.h"

using steamcore::Color;
using steamcore::Framebuffer;
using steamcore::GameState;
using steamcore::drawText;
using steamcore::glyphCharAt;
using steamcore::glyphFor;
using steamcore::kGlyphAdvance;
using steamcore::kGlyphCount;
using steamcore::kGlyphHeight;
using steamcore::kGlyphWidth;
using steamcore::kTitlePromptBounds;
using steamcore::kTitleWordmarkBounds;
using steamcore::Sprite;
using steamcore::TitleBounds;
using steamcore::drawTitleScreen;
using steamcore::test::framebuffersEqual;

namespace {

// Restated independently of steamcore::detail::kWordmarkText/kPromptText
// (review F1): if this file instead imported and reused the production
// constants, a drift in either string (e.g. a typo introduced later)
// would be invisible to every test below -- the "expected" side and the
// "actual" side would silently agree on the same wrong value. Hardcoding
// the literals here is what makes AC-1.2's exact wording ("PRESS START")
// a real, independently-checked fact rather than a restatement of
// whatever production happens to say.
constexpr char kExpectedWordmarkText[] = "STEAMCORE";
constexpr char kExpectedPromptText[] = "PRESS START";

// True iff rectangles `a` and `b` (top-left + size) overlap at all.
bool rectsIntersect(const TitleBounds& a, const TitleBounds& b) {
  const bool separated = a.x + a.w <= b.x || b.x + b.w <= a.x ||
                          a.y + a.h <= b.y || b.y + b.h <= a.y;
  return !separated;
}

// Builds the expected framebuffer with the test's OWN placement loop and
// its own advance arithmetic -- never by calling drawTitleScreen or
// drawText (mirrors text_fixture_test.cpp's placeExpected).
void placeExpected(Framebuffer& expected, int32_t x0, int32_t y0,
                    const char* text, Color ink) {
  int32_t gx = x0;
  for (const char* p = text; *p != '\0'; ++p, gx += kGlyphAdvance) {
    const Sprite glyph = glyphFor(*p);
    for (int32_t row = 0; row < kGlyphHeight; ++row) {
      for (int32_t col = 0; col < kGlyphWidth; ++col) {
        if (glyph.pixels[row * glyph.stride + col] != Color::BLACK) {
          expected.setPixel(gx + col, y0 + row, ink);
        }
      }
    }
  }
}

// True iff at least one pixel inside `bounds` is not BLACK -- a coarse
// "something was drawn here" sanity check; T3 proves the exact pixels.
bool anyNonBlackInside(const Framebuffer& fb, const TitleBounds& bounds) {
  for (int32_t y = bounds.y; y < bounds.y + bounds.h; ++y) {
    for (int32_t x = bounds.x; x < bounds.x + bounds.w; ++x) {
      if (fb.pixel(x, y) != Color::BLACK) return true;
    }
  }
  return false;
}

// True iff `c` is one of the font's kGlyphCount defined characters --
// i.e. NOT one that would draw as the tofu placeholder glyph.
bool isDefinedGlyphChar(char c) {
  for (int32_t i = 0; i < kGlyphCount; ++i) {
    if (glyphCharAt(i) == c) return true;
  }
  return false;
}

}  // namespace

// Walking skeleton (T1): READY draws something inside both documented
// rects -- the exact pixels are T3's job, this only proves the phase
// gate and the two drawText calls actually run.
STEAMCORE_TEST(title_screen_ready_draws_inside_both_bounds) {
  Framebuffer fb;
  fb.clear(Color::BLACK);
  drawTitleScreen(fb, GameState::READY);

  CHECK(anyNonBlackInside(fb, kTitleWordmarkBounds));
  CHECK(anyNonBlackInside(fb, kTitlePromptBounds));
}

// AC-2.1: PLAYING draws nothing at all -- a cleared framebuffer stays
// byte-identical to an untouched one.
STEAMCORE_TEST(title_screen_playing_draws_nothing) {
  Framebuffer drawn;
  drawn.clear(Color::BLACK);
  drawTitleScreen(drawn, GameState::PLAYING);

  Framebuffer untouched;
  untouched.clear(Color::BLACK);

  CHECK(framebuffersEqual(drawn, untouched));
}

// AC-1.4: every character of the wordmark is one the font actually
// defines -- no character in "STEAMCORE" can silently draw as the tofu
// placeholder glyph.
STEAMCORE_TEST(title_screen_wordmark_uses_only_defined_glyphs) {
  for (const char* p = kExpectedWordmarkText; *p != '\0'; ++p) {
    CHECK(isDefinedGlyphChar(*p));
  }
}

// AC-1.4's positive statement: every lit pixel on the READY screen came
// from the shipped font atlas in BRIGHT_ORANGE ink -- no decoded
// PNG/TTF byte, no bespoke art asset, drawn in any other colour.
STEAMCORE_TEST(title_screen_ready_uses_only_black_and_bright_orange) {
  Framebuffer fb;
  fb.clear(Color::BLACK);
  drawTitleScreen(fb, GameState::READY);

  for (int32_t y = 0; y < Framebuffer::height(); ++y) {
    for (int32_t x = 0; x < Framebuffer::width(); ++x) {
      const Color pixel = fb.pixel(x, y);
      CHECK(pixel == Color::BLACK || pixel == Color::BRIGHT_ORANGE);
    }
  }
}

// Same proof for GAME_OVER -- this feature draws nothing outside READY,
// with no exception for the other two states (plan §1 Decision 2).
STEAMCORE_TEST(title_screen_game_over_draws_nothing) {
  Framebuffer drawn;
  drawn.clear(Color::BLACK);
  drawTitleScreen(drawn, GameState::GAME_OVER);

  Framebuffer untouched;
  untouched.clear(Color::BLACK);

  CHECK(framebuffersEqual(drawn, untouched));
}

// AC-1.1/AC-1.2: the anti-false-green fixture. The expected framebuffer
// comes from the test's own placement arithmetic and its own restated
// strings, never from calling the code under test -- a wrong advance, a
// wrong string or a wrong ink colour cannot agree with itself.
STEAMCORE_TEST(title_screen_matches_expected_framebuffer_pixel_exact) {
  Framebuffer actual;
  actual.clear(Color::BLACK);
  drawTitleScreen(actual, GameState::READY);

  Framebuffer expected;  // starts all BLACK
  placeExpected(expected, kTitleWordmarkBounds.x, kTitleWordmarkBounds.y,
                kExpectedWordmarkText, Color::BRIGHT_ORANGE);
  placeExpected(expected, kTitlePromptBounds.x, kTitlePromptBounds.y,
                kExpectedPromptText, Color::BRIGHT_ORANGE);

  int32_t mismatches = 0;
  int32_t firstX = -1;
  int32_t firstY = -1;
  for (int32_t y = 0; y < Framebuffer::height(); ++y) {
    for (int32_t x = 0; x < Framebuffer::width(); ++x) {
      if (expected.pixel(x, y) != actual.pixel(x, y)) {
        if (mismatches == 0) {
          firstX = x;
          firstY = y;
        }
        ++mismatches;
      }
    }
  }
  if (mismatches != 0) {
    std::printf("title screen mismatch: %d pixel(s) differ, first at (%d,%d)\n",
                mismatches, firstX, firstY);
  }
  CHECK_EQ(mismatches, 0);
}

// AC-1.1/AC-1.2: every pixel outside both documented rects stays BLACK --
// the minimalism guarantee, restated as a Given/When/Then rather than
// left implicit in the pixel-exact fixture above.
STEAMCORE_TEST(title_screen_every_pixel_outside_both_rects_is_black) {
  Framebuffer fb;
  fb.clear(Color::BLACK);
  drawTitleScreen(fb, GameState::READY);

  auto insideBounds = [](int32_t x, int32_t y, const TitleBounds& b) {
    return x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h;
  };

  for (int32_t y = 0; y < Framebuffer::height(); ++y) {
    for (int32_t x = 0; x < Framebuffer::width(); ++x) {
      if (insideBounds(x, y, kTitleWordmarkBounds) ||
          insideBounds(x, y, kTitlePromptBounds)) {
        continue;
      }
      CHECK(fb.pixel(x, y) == Color::BLACK);
    }
  }
}

// AC-1.6: the test's own independently computed width and centred x for
// each element equal the exported bounds -- a layout edit that updates
// one but not the other fails here.
STEAMCORE_TEST(title_screen_bounds_match_independent_computation) {
  auto stringLength = [](const char* s) {
    int32_t n = 0;
    while (s[n] != '\0') ++n;
    return n;
  };

  const int32_t wordmarkWidth = stringLength(kExpectedWordmarkText) * kGlyphAdvance;
  const int32_t wordmarkX = (Framebuffer::width() - wordmarkWidth) / 2;
  CHECK_EQ(kTitleWordmarkBounds.w, wordmarkWidth);
  CHECK_EQ(kTitleWordmarkBounds.x, wordmarkX);

  const int32_t promptWidth = stringLength(kExpectedPromptText) * kGlyphAdvance;
  const int32_t promptX = (Framebuffer::width() - promptWidth) / 2;
  CHECK_EQ(kTitlePromptBounds.w, promptWidth);
  CHECK_EQ(kTitlePromptBounds.x, promptX);
}

// AC-1.6: the two exported rects, as actually used by drawTitleScreen,
// do not intersect -- computed by a rect-intersection helper written in
// this test, independent of the header's own static_assert.
STEAMCORE_TEST(title_screen_bounds_do_not_intersect) {
  CHECK(!rectsIntersect(kTitleWordmarkBounds, kTitlePromptBounds));
}

// AC-1.5: both title strings survive being drawn at every screen extreme
// under -fsanitize=address,undefined. Honestly defense-in-depth (plan §4
// Test Strategy): with no bespoke Sprite of this feature's own, the
// overstated-stride failure mode T4 originally guarded against is gone,
// and drawText's own per-glyph clipping is already proven by
// text-rendering's suite. What this battery still owns is the composed
// call and these two specific strings never regressing that guarantee.
STEAMCORE_TEST(title_screen_strings_survive_extreme_positions_under_asan) {
  constexpr int32_t kMin = std::numeric_limits<int32_t>::min();
  constexpr int32_t kMax = std::numeric_limits<int32_t>::max();
  const int32_t w = Framebuffer::width();
  const int32_t h = Framebuffer::height();

  const int32_t xs[] = {-w, -1, 0, w - 1, w, kMin, kMax};
  const int32_t ys[] = {-h, -1, 0, h - 1, h, kMin, kMax};

  Framebuffer fb;
  for (int32_t x : xs) {
    for (int32_t y : ys) {
      drawText(fb, x, y, kExpectedWordmarkText, Color::BRIGHT_ORANGE);
      drawText(fb, x, y, kExpectedPromptText, Color::BRIGHT_ORANGE);
    }
  }
  // Reaching here without an ASan/UBSan abort is the test; nothing else
  // to assert about pixel content at these positions.
  CHECK(true);
}

// AC-1.5: a fixed-position drawTitleScreen never writes outside the
// framebuffer in READY, and writes nothing at all in PLAYING or
// GAME_OVER -- re-asserted here specifically under the sanitizer build
// (review F5: this test previously built `ready` and never asserted
// anything about it, and its comment claimed GAME_OVER coverage that
// didn't exist).
STEAMCORE_TEST(title_screen_fixed_position_stays_in_bounds_under_asan) {
  Framebuffer ready;
  ready.clear(Color::BLACK);
  drawTitleScreen(ready, GameState::READY);
  CHECK(anyNonBlackInside(ready, kTitleWordmarkBounds));
  CHECK(anyNonBlackInside(ready, kTitlePromptBounds));

  Framebuffer untouched;
  untouched.clear(Color::BLACK);

  Framebuffer playing;
  playing.clear(Color::BLACK);
  drawTitleScreen(playing, GameState::PLAYING);
  CHECK(framebuffersEqual(playing, untouched));

  Framebuffer gameOver;
  gameOver.clear(Color::BLACK);
  drawTitleScreen(gameOver, GameState::GAME_OVER);
  CHECK(framebuffersEqual(gameOver, untouched));
}
