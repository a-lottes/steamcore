#include "steamcore/port/esp32/ili9488_display.h"

#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "steamcore/board_config.h"
#include "steamcore/config.h"

namespace steamcore::port::esp32 {
namespace {

constexpr char kLogTag[] = "ili9488";

constexpr uint8_t kCmdSoftwareReset = 0x01;
constexpr uint8_t kCmdSleepOut = 0x11;
constexpr uint8_t kCmdDisplayInversionOff = 0x20;
constexpr uint8_t kCmdDisplayOn = 0x29;
constexpr uint8_t kCmdColumnAddressSet = 0x2A;
constexpr uint8_t kCmdPageAddressSet = 0x2B;
constexpr uint8_t kCmdMemoryWrite = 0x2C;
constexpr uint8_t kCmdMemoryAccessControl = 0x36;
constexpr uint8_t kCmdPixelFormatSet = 0x3A;

constexpr uint8_t kPixelFormat18Bit = 0x66;    // 18bpp / 3 bytes per pixel
constexpr uint8_t kMadctlBgrLandscape = 0x28;  // MV=1 (landscape), BGR=1

// These helpers never abort -- they report success/failure by return
// value, so init() and SpiTransmitter::transmitTile() can each apply
// their own policy (log-and-continue vs. log-and-let-the-caller-decide)
// on top of the same primitives instead of duplicating them.

bool sendCommand(spi_device_handle_t dev, uint8_t cmd) {
  gpio_set_level(static_cast<gpio_num_t>(kPinDisplayDc), 0);
  spi_transaction_t transaction = {};
  transaction.length = 8;
  transaction.tx_buffer = &cmd;
  return spi_device_transmit(dev, &transaction) == ESP_OK;
}

bool sendData(spi_device_handle_t dev, const uint8_t* data,
              size_t lengthBytes) {
  if (lengthBytes == 0) return true;
  gpio_set_level(static_cast<gpio_num_t>(kPinDisplayDc), 1);
  spi_transaction_t transaction = {};
  transaction.length = lengthBytes * 8;
  transaction.tx_buffer = data;
  return spi_device_transmit(dev, &transaction) == ESP_OK;
}

bool sendDataByte(spi_device_handle_t dev, uint8_t byte) {
  return sendData(dev, &byte, 1);
}

bool setAddressWindow(spi_device_handle_t dev, const PanelWindow& window) {
  if (!sendCommand(dev, kCmdColumnAddressSet)) return false;
  const uint8_t colBytes[4] = {static_cast<uint8_t>(window.x0 >> 8),
                                static_cast<uint8_t>(window.x0 & 0xFF),
                                static_cast<uint8_t>(window.x1 >> 8),
                                static_cast<uint8_t>(window.x1 & 0xFF)};
  if (!sendData(dev, colBytes, sizeof(colBytes))) return false;

  if (!sendCommand(dev, kCmdPageAddressSet)) return false;
  const uint8_t rowBytes[4] = {static_cast<uint8_t>(window.y0 >> 8),
                                static_cast<uint8_t>(window.y0 & 0xFF),
                                static_cast<uint8_t>(window.y1 >> 8),
                                static_cast<uint8_t>(window.y1 & 0xFF)};
  return sendData(dev, rowBytes, sizeof(rowBytes));
}

void resetPulse() {
  gpio_set_level(static_cast<gpio_num_t>(kPinDisplayReset), 0);
  vTaskDelay(pdMS_TO_TICKS(20));
  gpio_set_level(static_cast<gpio_num_t>(kPinDisplayReset), 1);
  vTaskDelay(pdMS_TO_TICKS(120));
}

// Logs a named failure -- never aborts. init() decides what to do with
// the returned bool; this function's only job is that a failure is
// never silent (AC-4.4).
bool logStep(bool ok, const char* step) {
  if (!ok) ESP_LOGE(kLogTag, "init: %s failed", step);
  return ok;
}

}  // namespace

bool Ili9488Display::init() {
  gpio_config_t dcResetConfig = {};
  dcResetConfig.pin_bit_mask =
      (1ULL << kPinDisplayDc) | (1ULL << kPinDisplayReset);
  dcResetConfig.mode = GPIO_MODE_OUTPUT;
  // Setup calls, not steady-state transfers (AC-4.3's scope) -- still
  // fatal, same as the bring-up spike, since nothing useful can proceed
  // without them.
  ESP_ERROR_CHECK(gpio_config(&dcResetConfig));

  // Backlight is wired directly to 3V3 (board_config.h) -- nothing to
  // configure here.

  spi_bus_config_t busConfig = {};
  busConfig.mosi_io_num = kPinDisplayMosi;
  busConfig.miso_io_num = kPinDisplayMiso;
  busConfig.sclk_io_num = kPinDisplaySck;
  busConfig.quadwp_io_num = -1;
  busConfig.quadhd_io_num = -1;
  // The spike set this to one scanline (960 bytes) -- smaller than one
  // panel tile (kPanelTileBytes, 3072 bytes) and wrong for this driver:
  // every tile transfer would have failed, indistinguishable from a
  // wiring fault (plan §5 Risk 1).
  busConfig.max_transfer_sz = kPanelTileBytes;
  ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &busConfig, SPI_DMA_CH_AUTO));

  spi_device_interface_config_t deviceConfig = {};
  // US-5: tuned toward the constitution's 40 MHz budget in steps (10 ->
  // 20 -> 40 MHz on real hardware), each re-verified against T7's
  // 150-tile frame and T8's 20+-tick run: no corruption, streaking or
  // tearing at any step, so the full 40 MHz budget is reached and kept
  // -- see plan.md T11 for the measured per-tile transfer time at each
  // speed and the human-confirmed visual result.
  deviceConfig.clock_speed_hz = 40 * 1000 * 1000;
  deviceConfig.mode = 0;
  deviceConfig.spics_io_num = kPinDisplayCs;
  deviceConfig.queue_size = 1;
  ESP_ERROR_CHECK(
      spi_bus_add_device(SPI2_HOST, &deviceConfig, &spiDevice_));

  // One-time sanity check: the tile conversion buffer is a static
  // TilePusher member (no dynamic allocation anywhere, AC-3.4), so it is
  // always internal SRAM in this build -- never PSRAM, since nothing
  // places it there. Logged explicitly rather than assumed (plan §5
  // Risk 2): a silent failure here would look exactly like a wiring or
  // timing fault at the SPI layer instead.
  const bool dmaCapable =
      esp_ptr_dma_capable(static_cast<const void*>(pusher_.buffer()));
  ESP_LOGI(kLogTag, "tile conversion buffer DMA-capable: %s",
           dmaCapable ? "yes" : "no");

  ESP_LOGI(kLogTag, "resetting panel");
  resetPulse();

  if (!logStep(sendCommand(spiDevice_, kCmdSoftwareReset),
               "software reset")) {
    return false;
  }
  vTaskDelay(pdMS_TO_TICKS(150));

  if (!logStep(sendCommand(spiDevice_, kCmdSleepOut), "sleep out")) {
    return false;
  }
  vTaskDelay(pdMS_TO_TICKS(120));

  if (!logStep(sendCommand(spiDevice_, kCmdPixelFormatSet) &&
                   sendDataByte(spiDevice_, kPixelFormat18Bit),
               "pixel format set")) {
    return false;
  }

  if (!logStep(sendCommand(spiDevice_, kCmdMemoryAccessControl) &&
                   sendDataByte(spiDevice_, kMadctlBgrLandscape),
               "memory access control")) {
    return false;
  }

  if (!logStep(sendCommand(spiDevice_, kCmdDisplayInversionOff),
               "display inversion off")) {
    return false;
  }

  if (!logStep(sendCommand(spiDevice_, kCmdDisplayOn), "display on")) {
    return false;
  }
  vTaskDelay(pdMS_TO_TICKS(20));

  ESP_LOGI(kLogTag, "init sequence complete (COLMOD=0x%02x)",
           kPixelFormat18Bit);
  return true;
}

