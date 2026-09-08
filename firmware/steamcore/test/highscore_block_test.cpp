#include "steamcore/highscore.h"

#include "test_harness.h"

using steamcore::detail::HighscoreBlock;
using steamcore::HighscoreTable;
using steamcore::detail::decodeBlock;
using steamcore::detail::encodeBlock;
using steamcore::detail::insert;
using steamcore::kBlockSize;
using steamcore::kFirstLetter;
using steamcore::kGameSlotCount;
using steamcore::kMaxStoredScore;
using steamcore::kTableSize;

namespace {

void put(HighscoreTable& table, const char* initials, int32_t score) {
  const char letters[3] = {initials[0], initials[1], initials[2]};
  insert(table, letters, score);
}

bool tablesEqual(const HighscoreTable& a, const HighscoreTable& b) {
  for (int32_t i = 0; i < kTableSize; ++i) {
    if (a.entries[i].score != b.entries[i].score) return false;
    for (int32_t k = 0; k < 3; ++k) {
      if (a.entries[i].initials[k] != b.entries[i].initials[k]) return false;
    }
  }
  return true;
}

bool blocksEqual(const HighscoreBlock& a, const HighscoreBlock& b) {
  for (int32_t i = 0; i < kGameSlotCount; ++i) {
    if (!tablesEqual(a.tables[i], b.tables[i])) return false;
  }
  return true;
}

HighscoreBlock partialBlock() {
  HighscoreBlock block{};
  put(block.tables[0], "AND", 500);
  put(block.tables[0], "MAX", 300);
  put(block.tables[2], "EVA", 42);
  return block;
}

// Independent of highscore.cpp's own (private) fnv1aChecksum -- standard
// FNV-1a 32-bit constants, reimplemented here rather than exposed from
// production just for this test (CLAUDE.md "prove it twice").
uint32_t independentFnv1a(const uint8_t* data, int32_t length) {
  uint32_t hash = 0x811c9dc5u;
  for (int32_t i = 0; i < length; ++i) {
    hash ^= data[i];
    hash *= 0x01000193u;
  }
  return hash;
}

HighscoreBlock fullBlock() {
  HighscoreBlock block{};
  for (int32_t slot = 0; slot < kGameSlotCount; ++slot) {
    put(block.tables[slot], "AAA", 500);
    put(block.tables[slot], "BBB", 400);
    put(block.tables[slot], "CCC", 300);
    put(block.tables[slot], "DDD", 200);
    put(block.tables[slot], "EEE", 100);
  }
  return block;
}

}  // namespace

// encode -> decode round-trips exactly, for an empty, a partial and a
// full block.
STEAMCORE_TEST(highscore_block_round_trips_an_empty_block) {
  HighscoreBlock block{};
  uint8_t bytes[kBlockSize];
  encodeBlock(block, bytes);
  HighscoreBlock decoded{};
  CHECK(decodeBlock(bytes, decoded));
  CHECK(blocksEqual(block, decoded));
}

STEAMCORE_TEST(highscore_block_round_trips_a_partial_block) {
  const HighscoreBlock block = partialBlock();
  uint8_t bytes[kBlockSize];
  encodeBlock(block, bytes);
  HighscoreBlock decoded{};
  CHECK(decodeBlock(bytes, decoded));
  CHECK(blocksEqual(block, decoded));
}

STEAMCORE_TEST(highscore_block_round_trips_a_full_block) {
  const HighscoreBlock block = fullBlock();
  uint8_t bytes[kBlockSize];
  encodeBlock(block, bytes);
  HighscoreBlock decoded{};
  CHECK(decodeBlock(bytes, decoded));
  CHECK(blocksEqual(block, decoded));
}

// The exact byte offsets of magic/version/slot-count/first-score are
// asserted individually, hardcoded independently of the production
// constants that generate them -- a silent layout drift in encodeBlock()
// must fail this test, not merely happen to still agree with it.
STEAMCORE_TEST(highscore_block_layout_offsets_are_stable) {
  const HighscoreBlock block = partialBlock();
  uint8_t bytes[kBlockSize];
  encodeBlock(block, bytes);

  // Magic 0x53434831 ("SCH1"), little-endian, at offset 0.
  CHECK_EQ(bytes[0], 0x31);
  CHECK_EQ(bytes[1], 0x48);
  CHECK_EQ(bytes[2], 0x43);
  CHECK_EQ(bytes[3], 0x53);
  // Format version (1), little-endian, at offset 4.
  CHECK_EQ(bytes[4], 1);
  CHECK_EQ(bytes[5], 0);
  CHECK_EQ(bytes[6], 0);
  CHECK_EQ(bytes[7], 0);
  // Persisted slot count (5), little-endian, at offset 8.
  CHECK_EQ(bytes[8], 5);
  CHECK_EQ(bytes[9], 0);
  CHECK_EQ(bytes[10], 0);
  CHECK_EQ(bytes[11], 0);
  // First score (slot 0, rank 0 -- "AND", 500), little-endian, at offset 12.
  CHECK_EQ(bytes[12], 500 & 0xFF);
  CHECK_EQ(bytes[13], (500 >> 8) & 0xFF);
  CHECK_EQ(bytes[14], 0);
  CHECK_EQ(bytes[15], 0);
  // First entry's initials immediately follow, at offset 16.
  CHECK_EQ(bytes[16], 'A');
  CHECK_EQ(bytes[17], 'N');
  CHECK_EQ(bytes[18], 'D');
}

