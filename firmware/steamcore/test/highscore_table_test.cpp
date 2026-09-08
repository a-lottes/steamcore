#include "steamcore/highscore.h"

#include "test_harness.h"

using steamcore::HighscoreTable;
using steamcore::count;
using steamcore::detail::insert;
using steamcore::kMaxStoredScore;
using steamcore::kTableSize;
using steamcore::qualifies;

namespace {

// `insert()` takes exactly `kInitialsCount` raw letters, no null
// terminator (T4's `HighscoreStore::record()` uses the identical shape) --
// this helper lets test call sites write an ordinary, easier-to-read
// null-terminated string literal instead.
void put(HighscoreTable& table, const char* initials, int32_t score) {
  const char letters[3] = {initials[0], initials[1], initials[2]};
  insert(table, letters, score);
}

bool sortedAndHoleFree(const HighscoreTable& table) {
  bool seenEmpty = false;
  int32_t previousScore = kMaxStoredScore + 1;
  for (int32_t i = 0; i < kTableSize; ++i) {
    const bool occupied = table.entries[i].score > 0;
    if (seenEmpty && occupied) return false;  // a hole before an occupied entry
    if (!occupied) {
      seenEmpty = true;
      continue;
    }
    if (table.entries[i].score > previousScore) return false;
    previousScore = table.entries[i].score;
  }
  return true;
}

}  // namespace

// A7: a score of exactly 0 never qualifies, even into a completely empty
// table -- the total, single-source-of-truth occupancy rule (score > 0)
// applies to the incoming score too, not just to existing entries.
STEAMCORE_TEST(highscore_table_zero_score_never_qualifies_even_when_empty) {
  HighscoreTable table{};
  CHECK_EQ(count(table), 0);
  CHECK(!qualifies(table, 0));
}

// A7, the other half of the same rule: `insert()` itself is a no-op for a
// score of 0 or below, on an empty and on a partially-filled table alike
// -- it must never write letters into a rank `count()`/`qualifies()`
// still read as empty, and never store a negative score (which
// `decodeBlock()` rejects as malformed, discarding every slot's table).
// Peer-review F1.
STEAMCORE_TEST(highscore_table_a_non_positive_score_never_occupies_a_rank) {
  HighscoreTable empty{};
  put(empty, "AAA", 0);
  put(empty, "BBB", -5);
  CHECK_EQ(count(empty), 0);
  for (int32_t i = 0; i < kTableSize; ++i) {
    CHECK_EQ(empty.entries[i].score, 0);
    for (int32_t k = 0; k < 3; ++k) CHECK_EQ(empty.entries[i].initials[k], '\0');
  }

  HighscoreTable partial{};
  put(partial, "CCC", 300);
  put(partial, "AAA", 0);
  put(partial, "BBB", -5);
  CHECK_EQ(count(partial), 1);
  CHECK_EQ(partial.entries[0].score, 300);
  CHECK_EQ(partial.entries[1].score, 0);
  CHECK_EQ(partial.entries[1].initials[0], '\0');
  CHECK(sortedAndHoleFree(partial));
}

// A10: a score exactly equal to the table's current lowest entry does not
// bump it -- strictly greater is required, deterministic, no coin flip.
STEAMCORE_TEST(highscore_table_a_tying_score_does_not_bump_the_lowest) {
  HighscoreTable table{};
  put(table, "AAA", 500);
  put(table, "BBB", 400);
  put(table, "CCC", 300);
  put(table, "DDD", 200);
  put(table, "EEE", 100);
  CHECK_EQ(count(table), kTableSize);

  CHECK(!qualifies(table, 100));
  put(table, "ZZZ", 100);  // insert() itself must also be a no-op here
  CHECK_EQ(table.entries[4].score, 100);
  CHECK_EQ(table.entries[4].initials[0], 'E');
}

// A score exactly equal to rank 3 (index 2) inserts *below* that tie, at
// rank 4 -- the existing rank-3 entry is not displaced by an equal score,
// but the new score still legitimately qualifies against ranks 4/5.
STEAMCORE_TEST(highscore_table_a_tying_score_against_a_middle_rank_inserts_below_it) {
  HighscoreTable table{};
  put(table, "AAA", 500);
  put(table, "BBB", 400);
  put(table, "CCC", 300);
  put(table, "DDD", 200);
  put(table, "EEE", 100);

  CHECK(qualifies(table, 300));
  put(table, "NEW", 300);
  CHECK_EQ(table.entries[0].score, 500);
  CHECK_EQ(table.entries[1].score, 400);
  CHECK_EQ(table.entries[2].score, 300);
  CHECK_EQ(table.entries[2].initials[0], 'C');  // the original tie stays at rank 3
  CHECK_EQ(table.entries[3].score, 300);
  CHECK_EQ(table.entries[3].initials[0], 'N');  // the inserted entry lands just below it
  CHECK_EQ(table.entries[4].score, 200);        // old rank 4 shifts down
  CHECK(sortedAndHoleFree(table));
}

