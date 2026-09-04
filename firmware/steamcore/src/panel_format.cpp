#include "steamcore/panel_format.h"

namespace steamcore {

PanelPixel toPanelPixel(Color color) {
  // No `default`: a fifth Color enumerator becomes a -Wswitch build
  // break here, same convention as game_state.cpp's transition table.
  switch (color) {
    case Color::BLACK:
      return PanelPixel{0x00, 0x00, 0x00};
    case Color::DARK_ORANGE:
      return PanelPixel{0x4D, 0x26, 0x00};
    case Color::ORANGE:
      return PanelPixel{0xB3, 0x59, 0x00};
    case Color::BRIGHT_ORANGE:
      return PanelPixel{0xFF, 0x99, 0x33};
  }
  return PanelPixel{};
}

PanelWindow tileWindow(int32_t col, int32_t row) {
  const int32_t x0 = col * kPanelTileSize;
  const int32_t y0 = row * kPanelTileSize;
  return PanelWindow{x0, y0, x0 + kPanelTileSize - 1, y0 + kPanelTileSize - 1};
}

void expandTile(const Framebuffer& fb, int32_t col, int32_t row,
                 uint8_t* out) {
  const int32_t srcX0 = col * kTileSize;
  const int32_t srcY0 = row * kTileSize;

  int32_t index = 0;
  for (int32_t panelY = 0; panelY < kPanelTileSize; ++panelY) {
    const int32_t srcY = srcY0 + panelY / kPanelScale;
    for (int32_t panelX = 0; panelX < kPanelTileSize; ++panelX) {
      const int32_t srcX = srcX0 + panelX / kPanelScale;
      const PanelPixel px = toPanelPixel(fb.pixel(srcX, srcY));
      out[index] = px.r;
      out[index + 1] = px.g;
      out[index + 2] = px.b;
      index += kPanelBytesPerPixel;
    }
  }
}

}  // namespace steamcore
