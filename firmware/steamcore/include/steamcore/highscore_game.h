#pragma once

#include <cstdint>

#include "steamcore/color.h"
#include "steamcore/framebuffer.h"
#include "steamcore/game_loop.h"
#include "steamcore/highscore.h"
#include "steamcore/highscore_flow.h"
#include "steamcore/round_result.h"

// Composes a Highscore System *above* an unmodified `Game`, rather than
// inside it (highscore-system plan §1 Decision 2) -- `HighscoreGame`
// itself satisfies `GameLoop<Game>`'s own Game concept, so the shipped
// `GameLoop`/`GameSession`/`GameInput`/`Framebuffer` contracts stay
// composed exactly as shipped (NFR-4); `Game` gains only `roundResult()`
// (round_result.h) and needs no other change or knowledge of highscores.
//
// Example:
//   steamcore::games::GalacticInvasion game;
//   MyStore store;  // satisfies the Store shape below
//   steamcore::HighscoreGame<steamcore::games::GalacticInvasion, MyStore>
//       wrapped(game, store, /*slot=*/0, "GALACTIC INVASION");
//   steamcore::Framebuffer fb;
//   steamcore::GameLoop<decltype(wrapped)> loop(wrapped, fb);
//   loop.tick(steamcore::GameInput{true});  // start
//
// Contract:
//  - Exactly one `game.update()` call happens per tick, on every path
//    (never skipped): with the real input while the flow is inactive, or
//    while it is active and finishing this same tick (the `start` edge
//    that dismisses the flow is the same edge the wrapped game's own
//    `GameSession` restarts on -- skipping this would leave the game's
//    own edge-tracked restart state stale, needing a second press); with
//    a neutral, all-false `GameInput` on every other tick the flow is
//    active, so the wrapped game stays frozen without losing its own
//    internal book-keeping.
//  - The wrapped game's own `roundResult()` transition into `ended` is
//    read and acted on exactly once per ending: `qualifies` is asked at
//    most once per round, never once per tick for as long as GAME_OVER
//    holds.
//  - `render()` draws the flow, never the wrapped game, on any tick the
//    flow is active -- structurally, not by caller discipline: the
//    wrapped game's own end-of-round screen is simply never called.
//  - `Store` (a compile-time-bound concept, the same shape
//    `TilePusher<Transmitter>`/`InputReader<Source>` already established
//    -- no virtual, no vtable) needs `bool qualifies(int32_t slot, int32_t
//    score)` (consulted once per ending), `const HighscoreTable&
//    table(int32_t slot) const` and `bool record(int32_t slot, const char
//    (&initials)[kInitialsCount], int32_t score)` -- exactly
//    `HighscoreStore<Backend>`'s own public surface (highscore.h).
//  - Single-threaded, nothing throws, no error code, no dynamic
//    allocation -- same inherited contract as every other steamcore type.
namespace steamcore {

template <typename Game, typename Store>
class HighscoreGame {
 public:
  HighscoreGame(Game& game, Store& store, int32_t slot,
                const char* displayName)
      : game_(game), store_(store), slot_(slot), displayName_(displayName) {}

  void update(const GameInput& input) {
    if (flow_.active()) {
      const HighscoreFlowStep step = flow_.update(input);
      if (step.submitted) {
        char initials[kInitialsCount];
        flow_.initials(initials);
        store_.record(slot_, initials, flow_.score());
      }
      game_.update(step.finished ? input : GameInput{});
    } else {
      game_.update(input);
      const RoundResult result = game_.roundResult();
      if (!result.ended) {
        reported_ = false;
      } else if (!reported_) {
        reported_ = true;
        if (store_.qualifies(slot_, result.score)) {
          flow_.begin(result.score);
        }
      }
    }
  }

  void render(Framebuffer& fb) {
    if (flow_.active()) {
      fb.clear(Color::BLACK);
      flow_.render(fb, store_.table(slot_), displayName_);
    } else {
      game_.render(fb);
    }
  }

 private:
  Game& game_;
  Store& store_;
  int32_t slot_;
  const char* displayName_;
  HighscoreFlow flow_;
  bool reported_ = false;
};

}  // namespace steamcore
