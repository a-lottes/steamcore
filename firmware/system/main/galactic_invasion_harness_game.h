#pragma once

#include "esp_log.h"
#include "galactic_invasion/galactic_invasion.h"
#include "steamcore/framebuffer.h"
#include "steamcore/game_loop.h"

// T16: device-build insurance only (CLAUDE.md "a host-only module can
// still hide a device-build bug") -- this file's entire purpose is to
// give the ESP-IDF toolchain something that actually #includes
// galactic_invasion.h/.cpp, so idf.py build is the first time this
// feature's collision.h usage and constexpr sprite-art validator are
// compiled by that toolchain rather than only host clang/g++. Not
// flashed, not run on hardware, no AC depends on it.
//
// GalacticInvasion already satisfies GameLoop<Game>'s Game concept
// directly (update(GameInput)/render(Framebuffer&)) and owns its own
// GameSession internally (NFR-5: no accessor exposes it) -- so this
// wrapper's only job, mirroring title_screen_harness_game.h's pattern,
// is to manufacture a synthetic `start` pulse, since no physical button
// is wired to this harness. GameSession requires a rising edge
// (game_state.h), so the pulse is exactly one tick high, then low again
// -- never held, which would only be seen as the same single edge.
namespace steamcore::test {
namespace detail {
constexpr char kGalacticInvasionHarnessLogTag[] = "galactic_invasion_harness";
}  // namespace detail

class GalacticInvasionHarnessGame {
 public:
  void update(const GameInput&) {
    GameInput input{};
    input.start = shouldPulseStart();
    input.fire = true;
    game_.update(input);
  }

  void render(Framebuffer& fb) { game_.render(fb); }

 private:
  // Not a timing claim (NFR-3) -- a tick count chosen so a human reading
  // the log sees the READY screen for a few seconds before the
  // synthetic press, at whatever tick interval app_main.cpp documents.
  static constexpr int32_t kSyntheticStartAtTick = 150;

  bool shouldPulseStart() {
    ++tick_;
    if (tick_ != kSyntheticStartAtTick) return false;
    ESP_LOGI(detail::kGalacticInvasionHarnessLogTag,
             "synthetic start pulse at tick %d (no button wired)",
             static_cast<int>(tick_));
    return true;
  }

  steamcore::games::GalacticInvasion game_;
  int32_t tick_ = 0;
};

}  // namespace steamcore::test
