#include "steamcore/text.h"

#include "steamcore/font.h"
#include "steamcore/sprite.h"

namespace steamcore {

namespace {
constexpr int32_t kCellPixelCount = kGlyphWidth * kGlyphHeight;
}  // namespace

void drawText(Framebuffer& fb, int32_t x, int32_t y, const char* text, Color ink) {
  if (text == nullptr) return;

  // The atlas encodes "on" as BRIGHT_ORANGE and "off" as BLACK (font.cpp).
  // blit's transparent parameter must differ from `ink`, or nothing would
  // ever be skipped -- if the caller wants BLACK ink, BRIGHT_ORANGE
  // becomes the transparent key instead, so black text draws real black
  // pixels rather than vanishing.
  const Color key = (ink == Color::BLACK) ? Color::BRIGHT_ORANGE : Color::BLACK;

  // Computed in int64_t: a glyph origin can walk arbitrarily far from a
  // caller-supplied extreme x without ever overflowing the int32_t that
  // blit() takes. Row visibility is a single check outside the loop
  // since drawText never wraps lines.
  const int64_t y64 = y;
  const bool rowVisible = (y64 + kGlyphHeight > 0) && (y64 < Framebuffer::height());

  int64_t gx = x;
  for (const char* p = text; *p != '\0'; ++p, gx += kGlyphAdvance) {
    if (!rowVisible) continue;
    // Entirely off-screen horizontally: skip without ever casting `gx`
    // down to int32_t, which would be unsafe for an extreme x.
    if (gx + kGlyphWidth <= 0 || gx >= Framebuffer::width()) continue;

    const Sprite glyph = glyphFor(*p);

    Color cell[kCellPixelCount];
    for (int32_t row = 0; row < kGlyphHeight; ++row) {
      for (int32_t col = 0; col < kGlyphWidth; ++col) {
        const Color srcPixel = glyph.pixels[row * glyph.stride + col];
        cell[row * kGlyphWidth + col] = (srcPixel == Color::BLACK) ? key : ink;
      }
    }

    // Past the check above, gx is within (-kGlyphWidth, Framebuffer::width()),
    // always safely representable as int32_t.
    const Sprite cellSprite{cell, kGlyphWidth, kGlyphHeight, kGlyphWidth};
    fb.blit(cellSprite, static_cast<int32_t>(gx), y, key);
  }
}

}  // namespace steamcore
