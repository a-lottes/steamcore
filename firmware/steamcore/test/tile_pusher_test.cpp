#include "steamcore/tile_pusher.h"

#include "steamcore/color.h"
#include "steamcore/config.h"
#include "steamcore/dirty_tracker.h"
#include "steamcore/framebuffer.h"
#include "steamcore/panel_format.h"
#include "test_harness.h"

using steamcore::Color;
using steamcore::DirtyTracker;
using steamcore::Framebuffer;
using steamcore::kTileCols;
using steamcore::kTileCount;
using steamcore::kTileRows;
using steamcore::PanelWindow;
using steamcore::PushResult;
using steamcore::TileMask;
using steamcore::TilePusher;

namespace {

// Fails exactly the tiles named via failTile(); succeeds everything
// else. Recovers (col, row) from the window it's handed -- tileWindow()
// is a pure, exact function of (col, row), so this reconstruction is
// exact, not a heuristic.
class FakeTransmitter {
 public:
  void failTile(int32_t col, int32_t row) { failing_.set(col, row); }

  bool transmitTile(const PanelWindow& window, const uint8_t*, size_t) {
    const int32_t col = window.x0 / steamcore::kPanelTileSize;
    const int32_t row = window.y0 / steamcore::kPanelTileSize;
    attempted_.set(col, row);
    return !failing_.test(col, row);
  }

  bool wasAttempted(int32_t col, int32_t row) const {
    return attempted_.test(col, row);
  }

 private:
  TileMask failing_;
  TileMask attempted_;
};

// AC-3.4: TilePusher holds exactly one fixed tile-sized buffer and
// nothing else. Asserted here, not inside tile_pusher.h, because the
// class is templated -- a same-file assert on the buffer_ member's own
// declared size would be a tautology; this is the whole object's size,
// checked against a concrete instantiation (review round-1 F9).
static_assert(sizeof(TilePusher<FakeTransmitter>) ==
                  static_cast<size_t>(steamcore::kPanelTileBytes),
              "TilePusher<FakeTransmitter> must be exactly one "
              "tile-sized buffer, nothing else");

class AlwaysFailTransmitter {
 public:
  bool transmitTile(const PanelWindow&, const uint8_t*, size_t) {
    return false;
  }
};

int32_t countDirty(const TileMask& mask) {
  int32_t count = 0;
  for (int32_t row = 0; row < kTileRows; ++row) {
    for (int32_t col = 0; col < kTileCols; ++col) {
      if (mask.test(col, row)) ++count;
    }
  }
  return count;
}

}  // namespace

// A fresh DirtyTracker's first scan reports every tile dirty
// (dirty_tracker.h) -- an all-succeeding transmitter must commit all 150
// and leave the next scan clean.
STEAMCORE_TEST(tile_pusher_all_success_commits_everything) {
  Framebuffer fb;
  fb.clear(Color::ORANGE);
  DirtyTracker tracker;
  TilePusher<FakeTransmitter> pusher;
  FakeTransmitter tx;

  const PushResult result = pusher.push(fb, tracker, tx);
  CHECK_EQ(result.sent, kTileCount);
  CHECK_EQ(result.failed, 0);

  CHECK_EQ(countDirty(tracker.scan(fb)), 0);
}

// AC-4.1: only the tiles that actually transferred are committed -- the
// one failed tile stays dirty, every other tile is clean.
STEAMCORE_TEST(tile_pusher_failed_tile_stays_dirty_others_clean) {
  Framebuffer fb;
  fb.clear(Color::ORANGE);
  DirtyTracker tracker;
  TilePusher<FakeTransmitter> pusher;
  FakeTransmitter tx;
  constexpr int32_t kFailCol = 5;
  constexpr int32_t kFailRow = 3;
  tx.failTile(kFailCol, kFailRow);

  const PushResult result = pusher.push(fb, tracker, tx);
  CHECK_EQ(result.sent, kTileCount - 1);
  CHECK_EQ(result.failed, 1);

  const TileMask stillDirty = tracker.scan(fb);
  CHECK_EQ(countDirty(stillDirty), 1);
  CHECK(stillDirty.test(kFailCol, kFailRow));
}

// AC-4.2: the previously-failed tile is attempted again on the next
// push, never permanently skipped.
STEAMCORE_TEST(tile_pusher_retries_previously_failed_tile) {
  Framebuffer fb;
  fb.clear(Color::ORANGE);
  DirtyTracker tracker;
  TilePusher<FakeTransmitter> pusher;
  constexpr int32_t kFailCol = 5;
  constexpr int32_t kFailRow = 3;

  FakeTransmitter first;
  first.failTile(kFailCol, kFailRow);
  pusher.push(fb, tracker, first);  // 149 sent, tile (5,3) stays dirty

  FakeTransmitter second;
  second.failTile(kFailCol, kFailRow);
  const PushResult result = pusher.push(fb, tracker, second);

  CHECK(second.wasAttempted(kFailCol, kFailRow));
  CHECK_EQ(result.failed, 1);
  CHECK_EQ(result.sent, 0);  // only tile (5,3) was still dirty to attempt
}

// AC-4.3: a transmitter that fails every tile still returns normally --
// proven by simply reaching the assertions below. An abort would
// terminate the whole test binary, not just fail one CHECK.
STEAMCORE_TEST(tile_pusher_all_failure_returns_normally_no_abort) {
  Framebuffer fb;
  fb.clear(Color::ORANGE);
  DirtyTracker tracker;
  TilePusher<AlwaysFailTransmitter> pusher;
  AlwaysFailTransmitter tx;

  const PushResult result = pusher.push(fb, tracker, tx);
  CHECK_EQ(result.sent, 0);
  CHECK_EQ(result.failed, kTileCount);
  CHECK_EQ(countDirty(tracker.scan(fb)), kTileCount);
}

// A tile that fails once and then succeeds on a later push is cleared --
// the retry contract actually recovers, not just re-attempts forever.
STEAMCORE_TEST(tile_pusher_succeeding_retry_clears_the_tile) {
  Framebuffer fb;
  fb.clear(Color::ORANGE);
  DirtyTracker tracker;
  TilePusher<FakeTransmitter> pusher;
  constexpr int32_t kFailCol = 5;
  constexpr int32_t kFailRow = 3;

  FakeTransmitter failing;
  failing.failTile(kFailCol, kFailRow);
  pusher.push(fb, tracker, failing);

  FakeTransmitter succeeding;  // fails nothing
  pusher.push(fb, tracker, succeeding);

  CHECK_EQ(countDirty(tracker.scan(fb)), 0);
}
