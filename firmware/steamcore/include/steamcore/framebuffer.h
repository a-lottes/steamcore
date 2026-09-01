#pragma once

#include <cstdint>

#include "steamcore/color.h"
#include "steamcore/config.h"
#include "steamcore/sprite.h"

// Example:
//   steamcore::Framebuffer fb;               // starts all BLACK
//   fb.clear(steamcore::Color::BLACK);
//   fb.fillRect(10, 10, 32, 16, steamcore::Color::ORANGE);
//   fb.blit(playerSprite, playerX, playerY);  // BLACK pixels stay transparent
//   // fb.pixel(x, y) / raw storage is what a display driver (a later
//   // story) pushes to the panel, tile by tile, driven by DirtyTracker.

namespace steamcore {

// A fixed-size, freely-instantiable 240x160 framebuffer: one byte per
// pixel, 4-colour palette. No dynamic allocation, no global state, no
// static members — two independent instances never affect each other
// (constitution §3; spec A7 constrains the *engine* to one instance, but
// that is a policy the engine applies, not a restriction this type
// enforces itself).
//
// Contract that holds for every method below:
//  - Single-threaded. Nothing here synchronizes concurrent access; a
//    caller must not draw into a Framebuffer from one task while another
//    task reads or writes the same instance (constitution §3; revisited
//    once a real FreeRTOS task layout exists).
//  - Nothing throws and no method returns an error code. Invalid input
//    (an out-of-range coordinate, a non-positive size, a null sprite) is
//    silently clipped or ignored, never a crash and never UB — see each
//    method's own note for exactly what "invalid" means for it.
//  - Every coordinate and size is a plain int32_t. setPixel/pixel/fillRect
//    are correct for the *entire* range of that type, including
//    INT32_MIN/INT32_MAX and a size that would make x + width or
//    y + height overflow a 32-bit sum: the clipping arithmetic underneath
//    computes in int64_t before narrowing back (AC-2.9). blit()'s (x, y)
//    destination carries the same guarantee, but see blit()'s own note:
//    it additionally assumes `sprite.pixels` genuinely spans
//    `sprite.stride * sprite.height` valid Color elements — a Sprite
//    whose width/height/stride overstate its own backing memory is a
//    caller precondition violation this type cannot detect or clip away.
class Framebuffer {
 public:
  Framebuffer();

  // Canonical spelling for game/driver code: `kScreenWidth`/
  // `kScreenHeight` (config.h) exist for the engine's own storage sizing
  // and static_asserts, not as a second public API — call these instead
  // (review F10).
  static constexpr int32_t width() { return kScreenWidth; }
  static constexpr int32_t height() { return kScreenHeight; }

  // Sets every pixel to `color`.
  void clear(Color color);

  // Clipped: a coordinate outside [0, width()) x [0, height()) is a no-op
  // and never reads or writes out of bounds.
  void setPixel(int32_t x, int32_t y, Color color);

  // Clipped: an out-of-range coordinate returns Color::BLACK rather than
  // reading out of bounds.
  Color pixel(int32_t x, int32_t y) const;

  // Fills [x, x+w) x [y, y+h) with `color`, clipped to the framebuffer.
  // A width or height <= 0, or a rect entirely off-screen, is a no-op;
  // otherwise exactly the on-screen intersection is filled and every
  // off-screen row/column is left untouched.
  void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, Color color);

  // Draws `sprite` with its top-left corner at (x, y), clipped to the
  // framebuffer at all four edges — only the on-screen intersection is
  // read from `sprite.pixels` and written to the framebuffer. A source
  // pixel equal to `transparent` (default Color::BLACK) is skipped and
  // leaves the destination unchanged; every other source pixel overwrites
  // it. Passing a colour other than BLACK as `transparent` makes BLACK an
  // ordinary opaque colour for that call — e.g. a black silhouette can be
  // drawn over a lit background by passing the background colour as
  // `transparent`. `sprite.stride` is the number of Color entries between
  // source rows and may exceed `sprite.width`, selecting a sub-rectangle
  // out of a larger array (a sprite sheet or font atlas) without copying.
  // A null `sprite.pixels`, or a sprite entirely off-screen, is a no-op.
  // Precondition: `sprite.pixels` must actually span
  // `sprite.stride * sprite.height` valid Color elements. `x`/`y`
  // themselves may be any int32_t value, but a Sprite whose width/height
  // overstate its real backing memory is a caller bug this method cannot
  // detect — clipping only bounds what lands on the framebuffer, not how
  // far into `sprite.pixels` an off-screen origin is read from.
  void blit(const Sprite& sprite, int32_t x, int32_t y,
            Color transparent = Color::BLACK);

 private:
  uint8_t pixels_[kScreenWidth * kScreenHeight];
};

}  // namespace steamcore
