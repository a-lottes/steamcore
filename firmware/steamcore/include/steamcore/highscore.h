#pragma once

#include <cstdint>

// Pure, host-testable per-game top-5 table math (highscore-system plan
// §1 Decision 5/7): qualification, insert-at-rank, shift-down, discard.
// No storage, no flash, no game-specific knowledge -- HighscoreStore<Backend>
// (this same header, T4) composes this with a Backend to persist it.
//
// Example (the system's own public entry point, `HighscoreStore<Backend>`
// -- peer-review F3: `insert()`/`encodeBlock()`/`decodeBlock()` and the
// persisted byte layout are internal, `steamcore::detail`, below):
//   MyBackend backend;
//   steamcore::HighscoreStore<MyBackend> store(backend);
//   store.load();
//   if (store.qualifies(/*slot=*/0, score)) {
//     store.record(/*slot=*/0, "AND", score);
//   }
//
// Contract (NFR-5's four required points, stated here in one place --
// each is also documented in full where it is actually implemented):
//  - Qualification (AC-1.1/A7/A10): a score qualifies iff it is strictly
//    greater than 0 AND (the table has an empty slot OR the score is
//    strictly greater than the table's current lowest entry) -- a tie
//    with the lowest entry does not qualify, deterministic, no coin
//    flip. See `qualifies()` below.
//  - Format version (constitution §6, NFR-6): every persisted block
//    carries an explicit `kFormatVersion`. A version mismatch, wrong
//    persisted slot count, checksum failure, or corrupt/malformed data
//    all decode to a clean, freshly-defaulted, empty `HighscoreBlock` --
//    never a crash, never partially decoded, never displayed as if
//    valid. See `decodeBlock()` below.
//  - Initials entry (A12): `up`/`down` cycle the active position's
//    letter one step per rising edge (no auto-repeat); `fire`'s rising
//    edge confirms the active letter and advances; `start` is never read
//    at all, structurally, not merely ignored. See `InitialsEntry`
//    (steamcore/initials_entry.h) for the authoritative detail.
//  - Score clamp (A14): a score above `kMaxStoredScore` is clamped, never
//    overflowed or rejected outright, on `insert()`.
namespace steamcore {

// A–Z only (spec A11) -- the only subset of the shipped 43-glyph font
// this feature needs.
inline constexpr int32_t kLetterCount = 26;
inline constexpr char kFirstLetter = 'A';

// 3-letter arcade-style initials (spec A11); a top-5 table per game
// (spec A6/A14).
inline constexpr int32_t kInitialsCount = 3;
inline constexpr int32_t kTableSize = 5;

// Spec A14: up to 5 digits (0-99999), unpadded. A score above this is
// clamped, never overflowed or rejected outright, on record.
inline constexpr int32_t kMaxStoredScore = 99999;

// One ranked entry. Occupancy is `score > 0` (spec A7: a score of exactly
// 0 never qualifies, so it doubles, without a second field, as "this
// slot is empty" -- the same zero-size-means-absent convention
// galactic-invasion's own `Enemy`/`playerShot_` already established for
// this project, rather than a parallel `bool occupied` two sources of
// truth could drift apart).
struct HighscoreEntry {
  int32_t score = 0;
  char initials[kInitialsCount] = {};
};

// Ranked strictly by score, descending; every occupied entry precedes
// every empty one (no holes) -- `count()`/`insert()` below are the only
// two functions that need to know this invariant, and both preserve it.
struct HighscoreTable {
  HighscoreEntry entries[kTableSize] = {};
};

inline constexpr bool isOccupied(const HighscoreEntry& entry) {
  return entry.score > 0;
}

// The number of occupied (leading, since the table is hole-free) entries,
// in [0, kTableSize].
inline constexpr int32_t count(const HighscoreTable& table) {
  int32_t n = 0;
  for (int32_t i = 0; i < kTableSize; ++i) {
    if (!isOccupied(table.entries[i])) break;
    ++n;
  }
  return n;
}

// Spec AC-1.1/A7/A10: `score` qualifies iff it is strictly greater than 0
// AND (the table has an empty slot OR `score` strictly exceeds the
// table's current lowest entry). A tie with the lowest entry does not
// qualify (A10) -- deterministic, no arbitrary coin flip.
inline constexpr bool qualifies(const HighscoreTable& table, int32_t score) {
  if (score <= 0) return false;
  // Peer-review F9: clamped exactly as `insert()` clamps, so the two
  // functions always agree -- otherwise a score just above
  // `kMaxStoredScore` could qualify here yet insert nothing (an
  // already-clamped table can never actually rank a value insert() would
  // discard).
  const int32_t clamped = score > kMaxStoredScore ? kMaxStoredScore : score;
  const int32_t n = count(table);
  if (n < kTableSize) return true;
  return clamped > table.entries[kTableSize - 1].score;
}

// `steamcore::detail`: not part of this library's public surface (NFR-4)
// -- `HighscoreStore<Backend>`'s own `record()`/`qualifies()` below are
// the system's public entry point (US-6/AC-6.1); this namespace holds the
// rank-shifting and persisted-byte-layout internals `record()` composes.
namespace detail {

// Spec AC-1.1: inserts `initials`/`score` (clamped to `kMaxStoredScore`)
// at the correct rank, shifting every lower-or-equal-losing entry down
// one and discarding whatever falls past `kTableSize`. A no-op if
// `score` does not qualify -- including any score of 0 or below, which
// never occupies a rank even on a completely empty table (A7). Callers
// are expected to check `qualifies()` first; this function still never
// corrupts the table if they don't.
void insert(HighscoreTable& table, const char (&initials)[kInitialsCount],
            int32_t score);

}  // namespace detail

// Spec A6: one table per game slot. README names five planned games
// (galactic-invasion plus four more); the slot count is deliberately
// generous headroom, not a guess -- see below for why growing it later
// is safe.
inline constexpr int32_t kGameSlotCount = 5;

namespace detail {

struct HighscoreBlock {
  HighscoreTable tables[kGameSlotCount] = {};
};

// --- The persisted block (constitution §6 non-negotiable: persisted
// data carries a format version) ---
//
// `kFormatVersion` covers the whole block's byte layout: `kGameSlotCount`,
// `kTableSize`, `kInitialsCount`, any field's width, or the order fields
// appear in. Changing ANY of those bumps `kFormatVersion` in the same
// commit; no migration code is written for an old version, a version
// bump is deliberately a clean reset (constitution §6 permits either --
// this project's flash holds only re-earnable highscores, not
// irreplaceable data). `kGameSlotCount` is *also* stored inside the
// block and checked against the compile-time constant on decode
// (`decodeBlock` below) -- a second, independent guard, so a future
// slot-count change resets cleanly even if a reviewer forgets the
// version-bump rule above.
//
// The block is encoded byte-at-a-time, little-endian, explicitly --
// never `memcpy`'d as a struct image: `HighscoreEntry`/`HighscoreTable`
// carry implicit compiler padding whose bytes are never initialised,
// which would make a checksum over a raw struct image non-deterministic,
// and a raw image would also silently assume this build's endianness.
// Byte-shift widths are always `i * kBitsPerByte` inside a loop, never a
// bare shift literal (`>> 16` reads as this project's own tile-size
// literal to `tools/check_constraints.sh`'s global include/+src scan,
// and is banned there for an unrelated reason -- galactic-invasion's own
// `kRngShiftBits` hit the same coincidence first).
inline constexpr int32_t kBitsPerByte = 8;
inline constexpr int32_t kBytesPerU32 = 4;

inline constexpr uint32_t kMagic = 0x53434831u;  // "SCH1", arbitrary but stable
inline constexpr int32_t kFormatVersion = 1;

inline constexpr int32_t kEntryBytes = kBytesPerU32 + kInitialsCount;
inline constexpr int32_t kTableBytes = kTableSize * kEntryBytes;
inline constexpr int32_t kTablesBytes = kGameSlotCount * kTableBytes;
// magic, format version, persisted slot count.
inline constexpr int32_t kHeaderBytes = 3 * kBytesPerU32;
inline constexpr int32_t kChecksumBytes = kBytesPerU32;
inline constexpr int32_t kBlockSize =
    kHeaderBytes + kTablesBytes + kChecksumBytes;

// Encodes `block` into `out` (magic, version, slot count, every table's
// entries in slot/rank order, an FNV-1a checksum over everything before
// it) -- always succeeds, always produces exactly `kBlockSize` bytes.
void encodeBlock(const HighscoreBlock& block, uint8_t (&out)[kBlockSize]);

// Spec AC-1.3/NFR-6: decodes `in` into `block` only if every check
// passes -- magic, format version, persisted slot count (both structural
// guards above), checksum, and per-table well-formedness (every score in
// [0, kMaxStoredScore], non-increasing by rank, no occupied entry after
// an empty one, every occupied entry's initials in A-Z). On ANY failure,
// `block` is left as a clean, freshly-defaulted `HighscoreBlock{}` --
// never partially decoded, never garbage, never a crash -- and this
// function returns false. Never throws, never asserts on malformed
// input: `in` may be freshly-erased flash (all `0xFF`), all-zero, or
// bytes from an entirely different format version, and all of those are
// ordinary, expected inputs this function handles, not preconditions a
// caller must avoid.
bool decodeBlock(const uint8_t (&in)[kBlockSize], HighscoreBlock& block);

}  // namespace detail

// A `Backend` implementer needs to know how many bytes to reserve for the
// persisted block (`test::FakeFlashBackend`'s own fixed-size member,
// T4) -- everything else about the block's internal layout stays in
// `detail` above (NFR-4).
inline constexpr int32_t kBlockSize = detail::kBlockSize;

// Composes the pure block/table math above with a `Backend` (a compile-
// time-bound concept, the same shape `TilePusher<Transmitter>`/
// `InputReader<Source>` already established -- no virtual, no vtable):
// `bool read(uint8_t*, int32_t)` and `bool write(const uint8_t*, int32_t)`.
// `port::esp32::NvsHighscoreBackend` (T14) satisfies it on the real
// device; `test::FakeFlashBackend` (T4's own test file) satisfies it on
// host, over a plain byte array.
//
// A store that was never `load()`ed, or whose `load()` failed for any
// reason (nothing yet written, wrong format version, a corrupt read),
// presents every table as empty -- never garbage, never UB. Call
// `load()` exactly once, before the first `record()`: `record()` always
// re-encodes and writes this store's *whole* in-memory block, so
// recording into a store that was never loaded overwrites whatever the
// backend already held (every slot, not just this one) with an
// otherwise-empty block. `record()`
// always updates the in-memory table first, then attempts to persist it;
// if the backend write fails, the RAM table stays truthful for whatever
// the caller renders next, even though the entry may not survive a power
// cycle (this feature's one accepted, silent-failure consequence, since
// this console has no error-screen vocabulary for it).
template <typename Backend>
class HighscoreStore {
 public:
  explicit HighscoreStore(Backend& backend) : backend_(backend) {}

