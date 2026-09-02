#include "steamcore/font.h"
#include "test_harness.h"

using steamcore::Color;
using steamcore::glyphCharAt;
using steamcore::glyphFor;
using steamcore::kGlyphAdvance;
using steamcore::kGlyphCount;
using steamcore::kGlyphHeight;
using steamcore::kGlyphWidth;
using steamcore::Sprite;

namespace {

bool glyphPixelsEqual(const Sprite& a, const Sprite& b) {
  for (int32_t row = 0; row < kGlyphHeight; ++row) {
    for (int32_t col = 0; col < kGlyphWidth; ++col) {
      if (a.pixels[row * a.stride + col] != b.pixels[row * b.stride + col]) {
        return false;
      }
    }
  }
  return true;
}

// A character outside the defined set, guaranteed to map to the tofu
// placeholder regardless of what's currently defined (this task's set is
// a subset of A3's 43, '~' is never in it).
Sprite tofu() { return glyphFor('~'); }

}  // namespace

STEAMCORE_TEST(font_metrics_are_8x8_monospace) {
  CHECK_EQ(kGlyphWidth, 8);
  CHECK_EQ(kGlyphHeight, 8);
  CHECK_EQ(kGlyphAdvance, 8);
}

STEAMCORE_TEST(font_glyph_sprite_shape_is_correct_for_every_defined_char) {
  for (int32_t i = 0; i < kGlyphCount; ++i) {
    const char ch = glyphCharAt(i);
    const Sprite s = glyphFor(ch);
    CHECK(s.pixels != nullptr);
    CHECK_EQ(s.width, kGlyphWidth);
    CHECK_EQ(s.height, kGlyphHeight);
    CHECK_EQ(s.stride, kGlyphWidth);
  }
}

STEAMCORE_TEST(font_space_glyph_is_64_off_pixels) {
  const Sprite s = glyphFor(' ');
  int32_t onCount = 0;
  for (int32_t row = 0; row < kGlyphHeight; ++row) {
    for (int32_t col = 0; col < kGlyphWidth; ++col) {
      if (s.pixels[row * s.stride + col] != Color::BLACK) ++onCount;
    }
  }
  CHECK_EQ(onCount, 0);
}

// AC-1.2: no two glyphs share a bitmap. Asserted at runtime (not as a
// static_assert) specifically so a failing pair can be named -- a
// failing static_assert cannot say which two glyphs collided.
STEAMCORE_TEST(font_all_glyphs_including_tofu_are_pairwise_bit_distinct) {
  Sprite glyphs[kGlyphCount + 1];
  for (int32_t i = 0; i < kGlyphCount; ++i) glyphs[i] = glyphFor(glyphCharAt(i));
  glyphs[kGlyphCount] = tofu();

  int32_t collidingPairs = 0;
  for (int32_t i = 0; i < kGlyphCount + 1; ++i) {
    for (int32_t j = i + 1; j < kGlyphCount + 1; ++j) {
      if (glyphPixelsEqual(glyphs[i], glyphs[j])) ++collidingPairs;
    }
  }
  CHECK_EQ(collidingPairs, 0);
}

STEAMCORE_TEST(font_unsupported_and_negative_chars_return_tofu) {
  const Sprite reference = tofu();
  CHECK(glyphPixelsEqual(glyphFor('z'), reference));       // lowercase, out of set
  CHECK(glyphPixelsEqual(glyphFor('\n'), reference));      // control char
  CHECK(glyphPixelsEqual(glyphFor(static_cast<char>(-1)), reference));  // negative char
  CHECK(glyphPixelsEqual(glyphFor(static_cast<char>(0)), reference));   // NUL
}

// Plan §1 Decision 4: storage order is ASCII-ascending, which US-3's
// fixture (text_fixture_test.cpp) relies on via glyphCharAt(0)/
// glyphCharAt(kGlyphCount - 1). Without this check nothing catches a
// mid-table order drift -- only an accident that happens to move the
// first or last element is visible at all (review F5: this is exactly
// how T4-T6's incremental-append accident was caught, by luck, not by
// design). Demonstrated: swapping '0' and '1' wholesale in kGlyphArt
// leaves every other test green; only this one fails.
STEAMCORE_TEST(font_storage_order_is_ascii_ascending) {
  int32_t outOfOrderCount = 0;
  for (int32_t i = 0; i + 1 < kGlyphCount; ++i) {
    if (glyphCharAt(i) >= glyphCharAt(i + 1)) ++outOfOrderCount;
  }
  CHECK_EQ(outOfOrderCount, 0);
}

