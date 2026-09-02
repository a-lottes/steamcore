#pragma once

#include <cstdint>

#include "steamcore/framebuffer.h"
#include "steamcore/game_loop.h"

// Shared by the AC-4.1 determinism test and the NFR-1 bench, so the bench
// derives its tick count from here instead of hand-typing it a second
// time (text-rendering review F7). Test-only: not part of the engine's
// public surface.

namespace steamcore::test {

// >= 50 per AC-4.1's "at least 50 ticks". Not a multiple of 4, so the
// four-combination cycle below does not end mid-repeat and every replay
// covers a different phase of it than the last full cycle -- a detail
// that matters only in that it makes the fixture slightly less special-
// cased, not for correctness.
inline constexpr int32_t kReplayTicks = 53;

// Cycles all four start/fire combinations, in the same order US-3's own
// tests use.
inline GameInput replayInputAt(int32_t tickIndex) {
  // Positional aggregate init (start, fire), not designated initializers
  // -- {.start = ..., .fire = ...} is C++20, and constitution §3 pins
  // this project to one C++17 dialect for host and device alike (review
  // F3: neither host compiler could see the extension, since Apple
  // clang accepts it silently and /usr/bin/g++ is also clang).
  switch (tickIndex % 4) {
    case 0:
      return GameInput{false, false};
    case 1:
      return GameInput{true, false};
    case 2:
      return GameInput{false, true};
    default:
      return GameInput{true, true};
  }
}

// A deterministic consumer: `update` and `render` are a pure function of
// the object's own prior state and the tick's input only -- no wall-clock
// read, no random-number call anywhere in this type, which is what makes
// AC-4.2 true by construction rather than by a runtime check (T7's grep
// gate additionally proves no such call exists in this file at all).
// `start` walks an on-screen X position, `fire` walks Y, and every tick
// advances a 4-value colour index -- enough state interplay that a bug
// mutating call order, input delivery or framebuffer identity has
// somewhere to show up as a wrong pixel.
class ReplayGame {
 public:
  void update(const GameInput& input) {
    if (input.start) x_ = (x_ + 1) % Framebuffer::width();
    if (input.fire) y_ = (y_ + 1) % Framebuffer::height();
    colorIndex_ = (colorIndex_ + 1) % 4;
  }

  void render(Framebuffer& fb) {
    fb.setPixel(x_, y_, static_cast<Color>(colorIndex_));
  }

 private:
  int32_t x_ = 0;
  int32_t y_ = 0;
  int32_t colorIndex_ = 0;
};

}  // namespace steamcore::test
