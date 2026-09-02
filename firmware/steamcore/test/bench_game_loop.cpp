// Deliberately outside `make test`: a timing assertion inside the
// correctness gate is a flake generator (plan §4 Test Strategy). Measures
// NFR-1: two independent `GameLoop` replays of replay_fixture.h's
// deterministic consumer, kReplayTicks ticks each (the same replay
// AC-4.1's determinism test runs), must complete in < 5 ms with -O2 on
// the reference host. The budget names the `tick` calls only -- the
// comparison oracle's cost is measured and printed separately, not
// gated, since AC-4.1 is a correctness property and NFR-1 is a
// performance one about the mechanism itself.

#include <chrono>
#include <cstdio>

#include "fb_compare.h"
#include "replay_fixture.h"
#include "steamcore/framebuffer.h"
#include "steamcore/game_loop.h"

int main() {
  using steamcore::Color;
  using steamcore::Framebuffer;
  using steamcore::GameInput;
  using steamcore::GameLoop;
  using steamcore::test::framebuffersEqual;
  using steamcore::test::kReplayTicks;
  using steamcore::test::replayInputAt;
  using steamcore::test::ReplayGame;
  using Clock = std::chrono::high_resolution_clock;

  Framebuffer fbA;
  Framebuffer fbB;
  ReplayGame gameA;
  ReplayGame gameB;
  GameLoop<ReplayGame> loopA(gameA, fbA);
  GameLoop<ReplayGame> loopB(gameB, fbB);

  auto runReplay = [&]() {
    for (int32_t i = 0; i < kReplayTicks; ++i) {
      const GameInput input = replayInputAt(i);
      loopA.tick(input);
      loopB.tick(input);
    }
  };

  // One warm-up pass, then measure -- same shape as bench_text.cpp.
  volatile int warmupSink = 0;
  runReplay();
  warmupSink += fbA.pixel(0, 0) != Color::BLACK ? 1 : 0;

  const auto start = Clock::now();
  runReplay();
  const auto end = Clock::now();
  warmupSink += fbA.pixel(0, 0) != Color::BLACK ? 1 : 0;

  const double tickMs =
      std::chrono::duration<double, std::milli>(end - start).count();

  // Comparison cost, printed separately and never gated -- NFR-1 names
  // the `tick` calls, not this verification-only oracle.
  const auto compareStart = Clock::now();
  const bool equal = framebuffersEqual(fbA, fbB);
  const auto compareEnd = Clock::now();
  const double compareMs =
      std::chrono::duration<double, std::milli>(compareEnd - compareStart)
          .count();
  warmupSink += equal ? 1 : 0;
  (void)warmupSink;

  std::printf(
      "game loop replay: %.4f ms for %d ticks x 2 runs (budget: < 5 ms, "
      "NFR-1)\n",
      tickMs, kReplayTicks);
  // "for one framebuffersEqual call", not "x 2 runs" -- review F4: the
  // line above measures kReplayTicks x 2 tick() calls, this one measures
  // exactly one full-buffer comparison, and printing them stacked with
  // matching phrasing invited reading this one as also scaled by the
  // replay length (53x too low a reading on this host).
  std::printf(
      "  comparison cost for one framebuffersEqual call (not part of the "
      "budget): %.4f ms\n",
      compareMs);

  if (tickMs >= 5.0) {
    std::printf("BENCH FAILED: exceeded the 5 ms budget\n");
    return 1;
  }
  std::printf("BENCH OK\n");
  return 0;
}
