#pragma once

#include <cstdint>

// The engine-side, cross-game contract a Game-conforming type uses to
// report its round's outcome to anything composing above it (highscore-
// system plan §1 Decision 1) -- e.g. a HighscoreGame<Game, Store>
// wrapper. Additive only: a game exposing this adds exactly this and
// nothing else to its own public surface (galactic-invasion AC-5.2's
// amended NFR-5 exception is the first example).
//
// Example:
//   RoundResult result = game.roundResult();
//   if (result.ended) { /* game.score at the moment the round ended */ }
//
// Contract:
//  - `ended` mirrors whatever session-completion signal the game already
//    tracks internally (for a game built on game_state.h, its own
//    GameSession reaching GameState::GAME_OVER).
//  - `score` is the round's final score as of the tick `ended` first
//    became true, and stays stable from then until the game's own next
//    restart -- a game frozen at GAME_OVER does not keep scoring.
//  - Trivially copyable, no invariant between its two fields beyond the
//    above -- same inherited contract as every other steamcore value type.
namespace steamcore {

struct RoundResult {
  bool ended = false;
  int32_t score = 0;
};

}  // namespace steamcore
