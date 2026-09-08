#include "steamcore/highscore_screen.h"

#include "steamcore/color.h"
#include "steamcore/font.h"
#include "steamcore/game_loop.h"
#include "steamcore/highscore.h"
#include "steamcore/initials_entry.h"
#include "test_harness.h"

using steamcore::Color;
using steamcore::Entity;
using steamcore::Framebuffer;
using steamcore::GameInput;
using steamcore::HighscoreTable;
using steamcore::InitialsEntry;
using steamcore::count;
using steamcore::drawHighscoreTableScreen;
using steamcore::drawInitialsEntryScreen;
using steamcore::detail::insert;
using steamcore::kEntryHeaderBounds;
using steamcore::kEntryLettersBounds;
using steamcore::kEntryScoreBounds;
using steamcore::kEntryUnderlineRow;
using steamcore::kGlyphAdvance;
using steamcore::kGlyphHeight;
using steamcore::kInitialsCount;
using steamcore::kTableHeaderBounds;
using steamcore::kTableSize;
using steamcore::rowY;

namespace {

// A bounding box over a specific colour within a rectangular region --
// deliberately independent of collision.h's own `overlaps()` (an
// inclusive min/max box here, vs. `overlaps()`'s half-open [x, x+w)
// arithmetic) so this test proves the claim, rather than assuming
// whatever `overlaps()` itself already assumes (CLAUDE.md "prove it
// twice").
struct BBox {
  int32_t minX = -1, maxX = -1, minY = -1, maxY = -1;
  bool empty() const { return minX == -1; }
};

BBox scanRegion(const Framebuffer& fb, const Entity& region, Color color) {
  BBox box;
  for (int32_t y = region.y; y < region.y + region.h; ++y) {
    for (int32_t x = region.x; x < region.x + region.w; ++x) {
      if (fb.pixel(x, y) != color) continue;
      if (box.minX == -1 || x < box.minX) box.minX = x;
      if (x > box.maxX) box.maxX = x;
      if (box.minY == -1 || y < box.minY) box.minY = y;
      if (y > box.maxY) box.maxY = y;
    }
  }
  return box;
}

bool intersects(const BBox& a, const BBox& b) {
  if (a.empty() || b.empty()) return false;
  return a.minX <= b.maxX && b.minX <= a.maxX && a.minY <= b.maxY &&
         b.minY <= a.maxY;
}

void put(HighscoreTable& table, const char* initials, int32_t score) {
  const char letters[3] = {initials[0], initials[1], initials[2]};
  insert(table, letters, score);
}

}  // namespace

// Design Review R2/R3, runtime half (CLAUDE.md "prove it twice"): for
// scores of 1, 2 and 5 digits, every ENTRY-screen element's *actual
// rendered* bounding box (not the compile-time field rect) is pairwise
// disjoint from every other element's.
STEAMCORE_TEST(highscore_screen_entry_elements_never_overlap_at_any_score_width) {
  const int32_t scores[] = {5, 42, 99999};
  for (int32_t score : scores) {
    Framebuffer fb;
    InitialsEntry entry;
    entry.begin();
    drawInitialsEntryScreen(fb, score, entry);

    const BBox header = scanRegion(fb, kEntryHeaderBounds, Color::BRIGHT_ORANGE);
    const BBox scoreBox = scanRegion(fb, kEntryScoreBounds, Color::BRIGHT_ORANGE);
    const BBox letters = scanRegion(fb, kEntryLettersBounds, Color::BRIGHT_ORANGE);
    const Entity underlineRegion{0, rowY(kEntryUnderlineRow), Framebuffer::width(),
                                  kGlyphHeight};
    const BBox underline = scanRegion(fb, underlineRegion, Color::BRIGHT_ORANGE);

    CHECK(!header.empty());
    CHECK(!scoreBox.empty());
    CHECK(!letters.empty());
    CHECK(!underline.empty());

    CHECK(!intersects(header, scoreBox));
    CHECK(!intersects(header, letters));
    CHECK(!intersects(header, underline));
    CHECK(!intersects(scoreBox, letters));
    CHECK(!intersects(scoreBox, underline));
    CHECK(!intersects(letters, underline));
  }
}

