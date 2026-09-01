#include "test_harness.h"

#include <cstdio>
#include <cstring>

namespace steamcore::test {

namespace {

constexpr int kMaxCases = 512;

struct Case {
  const char* name;
  TestFn fn;
};

struct Registry {
  Case cases[kMaxCases];
  int count = 0;
};

// Construct-on-first-use: avoids a namespace-scope global and its
// undefined cross-TU initialization order (see plan §1 Decision 8 and the
// registry precedent it sets).
Registry& registry() {
  static Registry r;
  return r;
}

bool g_currentFailed = false;
const char* g_failFile = nullptr;
int g_failLine = 0;
const char* g_failExpr = nullptr;

// Set when more than kMaxCases have tried to register: those cases were
// silently dropped and never ran. Reported as a hard failure in runAll()
// rather than staying invisible (review F2).
bool g_overflowed = false;

}  // namespace

int registerCase(const char* name, TestFn fn) {
  Registry& r = registry();
  if (r.count < kMaxCases) {
    r.cases[r.count++] = Case{name, fn};
  } else {
    g_overflowed = true;
  }
  return 0;
}

void recordFailure(const char* file, int line, const char* expr) {
  if (!g_currentFailed) {
    g_currentFailed = true;
    g_failFile = file;
    g_failLine = line;
    g_failExpr = expr;
  }
}

int runAll(const char* filter) {
  Registry& r = registry();
  const bool hasFilter = filter != nullptr && filter[0] != '\0';
  int passed = 0;
  int failed = 0;

  for (int i = 0; i < r.count; ++i) {
    const Case& c = r.cases[i];
    if (hasFilter && std::strstr(c.name, filter) == nullptr) {
      continue;
    }

    g_currentFailed = false;
    g_failFile = nullptr;
    g_failLine = 0;
    g_failExpr = nullptr;

    c.fn();

    if (g_currentFailed) {
      std::printf("FAIL %s (%s:%d: %s)\n", c.name, g_failFile, g_failLine,
                   g_failExpr);
      ++failed;
    } else {
      ++passed;
    }
  }

  std::printf("%d passed, %d failed\n", passed, failed);

  // A filter that matched nothing ran zero tests, not zero failures — a
  // typo or a renamed test must not report success on having verified
  // nothing (review F2, constitution §8: FILTER is the handle QA uses to
  // record one AC at a time).
  if (hasFilter && passed + failed == 0) {
    std::printf("ERROR: no test matched filter \"%s\"\n", filter);
    return 1;
  }

  if (g_overflowed) {
    std::printf("ERROR: more than %d test cases were registered; some "
                "were dropped and never ran\n",
                kMaxCases);
    return 1;
  }

  return failed;
}

}  // namespace steamcore::test
