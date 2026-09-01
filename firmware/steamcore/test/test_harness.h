#pragma once

// Minimal hand-rolled test harness. No allocation, no exceptions, no
// third-party code — see .spark/rendering-core/plan.md §1 for why.

namespace steamcore::test {

using TestFn = void (*)();

// Registers a test case under `name`. Called only from file-scope static
// initializers via STEAMCORE_TEST; never call directly.
int registerCase(const char* name, TestFn fn);

// Records the first CHECK/CHECK_EQ failure of the currently running test.
// Subsequent failures in the same test are ignored: the first failure is
// the one the failing line number should point at.
void recordFailure(const char* file, int line, const char* expr);

// Runs every registered case whose name contains `filter` as a substring
// (an empty filter runs all cases). Prints one FAIL line per failing case
// and a final "<passed> passed, <failed> failed" line. Returns 0 only if
// the run is trustworthy: the number of failed cases, or 1 if a non-empty
// filter matched no case at all or more cases were registered than the
// fixed case array holds. Both of the latter two ran fewer tests than the
// caller asked for and must not be reported as success.
int runAll(const char* filter);

}  // namespace steamcore::test

#define STEAMCORE_TEST(name)                                               \
  static void steamcore_test_##name();                                    \
  static int steamcore_test_reg_##name =                                  \
      ::steamcore::test::registerCase(#name, steamcore_test_##name);      \
  static void steamcore_test_##name()

#define CHECK(cond)                                                        \
  do {                                                                     \
    if (!(cond)) {                                                         \
      ::steamcore::test::recordFailure(__FILE__, __LINE__, #cond);         \
    }                                                                      \
  } while (false)

#define CHECK_EQ(a, b)                                                      \
  do {                                                                     \
    if (!((a) == (b))) {                                                   \
      ::steamcore::test::recordFailure(__FILE__, __LINE__, #a " == " #b);  \
    }                                                                      \
  } while (false)
