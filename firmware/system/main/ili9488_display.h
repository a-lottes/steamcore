#pragma once

#include <cstdint>

// Minimal, write-only ILI9488 bring-up driver.
//
// SPIKE: this exists to verify constitution §3's unverified assumption --
// "over SPI this controller cannot take RGB565, only 18bpp/3 bytes per
// pixel" -- against the real KMRTM35018-SPI panel. It is deliberately not
// the project's real display driver: no dirty tiles, no DMA, no
// Framebuffer integration, no test coverage. Once the pixel format is
// confirmed, the real driver is a planned `/spark` increment, not a
// growth of this file.
namespace steamcore::bringup {

// Resets and initializes the panel: software reset, sleep-out, 18bpp pixel
// format (COLMOD = 0x66), landscape memory access control, display on.
void ili9488Init();

// Fills the whole visible panel with one solid RGB888-ish color. Each
// channel's top 6 bits are what the controller keeps in 18bpp mode; the
// bottom 2 bits of each byte are ignored by the panel, so any 0-255 value
// works as input.
void ili9488FillColor(uint8_t r, uint8_t g, uint8_t b);

}  // namespace steamcore::bringup
