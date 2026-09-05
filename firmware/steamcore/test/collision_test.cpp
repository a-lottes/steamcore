#include "steamcore/collision.h"

#include <cstring>

#include "test_harness.h"

using steamcore::checkCollision;
using steamcore::Entity;
using steamcore::overlaps;

namespace {

// The smallest consumer that can prove a callback fired and how often --
// a reference-capturing lambda increments this, no dynamic allocation.
struct CallCounter {
  int32_t calls = 0;
};

struct OverlapCase {
  Entity a;
  Entity b;
  bool expected;
};

// Asserts `overlaps` agrees with `expected` in BOTH argument orders --
// the invariant no single-order call can catch (T2 DoD).
void checkBothOrders(const OverlapCase& c) {
  CHECK_EQ(overlaps(c.a, c.b), c.expected);
  CHECK_EQ(overlaps(c.b, c.a), c.expected);
}

}  // namespace

// AC-1.1: two clearly-separated entities never overlap.
STEAMCORE_TEST(collision_clearly_separated_entities_do_not_overlap) {
  const Entity a{0, 0, 10, 10};
  const Entity b{100, 100, 10, 10};
  CHECK(!overlaps(a, b));
}

// AC-1.1: two entities sharing exactly a 1x1 region overlap.
STEAMCORE_TEST(collision_entities_sharing_a_1x1_region_overlap) {
  const Entity a{0, 0, 10, 10};
  const Entity b{9, 9, 10, 10};
  CHECK(overlaps(a, b));
}

// AC-2.1/AC-2.2: an overlapping pair invokes the callback exactly once.
STEAMCORE_TEST(collision_overlapping_pair_invokes_callback_exactly_once) {
  Entity a{0, 0, 10, 10};
  Entity b{9, 9, 10, 10};
  CallCounter counter;

  checkCollision(a, b, [&counter](Entity&, Entity&) { ++counter.calls; });

  CHECK_EQ(counter.calls, 1);
}

// AC-2.1/AC-2.2: a non-overlapping pair never invokes the callback.
STEAMCORE_TEST(collision_non_overlapping_pair_never_invokes_callback) {
  Entity a{0, 0, 10, 10};
  Entity b{100, 100, 10, 10};
  CallCounter counter;

  checkCollision(a, b, [&counter](Entity&, Entity&) { ++counter.calls; });

  CHECK_EQ(counter.calls, 0);
}

// AC-1.3/AC-1.6: all four touching-edge configurations do not overlap, in
// either argument order -- the strict `<` vs `<=` boundary pinned from
// the "touching is false" side.
STEAMCORE_TEST(collision_touching_edges_do_not_overlap_either_order) {
  const OverlapCase cases[] = {
      // a's right edge == b's left edge.
      {Entity{0, 0, 10, 10}, Entity{10, 0, 10, 10}, false},
      // a's left edge == b's right edge.
      {Entity{10, 0, 10, 10}, Entity{0, 0, 10, 10}, false},
      // a's bottom edge == b's top edge.
      {Entity{0, 0, 10, 10}, Entity{0, 10, 10, 10}, false},
      // a's top edge == b's bottom edge.
      {Entity{0, 10, 10, 10}, Entity{0, 0, 10, 10}, false},
  };
  for (const OverlapCase& c : cases) checkBothOrders(c);
}

// AC-1.3/AC-1.6: shifting each touching pair by exactly one pixel toward
// each other overlaps, in either argument order -- the same boundary
// pinned from the "one pixel past touching is true" side.
STEAMCORE_TEST(collision_one_pixel_past_touching_overlaps_either_order) {
  const OverlapCase cases[] = {
      {Entity{0, 0, 10, 10}, Entity{9, 0, 10, 10}, true},
      {Entity{9, 0, 10, 10}, Entity{0, 0, 10, 10}, true},
      {Entity{0, 0, 10, 10}, Entity{0, 9, 10, 10}, true},
      {Entity{0, 9, 10, 10}, Entity{0, 0, 10, 10}, true},
  };
  for (const OverlapCase& c : cases) checkBothOrders(c);
}

// AC-1.3: two rects meeting only at a single corner point do not overlap
// (a diagonal touch is still a touch, not an overlap).
STEAMCORE_TEST(collision_corner_only_contact_does_not_overlap) {
  const OverlapCase c{Entity{0, 0, 10, 10}, Entity{10, 10, 10, 10}, false};
  checkBothOrders(c);
}

// AC-1.4: any entity with w <= 0 or h <= 0 overlaps nothing at all,
// including another such degenerate entity, in either argument order.
STEAMCORE_TEST(collision_zero_or_negative_size_never_overlaps_either_order) {
  const Entity normal{0, 0, 10, 10};
  const OverlapCase cases[] = {
      {Entity{0, 0, 0, 10}, normal, false},    // w == 0
      {Entity{0, 0, 10, 0}, normal, false},    // h == 0
      {Entity{0, 0, -5, 10}, normal, false},   // w < 0
      {Entity{0, 0, 10, -5}, normal, false},   // h < 0
      // Two identical zero-size entities at the same coordinate.
      {Entity{5, 5, 0, 0}, Entity{5, 5, 0, 0}, false},
      // A zero-size entity strictly inside a large one -- the only shape
      // in which the `== 0` half of the guard is load-bearing: without
      // any guard at all, this one reports true.
      {Entity{5, 5, 0, 0}, Entity{0, 0, 10, 10}, false},
      // A NEGATIVE-size entity whose inverted extent brackets a normal
      // one (x=10, w=-5 spans [10, 5), so bx0 < ax1 < ax0 < bx1 holds) --
      // the only shape in which the `< 0` half of the guard is
      // load-bearing. The four degenerate rows above all report false
      // even with the guard weakened to `== 0`, so without these two the
      // negative-size half of AC-1.4 would pass for the wrong reason
      // (review F1).
      {Entity{10, 0, -5, 10}, Entity{0, 0, 20, 10}, false},
      {Entity{0, 10, 10, -5}, Entity{0, 0, 10, 20}, false},
  };
  for (const OverlapCase& c : cases) checkBothOrders(c);
}

// AC-1.6: overlaps() is pure -- neither operand's bytes change across the
// call. Compared by CHECK-testable proof, not just the const& signature.
STEAMCORE_TEST(collision_overlaps_does_not_mutate_either_operand) {
  Entity a{3, 4, 10, 10};
  Entity b{9, 9, 10, 10};
  Entity aBefore = a;
  Entity bBefore = b;

  (void)overlaps(a, b);

  CHECK_EQ(std::memcmp(&a, &aBefore, sizeof(Entity)), 0);
  CHECK_EQ(std::memcmp(&b, &bBefore, sizeof(Entity)), 0);
}
