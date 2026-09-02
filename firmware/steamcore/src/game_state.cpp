#include "steamcore/game_state.h"

namespace steamcore {

void GameSession::advance(const GameInput& input, bool sessionEnded) {
  const bool startRising = input.start && !prevStart_;
  prevStart_ = input.start;

  switch (state_) {
    case GameState::READY:
      if (startRising) state_ = GameState::PLAYING;
      break;
    case GameState::PLAYING:
      if (sessionEnded) state_ = GameState::GAME_OVER;
      break;
    case GameState::GAME_OVER:
      if (startRising) state_ = GameState::PLAYING;
      break;
  }
}

}  // namespace steamcore
