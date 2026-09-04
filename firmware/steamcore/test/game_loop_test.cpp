#include "steamcore/game_loop.h"

#include "steamcore/framebuffer.h"
#include "test_harness.h"

using steamcore::Color;
using steamcore::Framebuffer;
using steamcore::GameInput;
using steamcore::GameLoop;

namespace {

// The smallest consumer that can prove call order and count: two
// counters, no drawing, no state beyond the counts themselves.
struct CountingGame {
  int32_t updateCalls = 0;
  int32_t renderCalls = 0;

  void update(const GameInput&) { ++updateCalls; }
  void render(Framebuffer&) { ++renderCalls; }
};

// Records every update/render call as an event carrying which tick (by
// call count, 0-indexed) it belongs to -- a fixed-size log, no dynamic
// allocation, large enough for the N = 100 case (100 update + 100 render
// events).
struct LoggingGame {
  enum class Kind { kUpdate, kRender };
  struct Event {
    Kind kind;
    int32_t tickIndex;
  };

  static constexpr int32_t kMaxEvents = 256;
  Event log[kMaxEvents]{};
  int32_t logCount = 0;
  int32_t updateCalls = 0;
  int32_t renderCalls = 0;

  void update(const GameInput&) {
    if (logCount < kMaxEvents) log[logCount++] = Event{Kind::kUpdate, updateCalls};
    ++updateCalls;
  }
  void render(Framebuffer&) {
    if (logCount < kMaxEvents) log[logCount++] = Event{Kind::kRender, renderCalls};
    ++renderCalls;
  }
};

// Asserts the log is exactly `update(0), render(0), update(1), render(1),
// ..., update(n-1), render(n-1)` -- the actual content of AC-1.1, not just
// a count that a batched-then-interleaved bug could also satisfy.
void checkStrictInterleaving(const LoggingGame& game, int32_t n) {
  CHECK_EQ(game.logCount, 2 * n);
  for (int32_t i = 0; i < n; ++i) {
    CHECK(game.log[2 * i].kind == LoggingGame::Kind::kUpdate);
    CHECK_EQ(game.log[2 * i].tickIndex, i);
    CHECK(game.log[2 * i + 1].kind == LoggingGame::Kind::kRender);
    CHECK_EQ(game.log[2 * i + 1].tickIndex, i);
  }
}

// Records every GameInput `update` was called with, in order -- so a test
// can check tick i observed exactly index i's value, never a neighbor's
// or a cached/defaulted one.
struct RecordingGame {
  static constexpr int32_t kMaxRecorded = 16;
  GameInput recorded[kMaxRecorded]{};
  int32_t recordedCount = 0;

  void update(const GameInput& input) {
    if (recordedCount < kMaxRecorded) recorded[recordedCount++] = input;
  }
  void render(Framebuffer&) {}
};

bool inputEquals(const GameInput& a, const GameInput& b) {
  return a.start == b.start && a.fire == b.fire && a.select == b.select &&
         a.up == b.up && a.down == b.down && a.left == b.left &&
         a.right == b.right;
}

// Records the address `render` was called with, every tick.
struct AddressRecordingGame {
  static constexpr int32_t kMaxRecorded = 128;
  const Framebuffer* recorded[kMaxRecorded]{};
  int32_t recordedCount = 0;

  void update(const GameInput&) {}
  void render(Framebuffer& fb) {
    if (recordedCount < kMaxRecorded) recorded[recordedCount++] = &fb;
  }
};

// Draws one distinct pixel on tick 1 and a different one on tick 2, and
// never calls clear() -- if the mechanism ever implicitly cleared between
// ticks, the first pixel would be gone by the time tick 2's render runs.
struct TwoPixelGame {
  int32_t tickCount = 0;

  void update(const GameInput&) { ++tickCount; }
  void render(Framebuffer& fb) {
    if (tickCount == 1) fb.setPixel(5, 5, Color::ORANGE);
    if (tickCount == 2) fb.setPixel(10, 10, Color::BRIGHT_ORANGE);
  }
};

// `update` advances its own counter; `render` paints exactly that
// counter's current value. A pixel read back after tick i must reflect
// tick i's update, never a stale or future one.
struct StateVisibleGame {
  int32_t counter = 0;  // cycles 0..3, matching Color's four values

  void update(const GameInput&) { counter = (counter + 1) % 4; }
  void render(Framebuffer& fb) {
    fb.setPixel(0, 0, static_cast<Color>(counter));
  }
};

}  // namespace

STEAMCORE_TEST(game_loop_one_tick_calls_update_then_render_once) {
  CountingGame game;
  Framebuffer fb;
  GameLoop<CountingGame> loop(game, fb);

  CHECK_EQ(game.updateCalls, 0);
  CHECK_EQ(game.renderCalls, 0);

  loop.tick(GameInput{});

  CHECK_EQ(game.updateCalls, 1);
  CHECK_EQ(game.renderCalls, 1);
}

// AC-1.2: N = 0 means constructed, never ticked -- no call, no crash.
STEAMCORE_TEST(game_loop_n_zero_never_calls_update_or_render) {
  LoggingGame game;
  Framebuffer fb;
  GameLoop<LoggingGame> loop(game, fb);
  (void)loop;  // constructed; deliberately never ticked

  CHECK_EQ(game.updateCalls, 0);
  CHECK_EQ(game.renderCalls, 0);
  CHECK_EQ(game.logCount, 0);
}