bool Ili9488Display::SpiTransmitter::transmitTile(const PanelWindow& window,
                                                   const uint8_t* bytes,
                                                   size_t count) {
  if (!setAddressWindow(device_, window)) {
    ESP_LOGW(kLogTag, "tile push: address window failed at (%d,%d)",
             static_cast<int>(window.x0), static_cast<int>(window.y0));
    return false;
  }
  if (!sendCommand(device_, kCmdMemoryWrite)) {
    ESP_LOGW(kLogTag, "tile push: memory write command failed at (%d,%d)",
             static_cast<int>(window.x0), static_cast<int>(window.y0));
    return false;
  }
  if (!sendData(device_, bytes, count)) {
    ESP_LOGW(kLogTag, "tile push: data transfer failed at (%d,%d)",
             static_cast<int>(window.x0), static_cast<int>(window.y0));
    return false;
  }
  return true;
}

PushResult Ili9488Display::pushDirty(const Framebuffer& fb,
                                      DirtyTracker& tracker) {
  SpiTransmitter transmitter(spiDevice_);
  const int64_t startUs = esp_timer_get_time();
  const PushResult result = pusher_.push(fb, tracker, transmitter);
  const int64_t elapsedUs = esp_timer_get_time() - startUs;

  const double avgUsPerTile =
      result.sent > 0 ? static_cast<double>(elapsedUs) / result.sent : 0.0;
  ESP_LOGI(kLogTag,
           "push: sent=%d failed=%d elapsed_us=%lld avg_us_per_tile=%.1f",
           static_cast<int>(result.sent), static_cast<int>(result.failed),
           static_cast<long long>(elapsedUs), avgUsPerTile);
  return result;
}

}  // namespace steamcore::port::esp32
