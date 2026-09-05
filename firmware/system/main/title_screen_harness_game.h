#pragma once

#include "esp_log.h"
#include "steamcore/framebuffer.h"
#include "steamcore/game_loop.h"
#include "steamcore/game_state.h"
#include "steamcore/title_screen.h"

// The on-device proof for AC-3.1/AC-3.2 (start-screen T11): renders the
// unmodified drawTitleScreen(fb, session_.state()) into a real
// Framebuffer, exactly the game_state.h/title_screen.h composition
// pattern already established. Needs no physical buttons -- unlike
// input-driver's harness, this one drives GameSession's READY -> PLAYING
// transition with its own synthetic `start` pulse after a fixed tick
// count, so the disappearance this feature's whole point is (US-2) is
// observable on a panel that is already wired and proven (v0.2.0)
// without any wiring of its own.
//
// Labeled synthetic in the log, mirroring input-driver's
// InputHarnessGame -- a human reading the transcript must never mistake
// this for a real press.
namespace steamcore::test {
namespace detail {
constexpr char kTitleScreenHarnessLogTag[] = "title_screen_harness";
}  // namespace detail

class TitleScreenHarnessGame {
 public:
  // stateChanged_ starts true so the very first render (the initial
  // READY screen) is reported as a change worth dumping -- there is no
  // "previous" state before it to compare against otherwise.
  TitleScreenHarnessGame() : stateChanged_(true) {}

  void update(const GameInput&) {
    const GameState previousState = session_.state();
    const bool syntheticStart = shouldSyntheticallyPressStart(previousState);
    session_.advance(GameInput{syntheticStart}, /*sessionEnded=*/false);

    if (session_.state() != previousState) {
      ESP_LOGI(detail::kTitleScreenHarnessLogTag,
               "GameSession: %s -> %s (synthetic start pulse, no button "
               "wired)",
               stateName(previousState), stateName(session_.state()));
      stateChanged_ = true;
    }
  }

  void render(Framebuffer& fb) {
    fb.clear(Color::BLACK);
    drawTitleScreen(fb, session_.state());
  }

  GameState state() const { return session_.state(); }

  // True at most once per state change (and once, unconditionally, for
  // the very first tick) -- the caller uses this to decide when a fresh
  // SCFB dump is worth emitting, never once per tick (plan §5 Risk: the
  // dump is large enough that per-tick emission would be genuinely slow
  // over the serial console).
  bool consumeStateChanged() {
    const bool changed = stateChanged_;
    stateChanged_ = false;
    return changed;
  }

 private:
  // Not a timing claim (NFR-3) -- a tick count chosen so a human
  // watching the panel sees the READY screen for a few seconds before it
  // disappears, at whatever tick interval app_main.cpp documents.
  static constexpr int32_t kSyntheticStartAtTick = 150;

  bool shouldSyntheticallyPressStart(GameState state) {
    if (state != GameState::READY) return false;
    return ++readyTicks_ >= kSyntheticStartAtTick;
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
  int32_t readyTicks_ = 0;
  bool stateChanged_;
};

}  // namespace steamcore::test