// Six rejection paths, each its own case: a decode failure always leaves
// `block` a clean, fully empty HighscoreBlock, never a crash and never a
// partial decode.
STEAMCORE_TEST(highscore_block_rejects_wrong_magic) {
  const HighscoreBlock original = fullBlock();
  uint8_t bytes[kBlockSize];
  encodeBlock(original, bytes);
  bytes[0] = static_cast<uint8_t>(bytes[0] ^ 0xFF);  // corrupt magic

  HighscoreBlock decoded = fullBlock();  // deliberately dirty before decoding
  CHECK(!decodeBlock(bytes, decoded));
  CHECK(blocksEqual(decoded, HighscoreBlock{}));
}

STEAMCORE_TEST(highscore_block_rejects_wrong_format_version) {
  const HighscoreBlock original = fullBlock();
  uint8_t bytes[kBlockSize];
  encodeBlock(original, bytes);
  bytes[4] = static_cast<uint8_t>(bytes[4] + 1);  // bump the version field

  HighscoreBlock decoded = fullBlock();
  CHECK(!decodeBlock(bytes, decoded));
  CHECK(blocksEqual(decoded, HighscoreBlock{}));
}

// T12 mutation pass, mutation (b): the previous test's single-byte flip is
// also caught by the checksum guard (the version field sits inside the
// checksummed range), so it does not actually prove the version check
// itself does anything -- deleting that check entirely still passed the
// suite. This test recomputes and rewrites a *valid* checksum over the
// mutated bytes first, independently of encodeBlock()'s own private
// helper, isolating the version check as the only guard left that can
// reject it.
STEAMCORE_TEST(highscore_block_rejects_wrong_format_version_even_with_a_valid_checksum) {
  const HighscoreBlock original = fullBlock();
  uint8_t bytes[kBlockSize];
  encodeBlock(original, bytes);
  bytes[4] = static_cast<uint8_t>(bytes[4] + 1);  // bump the version field

  const int32_t payloadEnd = kBlockSize - 4;  // checksum is the trailing 4 bytes
  const uint32_t checksum = independentFnv1a(bytes, payloadEnd);
  for (int32_t i = 0; i < 4; ++i) {
    bytes[payloadEnd + i] = static_cast<uint8_t>(checksum >> (i * 8));
  }

  HighscoreBlock decoded = fullBlock();
  CHECK(!decodeBlock(bytes, decoded));
  CHECK(blocksEqual(decoded, HighscoreBlock{}));
}

STEAMCORE_TEST(highscore_block_rejects_wrong_persisted_slot_count) {
  const HighscoreBlock original = fullBlock();
  uint8_t bytes[kBlockSize];
  encodeBlock(original, bytes);
  bytes[8] = static_cast<uint8_t>(bytes[8] + 1);  // claim one more slot than compiled in

  HighscoreBlock decoded = fullBlock();
  CHECK(!decodeBlock(bytes, decoded));
  CHECK(blocksEqual(decoded, HighscoreBlock{}));
}

// T12 mutation pass, mutation (c): the same gap as the version check above
// -- flipping byte[8] alone is also caught by the checksum guard, not
// necessarily the slot-count check itself. Isolates it the same way: a
// valid checksum recomputed over the mutated bytes.
STEAMCORE_TEST(highscore_block_rejects_wrong_slot_count_even_with_a_valid_checksum) {
  const HighscoreBlock original = fullBlock();
  uint8_t bytes[kBlockSize];
  encodeBlock(original, bytes);
  bytes[8] = static_cast<uint8_t>(bytes[8] + 1);  // claim one more slot than compiled in

  const int32_t payloadEnd = kBlockSize - 4;
  const uint32_t checksum = independentFnv1a(bytes, payloadEnd);
  for (int32_t i = 0; i < 4; ++i) {
    bytes[payloadEnd + i] = static_cast<uint8_t>(checksum >> (i * 8));
  }

  HighscoreBlock decoded = fullBlock();
  CHECK(!decodeBlock(bytes, decoded));
  CHECK(blocksEqual(decoded, HighscoreBlock{}));
}

