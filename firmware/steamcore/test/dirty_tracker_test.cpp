#include "steamcore/dirty_tracker.h"
#include "steamcore/framebuffer.h"
#include "test_harness.h"

using steamcore::Color;
using steamcore::DirtyTracker;
using steamcore::Framebuffer;
using steamcore::kScreenHeight;
using steamcore::kScreenWidth;
using steamcore::kTileCols;
using steamcore::kTileRows;
using steamcore::kTileSize;
using steamcore::TileMask;

namespace {

int32_t countDirty(const TileMask& mask) {
  int32_t n = 0;
  for (int32_t row = 0; row < kTileRows; ++row) {
    for (int32_t col = 0; col < kTileCols; ++col) {
      if (mask.test(col, row)) ++n;
    }
  }
  return n;
}

// Every tile in `mask` set, matching a full-buffer commit or a first scan.
TileMask allTiles() {
  TileMask mask;
  for (int32_t row = 0; row < kTileRows; ++row) {
    for (int32_t col = 0; col < kTileCols; ++col) mask.set(col, row);
  }
  return mask;
}

}  // namespace

// --- AC-4.8: first scan on a fresh tracker reports everything dirty ---

STEAMCORE_TEST(ac_4_8_first_scan_reports_all_tiles_dirty) {
  DirtyTracker tracker;
  Framebuffer fb;  // fresh, nothing drawn
  const TileMask mask = tracker.scan(fb);
  CHECK_EQ(countDirty(mask), kTileCols * kTileRows);
  CHECK_EQ(countDirty(mask), 150);
}

// --- AC-4.1: identical buffer and comparison state reports zero dirty ---

STEAMCORE_TEST(ac_4_1_synced_buffer_reports_zero_dirty) {
  DirtyTracker tracker;
  Framebuffer fb;
  tracker.commit(fb, tracker.scan(fb));  // sync to the initial BLACK state
  const TileMask mask = tracker.scan(fb);
  CHECK_EQ(countDirty(mask), 0);
}

// --- AC-4.2: exactly one tile dirty for a single changed pixel ---

STEAMCORE_TEST(ac_4_2_single_pixel_at_tile_first_pixel) {
  DirtyTracker tracker;
  Framebuffer fb;
  tracker.commit(fb, tracker.scan(fb));

  fb.setPixel(0, 0, Color::ORANGE);  // first pixel of tile (0, 0)
  const TileMask mask = tracker.scan(fb);

  CHECK_EQ(countDirty(mask), 1);
  CHECK(mask.test(0, 0));
}

STEAMCORE_TEST(ac_4_2_single_pixel_at_tile_last_pixel) {
  DirtyTracker tracker;
  Framebuffer fb;
  tracker.commit(fb, tracker.scan(fb));

  // Last pixel of tile (0, 0), which spans [0,16) x [0,16).
  fb.setPixel(kTileSize - 1, kTileSize - 1, Color::ORANGE);
  const TileMask mask = tracker.scan(fb);

  CHECK_EQ(countDirty(mask), 1);
  CHECK(mask.test(0, 0));
}

// --- AC-4.3: changes in the four corner tiles report exactly those four ---

STEAMCORE_TEST(ac_4_3_four_corner_tiles) {
  DirtyTracker tracker;
  Framebuffer fb;
  tracker.commit(fb, tracker.scan(fb));

  fb.setPixel(0, 0, Color::ORANGE);
  fb.setPixel(kScreenWidth - 1, 0, Color::ORANGE);
  fb.setPixel(0, kScreenHeight - 1, Color::ORANGE);
  fb.setPixel(kScreenWidth - 1, kScreenHeight - 1, Color::ORANGE);

  const TileMask mask = tracker.scan(fb);
  CHECK_EQ(countDirty(mask), 4);
  CHECK(mask.test(0, 0));
  CHECK(mask.test(kTileCols - 1, 0));
  CHECK(mask.test(0, kTileRows - 1));
  CHECK(mask.test(kTileCols - 1, kTileRows - 1));
}

// --- AC-4.4: every pixel changed reports all 150 tiles dirty ---

STEAMCORE_TEST(ac_4_4_every_pixel_changed_reports_all_tiles) {
  DirtyTracker tracker;
  Framebuffer fb;
  tracker.commit(fb, tracker.scan(fb));  // sync to BLACK

  fb.clear(Color::ORANGE);  // every pixel now differs from the comparison
  const TileMask mask = tracker.scan(fb);

  CHECK_EQ(countDirty(mask), 150);
  for (int32_t row = 0; row < kTileRows; ++row) {
    for (int32_t col = 0; col < kTileCols; ++col) {
      CHECK(mask.test(col, row));
    }
  }
}

// Sanity check on the helper itself, used by AC-4.1/4.4 above.
STEAMCORE_TEST(dirty_tracker_all_tiles_helper_has_150_bits_set) {
  CHECK_EQ(countDirty(allTiles()), 150);
}

namespace {

bool tileMasksEqual(const TileMask& a, const TileMask& b) {
  for (int32_t row = 0; row < kTileRows; ++row) {
    for (int32_t col = 0; col < kTileCols; ++col) {
      if (a.test(col, row) != b.test(col, row)) return false;
    }
  }
  return true;
}

}  // namespace

// --- AC-4.5: re-scanning without a commit is idempotent; committing
// everything reported clears it all ---

STEAMCORE_TEST(ac_4_5_rescan_without_commit_reports_identical_set) {
  DirtyTracker tracker;
  Framebuffer fb;

  const TileMask first = tracker.scan(fb);   // first scan: all 150
  const TileMask second = tracker.scan(fb);  // no commit in between
  CHECK(tileMasksEqual(first, second));
  CHECK_EQ(countDirty(first), 150);
}

STEAMCORE_TEST(ac_4_5_committing_everything_reported_clears_it) {
  DirtyTracker tracker;
  Framebuffer fb;

  const TileMask dirty = tracker.scan(fb);
  tracker.commit(fb, dirty);
  const TileMask afterCommit = tracker.scan(fb);
  CHECK_EQ(countDirty(afterCommit), 0);
}

// --- AC-4.7: a partial commit (the failed-transfer case) never silently
// drops the tiles that were not actually transferred ---

STEAMCORE_TEST(ac_4_7_partial_commit_leaves_uncommitted_tiles_dirty) {
  DirtyTracker tracker;
  Framebuffer fb;
  tracker.commit(fb, tracker.scan(fb));  // sync to the initial BLACK state

  // Dirty exactly 10 tiles: row 0, columns 0..9.
  for (int32_t col = 0; col < 10; ++col) {
    fb.setPixel(col * kTileSize, 0, Color::ORANGE);
  }
  const TileMask reported = tracker.scan(fb);
  CHECK_EQ(countDirty(reported), 10);

  // The caller "transferred" only 4 of the 10 reported tiles.
  TileMask transferred;
  for (int32_t col = 0; col < 4; ++col) transferred.set(col, 0);
  tracker.commit(fb, transferred);

  const TileMask afterPartialCommit = tracker.scan(fb);
  CHECK_EQ(countDirty(afterPartialCommit), 6);
  for (int32_t col = 0; col < 10; ++col) {
    const bool expectedDirty = col >= 4;  // 0..3 committed (clean), 4..9 not
    CHECK(afterPartialCommit.test(col, 0) == expectedDirty);
  }
}
