#pragma once

#include <cstdint>

// Virtual resolution and tile geometry as compile-time constants
// (constitution §3). No other file in src/ or include/ may spell out
// 240, 160, 480, 320 or 16 — see `make lint`.

namespace steamcore {

inline constexpr int32_t kScreenWidth = 240;
inline constexpr int32_t kScreenHeight = 160;

inline constexpr int32_t kTileSize = 16;
inline constexpr int32_t kTileCols = kScreenWidth / kTileSize;
inline constexpr int32_t kTileRows = kScreenHeight / kTileSize;
inline constexpr int32_t kTileCount = kTileCols * kTileRows;

static_assert(kScreenWidth % kTileSize == 0,
              "screen width must divide evenly by the tile size");
static_assert(kScreenHeight % kTileSize == 0,
              "screen height must divide evenly by the tile size");
static_assert(kTileCount == 150,
              "240x160 at 16x16 tiles must be exactly 150 tiles");

// The real ILI9488 panel's landscape geometry, derived from the virtual
// resolution above, never spelled out a second time (display-driver plan
// §1 Decision 5). No literal 480, 320 or 32 appears outside this file or
// the static_asserts below that pin them.
inline constexpr int32_t kPanelScale = 2;
inline constexpr int32_t kPanelWidth = kScreenWidth * kPanelScale;
inline constexpr int32_t kPanelHeight = kScreenHeight * kPanelScale;
inline constexpr int32_t kPanelTileSize = kTileSize * kPanelScale;
inline constexpr int32_t kPanelBytesPerPixel = 3;  // 18bpp/SPI wire format
inline constexpr int32_t kPanelTileBytes =
    kPanelTileSize * kPanelTileSize * kPanelBytesPerPixel;

static_assert(kPanelWidth == 480,
              "panel width must be the confirmed x2 landscape mapping");
static_assert(kPanelHeight == 320,
              "panel height must be the confirmed x2 landscape mapping");
static_assert(kPanelTileSize == 32,
              "a 16x16 engine tile at x2 scale must be 32x32 panel pixels");
static_assert(kPanelTileBytes == 3072,
              "one panel tile must be exactly 3072 18bpp wire bytes");

}  // namespace steamcore
