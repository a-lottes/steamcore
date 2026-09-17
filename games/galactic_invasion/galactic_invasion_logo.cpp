#include "galactic_invasion/galactic_invasion_logo.h"

#include "galactic_invasion/galactic_invasion_generated_art.h"
#include "steamcore/color.h"
#include "steamcore/text.h"

namespace steamcore::games {

void drawGalacticInvasionLogo(Framebuffer& fb, GameState state) {
  switch (state) {
    case GameState::READY:
      fb.blit(kLogoSprite, kLogoBounds.x, kLogoBounds.y);
      drawText(fb, kGiPromptBounds.x, kGiPromptBounds.y, detail::kPromptText,
               Color::BRIGHT_ORANGE);
      break;
    case GameState::PLAYING:
    case GameState::GAME_OVER:
      break;
  }
}

}  // namespace steamcore::games
