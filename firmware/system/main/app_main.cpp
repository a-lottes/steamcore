// input-driver T9: the on-device proof for AC-4.1-4.5 -- InputReader<
// GpioInputSource> feeding a real, unmodified GameSession, ticking at a
// fixed documented interval, log-only (plan §1 Alternatives; the
// display path is display-driver's already-released concern, v0.2.0).
// GameSession/GameLoop's already-shipped public signatures are used
// exactly as documented, unmodified (AC-4.5) -- no new parameter, no new
// method on either.
//
// This overwrites the display-driver on-device harness (T8 there):
// harnesses are throwaway by construction (spec NFR-6/A13), and
// display-driver is already released and tagged at v0.2.0 -- git history
// keeps that harness verbatim (plan.md §5, accepted deliberately).

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "input_harness_game.h"
#include "steamcore/framebuffer.h"
#include "steamcore/game_loop.h"
#include "steamcore/input.h"
#include "steamcore/port/esp32/gpio_input_source.h"

namespace {
constexpr char kLogTag[] = "input_driver_main";

// Not a timing claim (NFR-3) -- how often this harness calls tick(), so
// a human at the controls can watch each press register. The debounce
// mechanism itself reads no clock regardless of this value. review F2:
// this directly sets the *effective* debounce window (kDebounceSamples *
// kTickDelayMs) -- a SLOWER tick here widens that window, it does not
// shrink it. At 20 ms the window is 2 * 20 = 40 ms, wider than the ≤33 ms
// design target `input.h` states for a 60 Hz (≤16.67 ms) caller; logged
// explicitly below so a T10 transcript carries the value actually in
// effect for NFR-1's `qa.md` record, not the design target.
constexpr uint32_t kTickDelayMs = 20;
}  // namespace

extern "C" void app_main() {
  ESP_LOGI(kLogTag, "input-driver T9: InputReader<GpioInputSource> + "
                     "GameSession, tick=%dms",
           static_cast<int>(kTickDelayMs));
  ESP_LOGI(kLogTag,
           "effective debounce window: %dms (kDebounceSamples=%d * "
           "tick=%dms) -- record this in qa.md for NFR-1",
           static_cast<int>(steamcore::kDebounceSamples * kTickDelayMs),
           static_cast<int>(steamcore::kDebounceSamples),
           static_cast<int>(kTickDelayMs));

  static steamcore::port::esp32::GpioInputSource source;
  static steamcore::InputReader<steamcore::port::esp32::GpioInputSource>
      reader;
  static steamcore::Framebuffer fb;  // unused; GameLoop<Game> requires it
  static steamcore::test::InputHarnessGame game;

  source.init();

  // GameLoop<Game>'s unmodified, already-shipped public signature
  // (AC-4.5) -- no change to game_loop.h.
  steamcore::GameLoop<steamcore::test::InputHarnessGame> loop(game, fb);

  ESP_LOGI(kLogTag, "harness running -- press physical controls now");
  for (;;) {
    loop.tick(reader.read(source));
    vTaskDelay(pdMS_TO_TICKS(kTickDelayMs));
  }
}
