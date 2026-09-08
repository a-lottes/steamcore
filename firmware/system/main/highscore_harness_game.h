#pragma once

#include "esp_log.h"
#include "galactic_invasion/galactic_invasion.h"
#include "steamcore/framebuffer.h"
#include "steamcore/game_loop.h"
#include "steamcore/highscore.h"
#include "steamcore/highscore_game.h"
#include "steamcore/port/esp32/nvs_highscore_backend.h"

// T14: device-build insurance for the highscore-system feature (CLAUDE.md
// "a host-only module can still hide a device-build bug") -- this file's
// entire purpose is to give the ESP-IDF toolchain something that actually
// #includes highscore.h/highscore_game.h/highscore_flow.h/
// highscore_screen.h/initials_entry.h and the real NvsHighscoreBackend, so
// idf.py build is the first time this feature's code is compiled by that
// toolchain rather than only host clang/g++. Not flashed, not run on
// hardware, no AC depends on it -- galactic_invasion_harness_game.h's own
// precedent exactly.
//
// No buttons are wired to this harness (input-driver's real GpioInputSource
// is not composed here, mirroring every prior harness), so every input is
// synthetic: `fire` is a periodic square wave -- long enough on-phases to
// let GalacticInvasion actually re-fire while PLAYING (its own cooldown,
// not edge-gated), and the off-phase in between is exactly what gives
// InitialsEntry a fresh rising edge to submit a letter with once the flow
// reaches ENTRY. `start` is a slower square wave for the same reason: its
// first rising edge carries READY into PLAYING, and later ones are what
// let a finished TABLE screen restart -- the same single mechanism serves
// both roles, since HighscoreGame's own composition only ever forwards a
// tick's real input to the wrapped game while the flow is inactive or
// finishing (highscore_game.h's own contract).
namespace steamcore::test {
namespace detail {
constexpr char kHighscoreHarnessLogTag[] = "highscore_harness";
}  // namespace detail

class HighscoreHarnessGame {
 public:
  using Store = HighscoreStore<port::esp32::NvsHighscoreBackend>;
  using Wrapped = HighscoreGame<games::GalacticInvasion, Store>;

  // Backend init happens once, before this type is ever ticked -- see
  // firmware/system/main/app_main.cpp. A failed init leaves `store_`
  // presenting every table empty (highscore.h's own contract); this
  // harness still runs, it simply never persists anything.
  bool initBackend() {
    const bool ok = backend_.init();
    if (ok) {
      store_.load();
    } else {
      ESP_LOGW(detail::kHighscoreHarnessLogTag,
               "NvsHighscoreBackend::init() failed -- running with an "
               "in-memory-only table");
    }
    return ok;
  }

  void update(const GameInput&) {
    GameInput input{};
    input.fire = fireOn();
    input.start = startOn();
    wrapped_.update(input);
    ++tick_;
  }

  void render(Framebuffer& fb) { wrapped_.render(fb); }

 private:
  // Not timing claims (NFR-3) -- tick counts chosen so a human reading the
  // log sees each phase for a few seconds at whatever tick interval
  // app_main.cpp documents, mirroring galactic_invasion_harness_game.h's
  // own kSyntheticStartAtTick.
  static constexpr int32_t kFireHalfPeriodTicks = 10;
  static constexpr int32_t kStartHalfPeriodTicks = 150;

  bool fireOn() {
    return (tick_ / kFireHalfPeriodTicks) % 2 == 0;
  }
  bool startOn() {
    return (tick_ / kStartHalfPeriodTicks) % 2 == 0;
  }

  port::esp32::NvsHighscoreBackend backend_;
  Store store_{backend_};
  games::GalacticInvasion game_;
  Wrapped wrapped_{game_, store_, games::kHighscoreSlot, games::kHighscoreName};
  int32_t tick_ = 0;
};

}  // namespace steamcore::test
