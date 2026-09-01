// Deliberately outside `make test`: a timing assertion inside the
// correctness gate is a flake generator (plan §4 Test Strategy). Measures
// NFR-1: a full dirty scan of a completely changed 240x160 framebuffer
// must complete in < 5 ms with -O2 on the reference host.

#include <chrono>
#include <cstdio>

#include "steamcore/dirty_tracker.h"
#include "steamcore/framebuffer.h"

int main() {
  using steamcore::Color;
  using steamcore::DirtyTracker;
  using steamcore::Framebuffer;
  using Clock = std::chrono::high_resolution_clock;

  DirtyTracker tracker;
  Framebuffer fb;
  tracker.commit(fb, tracker.scan(fb));  // sync so every pixel below is a real change
  fb.clear(Color::ORANGE);               // every one of the 150 tiles is now dirty

  // One warm-up scan (page faults, cache warm-up), then measure.
  volatile int warmupSink = 0;
  {
    const auto mask = tracker.scan(fb);
    warmupSink += mask.test(0, 0) ? 1 : 0;
  }

  const auto start = Clock::now();
  const auto mask = tracker.scan(fb);
  const auto end = Clock::now();
  warmupSink += mask.test(0, 0) ? 1 : 0;

  const double ms =
      std::chrono::duration<double, std::milli>(end - start).count();
  std::printf("dirty scan: %.4f ms (budget: < 5 ms, NFR-1)\n", ms);

  if (ms >= 5.0) {
    std::printf("BENCH FAILED: exceeded the 5 ms budget\n");
    return 1;
  }
  std::printf("BENCH OK\n");
  return 0;
}
