// T8: the real GameLoop<Consumer>-driven on-device proof (spec US-3).
// Tick 1's render() is the same fixture pattern T7 already proved
// (AC-3.1); every following tick moves a single full-tile marker one
// tile (AC-3.2). The push call happens from this loop, after tick()
// returns -- never from inside render() or GameLoop itself (AC-3.5).
// game_loop.h is used with its already-shipped, unmodified public
// signature: GameLoop<Consumer>(consumer, fb) then tick(GameInput{}).
//
// review F1 (round 1): T7's per-anchor log -- the mechanism AC-3.1 uses
// to verify the fixture's small elements (corner markers, the 1px line)
// without asking a human eye, per the Designer's F1 finding on the spec
// -- was dropped when this file was replaced for T8. Restored below,
// logged once, right after tick 1's push.

#include <cstdlib>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "harness_consumer.h"
#include "steamcore/config.h"
#include "steamcore/dirty_tracker.h"
#include "steamcore/framebuffer.h"
#include "steamcore/game_loop.h"
#include "steamcore/panel_format.h"
#include "steamcore/port/esp32/ili9488_display.h"
#include "steamcore/test/fixture_pattern.h"

namespace {
constexpr char kLogTag[] = "display_driver_main";

// 1 fixture tick + at least 20 further marker-move ticks (AC-3.2).
constexpr int32_t kTotalTicks = 21;
constexpr uint32_t kTickDelayMs = 400;  // slow enough for a human to watch

const char* colorName(steamcore::Color color) {
  switch (color) {
    case steamcore::Color::BLACK:
      return "BLACK";
    case steamcore::Color::DARK_ORANGE:
      return "DARK_ORANGE";
    case steamcore::Color::ORANGE:
      return "ORANGE";
    case steamcore::Color::BRIGHT_ORANGE:
      return "BRIGHT_ORANGE";
  }
  return "?";
}

// AC-3.1's log half: per fixture anchor, its source colour, its mapped
// panel coordinate (engine x/y * kPanelScale) and the 18bpp wire bytes a
// successful transaction carried -- the small elements (corner markers,
// the 1px line) are verified this way, never asked of the human eye
// (spec A11, Designer finding F1).
void logFixtureAnchors() {
  for (const steamcore::test::FixtureAnchor& anchor :
       steamcore::test::kFixtureAnchors) {
    const steamcore::PanelPixel wire = steamcore::toPanelPixel(anchor.expected);
    const int32_t panelX = anchor.x * steamcore::kPanelScale;
    const int32_t panelY = anchor.y * steamcore::kPanelScale;
    ESP_LOGI(kLogTag,
             "anchor engine=(%d,%d) panel=(%d,%d) color=%s "
             "wire=(0x%02X,0x%02X,0x%02X)",
             static_cast<int>(anchor.x), static_cast<int>(anchor.y),
             static_cast<int>(panelX), static_cast<int>(panelY),
             colorName(anchor.expected), wire.r, wire.g, wire.b);
  }
}
}  // namespace

extern "C" void app_main() {
  ESP_LOGI(kLogTag, "display-driver T8: GameLoop<Consumer>, %d ticks",
           static_cast<int>(kTotalTicks));

  // static, not local: see T6's task note in plan.md -- these together
  // are far larger than CONFIG_ESP_MAIN_TASK_STACK_SIZE and silently
  // stack-overflow app_main as plain locals.
  static steamcore::port::esp32::Ili9488Display display;
  static steamcore::Framebuffer fb;
  static steamcore::DirtyTracker tracker;
  static steamcore::test::HarnessConsumer consumer;

  if (!display.init()) {
    ESP_LOGE(kLogTag, "init failed, halting");
    std::abort();
  }

  // GameLoop<Consumer>'s unmodified, already-shipped public signature
  // (AC-3.5) -- no change to game_loop.h.
  steamcore::GameLoop<steamcore::test::HarnessConsumer> loop(consumer, fb);

  for (int32_t i = 1; i <= kTotalTicks; ++i) {
    loop.tick(steamcore::GameInput{});
    const steamcore::PushResult result = display.pushDirty(fb, tracker);
    ESP_LOGI(kLogTag, "tick %d/%d: push sent=%d failed=%d",
             static_cast<int>(i), static_cast<int>(kTotalTicks),
             static_cast<int>(result.sent), static_cast<int>(result.failed));
    if (i == 1) logFixtureAnchors();
    vTaskDelay(pdMS_TO_TICKS(kTickDelayMs));
  }

  ESP_LOGI(kLogTag, "harness run complete, idling");
  while (true) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
