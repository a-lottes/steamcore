#include "test_harness.h"

// argv[1], if present, is a substring filter (AC-1.4): only tests whose
// name contains it run. Exit code is 0 iff every executed test passed
// (AC-1.1), non-zero otherwise (AC-1.2).
int main(int argc, char** argv) {
  const char* filter = argc > 1 ? argv[1] : "";
  const int failed = steamcore::test::runAll(filter);
  return failed == 0 ? 0 : 1;
}
