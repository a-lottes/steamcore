#pragma once

#include "esp_log.h"
#include "steamcore/framebuffer.h"
#include "steamcore/game_loop.h"
#include "steamcore/game_state.h"

// Throwaway, non-public consumer for the input-driver on-device proof
// (spec US-4, AC-4.1-4.5). Owns a real, unmodified GameSession and
// drives it exactly the way game_state.h's own doc comment shows: each
// tick calls `session_.advance(input, sessionEnded)` and switches on
// `session_.state()`. `sessionEnded` is never derived from `GameInput`
// -- GameSession's own contract forbids that -- so this harness supplies
// its own synthetic trigger, exactly as AC-4.2 specifies ("via the
// harness's own synthetic sessionEnded trigger, not a real game"): once
// every kSyntheticGameOverEveryTicks ticks spent PLAYING.
//
// Logs every resulting GameSession transition (AC-4.1-4.3) and every
// debounced level change on the six signals GameSession itself never
// consumes -- fire, select, and the four directions (AC-4.3, AC-4.4).
// `start`'s own raw level is not separately logged: GameSession's
// transition log already covers what start does.
//
// render() is a deliberate no-op: this harness is log-only by design
// (plan §1 Alternatives, "drive the panel too" rejected as re-verifying
// an already-released feature). The Framebuffer GameLoop<Game> requires
// at construction exists only to satisfy that signature; nothing ever
// draws into it.
namespace steamcore::test {
namespace detail {
constexpr char kInputHarnessLogTag[] = "input_harness";
}  // namespace detail

class InputHarnessGame {
 public:
  void update(const GameInput& input) {
    const GameState previousState = session_.state();
    const bool sessionEnded = shouldSyntheticallyEnd(previousState);
    session_.advance(input, sessionEnded);

    if (session_.state() != previousState) {
      // review F3: `sessionEnded` is this harness's own synthetic trigger,
      // never a real press (see the class doc comment) -- every PLAYING ->
      // GAME_OVER transition therefore happens only when `sessionEnded` is
      // true here. Labeled explicitly rather than left implicit, so
      // AC-4.3's transition count is readable directly off the log: count
      // only the lines NOT marked "(synthetic...)" against the human's
      // counted deliberate START presses.
      if (sessionEnded) {
        ESP_LOGI(detail::kInputHarnessLogTag,
                 "GameSession: %s -> %s (synthetic sessionEnded trigger, "
                 "not a real press)",
                 stateName(previousState), stateName(session_.state()));
      } else {
        ESP_LOGI(detail::kInputHarnessLogTag, "GameSession: %s -> %s",
                 stateName(previousState), stateName(session_.state()));
      }
    }

    logLevelChange("fire", previousInput_.fire, input.fire);
    logLevelChange("select", previousInput_.select, input.select);
    logLevelChange("up", previousInput_.up, input.up);
    logLevelChange("down", previousInput_.down, input.down);
    logLevelChange("left", previousInput_.left, input.left);
    logLevelChange("right", previousInput_.right, input.right);
    previousInput_ = input;
  }

  void render(Framebuffer&) {}

 private:
  // Not a timing claim (NFR-3) -- a tick count, chosen so a human running
  // the harness at its documented tick interval sees a GAME_OVER without
  // waiting unreasonably long, and can still comfortably press START
  // again before the next one arrives.
  static constexpr int32_t kSyntheticGameOverEveryTicks = 150;

  bool shouldSyntheticallyEnd(GameState state) {
    if (state != GameState::PLAYING) return false;
    if (++playingTicks_ < kSyntheticGameOverEveryTicks) return false;
    playingTicks_ = 0;
    return true;
  }

  static void logLevelChange(const char* name, bool previous, bool current) {
    if (previous == current) return;
    ESP_LOGI(detail::kInputHarnessLogTag, "%s: %s", name,
             current ? "pressed" : "released");
  }

  static const char* stateName(GameState state) {
    switch (state) {
      case GameState::READY:
        return "READY";
      case GameState::PLAYING:
        return "PLAYING";
      case GameState::GAME_OVER:
        return "GAME_OVER";
    }
    return "?";
  }

  GameSession session_;
  GameInput previousInput_{};
  int32_t playingTicks_ = 0;
};

}  // namespace steamcore::test
