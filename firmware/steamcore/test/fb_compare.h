#pragma once

#include "steamcore/framebuffer.h"

// Test-only comparison oracle for the determinism proof (US-4): not part
// of the engine's public surface (NFR-6) -- Framebuffer itself is not
// owned by the game-loop feature, so no comparison method is added to it
// here. Compares through the same public pixel() every ordinary caller
// uses, never a private/raw byte read, so the oracle proves nothing that
// the engine's own contract doesn't already expose.

namespace steamcore::test {

// True iff `a` and `b` agree on every pixel of the whole framebuffer. On
// a mismatch, writes the first differing coordinate (in row-major order)
// to `*mismatchX`/`*mismatchY` and returns false; `mismatchX`/`mismatchY`
// may be nullptr if the caller only wants the bool.
inline bool framebuffersEqual(const Framebuffer& a, const Framebuffer& b,
                               int32_t* mismatchX = nullptr,
                               int32_t* mismatchY = nullptr) {
  for (int32_t y = 0; y < Framebuffer::height(); ++y) {
    for (int32_t x = 0; x < Framebuffer::width(); ++x) {
      if (a.pixel(x, y) != b.pixel(x, y)) {
        if (mismatchX != nullptr) *mismatchX = x;
        if (mismatchY != nullptr) *mismatchY = y;
        return false;
      }
    }
  }
  return true;
}

}  // namespace steamcore::test