STEAMCORE_TEST(game_loop_n_one_strict_interleaving) {
  LoggingGame game;
  Framebuffer fb;
  GameLoop<LoggingGame> loop(game, fb);

  loop.tick(GameInput{});

  CHECK_EQ(game.updateCalls, 1);
  CHECK_EQ(game.renderCalls, 1);
  checkStrictInterleaving(game, 1);
}

STEAMCORE_TEST(game_loop_n_hundred_strict_interleaving) {
  LoggingGame game;
  Framebuffer fb;
  GameLoop<LoggingGame> loop(game, fb);

  constexpr int32_t kN = 100;
  for (int32_t i = 0; i < kN; ++i) loop.tick(GameInput{});

  CHECK_EQ(game.updateCalls, kN);
  CHECK_EQ(game.renderCalls, kN);
  checkStrictInterleaving(game, kN);
}

// AC-3.1 / input-driver AC-1.3: GameInput{} default-constructs all seven
// fields false -- no floating or undefined state.
STEAMCORE_TEST(game_loop_input_default_constructs_all_seven_false) {
  GameInput input{};
  CHECK_EQ(input.start, false);
  CHECK_EQ(input.fire, false);
  CHECK_EQ(input.select, false);
  CHECK_EQ(input.up, false);
  CHECK_EQ(input.down, false);
  CHECK_EQ(input.left, false);
  CHECK_EQ(input.right, false);
}

// input-driver NFR-8: every already-shipped two-argument positional
// GameInput{a, b} literal keeps meaning exactly start=a, fire=b after the
// extension to seven fields -- not merely "still compiles". The trailing
// five fields aggregate-initialize to false.
STEAMCORE_TEST(game_loop_two_argument_literal_still_means_start_fire) {
  const GameInput input{true, false};
  CHECK_EQ(input.start, true);
  CHECK_EQ(input.fire, false);
  CHECK_EQ(input.select, false);
  CHECK_EQ(input.up, false);
  CHECK_EQ(input.down, false);
  CHECK_EQ(input.left, false);
  CHECK_EQ(input.right, false);
}

// AC-2.1 + AC-3.3: a sequence covering all four start/fire combinations,
// each distinct from the other three, driven across four separate ticks.
// Because every value in the sequence is distinct, tick i observing the
// wrong (neighboring, cached or defaulted) value is exactly what this
// test would catch -- not just a count.
STEAMCORE_TEST(game_loop_each_tick_observes_exactly_its_own_input) {
  const GameInput sequence[4] = {
      GameInput{false, false},
      GameInput{true, false},
      GameInput{false, true},
      GameInput{true, true},
  };

  RecordingGame game;
  Framebuffer fb;
  GameLoop<RecordingGame> loop(game, fb);

  for (const GameInput& input : sequence) loop.tick(input);

  CHECK_EQ(game.recordedCount, 4);
  for (int32_t i = 0; i < 4; ++i) {
    CHECK(inputEquals(game.recorded[i], sequence[i]));
  }
}

// AC-3.2: a single tick's input is observed as raw pass-through -- no
// debouncing, no edge-detection, no default substitution.
STEAMCORE_TEST(game_loop_single_tick_input_is_raw_pass_through) {
  RecordingGame game;
  Framebuffer fb;
  GameLoop<RecordingGame> loop(game, fb);

  loop.tick(GameInput{true, false});

  CHECK_EQ(game.recordedCount, 1);
  CHECK_EQ(game.recorded[0].start, true);
  CHECK_EQ(game.recorded[0].fire, false);
}

// AC-2.2: every render() call across 100 ticks receives the same
// Framebuffer instance -- same address, not merely identical content.
STEAMCORE_TEST(game_loop_render_always_receives_the_same_framebuffer_instance) {
  AddressRecordingGame game;
  Framebuffer fb;
  GameLoop<AddressRecordingGame> loop(game, fb);

  constexpr int32_t kN = 100;
  for (int32_t i = 0; i < kN; ++i) loop.tick(GameInput{});

  CHECK_EQ(game.recordedCount, kN);
  for (int32_t i = 0; i < kN; ++i) {
    CHECK(game.recorded[i] == &fb);
  }
}

// AC-2.3: the mechanism never implicitly clears between ticks -- a pixel
// drawn on tick 1 is still present after tick 2, even though tick 2's
// render never touches it and never calls clear().
STEAMCORE_TEST(game_loop_never_implicitly_clears_between_ticks) {
  TwoPixelGame game;
  Framebuffer fb;
  GameLoop<TwoPixelGame> loop(game, fb);

  loop.tick(GameInput{});
  loop.tick(GameInput{});

  CHECK(fb.pixel(5, 5) == Color::ORANGE);
  CHECK(fb.pixel(10, 10) == Color::BRIGHT_ORANGE);
}

// AC-2.4: render observes exactly the state update left for that same
// tick -- checked after every individual tick, not just the last.
STEAMCORE_TEST(game_loop_render_observes_this_ticks_update_state) {
  StateVisibleGame game;
  Framebuffer fb;
  GameLoop<StateVisibleGame> loop(game, fb);

  for (int32_t i = 0; i < 8; ++i) {
    loop.tick(GameInput{});
    const int32_t expectedCounter = (i + 1) % 4;
    CHECK(fb.pixel(0, 0) == static_cast<Color>(expectedCounter));
  }
}
