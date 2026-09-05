#pragma once

#include "steamcore/color.h"
#include "steamcore/game_state.h"
#include "steamcore/title_screen.h"

// Test-only consumer proving US-2's composition (T5): owns a real,
// unmodified GameSession and drives it exactly the way game_state.h's
// own doc comment and title_screen.h's own usage example show --
// update() advances the session, render() clears then composes
// drawTitleScreen(fb, session_.state()). Not part of the engine's public
// surface (NFR-6) -- this lives in test/, not include/.
namespace steamcore::test {

class TitleScreenGame {
 public:
  void update(const GameInput& input) {
    session_.advance(input, /*sessionEnded=*/false);
  }

  void render(Framebuffer& fb) {
    fb.clear(Color::BLACK);
    drawTitleScreen(fb, session_.state());
  }

  GameState state() const { return session_.state(); }

 private:
  GameSession session_;
};

}  // namespace steamcore::test
