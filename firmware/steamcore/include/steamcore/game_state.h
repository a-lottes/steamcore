#pragma once

#include "steamcore/game_loop.h"

// Example (a Game's own update, composing this in):
//   struct MyGame {
//     steamcore::GameSession session_;
//
//     void update(const steamcore::GameInput& input) {
//       const bool died = /* this game's own logic decides */ false;
//       session_.advance(input, died);
//       switch (session_.state()) {
//         case steamcore::GameState::READY:      /* draw "PRESS START" */ break;
//         case steamcore::GameState::PLAYING:    /* simulate */          break;
//         case steamcore::GameState::GAME_OVER:  /* draw "GAME OVER" */  break;
//       }
//     }
//     void render(steamcore::Framebuffer&) { /* ... */ }
//   };
//
// A game-over that must take effect within the same `update` calls
// `advance` after its own simulation, not before -- calling it first
// evaluates `sessionEnded` against last tick's state.

namespace steamcore {

// Exactly three values, no explicit numbers: this is never stored in a
// pixel buffer or written to flash, so no wire representation is needed,
// and giving no enumerator a `= <digit>` keeps every state name symbolic
// (NFR-4).
enum class GameState { READY, PLAYING, GAME_OVER };

// A minimal session-phase container games and future engine stories
// share instead of each inventing their own READY/PLAYING/GAME_OVER
// bookkeeping and, more importantly, their own button check -- which a
// naive `if (input.start)` gets wrong: `start` reads true on every tick
// a physical button stays held, not once per press.
//
// Contract:
//  - A fresh instance starts in READY (AC-1.1).
//  - READY -> PLAYING and GAME_OVER -> PLAYING both fire on a rising
//    edge of `input.start` -- the tick `start` newly becomes true, never
//    on a tick it merely stays true. A session that ends while `start`
//    is already held stays in GAME_OVER until `start` is released for at
//    least one tick and pressed again; the previous tick's `start` is
//    tracked internally, so a caller needs no bookkeeping of its own.
//    The very first tick a fresh instance ever receives counts as a
//    rising edge if `start` is true on it -- there being no prior tick
//    is an implicit "not pressed".
//  - GAME_OVER's restart goes directly to PLAYING, never through an
//    intermediate READY -- one press, per constitution Principle 4
//    ("restart always one button away").
//  - PLAYING -> GAME_OVER fires only when the caller passes
//    `sessionEnded = true` to `advance` -- never derived from
//    `GameInput`. This type does not decide *what* ends a session (that
//    is a future game's own logic); it only provides the transition.
//    `sessionEnded` is ignored in every state but PLAYING, so signalling
//    it twice in a row, or outside PLAYING, has no further effect.
//  - `input.fire` never affects the state, in any state.
//  - `advance` is the only entry point: at most one transition happens
//    per call, evaluated against the state the call began in.
//  - Usable and testable standalone -- a plain local `GameSession`
//    driven by a `GameInput` sequence, no `GameLoop<Game>` instantiation
//    or synthetic consumer required.
//  - Single-threaded, nothing throws, no error code, no dynamic
//    allocation -- same inherited contract as Framebuffer/GameLoop.
class GameSession {
 public:
  GameSession() = default;

  // Advances the session by one step. See the class contract above for
  // the exact transition rules.
  void advance(const GameInput& input, bool sessionEnded);

  GameState state() const { return state_; }

 private:
  GameState state_ = GameState::READY;
  bool prevStart_ = false;
};

}  // namespace steamcore
