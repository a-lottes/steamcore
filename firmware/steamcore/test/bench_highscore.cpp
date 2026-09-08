// Deliberately outside `make test`: a timing assertion inside the
// correctness gate is a flake generator (plan §4 Test Strategy, mirroring
// every other bench_*.cpp in this project). Measures NFR-1 for the three
// operations a real tick can actually pay for: `HighscoreStore::record()`
// (insert + encode + a fake write), `HighscoreStore::load()` (a fake read
// + decode + validate), and one `HighscoreGame::render()` of each of the
// two screens the flow ever shows -- all four checked against the full
// 16.667 ms 60 Hz tick budget (this feature adds at most one of these
// calls to any single tick, never several, so the full budget -- not
// collision-system's 10% share -- is the right bar).
//
// Every measurement runs at N and 2N iterations, from a fresh instance for
// each of the two runs, and checks the reported time roughly doubles --
// collision-system's own R1 lesson: `-O2` deleting or hoisting a loop over
// values the optimizer can prove unchanging would otherwise report a
// fictitious near-zero cost. `record()`/`load()` accumulate a `volatile`
// sink over their own boolean result; the render measurements accumulate
// one already-drawn pixel read through a `volatile` sink, the same shape
// bench_title_screen.cpp uses for a `void`-returning draw call.

#include <chrono>
#include <cstdio>

#include "galactic_invasion/galactic_invasion.h"

#include "fake_flash_backend.h"
#include "steamcore/color.h"
#include "steamcore/framebuffer.h"
#include "steamcore/game_loop.h"
#include "steamcore/highscore.h"
#include "steamcore/highscore_game.h"

using steamcore::Color;
using steamcore::Framebuffer;
using steamcore::GameInput;
using steamcore::GameLoop;
using steamcore::HighscoreGame;
using steamcore::HighscoreStore;
using steamcore::games::GalacticInvasion;
using steamcore::games::kHighscoreName;
using steamcore::games::kHighscoreSlot;
using steamcore::test::FakeFlashBackend;
using Clock = std::chrono::high_resolution_clock;
using Wrapped = HighscoreGame<GalacticInvasion, HighscoreStore<FakeFlashBackend>>;

