#pragma once

#include <cstdint>

#include "steamcore/color.h"
#include "steamcore/framebuffer.h"
#include "steamcore/sprite.h"

// The project's one shared visual-fixture pattern (docs/dump-format.md
// "Anchor table"), extracted from dump_format_test.cpp so the host test
// and the on-device display-driver harness (display-driver spec §6, T7)
// draw and check exactly the same pixels -- one definition, not two
// patterns that could silently drift apart. Test-only: not part of the
// engine's public surface.

namespace steamcore::test {

// The "F" glyph, 6 wide x 8 tall, no symmetry axis in either direction --
// deliberately so a horizontal flip, vertical flip or transpose changes
// it (docs/dump-format.md "Anchor table"). X = BRIGHT_ORANGE, . = BLACK
// (transparent under the default blit colour).
//
//   XXXXXX
//   X.....
//   X.....
//   XXXX..
//   X.....
//   X.....
//   X.....
//   X.....
inline constexpr int32_t kFixtureGlyphWidth = 6;
inline constexpr int32_t kFixtureGlyphHeight = 8;
inline constexpr Color kFixtureGlyphF[kFixtureGlyphWidth * kFixtureGlyphHeight] = {
    Color::BRIGHT_ORANGE, Color::BRIGHT_ORANGE, Color::BRIGHT_ORANGE,
    Color::BRIGHT_ORANGE, Color::BRIGHT_ORANGE, Color::BRIGHT_ORANGE,
    Color::BRIGHT_ORANGE, Color::BLACK,         Color::BLACK,
    Color::BLACK,         Color::BLACK,         Color::BLACK,
    Color::BRIGHT_ORANGE, Color::BLACK,         Color::BLACK,
    Color::BLACK,         Color::BLACK,         Color::BLACK,
    Color::BRIGHT_ORANGE, Color::BRIGHT_ORANGE, Color::BRIGHT_ORANGE,
    Color::BRIGHT_ORANGE, Color::BLACK,         Color::BLACK,
    Color::BRIGHT_ORANGE, Color::BLACK,         Color::BLACK,
    Color::BLACK,         Color::BLACK,         Color::BLACK,
    Color::BRIGHT_ORANGE, Color::BLACK,         Color::BLACK,
    Color::BLACK,         Color::BLACK,         Color::BLACK,
    Color::BRIGHT_ORANGE, Color::BLACK,         Color::BLACK,
    Color::BLACK,         Color::BLACK,         Color::BLACK,
    Color::BRIGHT_ORANGE, Color::BLACK,         Color::BLACK,
    Color::BLACK,         Color::BLACK,         Color::BLACK,
};

// The real fixture pattern: every element chosen so a flip, rotation,
// transposition, swapped width/height or a stride/row-length error
// changes at least one anchor value. See kFixtureAnchors below for the
// exact (x, y) -> colour truths this draws, asserted both by the host
// test and by the Python round-trip test.
inline void drawFixturePattern(Framebuffer& fb) {
  fb.clear(Color::BLACK);

  // Two unequal, overlapping rects: B is drawn after A, so their overlap
  // is ORANGE. Neither is square, so a row/column transposition changes
  // which pixels fall inside which rect.
  fb.fillRect(20, 20, 60, 40, Color::DARK_ORANGE);  // x[20,80) y[20,60)
  fb.fillRect(50, 40, 40, 70, Color::ORANGE);        // x[50,90) y[40,110)

  // Asymmetric glyph, blitted (not filled) so the pattern also exercises
  // Sprite::blit, not just fillRect.
  const Sprite glyph{kFixtureGlyphF, kFixtureGlyphWidth, kFixtureGlyphHeight,
                      kFixtureGlyphWidth};
  fb.blit(glyph, 150, 20);

  // A one-pixel-wide line spanning only the top third of the screen --
  // catches a stride/row-length error a filled region would hide.
  fb.fillRect(200, 0, 1, 53, Color::DARK_ORANGE);

  // Four corners, four different colours (BLACK is the untouched
  // background at the fourth corner, deliberately, not a fifth colour).
  fb.setPixel(0, 0, Color::DARK_ORANGE);
  fb.setPixel(Framebuffer::width() - 1, 0, Color::ORANGE);
  fb.setPixel(0, Framebuffer::height() - 1, Color::BRIGHT_ORANGE);
  // Bottom-right corner (width-1, height-1) is left as background BLACK.
}

struct FixtureAnchor {
  int32_t x;
  int32_t y;
  Color expected;
};

// Same table as docs/dump-format.md "Anchor table" -- keep both in sync
// by hand; this is the one place the C++ side states it in code.
inline constexpr FixtureAnchor kFixtureAnchors[] = {
    {5, 5, Color::BLACK},               // untouched background
    {25, 25, Color::DARK_ORANGE},       // inside rect A only
    {60, 50, Color::ORANGE},            // A/B overlap -- B drawn last, wins
    {85, 90, Color::ORANGE},            // inside rect B only
    {150, 20, Color::BRIGHT_ORANGE},    // glyph top-left, an "on" pixel
    {151, 21, Color::BLACK},            // glyph interior, a transparent hole
    {200, 10, Color::DARK_ORANGE},      // on the line
    {200, 100, Color::BLACK},           // below the line's span
    {0, 0, Color::DARK_ORANGE},         // corner: top-left
    {239, 0, Color::ORANGE},            // corner: top-right
    {0, 159, Color::BRIGHT_ORANGE},     // corner: bottom-left
    {239, 159, Color::BLACK},           // corner: bottom-right (untouched)
};

}  // namespace steamcore::test
