#include "steamcore/highscore_flow.h"

#include "steamcore/color.h"
#include "steamcore/font.h"
#include "steamcore/framebuffer.h"
#include "steamcore/game_loop.h"
#include "steamcore/highscore.h"
#include "steamcore/highscore_screen.h"
#include "test_harness.h"

using steamcore::Color;
using steamcore::Framebuffer;
using steamcore::GameInput;
using steamcore::HighscoreFlow;
using steamcore::HighscoreFlowStep;
using steamcore::HighscoreTable;
using steamcore::drawHighscoreTableScreen;
using steamcore::drawInitialsEntryScreen;
using steamcore::detail::insert;
using steamcore::kInitialsCount;

namespace {

void put(HighscoreTable& table, const char* initials, int32_t score) {
  const char letters[3] = {initials[0], initials[1], initials[2]};
  insert(table, letters, score);
}

GameInput press(bool up, bool down, bool fire, bool start) {
  GameInput input{};
  input.up = up;
  input.down = down;
  input.fire = fire;
  input.start = start;
  return input;
}

GameInput neutral() { return GameInput{}; }

GameInput fireEdgeInput() { return press(false, false, true, false); }
GameInput startEdgeInput() { return press(false, false, false, true); }

}  // namespace

// Render before the first `begin()` draws nothing.
STEAMCORE_TEST(highscore_flow_render_before_begin_draws_nothing) {
  HighscoreFlow flow;
  Framebuffer fb;
  HighscoreTable table{};
  flow.render(fb, table, "GALACTIC INVASION");
  for (int32_t y = 0; y < Framebuffer::height(); ++y) {
    for (int32_t x = 0; x < Framebuffer::width(); ++x) {
      CHECK(fb.pixel(x, y) == Color::BLACK);
    }
  }
  CHECK(!flow.active());
}

// AC-2.1: the entry screen shows exactly the score passed to `begin()`,
// for a 1-digit and a 5-digit score, matching an independently-rendered
// reference via drawInitialsEntryScreen directly.
STEAMCORE_TEST(highscore_flow_entry_screen_shows_the_score_passed_to_begin) {
  const int32_t scores[] = {5, 99999};
  for (int32_t score : scores) {
    HighscoreFlow flow;
    flow.begin(score);
    Framebuffer actual;
    HighscoreTable table{};
    flow.render(actual, table, "GALACTIC INVASION");

    Framebuffer expected;
    steamcore::InitialsEntry freshEntry;
    freshEntry.begin();
    drawInitialsEntryScreen(expected, score, freshEntry);

    for (int32_t y = 0; y < Framebuffer::height(); ++y) {
      for (int32_t x = 0; x < Framebuffer::width(); ++x) {
        CHECK(actual.pixel(x, y) == expected.pixel(x, y));
      }
    }
  }
}

// A full scripted session: ENTRY -> (3 fire edges) -> TABLE -> (start
// edge) -> INACTIVE, with the expected frame asserted at each phase
// against an independently-built reference (never the production
// screen-drawing functions used to build the assertion the OTHER way
// around, since that would just prove the code agrees with itself).
STEAMCORE_TEST(highscore_flow_full_session_walks_entry_table_inactive) {
  HighscoreFlow flow;
  flow.begin(12500);
  CHECK(flow.active());

  HighscoreTable table{};
  Framebuffer fb;
  flow.render(fb, table, "GALACTIC INVASION");
  {
    Framebuffer expected;
    steamcore::InitialsEntry freshEntry;
    freshEntry.begin();
    drawInitialsEntryScreen(expected, 12500, freshEntry);
    for (int32_t y = 0; y < Framebuffer::height(); ++y) {
      for (int32_t x = 0; x < Framebuffer::width(); ++x) {
        CHECK(fb.pixel(x, y) == expected.pixel(x, y));
      }
    }
  }

  // Settle past begin()'s own edge latch, then submit three letters.
  flow.update(neutral());
  HighscoreFlowStep step{};
  for (int32_t i = 0; i < 3; ++i) {
    step = flow.update(fireEdgeInput());
    if (i < 2) {
      CHECK(!step.submitted);
      flow.update(neutral());  // release, so the next press is a fresh edge
    }
  }
  CHECK(step.submitted);
  CHECK(step.active);
  CHECK(flow.active());

  char initials[kInitialsCount];
  flow.initials(initials);
  CHECK_EQ(initials[0], 'A');
  CHECK_EQ(initials[1], 'A');
  CHECK_EQ(initials[2], 'A');

  // Now in TABLE: render must match drawHighscoreTableScreen directly.
  // `fb` is cleared first, matching the real caller's own contract
  // (HighscoreGame::render() clears before delegating to the flow) --
  // HighscoreFlow::render() itself never clears (this file's own header
  // contract), so a reused, not-yet-cleared framebuffer would otherwise
  // still carry the previous ENTRY screen's own leftover pixels.
  put(table, "AND", 500);
  fb.clear(Color::BLACK);
  flow.render(fb, table, "GALACTIC INVASION");
  {
    Framebuffer expected;
    drawHighscoreTableScreen(expected, "GALACTIC INVASION", table);
    for (int32_t y = 0; y < Framebuffer::height(); ++y) {
      for (int32_t x = 0; x < Framebuffer::width(); ++x) {
        CHECK(fb.pixel(x, y) == expected.pixel(x, y));
      }
    }
  }

  // A start edge on TABLE finishes the flow. TABLE's own `start` tracker
  // latches `true` the instant it is entered (this file's own R5-derived
  // contract), so one neutral tick settles past that latch before the
  // real edge -- otherwise this press would read as still-held, not
  // fresh, exactly the gap `highscore_flow_start_during_entry_changes_
  // nothing` below exists to prove is deliberate, not a bug.
  flow.update(neutral());
  const HighscoreFlowStep finishStep = flow.update(startEdgeInput());
  CHECK(finishStep.finished);
  CHECK(!finishStep.active);
  CHECK(!flow.active());

  Framebuffer afterFb;
  flow.render(afterFb, table, "GALACTIC INVASION");
  for (int32_t y = 0; y < Framebuffer::height(); ++y) {
    for (int32_t x = 0; x < Framebuffer::width(); ++x) {
      CHECK(afterFb.pixel(x, y) == Color::BLACK);
    }
  }
}

// AC-2.4: `start` pressed (and held) throughout ENTRY changes neither the
// phase nor any rendered pixel.
STEAMCORE_TEST(highscore_flow_start_during_entry_changes_nothing) {
  HighscoreFlow withStart;
  withStart.begin(100);
  HighscoreFlow withoutStart;
  withoutStart.begin(100);

  for (int32_t i = 0; i < 30; ++i) {
    const bool fire = (i % 5) == 0;
    withStart.update(press(false, false, fire, /*start=*/true));
    withoutStart.update(press(false, false, fire, /*start=*/false));
    CHECK_EQ(withStart.active(), withoutStart.active());

    HighscoreTable table{};
    Framebuffer fbWith;
    Framebuffer fbWithout;
    withStart.render(fbWith, table, "GALACTIC INVASION");
    withoutStart.render(fbWithout, table, "GALACTIC INVASION");
    for (int32_t y = 0; y < Framebuffer::height(); ++y) {
      for (int32_t x = 0; x < Framebuffer::width(); ++x) {
        CHECK(fbWith.pixel(x, y) == fbWithout.pixel(x, y));
      }
    }
  }
}