namespace {

constexpr double kTickBudgetMs = 1000.0 / 60.0;  // NFR-1: the 60 Hz tick

GameInput fireInput() {
  GameInput input{};
  input.fire = true;
  return input;
}

double elapsedMs(Clock::time_point start, Clock::time_point end) {
  return std::chrono::duration<double, std::milli>(end - start).count();
}

// --- record()/load(): timed over `iterations` calls on one fresh store ---

double measureRecord(int32_t iterations) {
  FakeFlashBackend backend;
  HighscoreStore<FakeFlashBackend> store(backend);
  const char initials[3] = {'A', 'A', 'A'};

  volatile int64_t sink = 0;
  const auto start = Clock::now();
  for (int32_t i = 0; i < iterations; ++i) {
    sink += store.record(kHighscoreSlot, initials, 50000) ? 1 : 0;
  }
  const auto end = Clock::now();
  (void)sink;
  return elapsedMs(start, end);
}

double measureLoad(int32_t iterations) {
  FakeFlashBackend backend;
  HighscoreStore<FakeFlashBackend> store(backend);
  const char initials[3] = {'A', 'A', 'A'};
  store.record(kHighscoreSlot, initials, 50000);  // untimed: gives load() a real block to decode

  volatile int64_t sink = 0;
  const auto start = Clock::now();
  for (int32_t i = 0; i < iterations; ++i) {
    sink += store.load() ? 1 : 0;
  }
  const auto end = Clock::now();
  (void)sink;
  return elapsedMs(start, end);
}

// --- render(): a fresh wrapped instance driven (untimed) into ENTRY or
// TABLE, then `iterations` timed render() calls of that one static screen.

constexpr int32_t kMaxDriveTicks = 10000;

bool entryHeaderShown(const Framebuffer& fb) {
  // A loose "is anything BRIGHT_ORANGE on the header's own row" probe is
  // enough here (unlike the exact-pixel-match galactic_invasion_highscore_
  // test.cpp needs for correctness proofs): a false positive would only
  // ever make this bench measure ENTRY's render one tick later than the
  // very first opportunity, never a wrong screen.
  constexpr int32_t kHeaderRow = 3;  // highscore_screen.h's kEntryHeaderRow
  constexpr int32_t kGlyphHeight = 8;
  const int32_t y0 = kHeaderRow * kGlyphHeight;
  for (int32_t y = y0; y < y0 + kGlyphHeight; ++y) {
    for (int32_t x = 0; x < Framebuffer::width(); ++x) {
      if (fb.pixel(x, y) == Color::BRIGHT_ORANGE) return true;
    }
  }
  return false;
}

// Drives a fresh wrapped instance to a qualifying ending (ENTRY active),
// then -- if `throughTable` -- submits three default letters to reach
// TABLE. Returns the still-live instances by out-param so the caller can
// keep measuring against them.
struct WrappedFixture {
  FakeFlashBackend backend;
  HighscoreStore<FakeFlashBackend> store{backend};
  GalacticInvasion game;
  Wrapped wrapped{game, store, kHighscoreSlot, kHighscoreName};
  Framebuffer fb;
};

void driveToEntry(WrappedFixture& fixture) {
  GameLoop<Wrapped> loop(fixture.wrapped, fixture.fb);
  loop.tick(GameInput{});
  loop.tick(GameInput{/*start=*/true});
  for (int32_t i = 0; i < kMaxDriveTicks && !entryHeaderShown(fixture.fb); ++i) {
    loop.tick(fireInput());
  }
}

void driveToTable(WrappedFixture& fixture) {
  driveToEntry(fixture);
  GameLoop<Wrapped> loop(fixture.wrapped, fixture.fb);
  loop.tick(GameInput{});  // settle past InitialsEntry::begin()'s own latch
  for (int32_t i = 0; i < 3; ++i) {
    loop.tick(fireInput());
    loop.tick(GameInput{});
  }
}

double measureRenderEntry(int32_t iterations) {
  WrappedFixture fixture;
  driveToEntry(fixture);

  volatile int32_t sink = 0;
  const auto start = Clock::now();
  for (int32_t i = 0; i < iterations; ++i) {
    fixture.wrapped.render(fixture.fb);
  }
  const auto end = Clock::now();
  sink += fixture.fb.pixel(0, 0) != Color::BLACK ? 1 : 0;
  (void)sink;
  return elapsedMs(start, end);
}

double measureRenderTable(int32_t iterations) {
  WrappedFixture fixture;
  driveToTable(fixture);

  volatile int32_t sink = 0;
  const auto start = Clock::now();
  for (int32_t i = 0; i < iterations; ++i) {
    fixture.wrapped.render(fixture.fb);
  }
  const auto end = Clock::now();
  sink += fixture.fb.pixel(0, 0) != Color::BLACK ? 1 : 0;
  (void)sink;
  return elapsedMs(start, end);
}

// Runs `measure(N)`/`measure(2N)`, prints the per-call cost against the
// tick budget, checks the doubling ratio, and returns whether it passed.
bool runOne(const char* label, int32_t n, double (*measure)(int32_t)) {
  const double msAtN = measure(n);
  const double msAt2N = measure(2 * n);
  const double ratio = msAt2N / (msAtN > 0.0 ? msAtN : 1e-9);
  const double usPerCall = (msAt2N * 1000.0) / static_cast<double>(2 * n);

  std::printf(
      "%s: %.3f us/call over %d iterations (doubled to %d: %.4f ms -> "
      "%.4f ms, ratio %.2fx; budget: < %.4f ms)\n",
      label, usPerCall, n, 2 * n, msAtN, msAt2N, ratio, kTickBudgetMs);

  bool ok = true;
  if (ratio < 1.3 || ratio > 3.5) {
    std::printf(
        "BENCH FAILED: %s time did not scale roughly linearly with "
        "iteration count (ratio %.2fx) -- the loop may have been "
        "optimized away\n",
        label, ratio);
    ok = false;
  }
  if (usPerCall / 1000.0 >= kTickBudgetMs) {
    std::printf("BENCH FAILED: %s (%.4f ms) exceeded the 60Hz tick budget\n",
                label, usPerCall / 1000.0);
    ok = false;
  }
  return ok;
}

}  // namespace

int main() {
  bool ok = true;
  ok = runOne("HighscoreStore::record()", 20000, measureRecord) && ok;
  ok = runOne("HighscoreStore::load()", 20000, measureLoad) && ok;
  ok = runOne("HighscoreGame::render() [ENTRY]", 20000, measureRenderEntry) && ok;
  ok = runOne("HighscoreGame::render() [TABLE]", 20000, measureRenderTable) && ok;

  if (!ok) return 1;
  std::printf("BENCH OK\n");
  return 0;
}
