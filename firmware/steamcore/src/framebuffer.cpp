#include "steamcore/framebuffer.h"

#include <cstring>

#include "clip.h"

namespace steamcore {

Framebuffer::Framebuffer() { clear(Color::BLACK); }

void Framebuffer::clear(Color color) {
  std::memset(pixels_, static_cast<uint8_t>(color), sizeof(pixels_));
}

void Framebuffer::setPixel(int32_t x, int32_t y, Color color) {
  if (!detail::pixelInBounds(x, y, kScreenWidth, kScreenHeight)) return;
  pixels_[y * kScreenWidth + x] = static_cast<uint8_t>(color);
}

Color Framebuffer::pixel(int32_t x, int32_t y) const {
  if (!detail::pixelInBounds(x, y, kScreenWidth, kScreenHeight)) {
    return Color::BLACK;
  }
  return static_cast<Color>(pixels_[y * kScreenWidth + x]);
}

void Framebuffer::fillRect(int32_t x, int32_t y, int32_t w, int32_t h,
                            Color color) {
  const detail::ClippedRect clipped =
      detail::clipRect(x, y, w, h, kScreenWidth, kScreenHeight);
  if (!clipped.visible) return;

  const uint8_t byte = static_cast<uint8_t>(color);
  for (int32_t row = 0; row < clipped.height; ++row) {
    uint8_t* rowStart =
        pixels_ + (clipped.y + row) * kScreenWidth + clipped.x;
    std::memset(rowStart, byte, static_cast<std::size_t>(clipped.width));
  }
}

void Framebuffer::blit(const Sprite& sprite, int32_t x, int32_t y,
                        Color transparent) {
  if (sprite.pixels == nullptr) return;

  const detail::ClippedRect clipped = detail::clipRect(
      x, y, sprite.width, sprite.height, kScreenWidth, kScreenHeight);
  if (!clipped.visible) return;

  // How much the left/top of the sprite was clipped off, in source space.
  // Computed in int64_t, same reasoning as clip.h: `x`/`y` can be any
  // int32_t value independent of how much of the sprite actually lands
  // on-screen, so this offset (and its later multiplication by stride)
  // must not be allowed to overflow a 32-bit int (review F6).
  const int64_t srcOffsetX = static_cast<int64_t>(clipped.x) - x;
  const int64_t srcOffsetY = static_cast<int64_t>(clipped.y) - y;
  const int64_t stride = sprite.stride;

  for (int32_t row = 0; row < clipped.height; ++row) {
    const Color* srcRow = sprite.pixels + (srcOffsetY + row) * stride + srcOffsetX;
    uint8_t* dstRow = pixels_ + (clipped.y + row) * kScreenWidth + clipped.x;
    for (int32_t col = 0; col < clipped.width; ++col) {
      const Color c = srcRow[col];
      if (c == transparent) continue;
      dstRow[col] = static_cast<uint8_t>(c);
    }
  }
}

}  // namespace steamcore