// Design Review R2: the score line's actual rendered bounding box always
// falls fully within `kEntryScoreBounds` (the worst-case field), and is
// horizontally centred within it, for a 1-digit, 2-digit and 5-digit
// score alike.
STEAMCORE_TEST(highscore_screen_score_line_stays_inside_and_centred_in_its_field) {
  const int32_t scores[] = {5, 42, 99999};
  for (int32_t score : scores) {
    Framebuffer fb;
    InitialsEntry entry;
    entry.begin();
    drawInitialsEntryScreen(fb, score, entry);

    const BBox box = scanRegion(fb, kEntryScoreBounds, Color::BRIGHT_ORANGE);
    CHECK(!box.empty());
    CHECK(box.minX >= kEntryScoreBounds.x);
    CHECK(box.maxX < kEntryScoreBounds.x + kEntryScoreBounds.w);

    const int32_t leftMargin = box.minX - kEntryScoreBounds.x;
    const int32_t rightMargin =
        (kEntryScoreBounds.x + kEntryScoreBounds.w - 1) - box.maxX;
    // Centred to within one glyph-advance (integer-division rounding can
    // put the margins one glyph apart, never more).
    CHECK(leftMargin - rightMargin >= -kGlyphAdvance &&
          leftMargin - rightMargin <= kGlyphAdvance);
  }
}

// Peer-review F2/plan §1 Decision 8: a display name shorter than the
// worst-case `kMaxNameGlyphs` is centred within its own field, the same
// treatment the score line already gets above -- never left pinned to
// the worst-case field's fixed left edge.
STEAMCORE_TEST(highscore_screen_table_header_stays_inside_and_centred_in_its_field) {
  const char* names[] = {"AB", "GALACTIC INVASION"};
  for (const char* name : names) {
    Framebuffer fb;
    HighscoreTable table{};
    drawHighscoreTableScreen(fb, name, table);

    const BBox box = scanRegion(fb, kTableHeaderBounds, Color::BRIGHT_ORANGE);
    CHECK(!box.empty());
    CHECK(box.minX >= kTableHeaderBounds.x);
    CHECK(box.maxX < kTableHeaderBounds.x + kTableHeaderBounds.w);

    const int32_t leftMargin = box.minX - kTableHeaderBounds.x;
    const int32_t rightMargin =
        (kTableHeaderBounds.x + kTableHeaderBounds.w - 1) - box.maxX;
    // Centred to within one glyph-advance (integer-division rounding can
    // put the margins one glyph apart, never more).
    CHECK(leftMargin - rightMargin >= -kGlyphAdvance &&
          leftMargin - rightMargin <= kGlyphAdvance);
  }
}

// NFR-7/C13: the active-letter underline is exactly one `-` glyph, one
// row beneath the active letter position, and nowhere else on that row --
// checked for each of the three cursor positions.
STEAMCORE_TEST(highscore_screen_underline_sits_beneath_the_active_letter_only) {
  for (int32_t cursorPos = 0; cursorPos < kInitialsCount; ++cursorPos) {
    InitialsEntry entry;
    entry.begin();
    entry.update(GameInput{});  // settle past begin()'s own edge latch
    for (int32_t i = 0; i < cursorPos; ++i) {
      entry.update(GameInput{false, true, false, false, false, false, false});  // fire edge
      entry.update(GameInput{});  // release
    }
    CHECK_EQ(entry.cursor(), cursorPos);

    Framebuffer fb;
    drawInitialsEntryScreen(fb, 100, entry);

    const int32_t expectedX = steamcore::kLettersX + cursorPos * kGlyphAdvance;
    const int32_t rowTop = rowY(kEntryUnderlineRow);
    // Scan the *whole* glyph-height band, not just its top pixel row --
    // the `-` glyph's own "on" pixels sit partway down its 8x8 cell, not
    // at row 0 of it.
    for (int32_t y = rowTop; y < rowTop + kGlyphHeight; ++y) {
      for (int32_t x = 0; x < Framebuffer::width(); ++x) {
        const bool lit = fb.pixel(x, y) == Color::BRIGHT_ORANGE;
        const bool insideActiveCell =
            x >= expectedX && x < expectedX + kGlyphAdvance;
        if (!insideActiveCell) {
          CHECK(!lit);
        }
      }
    }
    bool anyLitUnderActive = false;
    for (int32_t y = rowTop; y < rowTop + kGlyphHeight; ++y) {
      for (int32_t x = expectedX; x < expectedX + kGlyphAdvance; ++x) {
        if (fb.pixel(x, y) == Color::BRIGHT_ORANGE) anyLitUnderActive = true;
      }
    }
    CHECK(anyLitUnderActive);
  }
}

// AC-2.1/F1/C10: the fixed header label reads exactly "HIGH SCORE" --
// restated independently here (title_screen_test.cpp's own precedent),
// since every other check in this feature's suite builds its expected
// frame from `kEntryHeaderText` itself and would therefore still pass if
// the label's text silently changed. Peer-review F6.
STEAMCORE_TEST(highscore_screen_entry_header_label_reads_high_score) {
  const char expected[] = "HIGH SCORE";
  CHECK_EQ(sizeof(steamcore::kEntryHeaderText), sizeof(expected));
  for (size_t i = 0; i < sizeof(expected); ++i) {
    CHECK_EQ(steamcore::kEntryHeaderText[i], expected[i]);
  }
}

