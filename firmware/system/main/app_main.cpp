// SPIKE: hardware bring-up for the KMRTM35018-SPI (ILI9488) panel that
// arrived 2026-09-02. Verifies constitution §3's unverified assumption that
// this controller only accepts 18bpp/3-bytes-per-pixel over SPI, by filling
// the panel with a sequence of known solid colors and logging each step.
// Not the real system firmware -- see ili9488_display.h.

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ili9488_display.h"

namespace {
constexpr char kLogTag[] = "bringup_main";
}

extern "C" void app_main() {
  ESP_LOGI(kLogTag, "steamcore display bring-up starting");

  steamcore::bringup::ili9488Init();

  // Red / green / blue / white, a few seconds each: enough to read the
  // panel's serial log against what's actually visible and confirm (or
  // refute) the 18bpp claim -- garbled or wrong-channel colors here mean
  // the assumption in constitution §3 was wrong, not that this code is.
  const struct {
    const char* name;
    uint8_t r, g, b;
  } colors[] = {
      {"red", 0xFF, 0x00, 0x00},
      {"green", 0x00, 0xFF, 0x00},
      {"blue", 0x00, 0x00, 0xFF},
      {"white", 0xFF, 0xFF, 0xFF},
  };

  while (true) {
    for (const auto& color : colors) {
      ESP_LOGI(kLogTag, "filling: %s", color.name);
      steamcore::bringup::ili9488FillColor(color.r, color.g, color.b);
      vTaskDelay(pdMS_TO_TICKS(3000));
    }
  }
}
