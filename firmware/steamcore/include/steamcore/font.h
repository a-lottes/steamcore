#pragma once

#include <cstdint>

#include "steamcore/sprite.h"

// The one 8x8 monospace bitmap font this engine has. Glyph pixel data is
// authored as ASCII art in font.cpp and validated at compile time — see
// that file for the shape rules behind the punctuation glyphs.

namespace steamcore {

inline constexpr int32_t kGlyphWidth = 8;
inline constexpr int32_t kGlyphHeight = 8;
inline constexpr int32_t kGlyphAdvance = 8;
// The final, complete set (T6): space, !, -, ., 0-9, :, >, ?, A-Z.
inline constexpr int32_t kGlyphCount = 43;

// Static memory budget (font.cpp), no runtime allocation:
//   atlas:      2,752 bytes (43 defined glyphs x 64 pixels)
//             +    64 bytes (1 tofu placeholder glyph)
//             = 2,816 bytes, pinned by a static_assert on kAtlas's size.
//   char->slot lookup table: 256 bytes (one entry per possible `char`).
//   Total: 3,072 bytes of .rodata.

// Returns the Sprite for `ch` (stride == width, tightly packed). `ch` not
// among the kGlyphCount defined characters -- including any negative
// char value -- returns the placeholder "tofu" glyph instead of failing.
Sprite glyphFor(char ch);

// Returns the character stored at font storage slot `index`, for
// index in [0, kGlyphCount) -- or '\0' (never one of the kGlyphCount
// defined characters) for any other index, including negative ones.
// Never undefined behaviour, matching every other public entry point in
// this engine (Framebuffer, Sprite, drawText): invalid input is always
// substituted with a safe, deterministic value, never UB (review F6).
// Exists so a caller (the text-rendering test suite) can derive
// properties from the font's own storage order instead of restating
// that order as prose.
char glyphCharAt(int32_t index);

}  // namespace steamcore
