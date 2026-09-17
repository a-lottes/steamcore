// Deliberately outside `make test`: a timing assertion inside the
// correctness gate is a flake generator (plan §4 Test Strategy). Measures
// NFR-1: GameLoop<GalacticInvasion>::tick() (update + render) against the
// 16.667 ms 60 Hz tick budget, near the densest state a real round can
// reach: all 18 enemies alive, a player shot continuously in flight, and
// (since T10) the enemy-shot pool periodically full -- movement held
// every tick.
//
// The scripted input never lets the player's shot actually hit an enemy:
// the player is held at kPlayerStartX, which sits in the persistent gap
// between the two centre columns at every offsetX the formation ever
// reaches (galactic_invasion_combat_test.cpp's own finding) -- so the
// pool stays continuously exercised (spawn -> travel -> clear -> respawn)
// without ever destroying a survivor, keeping the enemy count pinned at
// 18 for the whole measured run rather than draining toward an
// increasingly-cheaper endgame.
//
// Same anti-optimization-away technique as bench_collision.cpp: every
// measured quantity passes through a `volatile` sink (Framebuffer::pixel
// reads cannot be constant-folded once the seed reaching them is
// volatile), and the measurement is taken at N and 2N ticks with the
// roughly-linear ratio checked, so -O2 cannot silently delete or hoist
// the loop (collision-system R1's lesson, reused here since T7's own
// combat tests already needed the same "prove the loop really ran"
// discipline for a different reason).

#include <chrono>
#include <cstdio>

#include "galactic_invasion/galactic_invasion.h"

#include "steamcore/framebuffer.h"
#include "steamcore/game_loop.h"

using steamcore::Color;
using steamcore::Framebuffer;
using steamcore::GameInput;
using steamcore::GameLoop;
using steamcore::games::GalacticInvasion;
using Clock = std::chrono::high_resolution_clock;

namespace {

// Never moves, always fires -- keeps the player at kPlayerStartX (the
// persistent inter-column gap) with a shot continuously cycling through
// the pool, and lets enemy fire (T10) accumulate independently.
GameInput scriptedInputAt(int32_t /*tick*/) {
  GameInput input{};
  input.fire = true;
  return input;
}

// Runs `ticks` calls to GameLoop<GalacticInvasion>::tick() on a fresh
// instance (so the first ~150 ticks of warm-up toward the dense steady
// state are a small, fixed fraction of any measured length worth
// comparing), accumulating a pixel read into a volatile sink so neither
// the loop nor its side effects can be optimized away, and returns the
// elapsed time in milliseconds.
double measureTicks(int32_t ticks) {
  GalacticInvasion game;
  Framebuffer fb;
  GameLoop<GalacticInvasion> loop(game, fb);
  loop.tick(GameInput{});
  loop.tick(GameInput{/*start=*/true});

  volatile int64_t sink = 0;
  const auto start = Clock::now();
  for (int32_t i = 0; i < ticks; ++i) {
    loop.tick(scriptedInputAt(i));
    sink += fb.pixel(0, 0) != Color::BLACK ? 1 : 0;
  }
  const auto end = Clock::now();
  (void)sink;
  return std::chrono::duration<double, std::milli>(end - start).count();
}

// galactic-invasion-artwork T11/NFR-1: the READY screen's own cost --
// one logo blit (up to 200x56) plus one drawText call, never PLAYING's
// dozens of sprites. Held in READY the whole run (input.start always
// false), so this measures a case measureTicks() above never reaches on
// its own. Same sink technique as measureTicks: the compiler cannot
// constant-fold a runtime Framebuffer read regardless of which pixel is
// read, so this deliberately reuses (0,0) rather than assuming exactly
// where inside kLogoBounds the generated art happens to be lit.
double measureReadyTicks(int32_t ticks) {
  GalacticInvasion game;
  Framebuffer fb;
  GameLoop<GalacticInvasion> loop(game, fb);

  volatile int64_t sink = 0;
  const auto start = Clock::now();
  for (int32_t i = 0; i < ticks; ++i) {
    loop.tick(GameInput{});
    sink += fb.pixel(0, 0) != Color::BLACK ? 1 : 0;
  }
  const auto end = Clock::now();
  (void)sink;
  return std::chrono::duration<double, std::milli>(end - start).count();
}

}  // namespace

int main() {
  constexpr int32_t kTicks = 5000;
  constexpr double kTickBudgetMs = 1000.0 / 60.0;

  const double msAtN = measureTicks(kTicks);
  const double msAt2N = measureTicks(2 * kTicks);
  const double ratio = msAt2N / (msAtN > 0.0 ? msAtN : 1e-9);
  const double usPerTick = (msAt2N * 1000.0) / static_cast<double>(2 * kTicks);

  std::printf(
      "galactic-invasion tick: %.2f us/tick over %d ticks (doubled to %d: "
      "%.4f ms -> %.4f ms, ratio %.2fx; budget: < %.4f ms/tick, 60 Hz "
      "NFR-1)\n",
      usPerTick, kTicks, 2 * kTicks, msAtN, msAt2N, ratio, kTickBudgetMs);
  std::printf(
      "  (host timing only -- ESP32-S3 on-device timing is neither "
      "measured nor claimed here)\n");

  bool ok = true;
  if (ratio < 1.3 || ratio > 3.5) {
    std::printf(
        "BENCH FAILED: tick() time did not scale roughly linearly with "
        "tick count (ratio %.2fx) -- the loop may have been optimized "
        "away\n",
        ratio);
    ok = false;
  }
  if (usPerTick / 1000.0 >= kTickBudgetMs) {
    std::printf(
        "BENCH FAILED: one tick (%.4f ms) exceeded the 16.667 ms 60 Hz "
        "budget\n",
        usPerTick / 1000.0);
    ok = false;
  }

  const double readyMsAtN = measureReadyTicks(kTicks);
  const double readyMsAt2N = measureReadyTicks(2 * kTicks);
  const double readyRatio =
      readyMsAt2N / (readyMsAtN > 0.0 ? readyMsAtN : 1e-9);
  const double readyUsPerTick =
      (readyMsAt2N * 1000.0) / static_cast<double>(2 * kTicks);

  std::printf(
      "galactic-invasion READY tick: %.2f us/tick over %d ticks (doubled "
      "to %d: %.4f ms -> %.4f ms, ratio %.2fx; budget: < %.4f ms/tick, 60 "
      "Hz NFR-1)\n",
      readyUsPerTick, kTicks, 2 * kTicks, readyMsAtN, readyMsAt2N,
      readyRatio, kTickBudgetMs);

  if (readyRatio < 1.3 || readyRatio > 3.5) {
    std::printf(
        "BENCH FAILED: READY tick() time did not scale roughly linearly "
        "with tick count (ratio %.2fx) -- the loop may have been "
        "optimized away\n",
        readyRatio);
    ok = false;
  }
  if (readyUsPerTick / 1000.0 >= kTickBudgetMs) {
    std::printf(
        "BENCH FAILED: one READY tick (%.4f ms) exceeded the 16.667 ms "
        "60 Hz budget\n",
        readyUsPerTick / 1000.0);
    ok = false;
  }

  if (!ok) return 1;
  std::printf("BENCH OK\n");
  return 0;
}
