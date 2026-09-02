#include "steamcore/game_state.h"

#include <cstring>

#include "steamcore/game_loop.h"
#include "test_harness.h"

using steamcore::GameInput;
using steamcore::GameSession;
using steamcore::GameState;

// AC-1.1: a fresh instance starts in READY.
STEAMCORE_TEST(game_state_fresh_session_starts_ready) {
  GameSession session;
  CHECK(session.state() == GameState::READY);
}

STEAMCORE_TEST(game_state_one_start_press_reaches_playing) {
  GameSession session;
  session.advance(GameInput{true, false}, false);
  CHECK(session.state() == GameState::PLAYING);
}

// File-local, like every other test file's helpers (clipping_test.cpp,
// framebuffer_test.cpp, game_loop_test.cpp, ...): all test/*_test.cpp
// link into ONE binary, so a namespace-scope helper here would collide
// with -- or silently violate the ODR against -- a same-named helper in
// a future test file (review F1).
namespace {

GameSession sessionInPlaying() {
  GameSession session;
  session.advance(GameInput{true, false}, false);
  return session;
}

// Reaches GAME_OVER via a released `start` (so the very next rising edge
// is unambiguous for tests that need one), unless `endWhileStartHeld` is
// requested -- see T5(d) below, the one case that actually needs the
// opposite.
GameSession sessionInGameOver(bool endWhileStartHeld = false) {
  GameSession session = sessionInPlaying();
  if (endWhileStartHeld) {
    session.advance(GameInput{true, false}, true);  // ends with start held
  } else {
    session.advance(GameInput{false, false}, true);  // start released first
  }
  return session;
}

// `sessionInPlaying()` reaches PLAYING via a rising `start`, which leaves
// `start` held (review F2): any `start = true` case built directly on it
// is a level, not a press, so a mid-play rising edge is never exercised.
// This releases `start` for one step first, so a caller that then passes
// `start = true` gets a genuine rising edge -- the "pressing" half of
// AC-3.2, not just the "holding" half.
GameSession sessionInPlayingWithStartReleased() {
  GameSession session = sessionInPlaying();
  session.advance(GameInput{false, false}, false);
  return session;
}

// AC-1.2: "exactly three states" is pinned at compile time, not by a
// runtime assertion nobody may run. The switch below has no `default`
// label, so -Wswitch (part of -Wall, escalated to an error by -Werror)
// makes a fourth enumerator a build failure -- demonstrated once during
// T2 by temporarily adding one, quoted in the plan's task note, then
// reverted, and re-verified independently at review round 1.
//
// Returns a label, not an integer (review F3): an int standing in for a
// state outside the type's own definition is exactly what NFR-4 forbids,
// and it would also pin an enumerator ordering plan §1 Decision 3
// deliberately left unspecified.
const char* labelOfState(GameState state) {
  switch (state) {
    case GameState::READY:
      return "READY";
    case GameState::PLAYING:
      return "PLAYING";
    case GameState::GAME_OVER:
      return "GAME_OVER";
  }
  return "";
}

}  // namespace

STEAMCORE_TEST(game_state_exactly_three_states_are_named_and_distinct) {
  CHECK(std::strcmp(labelOfState(GameState::READY), "READY") == 0);
  CHECK(std::strcmp(labelOfState(GameState::PLAYING), "PLAYING") == 0);
  CHECK(std::strcmp(labelOfState(GameState::GAME_OVER), "GAME_OVER") == 0);
}

// AC-2.1: a held button only fires the transition once, on the rising
// edge -- asserted after EVERY step, not only at the end, because the
// defect this AC exists to catch (a level-triggered check) is a
// transition happening on a step where none should.
STEAMCORE_TEST(game_state_held_start_transitions_once_on_rising_edge) {
  GameSession session;

  session.advance(GameInput{false, false}, false);  // step 1: not pressed
  CHECK(session.state() == GameState::READY);

  for (int32_t step = 2; step <= 10; ++step) {
    session.advance(GameInput{true, false}, false);  // held from step 2
    CHECK(session.state() == GameState::PLAYING);
  }
}

// AC-2.2: the very first tick a fresh instance ever receives counts as a
// rising edge if `start` is already true on it.
STEAMCORE_TEST(game_state_first_ever_tick_with_start_true_is_a_rising_edge) {
  GameSession session;
  session.advance(GameInput{true, false}, false);
  CHECK(session.state() == GameState::PLAYING);
}

// AC-2.3: with `start` false, no combination of `fire` moves the state.
STEAMCORE_TEST(game_state_ready_stays_ready_without_start) {
  GameSession sessionFireFalse;
  sessionFireFalse.advance(GameInput{false, false}, false);
  CHECK(sessionFireFalse.state() == GameState::READY);

  GameSession sessionFireTrue;
  sessionFireTrue.advance(GameInput{false, true}, false);
  CHECK(sessionFireTrue.state() == GameState::READY);
}

// AC-3.1: one sessionEnded=true signal ends the session on that step.
STEAMCORE_TEST(game_state_session_ended_signal_ends_playing_session) {
  GameSession session = sessionInPlaying();
  session.advance(GameInput{false, false}, true);
  CHECK(session.state() == GameState::GAME_OVER);
}

