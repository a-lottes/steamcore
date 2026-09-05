// Deliberately outside `make test`: a timing assertion inside the
// correctness gate is a flake generator (plan §4 Test Strategy). Measures
// NFR-1/NFR-12: overlaps() over >= 1,000,000 pairs, and sweepCollisions()
// over >= 100,000 passes across a 32-entity array, against the 60 Hz
// tick budget.
//
// overlaps() is pure and constexpr (plan §1 Decision 3) -- at -O2 a naive
// loop over compile-time-known inputs would be entirely deleted or
// hoisted to a single precomputed constant, reporting a fictitious ~0 ns
// that measured nothing (risk R1). Every input here is therefore derived
// at runtime from a `volatile`-seeded generator: a `volatile` read is
// never a constant expression, so no value derived from it can be
// constant-folded, and every accumulation is written through a
// `volatile` sink so the store itself can never be eliminated. The
// generator is a fixed-seed, explicit arithmetic recurrence (a classic
// LCG) -- deterministic and reproducible run to run, never a clock or an
// unseeded RNG (constitution §4, this file's own lint scope).
//
// Each measurement runs at N and 2N iterations and checks the reported
// time roughly doubles -- the check that the loop actually ran the real
// arithmetic each time, rather than the optimizer having found a
// shortcut this file's author did not anticipate.

#include <chrono>
#include <cstdint>
#include <cstdio>

#include "steamcore/collision.h"

using steamcore::Entity;
using steamcore::overlaps;
using steamcore::sweepCollisions;
using Clock = std::chrono::high_resolution_clock;

namespace {

constexpr int32_t kEntityPoolSize = 1024;
constexpr int32_t kSweepEntityCount = 32;

// Well above the DoD's >= 1,000,000 / >= 100,000 floors: a larger sample
// keeps the doubling-ratio check (below) from being dominated by OS
// scheduling noise on a shared host, which measurements in the low
// single-digit milliseconds were (observed ratios from 1.5x to 3.0x
// across repeated runs at the floor values alone).
constexpr int64_t kPairIterations = 8000000;
constexpr int32_t kSweepPasses = 400000;

// 10% of the 16.667 ms 60 Hz tick, in milliseconds.
constexpr double kSweepBudgetMs = (1000.0 / 60.0) * 0.10;

// A fixed-seed, explicit arithmetic recurrence -- never <random>, rand()
// or a clock (this file's own lint scope bans all three, same as every
// other determinism-sensitive file in this codebase). The `volatile`
// seed read is what makes every value this function ever returns
// unknowable to the optimizer at compile time.
int32_t nextLcgValue(uint32_t& state) {
  state = state * 1103515245u + 12345u;
  return static_cast<int32_t>(state % 2000u) - 1000;  // [-1000, 999]
}

void fillEntityPool(Entity (&pool)[kEntityPoolSize], uint32_t seed) {
  for (int32_t i = 0; i < kEntityPoolSize; ++i) {
    const int32_t x = nextLcgValue(seed);
    const int32_t y = nextLcgValue(seed);
    const int32_t w = (nextLcgValue(seed) % 40) + 10;  // always positive
    const int32_t h = (nextLcgValue(seed) % 40) + 10;  // always positive
    pool[i] = Entity{x, y, w < 0 ? -w + 10 : w, h < 0 ? -h + 10 : h};
  }
}

// Runs `iterations` calls to overlaps() over consecutive pairs from
// `pool` (wrapping), accumulating into a volatile sink, and returns the
// elapsed time in milliseconds.
double measureOverlaps(const Entity (&pool)[kEntityPoolSize],
                        int64_t iterations) {
  volatile int64_t sink = 0;
  const auto start = Clock::now();
  for (int64_t i = 0; i < iterations; ++i) {
    const Entity& a = pool[i % kEntityPoolSize];
    const Entity& b = pool[(i + 1) % kEntityPoolSize];
    sink += overlaps(a, b) ? 1 : 0;
  }
  const auto end = Clock::now();
  (void)sink;
  return std::chrono::duration<double, std::milli>(end - start).count();
}

// Runs `passes` calls to sweepCollisions() over a fixed 32-entity array,
// accumulating the callback's fire count into a volatile sink, and
// returns the elapsed time in milliseconds.
double measureSweep(Entity (&entities)[kSweepEntityCount], int32_t passes) {
  volatile int64_t sink = 0;
  const auto start = Clock::now();
  for (int32_t p = 0; p < passes; ++p) {
    sweepCollisions(entities, kSweepEntityCount,
                     [&sink](Entity&, Entity&) { sink += 1; });
  }
  const auto end = Clock::now();
  (void)sink;
  return std::chrono::duration<double, std::milli>(end - start).count();
}

}  // namespace

