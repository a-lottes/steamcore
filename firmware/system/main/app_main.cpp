// highscore-system T14: device-build insurance for this feature (CLAUDE.md
// "a host-only module can still hide a device-build bug") -- drives a real
// HighscoreGame<GalacticInvasion, HighscoreStore<NvsHighscoreBackend>>
// through the unmodified GameLoop<Game>, pushed to the already-wired
// ILI9488 panel, with synthetic fire/start pulses so no buttons are
// needed. This is the first time this feature's highscore.h/
// highscore_game.h/highscore_flow.h/highscore_screen.h/initials_entry.h
// and the real NvsHighscoreBackend are compiled by the ESP-IDF toolchain
// rather than only host clang/g++ -- the exact gap font.cpp's
// `throw`/`<cstddef>` bugs hid in for two features (CLAUDE.md). idf.py
// build completing green is this task's entire point; nothing here is
// flashed, run, or reported as hardware-verified (constitution §4
// honest-status rule), and no AC depends on the board.
//
// This overwrites galactic-invasion's T16 harness: harnesses are
// throwaway by construction (spec NFR-6/A13, the same posture every prior
// harness swap took), galactic-invasion is already released at v0.7.0,
// and git history plus the harness files left on disk
// (title_screen_harness_game.h, galactic_invasion_harness_game.h) preserve
// every prior one.

#include <cstdlib>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "highscore_harness_game.h"
#include "steamcore/dirty_tracker.h"
#include "steamcore/framebuffer.h"
#include "steamcore/game_loop.h"
#include "steamcore/port/esp32/ili9488_display.h"

namespace {
constexpr char kLogTag[] = "highscore_harness_main";

// Not a timing claim (NFR-3) -- how often this harness calls tick().
constexpr uint32_t kTickDelayMs = 20;
}  // namespace

extern "C" void app_main() {
  ESP_LOGI(kLogTag, "highscore-system T14: device-build harness, tick=%dms",
           static_cast<int>(kTickDelayMs));

  static steamcore::port::esp32::Ili9488Display display;
  static steamcore::Framebuffer fb;
  static steamcore::DirtyTracker tracker;
  static steamcore::test::HighscoreHarnessGame game;

  if (!display.init()) {
    ESP_LOGE(kLogTag, "display init failed, halting");
    std::abort();
  }
  game.initBackend();  // logs a warning and continues in-memory-only on failure

  // GameLoop<Game>'s unmodified, already-shipped public signature -- no
  // change to game_loop.h.
  steamcore::GameLoop<steamcore::test::HighscoreHarnessGame> loop(game, fb);

  ESP_LOGI(kLogTag, "harness running -- watch the panel, no buttons needed");
  for (;;) {
    loop.tick(steamcore::GameInput{});
    display.pushDirty(fb, tracker);
    vTaskDelay(pdMS_TO_TICKS(kTickDelayMs));
  }
}
