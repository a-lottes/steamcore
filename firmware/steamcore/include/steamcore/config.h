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

}  // namespace steamcore
