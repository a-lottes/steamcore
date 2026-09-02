#include "steamcore/font.h"

#include <array>

namespace steamcore {

namespace {

// One glyph, authored as a picture: 8 rows of ' ' (off) / '#' (on),
// eight characters wide each. This is the single source of truth for
// storage order, the char->slot lookup, and kGlyphCount's static_assert
// below -- there is no second hand-maintained list that could disagree
// with this one.
struct GlyphArt {
  char ch;
  const char* rows[kGlyphHeight];
};

// Punctuation shape rules (design review, spec §8) -- binding on the
// five characters below, so a future edit can't reintroduce the
// confusion the review specifically checked for at 8x8 with no
// anti-aliasing:
//   '.'  one block at the very bottom (baseline-anchored) only.
//   ':'  two blocks in the mid-height band, deliberately clear of the
//        bottom row -- never touching where '.' sits.
//   '!'  a continuous vertical stroke through most of the cell height,
//        plus one baseline dot with a visible gap before it -- the
//        stroke-vs-dots contrast is what keeps it distinct from ':'.
//   '-'  a single horizontal bar, mid-cell, nowhere else.
//   '?'  a curve over a baseline dot, with a gap between them -- the dot
//        sits at the same baseline row as '.', not one row above it
//        (review F3).
//
// '>' and '?' are outside those five pinned shapes, but their pixel data
// is deliberately inset one column from the left edge (columns 1-5/1-6),
// matching every letter and digit -- they used to sit flush against
// column 0, one column left of the rest of the font, which read as a
// misaligned menu cursor and dot placement in the rendered PNG (review
// F8).
//
// clang-format off
constexpr GlyphArt kGlyphArt[] = {
    {' ', {
        "        ",
        "        ",
        "        ",
        "        ",
        "        ",
        "        ",
        "        ",
        "        ",
    }},
    {'!', {
        "  ##    ",
        "  ##    ",
        "  ##    ",
        "  ##    ",
        "  ##    ",
        "        ",
        "        ",
        "  ##    ",
    }},
    {'-', {
        "        ",
        "        ",
        "        ",
        " #####  ",
        "        ",
        "        ",
        "        ",
        "        ",
    }},
    {'.', {
        "        ",
        "        ",
        "        ",
        "        ",
        "        ",
        "        ",
        "        ",
        "  ##    ",
    }},
    {'0', {
        "  ###   ",
        " #   #  ",
        " #  ##  ",
        " # # #  ",
        " ##  #  ",
        " #   #  ",
        "  ###   ",
        "        ",
    }},
    {'1', {
        "   #    ",
        "  ##    ",
        "   #    ",
        "   #    ",
        "   #    ",
        "   #    ",
        "  ###   ",
        "        ",
    }},
    {'2', {
        "  ###   ",
        " #   #  ",
        "     #  ",
        "    #   ",
        "   #    ",
        "  #     ",
        " #####  ",
        "        ",
    }},
    {'3', {
        "  ###   ",
        " #   #  ",
        "     #  ",
        "   ##   ",
        "     #  ",
        " #   #  ",
        "  ###   ",
        "        ",
    }},
    {'4', {
        "    #   ",
        "   ##   ",
        "  # #   ",
        " #  #   ",
        " #####  ",
        "    #   ",
        "    #   ",
        "        ",
    }},
    {'5', {
        " #####  ",
        " #      ",
        " #      ",
        " ####   ",
        "     #  ",
        " #   #  ",
        "  ###   ",
        "        ",
    }},
    {'6', {
        "  ###   ",
        " #      ",
        " #      ",
        " ####   ",
        " #   #  ",
        " #   #  ",
        "  ###   ",
        "        ",
    }},
    {'7', {
        " #####  ",
        "     #  ",
        "    #   ",
        "   #    ",
        "  #     ",
        "  #     ",
        "  #     ",
        "        ",
    }},
    {'8', {
        "  ###   ",
        " #   #  ",
        " #   #  ",
        "  ###   ",
        " #   #  ",
        " #   #  ",
        "  ###   ",
        "        ",
    }},
    {'9', {
        "  ###   ",
        " #   #  ",
        " #   #  ",
        "  ####  ",
        "     #  ",
        "     #  ",
        "  ###   ",
        "        ",
    }},
    {':', {
        "        ",
        "        ",
        "  ##    ",
        "        ",
        "        ",
        "  ##    ",
        "        ",
        "        ",
    }},
    {'>', {
        " #      ",
        "  #     ",
        "   #    ",
        "    #   ",
        "   #    ",
        "  #     ",
        " #      ",
        "        ",
    }},
    {'?', {
        "  ###   ",
        " #   #  ",
        "     #  ",
        "    #   ",
        "   #    ",
        "        ",
        "        ",
        "   #    ",
    }},
    {'A', {
        "  ###   ",
        " #   #  ",
        " #   #  ",
        " #####  ",
        " #   #  ",
        " #   #  ",
        " #   #  ",
        "        ",
    }},
    {'B', {
        " ####   ",
        " #   #  ",
        " #   #  ",
        " ####   ",
        " #   #  ",
        " #   #  ",
        " ####   ",
        "        ",
    }},
    {'C', {
        "  ####  ",
        " #      ",
        " #      ",
        " #      ",
        " #      ",
        " #      ",
        "  ####  ",
        "        ",
    }},
    {'D', {
        " ####   ",
        " #   #  ",
        " #   #  ",
        " #   #  ",
        " #   #  ",
        " #   #  ",
        " ####   ",
        "        ",
    }},
    {'E', {
        " #####  ",
        " #      ",
        " #      ",
        " ####   ",
        " #      ",
        " #      ",
        " #####  ",
        "        ",
    }},
    {'F', {
        " #####  ",
        " #      ",
        " #      ",
        " ####   ",
        " #      ",
        " #      ",
        " #      ",
        "        ",
    }},
    {'G', {
        "  ####  ",
        " #      ",
        " #      ",
        " # ###  ",
        " #   #  ",
        " #   #  ",
        "  ####  ",
        "        ",
    }},
    {'H', {
        " #   #  ",
        " #   #  ",
        " #   #  ",
        " #####  ",
        " #   #  ",
        " #   #  ",
        " #   #  ",
        "        ",
    }},
    {'I', {
        " #####  ",
        "   #    ",
        "   #    ",
        "   #    ",
        "   #    ",
        "   #    ",
        " #####  ",
        "        ",
    }},
    {'J', {
        "   ###  ",
        "    #   ",
        "    #   ",
        "    #   ",
        "    #   ",
        " #  #   ",
        "  ##    ",
        "        ",
    }},
    {'K', {
        " #   #  ",
        " #  #   ",
        " # #    ",
        " ##     ",
        " # #    ",
        " #  #   ",
        " #   #  ",
        "        ",
    }},
    {'L', {
        " #      ",
        " #      ",
        " #      ",
        " #      ",
        " #      ",
        " #      ",
        " #####  ",
        "        ",
    }},
    {'M', {
        " #   #  ",
        " ## ##  ",
        " # # #  ",
        " #   #  ",
        " #   #  ",
        " #   #  ",
        " #   #  ",
        "        ",
    }},
    {'N', {
        " #   #  ",
        " ##  #  ",
        " # # #  ",
        " #  ##  ",
        " #   #  ",
        " #   #  ",
        " #   #  ",
        "        ",
    }},
    {'O', {
        "  ###   ",
        " #   #  ",
        " #   #  ",
        " #   #  ",
        " #   #  ",
        " #   #  ",
        "  ###   ",
        "        ",
    }},
    {'P', {
        " ####   ",
        " #   #  ",
        " #   #  ",
        " ####   ",
        " #      ",
        " #      ",
        " #      ",
        "        ",
    }},
    {'Q', {
        "  ###   ",
        " #   #  ",
        " #   #  ",
        " #   #  ",
        " # # #  ",
        " #  #   ",
        "  ## #  ",
        "        ",
    }},
    {'R', {
        " ####   ",
        " #   #  ",
        " #   #  ",
        " ####   ",
        " # #    ",
        " #  #   ",
        " #   #  ",
        "        ",
    }},
    {'S', {
        "  ####  ",
        " #      ",
        " #      ",
        "  ###   ",
        "     #  ",
        "     #  ",
        " ####   ",
        "        ",
    }},
    {'T', {
        " #####  ",
        "   #    ",
        "   #    ",
        "   #    ",
        "   #    ",
        "   #    ",
        "   #    ",
        "        ",
    }},
    {'U', {
        " #   #  ",
        " #   #  ",
        " #   #  ",
        " #   #  ",
        " #   #  ",
        " #   #  ",
        "  ###   ",
        "        ",
    }},
    {'V', {
        " #   #  ",
        " #   #  ",
        " #   #  ",
        " #   #  ",
        " #   #  ",
        "  # #   ",
        "   #    ",
        "        ",
    }},
    {'W', {
        " #   #  ",
        " #   #  ",
        " #   #  ",
        " # # #  ",
        " # # #  ",
        " ## ##  ",
        " #   #  ",
        "        ",
    }},
    {'X', {
        " #   #  ",
        " #   #  ",
        "  # #   ",
        "   #    ",
        "  # #   ",
        " #   #  ",
        " #   #  ",
        "        ",
    }},
    {'Y', {
        " #   #  ",
        " #   #  ",
        "  # #   ",
        "   #    ",
        "   #    ",
        "   #    ",
        "   #    ",
        "        ",
    }},
    {'Z', {
        " #####  ",
        "     #  ",
        "    #   ",
        "   #    ",
        "  #     ",
        " #      ",
        " #####  ",
        "        ",
    }},
};
// clang-format on

constexpr int32_t kDefinedGlyphCount =
    static_cast<int32_t>(sizeof(kGlyphArt) / sizeof(kGlyphArt[0]));
static_assert(kDefinedGlyphCount == kGlyphCount,
              "kGlyphCount must match the number of GlyphArt entries");

// One slot past the defined glyphs: the placeholder ("tofu") glyph for
// any character outside the defined set.
constexpr int32_t kTofuSlot = kDefinedGlyphCount;
constexpr int32_t kTotalSlots = kDefinedGlyphCount + 1;
constexpr int32_t kAtlasPixelCount = kTotalSlots * kGlyphWidth * kGlyphHeight;

constexpr Color kOnMarker = Color::BRIGHT_ORANGE;
constexpr Color kOffMarker = Color::BLACK;

// Reading `row[kGlyphWidth]` here also doubles as the width check: a row
// string literal shorter than kGlyphWidth characters makes this an
// out-of-bounds read, which constant evaluation itself rejects as not a
// constant expression -- the row-too-short case fails to compile without
// any explicit check.
constexpr bool rowIsExactWidth(const char* row) { return row[kGlyphWidth] == '\0'; }

// Throwing here is what turns "bad glyph art" into a compile error: a
// throw-expression is never a constant expression, so a build that hits
// this line fails instead of silently baking in wrong pixels.
constexpr Color glyphPixel(const char* row, int32_t col) {
  if (!rowIsExactWidth(row)) {
    throw "glyph row is not exactly kGlyphWidth characters wide";
  }
  if (row[col] == '#') return kOnMarker;
  if (row[col] == ' ') return kOffMarker;
  throw "glyph row contains a character that is neither ' ' nor '#'";
}

constexpr std::array<Color, kAtlasPixelCount> buildAtlas() {
  std::array<Color, kAtlasPixelCount> atlas{};
  int32_t offset = 0;
  for (const GlyphArt& g : kGlyphArt) {
    for (int32_t row = 0; row < kGlyphHeight; ++row) {
      for (int32_t col = 0; col < kGlyphWidth; ++col) {
        atlas[static_cast<size_t>(offset++)] = glyphPixel(g.rows[row], col);
      }
    }
  }
  // Tofu: a checkerboard, unambiguous as "undefined" rather than
  // resembling any real glyph.
  for (int32_t row = 0; row < kGlyphHeight; ++row) {
    for (int32_t col = 0; col < kGlyphWidth; ++col) {
      const bool on = (row + col) % 2 == 0;
      atlas[static_cast<size_t>(offset++)] = on ? kOnMarker : kOffMarker;
    }
  }
  return atlas;
}

constexpr std::array<Color, kAtlasPixelCount> kAtlas = buildAtlas();

// Pins the atlas to the exact byte budget documented in font.h -- a
// direct check on the memory this module actually costs, not just on
// the glyph count that formula is derived from.
static_assert(sizeof(kAtlas) ==
                  static_cast<size_t>(kGlyphCount + 1) *
                      static_cast<size_t>(kGlyphWidth) *
                      static_cast<size_t>(kGlyphHeight),
              "kAtlas size must match (kGlyphCount + 1) glyph cells "
              "(defined glyphs plus the tofu placeholder)");

constexpr std::array<int8_t, 256> buildCharToSlot() {
  std::array<int8_t, 256> table{};
  for (int8_t& slot : table) slot = static_cast<int8_t>(kTofuSlot);
  for (int32_t slot = 0; slot < kDefinedGlyphCount; ++slot) {
    const auto index = static_cast<size_t>(
        static_cast<unsigned char>(kGlyphArt[slot].ch));
    table[index] = static_cast<int8_t>(slot);
  }
  return table;
}

constexpr std::array<int8_t, 256> kCharToSlot = buildCharToSlot();

}  // namespace

Sprite glyphFor(char ch) {
  const int32_t slot = kCharToSlot[static_cast<unsigned char>(ch)];
  const Color* pixels = &kAtlas[static_cast<size_t>(slot * kGlyphWidth * kGlyphHeight)];
  return Sprite{pixels, kGlyphWidth, kGlyphHeight, kGlyphWidth};
}

char glyphCharAt(int32_t index) {
  if (index < 0 || index >= kGlyphCount) return '\0';
  return kGlyphArt[index].ch;
}

}  // namespace steamcore
