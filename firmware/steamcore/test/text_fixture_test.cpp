#include <cstdio>
#include <initializer_list>

#include "steamcore/dump_format.h"
#include "steamcore/font.h"
#include "steamcore/framebuffer.h"
#include "steamcore/text.h"
#include "test_harness.h"

#ifndef STEAMCORE_TEXT_DUMP
#error "STEAMCORE_TEXT_DUMP must be defined by the build (see Makefile)"
#endif

using steamcore::Color;
using steamcore::drawText;
using steamcore::Framebuffer;
using steamcore::glyphCharAt;
using steamcore::glyphFor;
using steamcore::kDumpHeaderSize;
using steamcore::kGlyphAdvance;
using steamcore::kGlyphCount;
using steamcore::kGlyphHeight;
using steamcore::kGlyphWidth;
using steamcore::serializeDump;
using steamcore::Sprite;

namespace {

constexpr size_t kBufferCapacity =
    kDumpHeaderSize +
    static_cast<size_t>(Framebuffer::width()) * Framebuffer::height();

// AC-3.1: a permutation of all 43 defined characters, each exactly once,
// with '.' and ':' adjacent at least once (the most visually confusable
// pair). Not the font's raw storage order (' !-.0-9:>?A-Z') -- that
// keeps '.' and ':' ten characters apart -- but still starting with
// glyphCharAt(0) and ending with glyphCharAt(kGlyphCount - 1), asserted
// below rather than assumed. 30 + 13 = 43, two rows.
constexpr const char* kFixtureRow1 = " !-.:0123456789>?ABCDEFGHIJKLM";  // 30 chars
constexpr const char* kFixtureRow2 = "NOPQRSTUVWXYZ";                    // 13 chars

constexpr int32_t kRow1X = 0;
constexpr int32_t kRow1Y = 10;
constexpr int32_t kRow2X = 0;
constexpr int32_t kRow2Y = 20;
constexpr Color kInk = Color::BRIGHT_ORANGE;

int32_t stringLength(const char* s) {
  int32_t n = 0;
  while (s[n] != '\0') ++n;
  return n;
}

// Independent of font.cpp's private slot table: finds the storage slot
// for `ch` purely through the public glyphCharAt accessor, which exists
// specifically so callers (this test) can do this (font.h, NFR-6).
// Returns -1 if `ch` is not one of the kGlyphCount defined characters.
int32_t slotOf(char ch) {
  for (int32_t i = 0; i < kGlyphCount; ++i) {
    if (glyphCharAt(i) == ch) return i;
  }
  return -1;
}

// Hand-written 8x8 "on" masks for '.', ':' and '!' -- authored directly
// from font.cpp's ASCII art and §8's shape rules, not read back out of
// glyphFor. These three are the ones AC-3.1 calls out as the highest
// confusion risk; every other character's expected pixels come from
// glyphFor itself, which is enough to prove drawText's MECHANISM
// (retrieval, placement, colour mapping) -- shape correctness for the
// full alphabet is font_test.cpp's anchor table's job.
bool handWrittenOn(char ch, int32_t row, int32_t col) {
  if (ch == '.') return row == 7 && (col == 2 || col == 3);
  if (ch == ':') return (row == 2 || row == 5) && (col == 2 || col == 3);
  if (ch == '!') {
    if (col != 2 && col != 3) return false;
    return row <= 4 || row == 7;
  }
  return false;
}

// Builds the expected framebuffer with the test's OWN placement loop and
// its own advance arithmetic -- never by calling drawText. For '.', ':'
// and '!' the "on" pixels come from handWrittenOn(); for every other
// character they come from glyphFor(), which is sufficient to prove
// drawText's placement/advance/colour mechanism (glyph shape correctness
// for the rest of the alphabet is covered independently by
// font_test.cpp's anchor table).
void placeExpected(Framebuffer& expected, int32_t x0, int32_t y0,
                    const char* text) {
  int32_t gx = x0;
  for (const char* p = text; *p != '\0'; ++p, gx += kGlyphAdvance) {
    const char ch = *p;
    const Sprite glyph = glyphFor(ch);
    for (int32_t row = 0; row < kGlyphHeight; ++row) {
      for (int32_t col = 0; col < kGlyphWidth; ++col) {
        const bool on = (ch == '.' || ch == ':' || ch == '!')
                            ? handWrittenOn(ch, row, col)
                            : glyph.pixels[row * glyph.stride + col] != Color::BLACK;
        if (on) expected.setPixel(gx + col, y0 + row, kInk);
      }
    }
  }
}

}  // namespace

