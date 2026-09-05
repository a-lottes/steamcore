// Overflow safety of overlaps() at int32_t extremes, proven twice (plan
// §1 Decision 3, T3): once as a namespace-scope static_assert -- UB is
// not a constant expression, so a 32-bit narrowing anywhere in the
// arithmetic fails to *compile* -- and once as a runtime CHECK on
// volatile-laundered inputs the optimizer cannot constant-fold, so the
// real add instructions execute and -fsanitize=undefined can actually
// observe them.

#include <cstdint>
#include <limits>

#include "steamcore/collision.h"
#include "test_harness.h"

using steamcore::Entity;
using steamcore::overlaps;

namespace {

constexpr int32_t kMin = std::numeric_limits<int32_t>::min();
constexpr int32_t kMax = std::numeric_limits<int32_t>::max();

}  // namespace

// --- Compile-time half: each case below also appears as a runtime CHECK
// in collision_overflow_extremes_match_the_compile_time_proof below. ---

// Case A: two identical entities just below INT32_MAX overlap (they are
// the same rect). This is also the case whose CORRECT answer (true)
// differs from what a naive 32-bit `x + w` would give: `INT32_MAX - 1 +
// 2` overflows a 32-bit sum to INT32_MIN, which would make a naive
// ax0 < bx1 comparison read "huge positive < huge negative" and report
// false -- the exact wrong answer this proof exists to rule out.
static_assert(overlaps(Entity{kMax - 1, 0, 2, 2}, Entity{kMax - 1, 0, 2, 2}),
              "identical near-INT32_MAX entities must overlap");

// Case B: an entity just below INT32_MAX and one starting at INT32_MIN
// are astronomically far apart in x and never overlap.
static_assert(!overlaps(Entity{kMax - 1, 0, 2, 2}, Entity{kMin, 0, 2, 2}),
              "entities near opposite int32_t extremes must not overlap");

// Case C1: the largest representable entity (INT32_MIN origin, INT32_MAX
// extent) overlaps itself.
static_assert(
    overlaps(Entity{kMin, kMin, kMax, kMax}, Entity{kMin, kMin, kMax, kMax}),
    "the largest representable entity must overlap itself");

// Case C2: the same largest representable entity does not overlap a
// small, far-away neighbour well outside its extent.
static_assert(
    !overlaps(Entity{kMin, kMin, kMax, kMax}, Entity{1000000, 1000000, 10, 10}),
    "the largest representable entity must not overlap a far-away neighbour");

// Ordinary AC-1.5 coverage: negative coordinates well away from the
// extremes overlap exactly like their positive-coordinate mirror.
static_assert(overlaps(Entity{-1000, -1000, 50, 50}, Entity{-980, -980, 50, 50}),
              "negative-coordinate entities overlap the same as positive ones");

// --- Runtime half: the same five cases, with every input read through a
// `volatile` so the optimizer cannot constant-fold overlaps() away and
// the real widen-then-add code path actually executes under UBSan. ---

STEAMCORE_TEST(collision_overflow_extremes_match_the_compile_time_proof) {
  volatile int32_t vMaxMinus1 = kMax - 1;
  volatile int32_t vMin = kMin;
  volatile int32_t vMax = kMax;
  volatile int32_t vZero = 0;
  volatile int32_t vTwo = 2;
  volatile int32_t vFar = 1000000;
  volatile int32_t vTen = 10;
  volatile int32_t vNeg1000 = -1000;
  volatile int32_t vNeg980 = -980;
  volatile int32_t vFifty = 50;

  // Case A -- overlapping, and the case that would flip under naive
  // 32-bit wraparound.
  const Entity a1{vMaxMinus1, vZero, vTwo, vTwo};
  const Entity b1{vMaxMinus1, vZero, vTwo, vTwo};
  CHECK(overlaps(a1, b1));

  // Case B -- disjoint, opposite extremes.
  const Entity a2{vMaxMinus1, vZero, vTwo, vTwo};
  const Entity b2{vMin, vZero, vTwo, vTwo};
  CHECK(!overlaps(a2, b2));

  // Case C1 -- the largest representable entity overlaps itself.
  const Entity a3{vMin, vMin, vMax, vMax};
  const Entity b3{vMin, vMin, vMax, vMax};
  CHECK(overlaps(a3, b3));

  // Case C2 -- the largest representable entity vs. a far-away neighbour.
  const Entity a4{vMin, vMin, vMax, vMax};
  const Entity b4{vFar, vFar, vTen, vTen};
  CHECK(!overlaps(a4, b4));

  // Ordinary negative-coordinate coverage, well away from the extremes.
  const Entity a5{vNeg1000, vNeg1000, vFifty, vFifty};
  const Entity b5{vNeg980, vNeg980, vFifty, vFifty};
  CHECK(overlaps(a5, b5));
}
