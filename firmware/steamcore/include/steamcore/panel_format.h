#pragma once

#include <cstdint>

#include "steamcore/color.h"
#include "steamcore/config.h"
#include "steamcore/framebuffer.h"

// Pure, host-testable pixel-format and coordinate-mapping math for the
// real ILI9488 panel (display-driver spec US-1/US-2). Converts the
// engine's 4-colour palette into the panel's 18bpp/3-bytes-per-pixel SPI
// wire format, and maps each of DirtyTracker's 150 tile positions to its
// exact destination window on the real 480x320 landscape panel.
//
// Zero ESP-IDF headers here or in the .cpp -- this is exactly the half of
// the display driver constitution §4's host gate can still see (plan §1
// Decision 1). The code that actually talks to the panel over SPI lives
// under firmware/steamcore/port/esp32/, outside this gate.
//
// Example (what TilePusher does with these, one tile at a time --
// tile_pusher.h's push() is the real caller):
//   const PanelWindow window = tileWindow(col, row);
//   uint8_t bytes[kPanelTileBytes];
//   expandTile(fb, col, row, bytes);
//   transmitter.transmitTile(window, bytes, kPanelTileBytes);
//
// Contract: every function here is pure, nothing throws, no error code,
// no dynamic allocation -- same inherited contract as every other
// steamcore type. toPanelPixel()/tileWindow() are deterministic (AC-1.3,
// NFR-5): the same input always produces the same output, no clock, no
// RNG.

namespace steamcore {

// One pixel in the panel's wire format: 3 bytes, R/G/B. In 18bpp mode
// (COLMOD=0x66) the controller keeps only each byte's top 6 bits and
// ignores the bottom 2 -- so a raw 8-bit channel value is sent as-is,
// truncation and all; these are exactly framebuffer-viewer's own
// already-approved RGB values (docs/dump-format.md), not a new mapping.
struct PanelPixel {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
};

// A rectangular column/row-address window on the panel, in panel pixel
// coordinates, both ends inclusive -- exactly what the ILI9488's Column
// Address Set / Page Address Set commands expect.
struct PanelWindow {
  int32_t x0 = 0;
  int32_t y0 = 0;
  int32_t x1 = 0;
  int32_t y1 = 0;
};

// Converts one engine palette colour into the panel's 18bpp wire bytes.
// Pure and deterministic: the same Color always produces the same
// PanelPixel (AC-1.3).
PanelPixel toPanelPixel(Color color);

// Maps engine tile (col, row) -- as DirtyTracker/TileMask address it --
// to its exact kPanelTileSize x kPanelTileSize destination window on the
// real panel. `col` must be in [0, kTileCols) and `row` in [0, kTileRows);
// this is an internal type driven only by the display driver, mirroring
// TileMask's own no-clip precondition (dirty_tracker.h) -- callers are
// the engine itself, not an external-input boundary.
PanelWindow tileWindow(int32_t col, int32_t row);

// Expands one engine tile's kTileSize x kTileSize pixels (read from `fb`)
// into `out`, which must have room for exactly kPanelTileBytes bytes:
// row-major panel order, each source pixel replicated into a
// kPanelScale x kPanelScale block of wire bytes. `col`/`row` are the same
// tile coordinates tileWindow() takes, and carry the same precondition.
void expandTile(const Framebuffer& fb, int32_t col, int32_t row,
                 uint8_t* out);

}  // namespace steamcore
