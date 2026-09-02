// Deliberately outside `make test`: a timing assertion inside the
// correctness gate is a flake generator (plan §4 Test Strategy). Measures
// NFR-1: drawing a full 240x160 screen of text (30x20 = 600 characters,
// the maximum this virtual resolution can hold) must complete in < 5 ms
// with -O2 on the reference host.

#include <chrono>
#include <cstdio>

#include "steamcore/font.h"
#include "steamcore/framebuffer.h"
#include "steamcore/text.h"

int main() {
  using steamcore::Color;
  using steamcore::drawText;
  using steamcore::Framebuffer;
  using steamcore::kGlyphAdvance;
  using steamcore::kGlyphHeight;
  using Clock = std::chrono::high_resolution_clock;

  // Derived, not hand-typed: NFR-1's budget is "a full 240x160 screen", and
  // a hand-typed 30/20 would silently keep measuring the old screen area
  // after a resolution or glyph-size change instead of the current one,
  // with `make bench` still printing BENCH OK (review F7).
  constexpr int32_t kColumns = Framebuffer::width() / kGlyphAdvance;
  constexpr int32_t kRows = Framebuffer::height() / kGlyphHeight;

  char line[kColumns + 1];
  for (int32_t i = 0; i < kColumns; ++i) {
    line[i] = static_cast<char>('A' + (i % 26));
  }
  line[kColumns] = '\0';

  Framebuffer fb;

  auto drawFullScreen = [&]() {
    for (int32_t r = 0; r < kRows; ++r) {
      drawText(fb, 0, r * kGlyphHeight, line, Color::BRIGHT_ORANGE);
    }
  };

  // One warm-up pass, then measure.
  volatile int warmupSink = 0;
  drawFullScreen();
  warmupSink += fb.pixel(0, 0) != Color::BLACK ? 1 : 0;

  const auto start = Clock::now();
  drawFullScreen();
  const auto end = Clock::now();
  warmupSink += fb.pixel(0, 0) != Color::BLACK ? 1 : 0;

  const double ms =
      std::chrono::duration<double, std::milli>(end - start).count();
  std::printf("text render: %.4f ms for %d characters (budget: < 5 ms, NFR-1)\n",
              ms, kColumns * kRows);

  if (ms >= 5.0) {
    std::printf("BENCH FAILED: exceeded the 5 ms budget\n");
    return 1;
  }
  std::printf("BENCH OK\n");
  return 0;
}
