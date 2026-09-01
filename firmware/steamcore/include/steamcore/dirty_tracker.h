#pragma once

#include <cstdint>

#include "steamcore/config.h"
#include "steamcore/framebuffer.h"

// Example (the future display-driver loop, illustrative — no driver
// exists in this increment):
//   steamcore::DirtyTracker tracker;
//   // ... game draws into fb every frame ...
//   const steamcore::TileMask dirty = tracker.scan(fb);
//   steamcore::TileMask transferred;
//   for (each tile set in dirty)
//     if (pushTileOverSpi(fb, tile)) transferred.set(tile.col, tile.row);
//   tracker.commit(fb, transferred);  // only the tiles that actually sent
//   // A tile that failed to send stays dirty and is retried next frame —
//   // see DirtyTracker::commit below.

namespace steamcore {

// A fixed-size bit field over the 15x10 tile grid (150 bits, 20 bytes):
// exactly the handover a display driver needs, with no allocation, no
// owning container and no callback (constitution §3, AC-4.6). The caller
// reads it directly with test()/set(); a `static_assert` below fails the
// build if the resolution and tile size ever stop dividing evenly, which
// is what keeps this exactly 20 bytes.
class TileMask {
 public:
  TileMask() = default;

  // Marks tile (col, row) as set. `col` must be in [0, kTileCols) and
  // `row` in [0, kTileRows) — this is an internal type driven only by
  // DirtyTracker and the driver story, not a public-input boundary, so it
  // does not clip; callers are the engine itself.
  void set(int32_t col, int32_t row);
  bool test(int32_t col, int32_t row) const;

 private:
  static int32_t bitIndex(int32_t col, int32_t row) {
    return row * kTileCols + col;
  }

  uint32_t words_[5] = {};
};

static_assert(sizeof(TileMask) == 20,
              "TileMask must be exactly the 150-bit field the driver "
              "contract promises");

// Detects which 16x16 tiles changed since the last commit, so a display
// driver can push only what actually changed instead of a full frame
// (constitution §3 — the ILI9488 prototype panel cannot sustain a
// full-frame push at a usable frame rate). Owns its own comparison
// buffer, initialised to a sentinel byte that is not a valid Color — so
// the first scan on a fresh tracker reports every tile dirty as a
// consequence of the ordinary compare rule, with no separate "first
// scan" flag and therefore no second state machine to keep in sync with
// partial commits.
//
// Single-threaded contract, same as Framebuffer: scan/commit and the
// game's drawing calls are assumed to run from the same task; nothing
// here synchronizes concurrent access. Nothing throws and no method
// returns an error code.
//
// Precondition that holds across both methods below: every call to
// scan()/commit() on one DirtyTracker instance must be passed the SAME
// Framebuffer instance — one tracker pairs with one framebuffer for its
// whole lifetime. Passing a different Framebuffer compiles (both take
// `const Framebuffer&`) but silently poisons the comparison buffer: the
// next scan() will compare against pixels that were never the tracked
// buffer's own history, and the driver ends up with stale tiles on the
// panel with no diagnostic.
class DirtyTracker {
 public:
  DirtyTracker();

  // Compares `fb` against the internal comparison buffer and returns the
  // set of tiles that differ. Pure read: calling scan() twice in a row
  // with no commit() in between returns the identical mask, and scanning
  // never mutates the comparison buffer. `fb` must be the same instance
  // on every call — see the class-level precondition above.
  TileMask scan(const Framebuffer& fb) const;

  // For every tile set in `tiles`, copies that tile's current pixels from
  // `fb` into the comparison buffer, so the next scan() reports it clean.
  // A tile NOT set in `tiles` is left exactly as it was — committing only
  // part of what scan() last reported (e.g. because a transfer failed
  // partway through) leaves exactly the uncommitted tiles dirty on the
  // next scan; no tile is ever silently dropped. `tiles` need not be, and
  // usually will not be, the same mask a preceding scan() returned. `fb`
  // must be the same instance on every call — see the class-level
  // precondition above.
  void commit(const Framebuffer& fb, const TileMask& tiles);

 private:
  uint8_t comparison_[kScreenWidth * kScreenHeight];
};

}  // namespace steamcore