  // Spec AC-1.3: reads and decodes the backend's bytes into this store's
  // tables. Returns false (leaving every table empty) if the backend has
  // nothing to read yet, or if what it read fails any of `decodeBlock`'s
  // checks -- both are ordinary, expected outcomes, not error states a
  // caller must special-case.
  bool load() {
    // Peer-review F10: zero-initialised defence in depth -- a `Backend`
    // returning `true` without filling every byte must never feed
    // indeterminate values into `decodeBlock`.
    uint8_t bytes[kBlockSize] = {};
    if (!backend_.read(bytes, kBlockSize)) {
      block_ = detail::HighscoreBlock{};
      return false;
    }
    return detail::decodeBlock(bytes, block_);
  }

  // AC-6.1: `slot` is the caller's own identifier -- this store holds no
  // game-specific name or branch anywhere in it. An out-of-range `slot`
  // returns a static, permanently-empty table rather than undefined
  // behaviour (this engine's established invalid-input posture).
  const HighscoreTable& table(int32_t slot) const {
    if (slot < 0 || slot >= kGameSlotCount) {
      static const HighscoreTable kEmptyTable{};
      return kEmptyTable;
    }
    return block_.tables[slot];
  }

  // A thin forward to the free `qualifies()` above, over this store's own
  // current table for `slot` -- so a caller (`HighscoreGame`) never has
  // to read `table(slot)` itself just to ask this one question. An
  // out-of-range `slot` (an empty table, per `table()` above) never
  // qualifies for any score.
  bool qualifies(int32_t slot, int32_t score) const {
    return steamcore::qualifies(table(slot), score);
  }

  // AC-1.1/AC-6.1: inserts into slot `slot`'s table (a no-op if `score`
  // does not actually qualify, `insert()`'s own contract), re-encodes the
  // *whole* block (every slot, not just this one) and writes it. Returns
  // the backend's own write result; the in-memory table is updated either
  // way (see this type's own contract above). An out-of-range `slot` is a
  // no-op, returning false.
  bool record(int32_t slot, const char (&initials)[kInitialsCount],
              int32_t score) {
    if (slot < 0 || slot >= kGameSlotCount) return false;
    detail::insert(block_.tables[slot], initials, score);
    uint8_t bytes[kBlockSize];
    detail::encodeBlock(block_, bytes);
    return backend_.write(bytes, kBlockSize);
  }

 private:
  Backend& backend_;
  detail::HighscoreBlock block_{};
};

}  // namespace steamcore
