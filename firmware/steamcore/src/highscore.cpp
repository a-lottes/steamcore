#include "steamcore/highscore.h"

namespace steamcore {
namespace detail {

void insert(HighscoreTable& table, const char (&initials)[kInitialsCount],
            int32_t score) {
  // A7: a score of 0 or below never qualifies, so it never occupies a
  // rank either -- without this guard it would be written into the first
  // empty slot (invisible to count()/qualifies(), which read `score > 0`,
  // but a negative score persists as a byte pattern decodeBlock() then
  // rejects as malformed, wiping every slot's table on the next load).
  if (score <= 0) return;
  const int32_t clamped = score > kMaxStoredScore ? kMaxStoredScore : score;

  // The first index whose current entry does not beat `clamped` -- empty
  // always loses (any qualifying score fills it), an occupied entry loses
  // only if `clamped` is strictly greater (A10: a tie keeps the existing
  // entry, deterministic, no coin flip).
  int32_t rank = kTableSize;
  for (int32_t i = 0; i < kTableSize; ++i) {
    if (!isOccupied(table.entries[i]) || clamped > table.entries[i].score) {
      rank = i;
      break;
    }
  }
  if (rank >= kTableSize) return;  // did not qualify; caller should have checked

  for (int32_t i = kTableSize - 1; i > rank; --i) {
    table.entries[i] = table.entries[i - 1];
  }
  table.entries[rank].score = clamped;
  for (int32_t k = 0; k < kInitialsCount; ++k) {
    table.entries[rank].initials[k] = initials[k];
  }
}

namespace {

void writeU32LE(uint8_t* dest, uint32_t value) {
  for (int32_t i = 0; i < kBytesPerU32; ++i) {
    dest[i] = static_cast<uint8_t>(value >> (i * kBitsPerByte));
  }
}

uint32_t readU32LE(const uint8_t* src) {
  uint32_t value = 0;
  for (int32_t i = 0; i < kBytesPerU32; ++i) {
    value |= static_cast<uint32_t>(src[i]) << (i * kBitsPerByte);
  }
  return value;
}

// FNV-1a, 32-bit. Not cryptographic -- this only needs to catch a torn
// or corrupted write, not resist tampering (no adversary exists on an
// offline, single-player cabinet).
uint32_t fnv1aChecksum(const uint8_t* data, int32_t length) {
  uint32_t hash = 0x811c9dc5u;  // FNV-1a 32-bit offset basis
  for (int32_t i = 0; i < length; ++i) {
    hash ^= data[i];
    hash *= 0x01000193u;  // FNV-1a 32-bit prime
  }
  return hash;
}

// Spec AC-1.3/NFR-6: every score in range, non-increasing by rank, no
// occupied entry after an empty one, every occupied entry's initials in
// A-Z. An empty entry's own score must still be exactly 0 -- decoded
// bytes claiming a negative or out-of-range "empty" score are exactly as
// untrustworthy as a corrupt occupied one.
bool wellFormed(const HighscoreTable& table) {
  bool seenEmpty = false;
  int32_t previousScore = kMaxStoredScore + 1;
  for (int32_t i = 0; i < kTableSize; ++i) {
    const HighscoreEntry& entry = table.entries[i];
    if (!isOccupied(entry)) {
      if (entry.score != 0) return false;
      seenEmpty = true;
      continue;
    }
    if (seenEmpty) return false;  // an occupied entry after an empty one -- a hole
    if (entry.score > kMaxStoredScore) return false;
    if (entry.score > previousScore) return false;  // must be non-increasing
    previousScore = entry.score;
    for (int32_t k = 0; k < kInitialsCount; ++k) {
      const char c = entry.initials[k];
      if (c < kFirstLetter || c >= kFirstLetter + kLetterCount) return false;
    }
  }
  return true;
}

}  // namespace

void encodeBlock(const HighscoreBlock& block, uint8_t (&out)[kBlockSize]) {
  int32_t offset = 0;
  writeU32LE(&out[offset], kMagic);
  offset += kBytesPerU32;
  writeU32LE(&out[offset], static_cast<uint32_t>(kFormatVersion));
  offset += kBytesPerU32;
  writeU32LE(&out[offset], static_cast<uint32_t>(kGameSlotCount));
  offset += kBytesPerU32;

  for (int32_t slot = 0; slot < kGameSlotCount; ++slot) {
    for (int32_t rank = 0; rank < kTableSize; ++rank) {
      const HighscoreEntry& entry = block.tables[slot].entries[rank];
      writeU32LE(&out[offset], static_cast<uint32_t>(entry.score));
      offset += kBytesPerU32;
      for (int32_t k = 0; k < kInitialsCount; ++k) {
        out[offset++] = static_cast<uint8_t>(entry.initials[k]);
      }
    }
  }

  const uint32_t checksum = fnv1aChecksum(out, offset);
  writeU32LE(&out[offset], checksum);
  offset += kBytesPerU32;
}

bool decodeBlock(const uint8_t (&in)[kBlockSize], HighscoreBlock& block) {
  block = HighscoreBlock{};  // AC-1.3: any failure below leaves this untouched further

  int32_t offset = 0;
  const uint32_t magic = readU32LE(&in[offset]);
  offset += kBytesPerU32;
  if (magic != kMagic) return false;

  const uint32_t version = readU32LE(&in[offset]);
  offset += kBytesPerU32;
  if (version != static_cast<uint32_t>(kFormatVersion)) return false;

  const uint32_t slotCount = readU32LE(&in[offset]);
  offset += kBytesPerU32;
  if (slotCount != static_cast<uint32_t>(kGameSlotCount)) return false;

  const int32_t payloadEnd = offset + kTablesBytes;
  const uint32_t expectedChecksum = fnv1aChecksum(in, payloadEnd);
  const uint32_t storedChecksum = readU32LE(&in[payloadEnd]);
  if (storedChecksum != expectedChecksum) return false;

  HighscoreBlock decoded{};
  for (int32_t slot = 0; slot < kGameSlotCount; ++slot) {
    for (int32_t rank = 0; rank < kTableSize; ++rank) {
      HighscoreEntry& entry = decoded.tables[slot].entries[rank];
      entry.score = static_cast<int32_t>(readU32LE(&in[offset]));
      offset += kBytesPerU32;
      for (int32_t k = 0; k < kInitialsCount; ++k) {
        entry.initials[k] = static_cast<char>(in[offset++]);
      }
    }
    if (!wellFormed(decoded.tables[slot])) return false;
  }

  block = decoded;
  return true;
}

}  // namespace detail
}  // namespace steamcore