// AC-3.1/AC-3.2/AC-2.2: the anti-false-green fixture. Full-buffer,
// pixel-exact proof that drawText draws every one of the 43 characters
// at the right place, in the right colour -- not a sample, not a
// plausible-looking picture.
STEAMCORE_TEST(text_fixture_covers_all_43_characters_with_no_repeat) {
  CHECK_EQ(stringLength(kFixtureRow1) + stringLength(kFixtureRow2), kGlyphCount);

  bool seenSlot[kGlyphCount] = {};
  int32_t repeats = 0;
  int32_t unknown = 0;
  for (const char* row : {kFixtureRow1, kFixtureRow2}) {
    for (const char* p = row; *p != '\0'; ++p) {
      const int32_t slot = slotOf(*p);
      if (slot < 0) {
        ++unknown;
        continue;
      }
      if (seenSlot[slot]) ++repeats;
      seenSlot[slot] = true;
    }
  }
  CHECK_EQ(unknown, 0);
  CHECK_EQ(repeats, 0);
}

STEAMCORE_TEST(text_fixture_starts_and_ends_at_the_fonts_own_storage_extremes) {
  // A change to the font's storage order changes what glyphCharAt(0) and
  // glyphCharAt(kGlyphCount - 1) return, which fails this test loudly --
  // the fixture's extremes are asserted, not assumed (AC-3.1).
  CHECK_EQ(kFixtureRow1[0], glyphCharAt(0));
  const int32_t lastIndex = stringLength(kFixtureRow2) - 1;
  CHECK_EQ(kFixtureRow2[lastIndex], glyphCharAt(kGlyphCount - 1));
}

STEAMCORE_TEST(text_fixture_has_dot_and_colon_adjacent) {
  bool adjacent = false;
  for (const char* row : {kFixtureRow1, kFixtureRow2}) {
    for (int32_t i = 0; row[i] != '\0' && row[i + 1] != '\0'; ++i) {
      if ((row[i] == '.' && row[i + 1] == ':') ||
          (row[i] == ':' && row[i + 1] == '.')) {
        adjacent = true;
      }
    }
  }
  CHECK(adjacent);
}

STEAMCORE_TEST(text_fixture_matches_expected_framebuffer_pixel_exact) {
  Framebuffer actual;
  drawText(actual, kRow1X, kRow1Y, kFixtureRow1, kInk);
  drawText(actual, kRow2X, kRow2Y, kFixtureRow2, kInk);

  Framebuffer expected;  // starts all BLACK
  placeExpected(expected, kRow1X, kRow1Y, kFixtureRow1);
  placeExpected(expected, kRow2X, kRow2Y, kFixtureRow2);

  int32_t mismatches = 0;
  int32_t firstX = -1;
  int32_t firstY = -1;
  Color firstExpected = Color::BLACK;
  Color firstActual = Color::BLACK;
  for (int32_t y = 0; y < Framebuffer::height(); ++y) {
    for (int32_t x = 0; x < Framebuffer::width(); ++x) {
      const Color e = expected.pixel(x, y);
      const Color a = actual.pixel(x, y);
      if (e != a) {
        if (mismatches == 0) {
          firstX = x;
          firstY = y;
          firstExpected = e;
          firstActual = a;
        }
        ++mismatches;
      }
    }
  }
  if (mismatches != 0) {
    std::printf("text fixture mismatch: %d pixel(s) differ, first at (%d,%d): "
                "expected colour %d, got %d\n",
                mismatches, firstX, firstY, static_cast<int>(firstExpected),
                static_cast<int>(firstActual));
  }
  CHECK_EQ(mismatches, 0);
}

// AC-3.2: the same fixture, dumped to the path the TEXT_DUMP Makefile
// variable names, so `make view` renders it as a legible PNG.
STEAMCORE_TEST(text_fixture_dumps_to_disk_for_visual_check) {
  Framebuffer fb;
  drawText(fb, kRow1X, kRow1Y, kFixtureRow1, kInk);
  drawText(fb, kRow2X, kRow2Y, kFixtureRow2, kInk);

  static uint8_t buffer[kBufferCapacity];
  const size_t written = serializeDump(fb, buffer, sizeof(buffer));
  CHECK_EQ(written, kBufferCapacity);

  FILE* out = std::fopen(STEAMCORE_TEXT_DUMP, "wb");
  if (out == nullptr) {
    CHECK(false && "could not open " STEAMCORE_TEXT_DUMP
                    " for writing -- run make from the repo root");
    return;
  }
  const size_t fwritten = std::fwrite(buffer, 1, written, out);
  std::fclose(out);
  CHECK_EQ(fwritten, written);
}
