// Deliberately outside `make test`: a timing assertion inside the
// correctness gate is a flake generator (plan §4 Test Strategy). Measures
// NFR-1: drawTitleScreen(fb, READY) draws two short strings (20
// characters total) -- far cheaper per call than bench_text.cpp's full
// 600-character screen, so a single measured call would be too close to
// timer-resolution noise to trust. Looped kIterations times instead and
// reported as a per-call average, checked against the 16.67 ms 60 Hz
// tick budget (NFR-1) rather than bench_text.cpp/bench_game_loop.cpp's
// fixed 5 ms figure, which is this project's own budget for a
// full-screen operation, not a two-line one.

#include <chrono>
#include <cstdio>

#include "steamcore/framebuffer.h"
#include "steamcore/game_state.h"
#include "steamcore/title_screen.h"

int main() {
  using steamcore::Color;
  using steamcore::drawTitleScreen;
  using steamcore::Framebuffer;
  using steamcore::GameState;
  using Clock = std::chrono::high_resolution_clock;

  constexpr int32_t kIterations = 10000;
  constexpr double kTickBudgetMs = 1000.0 / 60.0;  // NFR-1: the 60 Hz tick

  Framebuffer fb;

  // One warm-up call, then measure -- same shape as bench_text.cpp.
  volatile int warmupSink = 0;
  drawTitleScreen(fb, GameState::READY);
  warmupSink += fb.pixel(0, 0) != Color::BLACK ? 1 : 0;

  const auto start = Clock::now();
  for (int32_t i = 0; i < kIterations; ++i) {
    drawTitleScreen(fb, GameState::READY);
  }
  const auto end = Clock::now();
  warmupSink += fb.pixel(0, 0) != Color::BLACK ? 1 : 0;
  (void)warmupSink;

  const double totalMs =
      std::chrono::duration<double, std::milli>(end - start).count();
  const double usPerCall = totalMs * 1000.0 / kIterations;

  std::printf(
      "title screen render: %.4f us/call over %d iterations (budget: << "
      "%.2f ms 60Hz tick, NFR-1)\n",
      usPerCall, kIterations, kTickBudgetMs);

  if (usPerCall / 1000.0 >= kTickBudgetMs) {
    std::printf("BENCH FAILED: exceeded the 60Hz tick budget\n");
    return 1;
  }
  std::printf("BENCH OK\n");
  return 0;
}
