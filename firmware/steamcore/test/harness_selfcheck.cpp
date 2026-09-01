#include "test_harness.h"

// Deliberately-failing case, built into its own binary (never matched by
// the `*_test.cpp` wildcard the main suite uses) so `make test-negative`
// can prove the harness reports a failure loudly instead of silently
// passing. See plan §1 Decision 4 / Risks: the harness is our own code and
// must be shown to fail, not just shown to pass.
STEAMCORE_TEST(selfcheck_deliberate_failure) { CHECK(1 == 2); }
