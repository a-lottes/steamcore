// start-screen T11: the on-device proof for AC-3.1/AC-3.2 -- an
// unmodified drawTitleScreen(fb, session_.state()) pushed to the real,
// already-wired ILI9488 panel (v0.2.0), with a synthetic `start` pulse
// driving READY -> PLAYING so the disappearance US-2 exists for is
// observable with no buttons wired. Every state change (and the initial
// READY render) also emits the framebuffer as an SCFB dump over the
// serial console, hex-encoded between sentinel markers, so
// tools/scfb_capture.py can turn a captured transcript into a real
// decodable .scfb file -- constitution §8's declared substitute
// verification method, made enforceable on device for the first time.
//
// This overwrites input-driver's still-blocked T10 harness: harnesses
// are throwaway by construction (spec NFR-6/A13, the same posture this
// project has taken every time), input-driver is already released at
// v0.3.0, and git history plus input_harness_game.h (left on disk)
// preserve it.

#include <cstdio>
#include <cstdlib>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "steamcore/dirty_tracker.h"
#include "steamcore/dump_format.h"
#include "steamcore/framebuffer.h"
#include "steamcore/game_loop.h"
#include "steamcore/port/esp32/ili9488_display.h"
#include "title_screen_harness_game.h"

namespace {
constexpr char kLogTag[] = "start_screen_main";

// Not a timing claim (NFR-3) -- how often this harness calls tick().
constexpr uint32_t kTickDelayMs = 20;

// Hex-encodes and prints `fb` between sentinel markers so
// tools/scfb_capture.py can extract it from a captured serial transcript.
// Uses raw printf, not ESP_LOGI, for every payload line: ESP_LOGI's own
// timestamp/tag prefix would land inside what must otherwise be pure hex
// characters. 32 bytes (64 hex characters) per line -- long enough to
// keep the total line count manageable, short enough to stay well under
// any serial console's line-length limit.
//
// Found on real hardware (T12): ~1,200 lines printed back-to-back with no
// yield starves the idle task long enough to trip the task watchdog
// (default 5 s) mid-dump, which then interleaves a watchdog backtrace
// into the middle of the hex payload -- exactly the "non-hex noise inside
// a block" case tools/scfb_capture.py is built to reject, so it silently
// discarded the whole corrupted capture rather than mis-decoding it (the
// tool did its job; the harness had the bug). Fixed with a periodic
// vTaskDelay, letting the idle task run and feed the watchdog.
void dumpFramebufferOverSerial(const steamcore::Framebuffer& fb) {
  using steamcore::kDumpHeaderSize;
  using steamcore::serializeDump;

  constexpr size_t kBufferCapacity =
      kDumpHeaderSize + static_cast<size_t>(steamcore::Framebuffer::width()) *
                             steamcore::Framebuffer::height();
  static uint8_t buffer[kBufferCapacity];
  const size_t written = serializeDump(fb, buffer, sizeof(buffer));
  if (written != kBufferCapacity) {
    ESP_LOGE(kLogTag, "serializeDump failed: wrote %d, expected %d",
             static_cast<int>(written), static_cast<int>(kBufferCapacity));
    return;
  }

  constexpr size_t kBytesPerLine = 32;
  char hexLine[kBytesPerLine * 2 + 1];

  constexpr int32_t kLinesPerYield = 20;
  int32_t lineCount = 0;

  std::printf("SCFB-DUMP-BEGIN\n");
  for (size_t offset = 0; offset < written; offset += kBytesPerLine) {
    const size_t lineBytes =
        (written - offset) < kBytesPerLine ? (written - offset) : kBytesPerLine;
    for (size_t i = 0; i < lineBytes; ++i) {
      std::snprintf(&hexLine[i * 2], 3, "%02x", buffer[offset + i]);
    }
    hexLine[lineBytes * 2] = '\0';
    std::printf("%s\n", hexLine);

    if (++lineCount % kLinesPerYield == 0) {
      vTaskDelay(1);
    }
  }
  std::printf("SCFB-DUMP-END\n");
}
}  // namespace

extern "C" void app_main() {
  ESP_LOGI(kLogTag, "start-screen T11: title screen harness, tick=%dms",
           static_cast<int>(kTickDelayMs));

  static steamcore::port::esp32::Ili9488Display display;
  static steamcore::Framebuffer fb;
  static steamcore::DirtyTracker tracker;
  static steamcore::test::TitleScreenHarnessGame game;

  if (!display.init()) {
    ESP_LOGE(kLogTag, "init failed, halting");
    std::abort();
  }

  // GameLoop<Game>'s unmodified, already-shipped public signature -- no
  // change to game_loop.h.
  steamcore::GameLoop<steamcore::test::TitleScreenHarnessGame> loop(game, fb);

  ESP_LOGI(kLogTag, "harness running -- watch the panel, no buttons needed");
  for (;;) {
    loop.tick(steamcore::GameInput{});
    display.pushDirty(fb, tracker);
    if (game.consumeStateChanged()) {
      ESP_LOGI(kLogTag, "state=%s, dumping framebuffer over serial",
               game.state() == steamcore::GameState::READY ? "READY"
               : game.state() == steamcore::GameState::PLAYING ? "PLAYING"
                                                                 : "GAME_OVER");
      dumpFramebufferOverSerial(fb);
    }
    vTaskDelay(pdMS_TO_TICKS(kTickDelayMs));
  }
}
