#pragma once

#include <cstddef>
#include <cstdint>

#include "steamcore/framebuffer.h"

// See docs/dump-format.md for the authoritative SCFB v1 byte layout —
// this header expresses that contract in code, it does not restate it.

namespace steamcore {

inline constexpr uint16_t kDumpFormatVersion = 1;
inline constexpr size_t kDumpHeaderSize = 10;

// Serializes `fb` into `out` as an SCFB v1 dump: a kDumpHeaderSize-byte
// little-endian header (magic "SCFB", version, width, height) followed
// by width*height row-major palette-index bytes. Reads only the public
// Framebuffer::pixel() API — no raw-byte accessor exists or is needed.
// Allocates nothing and touches no file; all I/O is the caller's concern.
//
// Returns the number of bytes written — always
// kDumpHeaderSize + Framebuffer::width() * Framebuffer::height() on
// success — or 0 if `capacity` is smaller than that, in which case `out`
// is left untouched.
size_t serializeDump(const Framebuffer& fb, uint8_t* out, size_t capacity);

}  // namespace steamcore