int main() {
  volatile uint32_t vSeed = 12345u;
  uint32_t seed = vSeed;

  Entity pool[kEntityPoolSize];
  fillEntityPool(pool, seed);

  Entity sweepEntities[kSweepEntityCount];
  for (int32_t i = 0; i < kSweepEntityCount; ++i) sweepEntities[i] = pool[i];

  bool ok = true;

  // --- overlaps(): N and 2N, checked for roughly-linear scaling ---
  const double overlapsMsAtN = measureOverlaps(pool, kPairIterations);
  const double overlapsMsAt2N = measureOverlaps(pool, 2 * kPairIterations);
  const double overlapsRatio = overlapsMsAt2N / (overlapsMsAtN > 0.0 ? overlapsMsAtN : 1e-9);
  const double nsPerCall = (overlapsMsAt2N * 1e6) / static_cast<double>(2 * kPairIterations);

  std::printf(
      "overlaps(): %.2f ns/call over %lld pairs (doubled to %lld: %.4f ms "
      "-> %.4f ms, ratio %.2fx)\n",
      nsPerCall, static_cast<long long>(kPairIterations),
      static_cast<long long>(2 * kPairIterations), overlapsMsAtN,
      overlapsMsAt2N, overlapsRatio);

  if (overlapsRatio < 1.3 || overlapsRatio > 3.5) {
    std::printf(
        "BENCH FAILED: overlaps() time did not scale roughly linearly with "
        "iteration count (ratio %.2fx) -- the loop may have been "
        "optimized away\n",
        overlapsRatio);
    ok = false;
  }

  // --- sweepCollisions(): N and 2N passes, same scaling check ---
  const double sweepMsAtN = measureSweep(sweepEntities, kSweepPasses);
  const double sweepMsAt2N = measureSweep(sweepEntities, 2 * kSweepPasses);
  const double sweepRatio = sweepMsAt2N / (sweepMsAtN > 0.0 ? sweepMsAtN : 1e-9);
  const double usPerSweep = (sweepMsAt2N * 1000.0) / static_cast<double>(2 * kSweepPasses);

  std::printf(
      "sweepCollisions(): %.2f us/sweep (32 entities) over %d passes "
      "(doubled to %d: %.4f ms -> %.4f ms, ratio %.2fx; budget: < %.4f ms)\n",
      usPerSweep, kSweepPasses, 2 * kSweepPasses, sweepMsAtN, sweepMsAt2N,
      sweepRatio, kSweepBudgetMs);

  if (sweepRatio < 1.3 || sweepRatio > 3.5) {
    std::printf(
        "BENCH FAILED: sweepCollisions() time did not scale roughly "
        "linearly with pass count (ratio %.2fx) -- the loop may have been "
        "optimized away\n",
        sweepRatio);
    ok = false;
  }

  if (usPerSweep / 1000.0 >= kSweepBudgetMs) {
    std::printf(
        "BENCH FAILED: one 32-entity sweep (%.4f ms) exceeded the 10%% tick "
        "budget (%.4f ms)\n",
        usPerSweep / 1000.0, kSweepBudgetMs);
    ok = false;
  }

  if (!ok) return 1;
  std::printf("BENCH OK\n");
  return 0;
}