// Insertion at each of ranks 1-5 in turn, each time on a freshly built
// full table, checking the exact shift-down and the exact discard of the
// old rank-5 entry.
STEAMCORE_TEST(highscore_table_insertion_at_each_rank_shifts_and_discards_correctly) {
  auto fullTable = []() {
    HighscoreTable table{};
    put(table, "AAA", 500);
    put(table, "BBB", 400);
    put(table, "CCC", 300);
    put(table, "DDD", 200);
    put(table, "EEE", 100);
    return table;
  };

  {  // rank 1: beats every existing entry
    HighscoreTable table = fullTable();
    put(table, "NEW", 600);
    CHECK_EQ(table.entries[0].score, 600);
    CHECK_EQ(table.entries[1].score, 500);
    CHECK_EQ(table.entries[4].score, 200);  // old rank 5 (100) discarded
    CHECK(sortedAndHoleFree(table));
  }
  {  // rank 2
    HighscoreTable table = fullTable();
    put(table, "NEW", 450);
    CHECK_EQ(table.entries[0].score, 500);
    CHECK_EQ(table.entries[1].score, 450);
    CHECK_EQ(table.entries[2].score, 400);
    CHECK_EQ(table.entries[4].score, 200);
    CHECK(sortedAndHoleFree(table));
  }
  {  // rank 3
    HighscoreTable table = fullTable();
    put(table, "NEW", 350);
    CHECK_EQ(table.entries[2].score, 350);
    CHECK_EQ(table.entries[3].score, 300);
    CHECK_EQ(table.entries[4].score, 200);
    CHECK(sortedAndHoleFree(table));
  }
  {  // rank 4: beats 200 (index 3) but not 300 (index 2)
    HighscoreTable table = fullTable();
    put(table, "NEW", 250);
    CHECK_EQ(table.entries[3].score, 250);
    CHECK_EQ(table.entries[4].score, 200);  // old rank 4 shifts down, old rank 5 (100) discarded
    CHECK(sortedAndHoleFree(table));
  }
  {  // rank 5: beats only the current lowest (100), not rank 4 (200)
    HighscoreTable table = fullTable();
    put(table, "NEW", 150);
    CHECK_EQ(table.entries[4].score, 150);
    CHECK_EQ(table.entries[3].score, 200);
    CHECK(sortedAndHoleFree(table));
  }
}

// Insertion into a table with fewer than 5 entries fills the next empty
// slot directly, no shifting needed.
STEAMCORE_TEST(highscore_table_insertion_into_a_partial_table_fills_the_next_slot) {
  HighscoreTable table{};
  put(table, "AAA", 10);
  CHECK_EQ(count(table), 1);
  put(table, "BBB", 20);
  CHECK_EQ(count(table), 2);
  CHECK_EQ(table.entries[0].score, 20);
  CHECK_EQ(table.entries[1].score, 10);
  CHECK(sortedAndHoleFree(table));
}

// A13: duplicate initials across ranks are allowed -- no dedup logic
// anywhere in insert()/qualifies().
STEAMCORE_TEST(highscore_table_duplicate_initials_across_ranks_are_allowed) {
  HighscoreTable table{};
  put(table, "AAA", 100);
  put(table, "AAA", 50);
  CHECK_EQ(count(table), 2);
  CHECK_EQ(table.entries[0].initials[0], 'A');
  CHECK_EQ(table.entries[1].initials[0], 'A');
}

// A score above kMaxStoredScore is clamped on record, never overflowed
// or rejected outright.
STEAMCORE_TEST(highscore_table_a_score_above_the_max_is_clamped_on_insert) {
  HighscoreTable table{};
  CHECK(qualifies(table, kMaxStoredScore + 12345));
  put(table, "BIG", kMaxStoredScore + 12345);
  CHECK_EQ(table.entries[0].score, kMaxStoredScore);
}

// Peer-review F9: `qualifies()` must clamp exactly as `insert()` does, so
// the two never disagree -- a table whose lowest entry is already the
// clamp ceiling must not tell a caller a further above-ceiling score
// qualifies, since inserting it would be a no-op (a tie against the
// already-clamped lowest entry, A10).
STEAMCORE_TEST(highscore_table_qualifies_agrees_with_insert_at_the_clamp_ceiling) {
  HighscoreTable table{};
  put(table, "AAA", kMaxStoredScore);
  put(table, "BBB", kMaxStoredScore);
  put(table, "CCC", kMaxStoredScore);
  put(table, "DDD", kMaxStoredScore);
  put(table, "EEE", kMaxStoredScore);
  // Every rank already sits at the clamp ceiling -- a further above-
  // ceiling score clamps to a tie with the lowest entry, which A10 says
  // does not qualify.
  CHECK(!qualifies(table, kMaxStoredScore + 12345));
  put(table, "BIG", kMaxStoredScore + 12345);
  CHECK_EQ(table.entries[4].initials[0], 'E');  // unchanged: the insert was a no-op
}

// A deterministic 50-insert script leaves the table sorted, hole-free,
// and exactly kTableSize entries deep -- no drift over many operations.
STEAMCORE_TEST(highscore_table_stays_sorted_and_hole_free_after_fifty_inserts) {
  HighscoreTable table{};
  // A fixed, non-monotonic sequence -- not just ascending or descending,
  // so both "shift down" and "insert below everything" paths are
  // exercised repeatedly.
  const int32_t scores[] = {10,  500, 30,  480, 999, 1,   250, 250, 999, 5,
                             77,  88,  99,  1,   2,   3,   4,   5,   6,   7,
                             800, 810, 820, 1,   2,   400, 401, 402, 403, 404,
                             1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
                             600, 601, 602, 603, 604, 605, 606, 607, 608, 609};
  for (int32_t s : scores) {
    if (qualifies(table, s)) {
      put(table, "XYZ", s);
    }
  }
  CHECK_EQ(count(table), kTableSize);
  CHECK(sortedAndHoleFree(table));
  // The five largest values anywhere in `scores`: 999, 999, 820, 810, 800
  // (independently verified against a throwaway simulation of this same
  // scripted sequence before being hardcoded here).
  CHECK_EQ(table.entries[0].score, 999);
  CHECK_EQ(table.entries[1].score, 999);
  CHECK_EQ(table.entries[2].score, 820);
  CHECK_EQ(table.entries[3].score, 810);
  CHECK_EQ(table.entries[4].score, 800);
}
