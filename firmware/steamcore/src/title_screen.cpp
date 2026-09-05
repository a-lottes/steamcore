#include "steamcore/title_screen.h"

#include "steamcore/color.h"
#include "steamcore/text.h"

namespace steamcore {

void drawTitleScreen(Framebuffer& fb, GameState state) {
  switch (state) {
    case GameState::READY:
      drawText(fb, kTitleWordmarkBounds.x, kTitleWordmarkBounds.y,
               detail::kWordmarkText, Color::BRIGHT_ORANGE);
      drawText(fb, kTitlePromptBounds.x, kTitlePromptBounds.y,
               detail::kPromptText, Color::BRIGHT_ORANGE);
      break;
    case GameState::PLAYING:
    case GameState::GAME_OVER:
      break;
  }
}

}  // namespace steamcore