// AC/NFR-6: never UB, matching every other public entry point (review
// F6). An index at, one past, and far past the valid range must all
// return the safe '\0' sentinel, not read out of bounds.
STEAMCORE_TEST(font_glyph_char_at_out_of_range_index_is_safe) {
  CHECK_EQ(glyphCharAt(kGlyphCount), '\0');
  CHECK_EQ(glyphCharAt(kGlyphCount + 1000), '\0');
  CHECK_EQ(glyphCharAt(-1), '\0');
  CHECK_EQ(glyphCharAt(-1000), '\0');
}

namespace {

struct GlyphAnchor {
  char ch;
  int32_t col;
  int32_t row;
  bool on;
};

// (char, col, row) -> on/off truths, derived from the same
// independently-authored design data that was hand-typed into the
// GlyphArt table above -- never by calling glyphFor and reading a value
// back. This is the check in this file that a bug *inside* glyphFor
// itself (a wrong slot, a swapped glyph, an offset error) cannot satisfy
// just by being internally consistent -- see docs/dump-format.md's
// "Anchor table" for the same reasoning applied to the dump format, and
// text_fixture_test.cpp for the matching table used on the drawText/
// fixture side.
constexpr GlyphAnchor kAnchors[] = {
    // Computed (not hand-picked): for each of the 43 defined glyphs, a
    // set of (col, row, on/off) coordinates chosen so that swapping this
    // glyph's art with ANY other glyph's disagrees with at least one of
    // its own anchors -- a mathematical guarantee against a whole-glyph
    // substitution bug (review F1: swapping 'B' and 'C' passed all 71
    // tests before this table existed for every character). Derived from
    // the same independently-authored design data that was hand-typed
    // into the GlyphArt table above, not read back out of glyphFor --
    // see tools/generate_font_anchors.py, which regenerates this exact
    // block (verified byte-for-byte identical) from that source data and
    // re-proves the pairwise swap guarantee every time it runs (review
    // F10: a prior version of this comment pointed at a script that only
    // existed in an ephemeral scratch directory, not in the repo). One
    // line per glyph, in ASCII order matching kGlyphArt's own storage
    // order (also checked independently by
    // font_storage_order_is_ascii_ascending above).
    {' ', 1, 0, false}, {' ', 2, 0, false}, {' ', 3, 0, false}, {' ', 4, 0, false}, {' ', 2, 2, false}, {' ', 1, 3, false}, {' ', 2, 7, false},
    {'!', 1, 0, false}, {'!', 2, 0, true}, {'!', 4, 0, false},
    {'-', 1, 0, false}, {'-', 2, 0, false}, {'-', 3, 0, false}, {'-', 4, 0, false}, {'-', 2, 2, false}, {'-', 1, 3, true},
    {'.', 1, 0, false}, {'.', 2, 0, false}, {'.', 3, 0, false}, {'.', 4, 0, false}, {'.', 2, 2, false}, {'.', 1, 3, false}, {'.', 2, 7, true},
    {'0', 1, 0, false}, {'0', 2, 0, true}, {'0', 4, 0, true}, {'0', 5, 0, false}, {'0', 5, 1, true}, {'0', 1, 2, true}, {'0', 4, 2, true},
    {'1', 1, 0, false}, {'1', 2, 0, false}, {'1', 3, 0, true}, {'1', 4, 0, false},
    {'2', 1, 0, false}, {'2', 2, 0, true}, {'2', 4, 0, true}, {'2', 5, 0, false}, {'2', 5, 1, true}, {'2', 1, 2, false}, {'2', 3, 3, false}, {'2', 2, 5, true},
    {'3', 1, 0, false}, {'3', 2, 0, true}, {'3', 4, 0, true}, {'3', 5, 0, false}, {'3', 5, 1, true}, {'3', 1, 2, false}, {'3', 3, 3, true},
    {'4', 1, 0, false}, {'4', 2, 0, false}, {'4', 3, 0, false}, {'4', 4, 0, true},
    {'5', 1, 0, true}, {'5', 2, 0, true}, {'5', 5, 0, true}, {'5', 1, 1, true}, {'5', 1, 4, false},
    {'6', 1, 0, false}, {'6', 2, 0, true}, {'6', 4, 0, true}, {'6', 5, 0, false}, {'6', 5, 1, false},
    {'7', 1, 0, true}, {'7', 2, 0, true}, {'7', 5, 0, true}, {'7', 1, 1, false}, {'7', 3, 1, false}, {'7', 1, 5, false},
    {'8', 1, 0, false}, {'8', 2, 0, true}, {'8', 4, 0, true}, {'8', 5, 0, false}, {'8', 5, 1, true}, {'8', 1, 2, true}, {'8', 4, 2, false}, {'8', 1, 3, false}, {'8', 5, 3, false},
    {'9', 1, 0, false}, {'9', 2, 0, true}, {'9', 4, 0, true}, {'9', 5, 0, false}, {'9', 5, 1, true}, {'9', 1, 2, true}, {'9', 4, 2, false}, {'9', 1, 3, false}, {'9', 5, 3, true},
    {':', 1, 0, false}, {':', 2, 0, false}, {':', 3, 0, false}, {':', 4, 0, false}, {':', 2, 2, true},
    {'>', 1, 0, true}, {'>', 2, 0, false}, {'>', 5, 0, false}, {'>', 1, 1, false},
    {'?', 1, 0, false}, {'?', 2, 0, true}, {'?', 4, 0, true}, {'?', 5, 0, false}, {'?', 5, 1, true}, {'?', 1, 2, false}, {'?', 3, 3, false}, {'?', 2, 5, false},
    {'A', 1, 0, false}, {'A', 2, 0, true}, {'A', 4, 0, true}, {'A', 5, 0, false}, {'A', 5, 1, true}, {'A', 1, 2, true}, {'A', 4, 2, false}, {'A', 1, 3, true}, {'A', 2, 3, true},
    {'B', 1, 0, true}, {'B', 2, 0, true}, {'B', 5, 0, false}, {'B', 2, 3, true}, {'B', 3, 4, false}, {'B', 5, 4, true},
    {'C', 1, 0, false}, {'C', 2, 0, true}, {'C', 4, 0, true}, {'C', 5, 0, true}, {'C', 1, 3, true}, {'C', 3, 3, false},
    {'D', 1, 0, true}, {'D', 2, 0, true}, {'D', 5, 0, false}, {'D', 2, 3, false},
    {'E', 1, 0, true}, {'E', 2, 0, true}, {'E', 5, 0, true}, {'E', 1, 1, true}, {'E', 1, 4, true}, {'E', 2, 6, true},
    {'F', 1, 0, true}, {'F', 2, 0, true}, {'F', 5, 0, true}, {'F', 1, 1, true}, {'F', 1, 4, true}, {'F', 2, 6, false},
    {'G', 1, 0, false}, {'G', 2, 0, true}, {'G', 4, 0, true}, {'G', 5, 0, true}, {'G', 1, 3, true}, {'G', 3, 3, true},
    {'H', 1, 0, true}, {'H', 2, 0, false}, {'H', 5, 0, true}, {'H', 2, 1, false}, {'H', 4, 1, false}, {'H', 1, 2, true}, {'H', 2, 3, true},
    {'I', 1, 0, true}, {'I', 2, 0, true}, {'I', 5, 0, true}, {'I', 1, 1, false}, {'I', 3, 1, true}, {'I', 1, 6, true},
    {'J', 1, 0, false}, {'J', 2, 0, false}, {'J', 3, 0, true}, {'J', 4, 0, true},
    {'K', 1, 0, true}, {'K', 2, 0, false}, {'K', 5, 0, true}, {'K', 2, 1, false}, {'K', 4, 1, true},
    {'L', 1, 0, true}, {'L', 2, 0, false}, {'L', 5, 0, false}, {'L', 1, 1, true},
    {'M', 1, 0, true}, {'M', 2, 0, false}, {'M', 5, 0, true}, {'M', 2, 1, true}, {'M', 4, 1, true},
    {'N', 1, 0, true}, {'N', 2, 0, false}, {'N', 5, 0, true}, {'N', 2, 1, true}, {'N', 4, 1, false},
    {'O', 1, 0, false}, {'O', 2, 0, true}, {'O', 4, 0, true}, {'O', 5, 0, false}, {'O', 5, 1, true}, {'O', 1, 2, true}, {'O', 4, 2, false}, {'O', 1, 3, true}, {'O', 2, 3, false}, {'O', 3, 4, false},
    {'P', 1, 0, true}, {'P', 2, 0, true}, {'P', 5, 0, false}, {'P', 2, 3, true}, {'P', 3, 4, false}, {'P', 5, 4, false},
    {'Q', 1, 0, false}, {'Q', 2, 0, true}, {'Q', 4, 0, true}, {'Q', 5, 0, false}, {'Q', 5, 1, true}, {'Q', 1, 2, true}, {'Q', 4, 2, false}, {'Q', 1, 3, true}, {'Q', 2, 3, false}, {'Q', 3, 4, true},
    {'R', 1, 0, true}, {'R', 2, 0, true}, {'R', 5, 0, false}, {'R', 2, 3, true}, {'R', 3, 4, true},
    {'S', 1, 0, false}, {'S', 2, 0, true}, {'S', 4, 0, true}, {'S', 5, 0, true}, {'S', 1, 3, false},
    {'T', 1, 0, true}, {'T', 2, 0, true}, {'T', 5, 0, true}, {'T', 1, 1, false}, {'T', 3, 1, true}, {'T', 1, 6, false},
    {'U', 1, 0, true}, {'U', 2, 0, false}, {'U', 5, 0, true}, {'U', 2, 1, false}, {'U', 4, 1, false}, {'U', 1, 2, true}, {'U', 2, 3, false}, {'U', 3, 3, false}, {'U', 1, 5, true},
    {'V', 1, 0, true}, {'V', 2, 0, false}, {'V', 5, 0, true}, {'V', 2, 1, false}, {'V', 4, 1, false}, {'V', 1, 2, true}, {'V', 2, 3, false}, {'V', 3, 3, false}, {'V', 1, 5, false},
    {'W', 1, 0, true}, {'W', 2, 0, false}, {'W', 5, 0, true}, {'W', 2, 1, false}, {'W', 4, 1, false}, {'W', 1, 2, true}, {'W', 2, 3, false}, {'W', 3, 3, true},
    {'X', 1, 0, true}, {'X', 2, 0, false}, {'X', 5, 0, true}, {'X', 2, 1, false}, {'X', 4, 1, false}, {'X', 1, 2, false}, {'X', 2, 4, true},
    {'Y', 1, 0, true}, {'Y', 2, 0, false}, {'Y', 5, 0, true}, {'Y', 2, 1, false}, {'Y', 4, 1, false}, {'Y', 1, 2, false}, {'Y', 2, 4, false},
    {'Z', 1, 0, true}, {'Z', 2, 0, true}, {'Z', 5, 0, true}, {'Z', 1, 1, false}, {'Z', 3, 1, false}, {'Z', 1, 5, true},
    // tofu (checkerboard): on iff (row + col) is even -- unambiguous as
    // "undefined", never resembling a real glyph. Hand-written, not
    // computed, since it isn't part of the 43-glyph swap problem above.
    {'~', 0, 0, true}, {'~', 1, 0, false}, {'~', 0, 1, false}, {'~', 1, 1, true},
};

}  // namespace

