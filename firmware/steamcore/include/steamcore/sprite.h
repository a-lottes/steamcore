#pragma once

#include <cstdint>

#include "steamcore/color.h"

// Example (a tightly-packed 8x8 sprite):
//   static const steamcore::Color kIcon[8 * 8] = { /* ... */ };
//   steamcore::Sprite icon{kIcon, 8, 8, /*stride=*/8};
//   fb.blit(icon, x, y);
//
// Example (an 8x8 sub-rectangle at offset (8,8) of a 32-wide atlas):
//   steamcore::Sprite tile{&atlas[8 * 32 + 8], 8, 8, /*stride=*/32};
//   fb.blit(tile, x, y);  // advancing one sprite row skips a full atlas row

namespace steamcore {

// A read-only view onto sprite pixel data, same one-byte-per-pixel format
// as the framebuffer (Color, not a raw byte, so a mismatched palette
// cannot be passed by accident). `stride` is the number of Color entries
// between the start of consecutive source rows — not necessarily equal to
// `width` — so a Sprite can select a sub-rectangle out of a larger array
// (e.g. a sprite sheet or a font atlas) without copying; a tightly-packed
// sprite sets stride == width. `pixels` must stay valid for the lifetime
// of every Framebuffer::blit call that uses this Sprite, and must
// genuinely span `stride * height` valid Color elements — the type does
// not own, copy or bounds-check the data, so an overstated width/height/
// stride is a caller bug, not something a blit clipped to the screen can
// detect (see Framebuffer::blit). A POD descriptor with no out-of-line
// code: no .cpp file pairs with this header.
struct Sprite {
  const Color* pixels = nullptr;
  int32_t width = 0;
  int32_t height = 0;
  int32_t stride = 0;
};

}  // namespace steamcore
