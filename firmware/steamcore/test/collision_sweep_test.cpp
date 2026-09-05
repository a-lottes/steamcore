// sweepCollisions (plan §1 Decision 5, T6): every distinct unordered
// pair exactly once, delegating to checkCollision so the sweep and the
// pair test can never disagree.

#include "steamcore/collision.h"
#include "test_harness.h"

using steamcore::Entity;
using steamcore::sweepCollisions;

namespace {

constexpr int32_t kMaxN = 32;

int32_t triangularCount(int32_t n) { return n * (n - 1) / 2; }

// Every entity identical and overlapping-sized: every distinct pair
// among the first `n` overlaps.
void buildAllOverlapping(Entity (&entities)[kMaxN], int32_t n) {
  for (int32_t i = 0; i < n; ++i) entities[i] = Entity{0, 0, 100, 100};
}

// Spaced 1000 units apart: no two of the first `n` ever overlap.
void buildAllDisjoint(Entity (&entities)[kMaxN], int32_t n) {
  for (int32_t i = 0; i < n; ++i) entities[i] = Entity{i * 1000, 0, 10, 10};
}

// Counts invocations and asserts the two references it was given are
// never the same object (AC-3.1: never a pair against itself).
struct CountingFunctor {
  int32_t calls = 0;
  void operator()(Entity& a, Entity& b) {
    CHECK(&a != &b);
    ++calls;
  }
};

// Records which (i, j) index pair each invocation carried, by pointer
// arithmetic against the array's own base address -- checkCollision is
// always called with references into the original array, so this is
// exact, not a heuristic.
struct PairRecorder {
  static constexpr int32_t kMaxPairs = 32;
  int32_t firstIdx[kMaxPairs]{};
  int32_t secondIdx[kMaxPairs]{};
  int32_t count = 0;
  Entity* base = nullptr;

  void operator()(Entity& a, Entity& b) {
    if (count < kMaxPairs) {
      firstIdx[count] = static_cast<int32_t>(&a - base);
      secondIdx[count] = static_cast<int32_t>(&b - base);
    }
    ++count;
  }
};

}  // namespace

// AC-3.2: count == 0, with a valid non-null pointer, never fires.
STEAMCORE_TEST(collision_sweep_count_zero_never_fires) {
  Entity entities[3] = {{0, 0, 10, 10}, {0, 0, 10, 10}, {0, 0, 10, 10}};
  CountingFunctor counter;

  sweepCollisions(entities, 0, counter);

  CHECK_EQ(counter.calls, 0);
}

// AC-3.2: count == 1 never fires -- there is no second entity to pair.
STEAMCORE_TEST(collision_sweep_count_one_never_fires) {
  Entity entities[1] = {{0, 0, 10, 10}};
  CountingFunctor counter;

  sweepCollisions(entities, 1, counter);

  CHECK_EQ(counter.calls, 0);
}

// AC-3.2: a nullptr array is a no-op, not a crash.
STEAMCORE_TEST(collision_sweep_nullptr_is_noop) {
  CountingFunctor counter;

  sweepCollisions(static_cast<Entity*>(nullptr), 5, counter);

  CHECK_EQ(counter.calls, 0);
}

// AC-3.2: a negative count is a no-op, not a crash.
STEAMCORE_TEST(collision_sweep_negative_count_is_noop) {
  Entity entities[3] = {{0, 0, 10, 10}, {0, 0, 10, 10}, {0, 0, 10, 10}};
  CountingFunctor counter;

  sweepCollisions(entities, -1, counter);

  CHECK_EQ(counter.calls, 0);
}

// AC-3.1/AC-3.3: a hand-built 5-entity arrangement with a known set of
// overlapping pairs fires the callback for exactly that set, each pair
// exactly once -- entities 0/1 overlap, entities 2/3 overlap, entity 4
// and every other cross-pair is disjoint.
STEAMCORE_TEST(collision_sweep_fires_for_exactly_the_known_overlapping_pairs) {
  Entity entities[5] = {
      Entity{0, 0, 10, 10},      // 0: overlaps 1
      Entity{5, 5, 10, 10},      // 1: overlaps 0
      Entity{100, 100, 10, 10},  // 2: overlaps 3
      Entity{102, 100, 10, 10},  // 3: overlaps 2
      Entity{200, 200, 10, 10},  // 4: overlaps nothing
  };
  PairRecorder recorder;
  recorder.base = entities;

  sweepCollisions(entities, 5, recorder);

  CHECK_EQ(recorder.count, 2);
  CHECK_EQ(recorder.firstIdx[0], 0);
  CHECK_EQ(recorder.secondIdx[0], 1);
  CHECK_EQ(recorder.firstIdx[1], 2);
  CHECK_EQ(recorder.secondIdx[1], 3);
}

// AC-3.3: an all-overlapping arrangement fires exactly N(N-1)/2 times at
// N = 2, 3, 8 and 32 -- checked against the computed triangular-number
// formula, not four hardcoded counts.
STEAMCORE_TEST(collision_sweep_all_overlapping_fires_exactly_n_choose_2) {
  const int32_t ns[] = {2, 3, 8, 32};
  for (int32_t n : ns) {
    Entity entities[kMaxN];
    buildAllOverlapping(entities, n);
    CountingFunctor counter;

    sweepCollisions(entities, n, counter);

    CHECK_EQ(counter.calls, triangularCount(n));
  }
}

// AC-3.3: a fully-disjoint arrangement fires zero times at the same N
// values.
STEAMCORE_TEST(collision_sweep_all_disjoint_fires_zero_times) {
  const int32_t ns[] = {2, 3, 8, 32};
  for (int32_t n : ns) {
    Entity entities[kMaxN];
    buildAllDisjoint(entities, n);
    CountingFunctor counter;

    sweepCollisions(entities, n, counter);

    CHECK_EQ(counter.calls, 0);
  }
}
