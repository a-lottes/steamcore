#include "steamcore/dirty_tracker.h"

#include <cstring>

namespace steamcore {

void TileMask::set(int32_t col, int32_t row) {
  const int32_t bit = bitIndex(col, row);
  words_[bit / 32] |= (1u << (bit % 32));
}

bool TileMask::test(int32_t col, int32_t row) const {
  const int32_t bit = bitIndex(col, row);
  return (words_[bit / 32] & (1u << (bit % 32))) != 0;
}

namespace {
// Not a valid Color (which only spans 0-3): every pixel compares unequal
// to this on the first scan, so all kTileCount tiles come back dirty
// without a dedicated "first scan" flag (AC-4.8).
constexpr uint8_t kSentinelByte = 0xFF;
}  // namespace

DirtyTracker::DirtyTracker() {
  std::memset(comparison_, kSentinelByte, sizeof(comparison_));
}

TileMask DirtyTracker::scan(const Framebuffer& fb) const {
  TileMask mask;
  for (int32_t tileRow = 0; tileRow < kTileRows; ++tileRow) {
    const int32_t y0 = tileRow * kTileSize;
    for (int32_t tileCol = 0; tileCol < kTileCols; ++tileCol) {
      const int32_t x0 = tileCol * kTileSize;
      bool dirty = false;
      for (int32_t y = y0; y < y0 + kTileSize && !dirty; ++y) {
        const uint8_t* compareRow = comparison_ + y * kScreenWidth;
        for (int32_t x = x0; x < x0 + kTileSize; ++x) {
          if (static_cast<uint8_t>(fb.pixel(x, y)) != compareRow[x]) {
            dirty = true;
            break;
          }
        }
      }
      if (dirty) mask.set(tileCol, tileRow);
    }
  }
  return mask;
}

void DirtyTracker::commit(const Framebuffer& fb, const TileMask& tiles) {
  for (int32_t tileRow = 0; tileRow < kTileRows; ++tileRow) {
    const int32_t y0 = tileRow * kTileSize;
    for (int32_t tileCol = 0; tileCol < kTileCols; ++tileCol) {
      if (!tiles.test(tileCol, tileRow)) continue;
      const int32_t x0 = tileCol * kTileSize;
      for (int32_t y = y0; y < y0 + kTileSize; ++y) {
        uint8_t* compareRow = comparison_ + y * kScreenWidth;
        for (int32_t x = x0; x < x0 + kTileSize; ++x) {
          compareRow[x] = static_cast<uint8_t>(fb.pixel(x, y));
        }
      }
    }
  }
}

}  // namespace steamcore
