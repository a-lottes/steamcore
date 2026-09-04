#pragma once

#include <type_traits>
#include <utility>

#include "steamcore/framebuffer.h"

// Example (a minimal consumer, driven for a handful of ticks):
//   struct MyGame {
//     void update(const steamcore::GameInput& input) {
//       if (input.start) { /* ... */ }
//     }
//     void render(steamcore::Framebuffer& fb) {
//       fb.setPixel(0, 0, steamcore::Color::ORANGE);
//     }
//   };
//
//   MyGame game;
//   steamcore::Framebuffer fb;
//   steamcore::GameLoop<MyGame> loop(game, fb);
//   loop.tick(steamcore::GameInput{true});  // start
//   loop.tick(steamcore::GameInput{});  // update then render, again

namespace steamcore {

// Seven raw, independent boolean signals -- no decoding, no diagonal
// encoding, no debouncing or edge-detection anywhere in this file
// (input-driver spec A2, A8). `start`/`fire` are declared first,
// deliberately: every already-shipped positional `GameInput{a, b}`
// literal keeps its original `start=a, fire=b` meaning unchanged after
// this extension (input-driver NFR-8) -- the five trailing fields
// aggregate-initialize to `false` for any such literal. `GameSession`
// (game_state.h) is the sole owner of edge-detection, and only for
// `start`; every other consumer of this type sees a level, never a
// pulse. The four direction fields make no attempt to resolve or forbid
// simultaneous presses into a single "diagonal" value -- holding two
// adjacent directions is just two fields reading `true` at once.
struct GameInput {
  bool start = false;
  bool fire = false;
  bool select = false;
  bool up = false;
  bool down = false;
  bool left = false;
  bool right = false;
};

static_assert(sizeof(GameInput) == 7 * sizeof(bool),
              "GameInput must carry exactly its seven named fields");

namespace detail {

// A wrong `update`/`render` signature on a consumer becomes this named
// static_assert instead of an unreadable template-expansion wall.
// Detects the *call expression*, not `&Game::update`/`&Game::render`
// themselves -- taking the address of a member is ill-formed whenever
// the member is missing, overloaded, a template, or private, which would
// make the whole static_assert unreachable in exactly the cases this
// check exists to name (review F2: verified against all four). The
// primary template's `void` default and the specialization's SFINAE'd
// `std::void_t` are the standard C++17 detection idiom -- non-public,
// same convention as any other implementation-detail helper: no caller
// ever names anything in this namespace, whether it lives beside its
// type (as here, header-only) or in its own file (as src/clip.h does for
// Framebuffer).
template <typename Game, typename = void>
inline constexpr bool kGameHasUpdate = false;
template <typename Game>
inline constexpr bool kGameHasUpdate<
    Game, std::void_t<decltype(std::declval<Game&>().update(
              std::declval<const GameInput&>()))>> = true;

template <typename Game, typename = void>
inline constexpr bool kGameHasRender = false;
template <typename Game>
inline constexpr bool kGameHasRender<
    Game, std::void_t<decltype(std::declval<Game&>().render(
              std::declval<Framebuffer&>()))>> = true;

}  // namespace detail

// Drives one `Game` against one `Framebuffer` with a fixed, deterministic
// per-tick call order: `tick()` calls the consumer's `update` once and
// then its `render` once, every time, in that order -- never the reverse,
// never batched, never skipped (constitution §3 Timing).
//
// Contract:
//  - A tick is a caller-driven discrete step, never a measured wall-clock
//    interval. This type reads no clock and makes no real-time pacing
//    claim; "N ticks" means the caller called `tick()` N times.
//  - The `Framebuffer&` passed at construction is the one and only buffer
//    every `render()` call draws into -- bound once, never swapped,
//    identical on every tick. This is a structural guarantee, not caller
//    discipline: `tick()` takes no buffer argument.
//  - The framebuffer is never implicitly cleared between ticks. If a
//    game wants a clear each frame, `render()` calls `fb.clear(...)`
//    itself, exactly like any other drawing it does.
//  - Single-threaded, nothing throws, no error code -- same inherited
//    contract as Framebuffer/Sprite/drawText. A `Game` whose `update`/
//    `render` cannot be called as `update(const GameInput&)`/
//    `render(Framebuffer&)` is a named compile error
//    (`detail::kGameHasUpdate`/`kGameHasRender` above), not a runtime
//    failure -- but "cannot be called as" is about the call expression,
//    not the exact parameter type: a `render` taking the framebuffer by
//    value or by `const Framebuffer&` still compiles, since a mutable
//    reference converts to either, and MUST NOT be written -- by-value
//    silently draws into a throwaway stack copy of the whole 38,400-byte
//    framebuffer, and `const&` can never draw at all (review F10).
//  - No dynamic allocation: `GameLoop` holds two references and nothing
//    else.
template <typename Game>
class GameLoop {
  static_assert(detail::kGameHasUpdate<Game>,
                "Game must have a member function "
                "update(const steamcore::GameInput&)");
  static_assert(detail::kGameHasRender<Game>,
                "Game must have a member function "
                "render(steamcore::Framebuffer&)");

 public:
  GameLoop(Game& game, Framebuffer& fb) : game_(game), fb_(fb) {}

  // Calls `game.update(input)` then `game.render(fb)`, in that order.
  void tick(const GameInput& input) {
    game_.update(input);
    game_.render(fb_);
  }

 private:
  Game& game_;
  Framebuffer& fb_;
};

}  // namespace steamcore
