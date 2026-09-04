#include "steamcore/game_state.h"
#include "steamcore/input.h"

#include "fake_input_source.h"
#include "test_harness.h"

using steamcore::GameSession;
using steamcore::GameState;
using steamcore::InputReader;
using steamcore::InputSignal;
using steamcore::test::FakeInputSource;

namespace {

// input-driver T5 (AC-2.3): composing InputReader's debounced output
// straight into the unmodified GameSession -- proving the two features
// combine with no glue logic beyond passing InputReader::read()'s result
// to GameSession::advance(). Every read() drives the source and the
// session forward by exactly one tick.
void driveTicks(InputReader<FakeInputSource>& reader, FakeInputSource& source,
                 GameSession& session, int32_t count) {
  for (int32_t i = 0; i < count; ++i) {
    session.advance(reader.read(source), false);
  }
}

}  // namespace

// A bouncing start press (noisy raw levels, same fixture shape as T4's)
// driven through InputReader into an unmodified GameSession from READY
// still produces exactly one READY -> PLAYING transition -- the noise
// is fully absorbed before GameSession ever sees an edge.
STEAMCORE_TEST(input_session_bouncing_start_press_causes_one_ready_to_playing) {
  FakeInputSource source;
  InputReader<FakeInputSource> reader;
  GameSession session;

  CHECK(session.state() == GameState::READY);

  // Noisy press: isolated blips, then two consecutive agreeing samples.
  const bool bounce[] = {true, false, true, false, true, true};
  for (bool raw : bounce) {
    source.setLevel(InputSignal::kStart, raw);
    session.advance(reader.read(source), false);
  }

  CHECK(session.state() == GameState::PLAYING);
}

// Holding start for many further ticks after the transition produces no
// second transition -- GameSession's own edge-detection (on the level
// InputReader now hands it) still fires once, not once per tick held.
STEAMCORE_TEST(input_session_held_start_after_transition_causes_no_second_transition) {
  FakeInputSource source;
  InputReader<FakeInputSource> reader;
  GameSession session;

  source.setLevel(InputSignal::kStart, true);
  driveTicks(reader, source, session, 2);  // debounce settles -> PLAYING
  CHECK(session.state() == GameState::PLAYING);

  for (int32_t i = 0; i < 20; ++i) {
    session.advance(reader.read(source), false);
    CHECK(session.state() == GameState::PLAYING);
  }
}

// A release-and-press-again produces the next transition: PLAYING ->
// GAME_OVER (via sessionEnded) -> PLAYING again on the next debounced
// rising edge.
STEAMCORE_TEST(input_session_release_then_press_again_causes_next_transition) {
  FakeInputSource source;
  InputReader<FakeInputSource> reader;
  GameSession session;

  source.setLevel(InputSignal::kStart, true);
  driveTicks(reader, source, session, 2);
  CHECK(session.state() == GameState::PLAYING);

  session.advance(reader.read(source), /*sessionEnded=*/true);
  CHECK(session.state() == GameState::GAME_OVER);

  // Release start, let it settle, then press again.
  source.setLevel(InputSignal::kStart, false);
  driveTicks(reader, source, session, 2);
  CHECK(session.state() == GameState::GAME_OVER);  // no edge yet

  source.setLevel(InputSignal::kStart, true);
  driveTicks(reader, source, session, 2);
  CHECK(session.state() == GameState::PLAYING);
}

// The same GAME_OVER -> PLAYING restart proven with a bouncing press,
// mirroring the READY -> PLAYING case above.
STEAMCORE_TEST(input_session_bouncing_start_press_restarts_from_game_over) {
  FakeInputSource source;
  InputReader<FakeInputSource> reader;
  GameSession session;

  source.setLevel(InputSignal::kStart, true);
  driveTicks(reader, source, session, 2);
  session.advance(reader.read(source), /*sessionEnded=*/true);
  CHECK(session.state() == GameState::GAME_OVER);

  source.setLevel(InputSignal::kStart, false);
  driveTicks(reader, source, session, 2);

  const bool bounce[] = {true, false, true, false, true, true};
  for (bool raw : bounce) {
    source.setLevel(InputSignal::kStart, raw);
    session.advance(reader.read(source), false);
  }

  CHECK(session.state() == GameState::PLAYING);
}
