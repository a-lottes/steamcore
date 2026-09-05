// checkCollision's dispatch contract (plan §1 Decision 4, T5): argument
// order, all three callable kinds AC-2.4 names, the callback's own
// statelessness with respect to the mechanism, and idempotence.

#include <cstring>

#include "steamcore/collision.h"
#include "test_harness.h"

using steamcore::checkCollision;
using steamcore::Entity;

namespace {

// --- Free function callable ---
int32_t g_freeFunctionCalls = 0;
void freeFunctionCallback(Entity&, Entity&) { ++g_freeFunctionCalls; }

// --- Stateful function object callable ---
struct CountingFunctor {
  int32_t calls = 0;
  void operator()(Entity&, Entity&) { ++calls; }
};

// Records the addresses `checkCollision` invoked the callback with, in
// the order it invoked them -- compared by address, not by value, so an
// accidental copy is caught alongside a swap.
struct OrderRecordingFunctor {
  const Entity* firstSeen = nullptr;
  const Entity* secondSeen = nullptr;
  void operator()(Entity& x, Entity& y) {
    firstSeen = &x;
    secondSeen = &y;
  }
};

}  // namespace

// AC-2.4: a capturing lambda fires exactly once on overlap.
STEAMCORE_TEST(collision_dispatch_capturing_lambda_fires_once_on_overlap) {
  Entity a{0, 0, 10, 10};
  Entity b{9, 9, 10, 10};
  int32_t calls = 0;

  checkCollision(a, b, [&calls](Entity&, Entity&) { ++calls; });

  CHECK_EQ(calls, 1);
}

// AC-2.4: a capturing lambda never fires when the pair does not overlap.
STEAMCORE_TEST(collision_dispatch_capturing_lambda_never_fires_without_overlap) {
  Entity a{0, 0, 10, 10};
  Entity b{100, 100, 10, 10};
  int32_t calls = 0;

  checkCollision(a, b, [&calls](Entity&, Entity&) { ++calls; });

  CHECK_EQ(calls, 0);
}

// AC-2.4: a free function fires exactly once on overlap.
STEAMCORE_TEST(collision_dispatch_free_function_fires_once_on_overlap) {
  g_freeFunctionCalls = 0;
  Entity a{0, 0, 10, 10};
  Entity b{9, 9, 10, 10};

  checkCollision(a, b, freeFunctionCallback);

  CHECK_EQ(g_freeFunctionCalls, 1);
}

// AC-2.4: a free function never fires when the pair does not overlap.
STEAMCORE_TEST(collision_dispatch_free_function_never_fires_without_overlap) {
  g_freeFunctionCalls = 0;
  Entity a{0, 0, 10, 10};
  Entity b{100, 100, 10, 10};

  checkCollision(a, b, freeFunctionCallback);

  CHECK_EQ(g_freeFunctionCalls, 0);
}

// AC-2.4/AC-2.5: a stateful function object fires exactly once on
// overlap, and the CALLER'S OWN instance observes it -- proving
// `Callback&&` did not copy the functor away (§1 Decision 4).
STEAMCORE_TEST(collision_dispatch_stateful_functor_fires_once_and_is_not_copied) {
  Entity a{0, 0, 10, 10};
  Entity b{9, 9, 10, 10};
  CountingFunctor functor;

  checkCollision(a, b, functor);

  CHECK_EQ(functor.calls, 1);
}

// AC-2.4: a stateful function object never fires without overlap, and
// the caller's own instance stays at zero.
STEAMCORE_TEST(collision_dispatch_stateful_functor_never_fires_without_overlap) {
  Entity a{0, 0, 10, 10};
  Entity b{100, 100, 10, 10};
  CountingFunctor functor;

  checkCollision(a, b, functor);

  CHECK_EQ(functor.calls, 0);
}

// AC-2.1: the callback receives &a first and &b second, compared by
// address -- an accidental copy is also caught, not just a swap.
STEAMCORE_TEST(collision_dispatch_callback_receives_a_first_b_second) {
  Entity a{0, 0, 10, 10};
  Entity b{9, 9, 10, 10};
  OrderRecordingFunctor functor;

  checkCollision(a, b, functor);

  CHECK(functor.firstSeen == &a);
  CHECK(functor.secondSeen == &b);
}

// AC-2.1: the same fixed order holds even when b sits left of and above
// a -- asymmetric geometry a swap could otherwise hide behind.
STEAMCORE_TEST(collision_dispatch_order_holds_when_b_is_left_of_and_above_a) {
  Entity a{10, 10, 10, 10};
  Entity b{5, 5, 10, 10};
  OrderRecordingFunctor functor;

  checkCollision(a, b, functor);

  CHECK(functor.firstSeen == &a);
  CHECK(functor.secondSeen == &b);
}

// AC-2.5: running the same pair through the same callable twice, with no
// mutation between, detects identically both times, and neither entity's
// bytes change across either call.
STEAMCORE_TEST(collision_dispatch_is_idempotent) {
  Entity a{0, 0, 10, 10};
  Entity b{9, 9, 10, 10};
  const Entity aBefore = a;
  const Entity bBefore = b;
  CountingFunctor functor;

  checkCollision(a, b, functor);
  checkCollision(a, b, functor);

  CHECK_EQ(functor.calls, 2);
  CHECK_EQ(std::memcmp(&a, &aBefore, sizeof(Entity)), 0);
  CHECK_EQ(std::memcmp(&b, &bBefore, sizeof(Entity)), 0);
}