// AC-3.2: no GameInput combination, on its own, ends a session -- only
// the explicit sessionEnded signal does. Each combo starts from a
// released `start` (review F2), so the `start = true` combos are a
// genuine mid-play *press*, not a continuation of the press that reached
// PLAYING -- otherwise a PLAYING-arm mutant that reacts to a rising
// `start` (e.g. resetting to READY) would pass unnoticed, since every
// combo's `start` would already be held.
STEAMCORE_TEST(game_state_no_input_combination_ends_a_session) {
  const GameInput combos[4] = {
      GameInput{false, false},
      GameInput{true, false},
      GameInput{false, true},
      GameInput{true, true},
  };
  for (const GameInput& input : combos) {
    GameSession session = sessionInPlayingWithStartReleased();
    session.advance(input, /*sessionEnded=*/false);
    CHECK(session.state() == GameState::PLAYING);
  }
}

// AC-3.3: signalling session-ended twice in a row is idempotent -- the
// second signal changes nothing, it does not merely leave GAME_OVER as
// a coincidence.
STEAMCORE_TEST(game_state_repeated_session_ended_signal_is_idempotent) {
  GameSession session = sessionInPlaying();
  session.advance(GameInput{false, false}, true);
  CHECK(session.state() == GameState::GAME_OVER);

  session.advance(GameInput{false, false}, true);
  CHECK(session.state() == GameState::GAME_OVER);
}

// sessionEnded outside PLAYING (here, READY) has no effect -- §6 defines
// no such transition.
STEAMCORE_TEST(game_state_session_ended_in_ready_is_ignored) {
  GameSession session;
  session.advance(GameInput{false, false}, true);
  CHECK(session.state() == GameState::READY);
}

// AC-4.1: restart goes directly to PLAYING -- READY is never observed at
// any step of the whole sequence, not just "not observed at the end".
STEAMCORE_TEST(game_state_restart_never_passes_through_ready) {
  GameSession session = sessionInPlaying();
  session.advance(GameInput{false, false}, true);  // -> GAME_OVER
  CHECK(session.state() == GameState::GAME_OVER);

  session.advance(GameInput{true, false}, false);  // rising edge restart
  CHECK(session.state() == GameState::PLAYING);
}

// AC-4.2: holding start after a restart fires no second transition.
STEAMCORE_TEST(game_state_held_start_after_restart_fires_once) {
  GameSession session = sessionInGameOver();
  session.advance(GameInput{true, false}, false);  // rising edge restart
  CHECK(session.state() == GameState::PLAYING);

  for (int32_t step = 0; step < 5; ++step) {
    session.advance(GameInput{true, false}, false);  // still held
    CHECK(session.state() == GameState::PLAYING);
  }
}

// AC-4.3: only `start` restarts; `fire` alone does nothing in GAME_OVER.
STEAMCORE_TEST(game_state_fire_alone_does_not_restart) {
  GameSession session = sessionInGameOver();
  session.advance(GameInput{false, true}, false);
  CHECK(session.state() == GameState::GAME_OVER);
}

// T5(d): the one sequence that actually separates edge- from
// level-triggering. The session ends while `start` is already held --
// a level-triggered `if (input.start)` would restart on the very next
// step; the correct, edge-triggered behaviour stays in GAME_OVER until
// `start` is released for at least one step and pressed again. Every
// other sequence in T3/T5 produces an identical trace under either
// implementation.
STEAMCORE_TEST(game_state_ending_while_start_held_does_not_auto_restart) {
  GameSession session = sessionInGameOver(/*endWhileStartHeld=*/true);
  CHECK(session.state() == GameState::GAME_OVER);

  // start stays held for several more steps: must NOT restart.
  for (int32_t step = 0; step < 3; ++step) {
    session.advance(GameInput{true, false}, false);
    CHECK(session.state() == GameState::GAME_OVER);
  }

  // start released for one step: still GAME_OVER, no transition yet.
  session.advance(GameInput{false, false}, false);
  CHECK(session.state() == GameState::GAME_OVER);

  // start pressed again: NOW it restarts.
  session.advance(GameInput{true, false}, false);
  CHECK(session.state() == GameState::PLAYING);
}

// Header contract (review F4): "at most one transition happens per call,
// evaluated against the state the call began in". Starting from PLAYING,
// only `sessionEnded` decides the outcome -- a rising `start` on that
// same call must not matter, since `advance` evaluates PLAYING's rule
// against the state the call began in, not against a state a first
// transition already moved it to. A sequential if-chain rewrite that
// re-reads `state_` after each `if` would apply the READY->PLAYING rule
// (skipped, since the call begins in PLAYING, not READY), then the
// PLAYING->GAME_OVER rule (fires), then re-check the now-current
// GAME_OVER against its own rule and, seeing the same rising `start`,
// bounce straight back to PLAYING -- two transitions in one call
// producing the wrong final state while every other existing test still
// passes.
STEAMCORE_TEST(game_state_playing_ignores_rising_start_when_session_also_ends) {
  GameSession session = sessionInPlayingWithStartReleased();
  session.advance(GameInput{true, false}, /*sessionEnded=*/true);
  CHECK(session.state() == GameState::GAME_OVER);
}
