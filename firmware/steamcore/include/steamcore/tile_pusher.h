#pragma once

#include <cstddef>
#include <cstdint>

#include "steamcore/config.h"
#include "steamcore/dirty_tracker.h"
#include "steamcore/framebuffer.h"
#include "steamcore/panel_format.h"

// The pure/impure seam (display-driver spec US-3/US-4, plan §1 Decision
// 3): TilePusher owns the whole scan -> expand -> transmit ->
// commit-only-transferred loop, parameterized on a Transmitter type
// rather than a virtual interface -- the same compile-time-binding
// rationale GameLoop<Game> already established for this codebase (no
// vtable, nothing virtual exists anywhere in it).
//
// A Transmitter must provide:
//   bool transmitTile(const PanelWindow& window, const uint8_t* bytes,
//                      size_t count);
// returning true iff the tile actually transferred. Host tests
// instantiate TilePusher with a fake Transmitter that fails chosen
// tiles, proving the retry/commit contract without any ESP-IDF
// dependency; steamcore::port::esp32::Ili9488Display satisfies the same
// concept for the real on-device driver.
//
// Single-threaded, nothing throws, no error code -- same inherited
// contract as every other steamcore type. No dynamic allocation: one
// fixed tile-sized buffer, nothing else (AC-3.4). Deterministic: push()
// reads no clock and no RNG (NFR-5) -- the tiles it sends are a pure
// function of `fb`'s current pixels and `tracker`'s prior comparison
// buffer, nothing else.
//
// Example (steamcore::port::esp32::Ili9488Display::pushDirty() is the
// real caller):
//   TilePusher<MyTransmitter> pusher;
//   MyTransmitter transmitter(spiDevice);
//   const PushResult result = pusher.push(fb, tracker, transmitter);
//   // result.sent tiles are now clean; result.failed tiles stay dirty
//   // and will be retried on the next push() call.

namespace steamcore {

struct PushResult {
  int32_t sent = 0;
  int32_t failed = 0;
};

template <typename Transmitter>
class TilePusher {
 public:
  // Scans `fb` via `tracker`, expands and transmits each dirty tile
  // through `transmitter`, then commits to `tracker` only the tiles that
  // actually transferred -- a tile whose transmitTile() call returns
  // false stays dirty for the next push (US-4, AC-4.1/AC-4.2). Never
  // aborts: a transmitter that fails every tile still returns normally
  // with PushResult{0, <tiles attempted>} (AC-4.3).
  PushResult push(const Framebuffer& fb, DirtyTracker& tracker,
                   Transmitter& transmitter) {
    const TileMask dirty = tracker.scan(fb);
    TileMask transferred;
    PushResult result;

    for (int32_t row = 0; row < kTileRows; ++row) {
      for (int32_t col = 0; col < kTileCols; ++col) {
        if (!dirty.test(col, row)) continue;

        expandTile(fb, col, row, buffer_);
        const PanelWindow window = tileWindow(col, row);
        if (transmitter.transmitTile(window, buffer_, kPanelTileBytes)) {
          transferred.set(col, row);
          ++result.sent;
        } else {
          ++result.failed;
        }
      }
    }

    tracker.commit(fb, transferred);
    return result;
  }

  // Diagnostics only (e.g. a one-time DMA-capability check at driver
  // init, plan T6 Risk 2) -- not part of the scan/push/commit contract
  // and never read by this class itself.
  const uint8_t* buffer() const { return buffer_; }

 private:
  // "Exactly one fixed tile-sized buffer and nothing else" (AC-3.4) is
  // asserted on the whole class, from tile_pusher_test.cpp, where a
  // concrete Transmitter makes the type complete -- a same-file
  // static_assert on sizeof(buffer_) here would be a tautology (buffer_
  // is *declared* this size two lines up; review round-1 F9).
  uint8_t buffer_[kPanelTileBytes];
};

}  // namespace steamcore
