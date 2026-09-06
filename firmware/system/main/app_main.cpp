// analog-joystick-input T7: the buildable half of US-3's on-device
// confirmation -- drives the real AnalogJoystickSource through the
// completely unmodified InputReader<Source>, ticking at a fixed,
// documented interval. Nothing needs to be physically wired to build and
// flash this: every direction reads permanently neutral until
// AnalogJoystickSource::init() actually finds a working ADC, and every
// button reads unpressed until the pins are connected
// (docs/wiring-analog-joystick.md) -- T8 is where a human at the real
// controls confirms the rest.
//
// This overwrites start-screen's T11 harness: harnesses are throwaway by
// construction (spec A12/NFR-6, the same posture every prior harness swap
// took), start-screen is already released at v0.4.0 (commit a8e590f, tag
// v0.4.0), and git history plus title_screen_harness_game.h (left on
// disk) preserve it.

#include <cstdio>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "steamcore/input.h"
#include "steamcore/port/esp32/analog_joystick_source.h"

namespace {

constexpr char kLogTag[] = "analog_joystick_harness";

// Not a timing claim (NFR-3) -- how often this harness calls read().
constexpr uint32_t kTickDelayMs = 20;

void logLevelChange(const char* name, bool previous, bool current) {
  if (previous == current) return;
  ESP_LOGI(kLogTag, "%s: %s", name, current ? "pressed" : "released");
}

}  // namespace

extern "C" void app_main() {
  ESP_LOGI(kLogTag, "analog-joystick-input T7: harness, tick=%dms",
           static_cast<int>(kTickDelayMs));

  static steamcore::port::esp32::AnalogJoystickSource source;
  static steamcore::InputReader<steamcore::port::esp32::AnalogJoystickSource>
      reader;

  if (!source.init()) {
    ESP_LOGE(kLogTag,
             "ADC init failed -- directions will read neutral forever; "
             "start/fire/select keep working regardless");
  }

  ESP_LOGI(kLogTag,
           "harness running -- move the stick and press the buttons");

  steamcore::GameInput previous{};
  for (;;) {
    const steamcore::GameInput input = reader.read(source);

    // NFR-5: every tick, each axis' raw ADC sample beside its derived
    // direction booleans -- continuous visibility for a human watching
    // the stick move, unlike the three buttons below, which log only on
    // change.
    ESP_LOGI(kLogTag, "axes: rawX=%d rawY=%d up=%d down=%d left=%d right=%d",
             static_cast<int>(source.lastRawX()),
             static_cast<int>(source.lastRawY()), input.up, input.down,
             input.left, input.right);

    logLevelChange("start", previous.start, input.start);
    logLevelChange("fire", previous.fire, input.fire);
    logLevelChange("select", previous.select, input.select);
    previous = input;

    vTaskDelay(pdMS_TO_TICKS(kTickDelayMs));
  }
}
