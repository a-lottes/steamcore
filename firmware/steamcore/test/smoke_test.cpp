#include "test_harness.h"

// Proves the harness itself can compile, register a case, run it and
// report a pass (AC-1.1) before any engine logic exists.
STEAMCORE_TEST(smoke_trivial) { CHECK(1 + 1 == 2); }