// The exact 43-character set from spec A3, spelled out independently of
// font.cpp's table order -- this is the ground truth AC-1.1/AC-1.4 check
// against, not something read back out of the font.
constexpr const char* kExpectedCharacterSet =
    " !-.0123456789:>?ABCDEFGHIJKLMNOPQRSTUVWXYZ";

STEAMCORE_TEST(font_defines_exactly_the_43_character_set_and_nothing_else) {
  const Sprite reference = tofu();

  bool expected[256] = {};
  for (const char* p = kExpectedCharacterSet; *p != '\0'; ++p) {
    expected[static_cast<unsigned char>(*p)] = true;
  }

  int32_t definedCount = 0;
  int32_t wrongClassification = 0;
  for (int32_t code = 0; code < 256; ++code) {
    const char ch = static_cast<char>(static_cast<unsigned char>(code));
    const bool isTofu = glyphPixelsEqual(glyphFor(ch), reference);
    if (expected[code]) {
      ++definedCount;
      if (isTofu) ++wrongClassification;  // a defined char must not be tofu
    } else {
      if (!isTofu) ++wrongClassification;  // an undefined char must be tofu
    }
  }
  CHECK_EQ(definedCount, 43);
  CHECK_EQ(wrongClassification, 0);
}

STEAMCORE_TEST(font_glyph_char_at_covers_the_43_set_with_no_repeat) {
  bool seen[256] = {};
  int32_t repeats = 0;
  int32_t outsideExpectedSet = 0;

  bool expected[256] = {};
  for (const char* p = kExpectedCharacterSet; *p != '\0'; ++p) {
    expected[static_cast<unsigned char>(*p)] = true;
  }

  for (int32_t i = 0; i < kGlyphCount; ++i) {
    const auto code = static_cast<unsigned char>(glyphCharAt(i));
    if (seen[code]) ++repeats;
    seen[code] = true;
    if (!expected[code]) ++outsideExpectedSet;
  }
  CHECK_EQ(repeats, 0);
  CHECK_EQ(outsideExpectedSet, 0);
}

STEAMCORE_TEST(font_matches_hand_written_anchor_table) {
  int32_t mismatches = 0;
  for (const GlyphAnchor& anchor : kAnchors) {
    const Sprite s = glyphFor(anchor.ch);
    const Color pixel = s.pixels[anchor.row * s.stride + anchor.col];
    const bool actualOn = pixel != Color::BLACK;
    if (actualOn != anchor.on) ++mismatches;
  }
  CHECK_EQ(mismatches, 0);
}
