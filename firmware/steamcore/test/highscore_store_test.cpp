#include "steamcore/highscore.h"

#include "fake_flash_backend.h"
#include "test_harness.h"

using steamcore::HighscoreStore;
using steamcore::count;
using steamcore::kGameSlotCount;
using steamcore::test::FakeFlashBackend;

namespace {

void put3(char (&out)[3], const char* initials) {
  out[0] = initials[0];
  out[1] = initials[1];
  out[2] = initials[2];
}

}  // namespace

// AC-1.2: recording an entry, then constructing a *second*, independent
// store over the same backing bytes -- the host's own stand-in for a
// power cycle, since there is no OS reboot to actually perform -- reads
// back the exact same table.
STEAMCORE_TEST(highscore_store_survives_reconstruction_over_the_same_backend) {
  FakeFlashBackend backend;
  HighscoreStore<FakeFlashBackend> store(backend);
  CHECK(!store.load());  // nothing written yet

  char initials[3];
  put3(initials, "AND");
  CHECK(store.record(0, initials, 12500));
  CHECK_EQ(store.table(0).entries[0].score, 12500);

  HighscoreStore<FakeFlashBackend> reconstructed(backend);
  CHECK(reconstructed.load());
  CHECK_EQ(reconstructed.table(0).entries[0].score, 12500);
  CHECK_EQ(reconstructed.table(0).entries[0].initials[0], 'A');
  CHECK_EQ(reconstructed.table(0).entries[0].initials[1], 'N');
  CHECK_EQ(reconstructed.table(0).entries[0].initials[2], 'D');
}

// AC-1.3: a backend holding a wrong-format-version block, or a block with
// a corrupted byte, both yield a clean, empty table on load -- through
// the store, not just through decodeBlock() directly (T3 already proved
// decodeBlock() itself; this proves the store wires it up correctly).
STEAMCORE_TEST(highscore_store_load_of_a_wrong_version_block_yields_empty_tables) {
  FakeFlashBackend backend;
  HighscoreStore<FakeFlashBackend> store(backend);
  char initials[3];
  put3(initials, "AND");
  store.record(0, initials, 100);

  backend.rawBytes()[4] =
      static_cast<uint8_t>(backend.rawBytes()[4] + 1);  // corrupt the version field

  HighscoreStore<FakeFlashBackend> reloaded(backend);
  CHECK(!reloaded.load());
  for (int32_t slot = 0; slot < kGameSlotCount; ++slot) {
    CHECK_EQ(count(reloaded.table(slot)), 0);
  }
}

STEAMCORE_TEST(highscore_store_load_of_a_corrupt_byte_block_yields_empty_tables) {
  FakeFlashBackend backend;
  HighscoreStore<FakeFlashBackend> store(backend);
  char initials[3];
  put3(initials, "AND");
  store.record(0, initials, 100);

  // Flip a byte inside the table payload, well past the header -- only
  // the checksum can catch this.
  backend.rawBytes()[20] =
      static_cast<uint8_t>(backend.rawBytes()[20] ^ 0x01);

  HighscoreStore<FakeFlashBackend> reloaded(backend);
  CHECK(!reloaded.load());
  for (int32_t slot = 0; slot < kGameSlotCount; ++slot) {
    CHECK_EQ(count(reloaded.table(slot)), 0);
  }
}

// AC-6.2: two distinct slots recorded side by side never cross-
// contaminate, checked in both possible recording orders.
STEAMCORE_TEST(highscore_store_two_slots_never_cross_contaminate_forward_order) {
  FakeFlashBackend backend;
  HighscoreStore<FakeFlashBackend> store(backend);
  char first[3];
  char second[3];
  put3(first, "AAA");
  put3(second, "BBB");
  store.record(0, first, 100);
  store.record(2, second, 200);

  CHECK_EQ(count(store.table(0)), 1);
  CHECK_EQ(store.table(0).entries[0].score, 100);
  CHECK_EQ(count(store.table(1)), 0);
  CHECK_EQ(count(store.table(2)), 1);
  CHECK_EQ(store.table(2).entries[0].score, 200);
  CHECK_EQ(count(store.table(3)), 0);
  CHECK_EQ(count(store.table(4)), 0);
}

STEAMCORE_TEST(highscore_store_two_slots_never_cross_contaminate_reverse_order) {
  FakeFlashBackend backend;
  HighscoreStore<FakeFlashBackend> store(backend);
  char first[3];
  char second[3];
  put3(first, "AAA");
  put3(second, "BBB");
  store.record(2, second, 200);
  store.record(0, first, 100);

  CHECK_EQ(count(store.table(0)), 1);
  CHECK_EQ(store.table(0).entries[0].score, 100);
  CHECK_EQ(count(store.table(2)), 1);
  CHECK_EQ(store.table(2).entries[0].score, 200);
}

// A failing write() still leaves the in-memory table updated (this
// feature's one accepted silent-failure consequence, NFR-9) and returns
// false so a caller *can* react if it chooses to.
STEAMCORE_TEST(highscore_store_a_failing_write_still_updates_the_ram_table) {
  FakeFlashBackend backend;
  HighscoreStore<FakeFlashBackend> store(backend);
  backend.setFailWrites(true);

  char initials[3];
  put3(initials, "AND");
  CHECK(!store.record(0, initials, 100));
  CHECK_EQ(store.table(0).entries[0].score, 100);
  CHECK(!backend.present());  // nothing actually landed in the backend
}

// Peer-review F1: a caller that reports a non-positive score without
// checking `qualifies()` first (US-6 makes this API reusable by games
// this project has not written yet) must not be able to poison the
// persisted block. Before the fix, a negative score was written into
// slot 0's first rank, and `decodeBlock()` then rejected the whole block
// as malformed -- silently discarding *another* slot's legitimate entry
// on the next load.
STEAMCORE_TEST(highscore_store_a_non_positive_score_never_poisons_the_block) {
  FakeFlashBackend backend;
  HighscoreStore<FakeFlashBackend> store(backend);
  char initials[3];
  put3(initials, "AND");
  CHECK(store.record(1, initials, 500));

  char other[3];
  put3(other, "BAD");
  store.record(0, other, -5);
  store.record(0, other, 0);
  CHECK_EQ(count(store.table(0)), 0);

  HighscoreStore<FakeFlashBackend> reloaded(backend);
  CHECK(reloaded.load());
  CHECK_EQ(count(reloaded.table(1)), 1);
  CHECK_EQ(reloaded.table(1).entries[0].score, 500);
  CHECK_EQ(count(reloaded.table(0)), 0);
}

// AC-1.2/robustness: an out-of-range slot is a safe no-op on both read
// and write, never undefined behaviour.
STEAMCORE_TEST(highscore_store_out_of_range_slot_is_a_safe_no_op) {
  FakeFlashBackend backend;
  HighscoreStore<FakeFlashBackend> store(backend);
  char initials[3];
  put3(initials, "AND");
  CHECK(!store.record(kGameSlotCount, initials, 100));
  CHECK(!store.record(-1, initials, 100));
  CHECK_EQ(count(store.table(kGameSlotCount)), 0);
  CHECK_EQ(count(store.table(-1)), 0);
}