STEAMCORE_TEST(highscore_block_rejects_a_checksum_mismatch) {
  const HighscoreBlock original = fullBlock();
  uint8_t bytes[kBlockSize];
  encodeBlock(original, bytes);
  // Flip a byte well inside the table payload, leaving the header (and
  // therefore every other check) valid -- only the checksum can catch this.
  bytes[20] = static_cast<uint8_t>(bytes[20] ^ 0x01);

  HighscoreBlock decoded = fullBlock();
  CHECK(!decodeBlock(bytes, decoded));
  CHECK(blocksEqual(decoded, HighscoreBlock{}));
}

STEAMCORE_TEST(highscore_block_rejects_initials_outside_a_to_z) {
  HighscoreBlock block{};
  block.tables[0].entries[0].score = 100;
  block.tables[0].entries[0].initials[0] = static_cast<char>(kFirstLetter - 1);
  block.tables[0].entries[0].initials[1] = 'A';
  block.tables[0].entries[0].initials[2] = 'A';
  uint8_t bytes[kBlockSize];
  encodeBlock(block, bytes);

  HighscoreBlock decoded = fullBlock();
  CHECK(!decodeBlock(bytes, decoded));
  CHECK(blocksEqual(decoded, HighscoreBlock{}));
}

STEAMCORE_TEST(highscore_block_rejects_a_score_above_the_max) {
  HighscoreBlock block{};
  block.tables[0].entries[0].score = kMaxStoredScore + 1;
  block.tables[0].entries[0].initials[0] = 'A';
  block.tables[0].entries[0].initials[1] = 'A';
  block.tables[0].entries[0].initials[2] = 'A';
  uint8_t bytes[kBlockSize];
  encodeBlock(block, bytes);

  HighscoreBlock decoded = fullBlock();
  CHECK(!decodeBlock(bytes, decoded));
  CHECK(blocksEqual(decoded, HighscoreBlock{}));
}

STEAMCORE_TEST(highscore_block_rejects_non_descending_scores) {
  HighscoreBlock block{};
  block.tables[0].entries[0].score = 100;
  block.tables[0].entries[0].initials[0] = 'A';
  block.tables[0].entries[0].initials[1] = 'A';
  block.tables[0].entries[0].initials[2] = 'A';
  block.tables[0].entries[1].score = 200;  // rank 2 > rank 1 -- not descending
  block.tables[0].entries[1].initials[0] = 'B';
  block.tables[0].entries[1].initials[1] = 'B';
  block.tables[0].entries[1].initials[2] = 'B';
  uint8_t bytes[kBlockSize];
  encodeBlock(block, bytes);

  HighscoreBlock decoded = fullBlock();
  CHECK(!decodeBlock(bytes, decoded));
  CHECK(blocksEqual(decoded, HighscoreBlock{}));
}

STEAMCORE_TEST(highscore_block_rejects_an_occupied_entry_after_an_empty_one) {
  HighscoreBlock block{};
  block.tables[0].entries[0].score = 0;  // empty
  block.tables[0].entries[1].score = 100;  // occupied, after an empty slot -- a hole
  block.tables[0].entries[1].initials[0] = 'A';
  block.tables[0].entries[1].initials[1] = 'A';
  block.tables[0].entries[1].initials[2] = 'A';
  uint8_t bytes[kBlockSize];
  encodeBlock(block, bytes);

  HighscoreBlock decoded = fullBlock();
  CHECK(!decodeBlock(bytes, decoded));
  CHECK(blocksEqual(decoded, HighscoreBlock{}));
}

// Freshly-erased flash (all 0xFF) and an all-zero block both reject
// cleanly -- both are ordinary, expected inputs (a never-written NVS
// blob, or a zero-filled one), never a crash.
STEAMCORE_TEST(highscore_block_rejects_an_all_0xff_block) {
  uint8_t bytes[kBlockSize];
  for (uint8_t& b : bytes) b = 0xFF;

  HighscoreBlock decoded = fullBlock();
  CHECK(!decodeBlock(bytes, decoded));
  CHECK(blocksEqual(decoded, HighscoreBlock{}));
}

STEAMCORE_TEST(highscore_block_rejects_an_all_zero_block) {
  uint8_t bytes[kBlockSize] = {};

  HighscoreBlock decoded = fullBlock();
  CHECK(!decodeBlock(bytes, decoded));
  CHECK(blocksEqual(decoded, HighscoreBlock{}));
}