// C12: the initials-entry screen never draws WIN/LOSS outcome text --
// checked by confirming nothing outside the four known element rows is
// ever lit.
STEAMCORE_TEST(highscore_screen_entry_screen_draws_nothing_outside_its_own_rows) {
  Framebuffer fb;
  InitialsEntry entry;
  entry.begin();
  drawInitialsEntryScreen(fb, 12345, entry);

  const int32_t knownRows[] = {
      rowY(steamcore::kEntryHeaderRow), rowY(steamcore::kEntryScoreRow),
      rowY(steamcore::kEntryLettersRow), rowY(steamcore::kEntryUnderlineRow)};
  for (int32_t y = 0; y < Framebuffer::height(); ++y) {
    bool isKnownRow = false;
    for (int32_t knownRow : knownRows) {
      if (y >= knownRow && y < knownRow + kGlyphHeight) isKnownRow = true;
    }
    if (isKnownRow) continue;
    for (int32_t x = 0; x < Framebuffer::width(); ++x) {
      CHECK(fb.pixel(x, y) == Color::BLACK);
    }
  }
}

// AC-3.1/AC-3.2: the TABLE screen draws exactly `count(table)` rows --
// nothing for an empty slot -- for tables holding 1, 2 and kTableSize
// (5) entries.
STEAMCORE_TEST(highscore_screen_table_draws_exactly_count_rows) {
  {
    HighscoreTable table{};
    put(table, "AND", 500);
    Framebuffer fb;
    drawHighscoreTableScreen(fb, "GALACTIC INVASION", table);
    for (int32_t rank = 0; rank < kTableSize; ++rank) {
      const Entity bounds = steamcore::tableRowBounds(rank);
      const BBox box = scanRegion(fb, bounds, Color::BRIGHT_ORANGE);
      CHECK_EQ(box.empty(), rank >= count(table));
    }
  }
  {
    HighscoreTable table{};
    put(table, "AND", 500);
    put(table, "MAX", 300);
    Framebuffer fb;
    drawHighscoreTableScreen(fb, "GALACTIC INVASION", table);
    for (int32_t rank = 0; rank < kTableSize; ++rank) {
      const Entity bounds = steamcore::tableRowBounds(rank);
      const BBox box = scanRegion(fb, bounds, Color::BRIGHT_ORANGE);
      CHECK_EQ(box.empty(), rank >= count(table));
    }
  }
  {
    HighscoreTable table{};
    put(table, "AAA", 500);
    put(table, "BBB", 400);
    put(table, "CCC", 300);
    put(table, "DDD", 200);
    put(table, "EEE", 100);
    Framebuffer fb;
    drawHighscoreTableScreen(fb, "GALACTIC INVASION", table);
    CHECK_EQ(count(table), kTableSize);
    for (int32_t rank = 0; rank < kTableSize; ++rank) {
      const Entity bounds = steamcore::tableRowBounds(rank);
      const BBox box = scanRegion(fb, bounds, Color::BRIGHT_ORANGE);
      CHECK(!box.empty());
    }
  }
}

// Runtime disjointness for the TABLE screen too: the header and every
// occupied row's actual bounding boxes are pairwise disjoint, for 1, 2
// and 5 occupied rows.
STEAMCORE_TEST(highscore_screen_table_elements_never_overlap_at_any_row_count) {
  auto checkDisjoint = [](const HighscoreTable& table) {
    Framebuffer fb;
    drawHighscoreTableScreen(fb, "GALACTIC INVASION", table);
    BBox boxes[kTableSize + 1];
    boxes[0] = scanRegion(fb, steamcore::kTableHeaderBounds, Color::BRIGHT_ORANGE);
    CHECK(!boxes[0].empty());
    const int32_t n = count(table);
    for (int32_t i = 0; i < n; ++i) {
      boxes[i + 1] = scanRegion(fb, steamcore::tableRowBounds(i), Color::BRIGHT_ORANGE);
      CHECK(!boxes[i + 1].empty());
    }
    for (int32_t a = 0; a <= n; ++a) {
      for (int32_t b = a + 1; b <= n; ++b) {
        CHECK(!intersects(boxes[a], boxes[b]));
      }
    }
  };

  {
    HighscoreTable table{};
    put(table, "AND", 500);
    checkDisjoint(table);
  }
  {
    HighscoreTable table{};
    put(table, "AND", 500);
    put(table, "MAX", 300);
    checkDisjoint(table);
  }
  {
    HighscoreTable table{};
    put(table, "AAA", 500);
    put(table, "BBB", 400);
    put(table, "CCC", 300);
    put(table, "DDD", 200);
    put(table, "EEE", 100);
    checkDisjoint(table);
  }
}
