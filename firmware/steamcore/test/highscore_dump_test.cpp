#include <cstdio>

#include "steamcore/color.h"
#include "steamcore/dump_format.h"
#include "steamcore/framebuffer.h"
#include "steamcore/game_loop.h"
#include "steamcore/highscore.h"
#include "steamcore/highscore_screen.h"
#include "steamcore/initials_entry.h"
#include "test_harness.h"

#ifndef STEAMCORE_ENTRY_DUMP
#error "STEAMCORE_ENTRY_DUMP must be defined by the build (see Makefile)"
#endif
#ifndef STEAMCORE_TABLE_DUMP
#error "STEAMCORE_TABLE_DUMP must be defined by the build (see Makefile)"
#endif

using steamcore::Color;
using steamcore::Framebuffer;
using steamcore::GameInput;
using steamcore::HighscoreTable;
using steamcore::InitialsEntry;
using steamcore::count;
using steamcore::drawHighscoreTableScreen;
using steamcore::drawInitialsEntryScreen;
using steamcore::detail::insert;
using steamcore::kDumpHeaderSize;
using steamcore::kInitialsCount;
using steamcore::kTableSize;
using steamcore::serializeDump;

namespace {

constexpr size_t kBufferCapacity =
    kDumpHeaderSize +
    static_cast<size_t>(Framebuffer::width()) * Framebuffer::height();

void writeDump(const Framebuffer& fb, const char* path) {
  static uint8_t buffer[kBufferCapacity];
  const size_t written = serializeDump(fb, buffer, sizeof(buffer));
  CHECK_EQ(written, kBufferCapacity);

  FILE* out = std::fopen(path, "wb");
  if (out == nullptr) {
    CHECK(false && "could not open dump path for writing -- run make from the repo root");
    return;
  }
  const size_t fwritten = std::fwrite(buffer, 1, written, out);
  std::fclose(out);
  CHECK_EQ(fwritten, written);
}

GameInput fireEdge() {
  GameInput input{};
  input.fire = true;
  return input;
}

void put(HighscoreTable& table, const char* initials, int32_t score) {
  const char letters[3] = {initials[0], initials[1], initials[2]};
  insert(table, letters, score);
}

}  // namespace

// NFR-7/AC-2.1: dumps the initials-entry screen mid-entry -- a 5-digit
// score (kMaxScoreGlyphs, the worst-case width the score field's own
// static_assert proves on-screen at compile time, highscore_screen.h) and
// the cursor on position 2 (the first two letters already locked away
// from the default 'A', so the screen visibly shows real per-position
// state rather than three identical fresh cursors) -- so `make view` can
// render it and the underline's position under exactly one letter can
// actually be looked at.
STEAMCORE_TEST(highscore_entry_screen_dumps_to_disk_for_visual_check) {
  InitialsEntry entry;
  entry.begin();
  entry.update(GameInput{});  // settle past begin()'s own edge latch

  // Position 0: step to 'G', lock it in.
  for (int32_t i = 0; i < 6; ++i) {
    GameInput up{};
    up.up = true;
    entry.update(up);
    entry.update(GameInput{});
  }
  entry.update(fireEdge());
  entry.update(GameInput{});

  // Position 1: step to 'O', lock it in.
  for (int32_t i = 0; i < 14; ++i) {
    GameInput up{};
    up.up = true;
    entry.update(up);
    entry.update(GameInput{});
  }
  entry.update(fireEdge());
  entry.update(GameInput{});

  CHECK_EQ(entry.cursor(), 2);
  CHECK_EQ(entry.letter(0), 'G');
  CHECK_EQ(entry.letter(1), 'O');
  CHECK(!entry.complete());

  Framebuffer fb;
  drawInitialsEntryScreen(fb, 99999, entry);
  writeDump(fb, STEAMCORE_ENTRY_DUMP);
}

// NFR-7/AC-3.1: dumps a full, all-five-ranks-occupied top-5 table (no
// empty-slot gap to eyeball past) under the longest known display name,
// so `make view` can render it and the header/rows can be checked for
// legibility and non-overlap.
STEAMCORE_TEST(highscore_table_screen_dumps_to_disk_for_visual_check) {
  HighscoreTable table{};
  put(table, "ZZZ", 99999);
  put(table, "YYY", 54321);
  put(table, "XXX", 10000);
  put(table, "WWW", 500);
  put(table, "AAA", 10);
  CHECK_EQ(count(table), kTableSize);

  Framebuffer fb;
  drawHighscoreTableScreen(fb, "GALACTIC INVASION", table);
  writeDump(fb, STEAMCORE_TABLE_DUMP);
}
