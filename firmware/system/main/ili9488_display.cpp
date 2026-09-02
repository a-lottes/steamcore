#include "ili9488_display.h"

#include <cstring>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "steamcore/board_config.h"

namespace steamcore::bringup {
namespace {

constexpr char kLogTag[] = "ili9488_bringup";

// Native panel GRAM geometry (datasheet: 320(RGB) x 480), independent of
// the engine's 240x160 virtual resolution -- this bring-up test never
// touches steamcore::kScreenWidth/Height.
constexpr int kPanelWidth = 320;
constexpr int kPanelHeight = 480;

constexpr uint8_t kCmdSoftwareReset = 0x01;
constexpr uint8_t kCmdSleepOut = 0x11;
constexpr uint8_t kCmdDisplayInversionOff = 0x20;
constexpr uint8_t kCmdDisplayOn = 0x29;
constexpr uint8_t kCmdColumnAddressSet = 0x2A;
constexpr uint8_t kCmdPageAddressSet = 0x2B;
constexpr uint8_t kCmdMemoryWrite = 0x2C;
constexpr uint8_t kCmdMemoryAccessControl = 0x36;
constexpr uint8_t kCmdPixelFormatSet = 0x3A;

constexpr uint8_t kPixelFormat18Bit = 0x66;   // 18 bpp / 3 bytes per pixel -- the claim under test
constexpr uint8_t kMadctlBgrLandscape = 0x28;  // MV=1 (landscape), BGR=1

spi_device_handle_t g_spiDevice = nullptr;

void resetPulse() {
  gpio_set_level(static_cast<gpio_num_t>(kPinDisplayReset), 0);
  vTaskDelay(pdMS_TO_TICKS(20));
  gpio_set_level(static_cast<gpio_num_t>(kPinDisplayReset), 1);
  vTaskDelay(pdMS_TO_TICKS(120));
}

void sendCommand(uint8_t cmd) {
  gpio_set_level(static_cast<gpio_num_t>(kPinDisplayDc), 0);
  spi_transaction_t transaction = {};
  transaction.length = 8;
  transaction.tx_buffer = &cmd;
  ESP_ERROR_CHECK(spi_device_transmit(g_spiDevice, &transaction));
}

void sendData(const uint8_t* data, size_t lengthBytes) {
  if (lengthBytes == 0) return;
  gpio_set_level(static_cast<gpio_num_t>(kPinDisplayDc), 1);
  spi_transaction_t transaction = {};
  transaction.length = lengthBytes * 8;
  transaction.tx_buffer = data;
  ESP_ERROR_CHECK(spi_device_transmit(g_spiDevice, &transaction));
}

void sendDataByte(uint8_t byte) { sendData(&byte, 1); }

void setAddressWindow(int x0, int y0, int x1, int y1) {
  sendCommand(kCmdColumnAddressSet);
  const uint8_t colBytes[4] = {
      static_cast<uint8_t>(x0 >> 8), static_cast<uint8_t>(x0 & 0xFF),
      static_cast<uint8_t>(x1 >> 8), static_cast<uint8_t>(x1 & 0xFF)};
  sendData(colBytes, sizeof(colBytes));

  sendCommand(kCmdPageAddressSet);
  const uint8_t rowBytes[4] = {
      static_cast<uint8_t>(y0 >> 8), static_cast<uint8_t>(y0 & 0xFF),
      static_cast<uint8_t>(y1 >> 8), static_cast<uint8_t>(y1 & 0xFF)};
  sendData(rowBytes, sizeof(rowBytes));
}

}  // namespace

void ili9488Init() {
  gpio_config_t dcResetConfig = {};
  dcResetConfig.pin_bit_mask =
      (1ULL << kPinDisplayDc) | (1ULL << kPinDisplayReset);
  dcResetConfig.mode = GPIO_MODE_OUTPUT;
  ESP_ERROR_CHECK(gpio_config(&dcResetConfig));

  // Backlight is wired directly to 3V3 (see board_config.h) -- nothing to
  // configure here.

  spi_bus_config_t busConfig = {};
  busConfig.mosi_io_num = kPinDisplayMosi;
  busConfig.miso_io_num = kPinDisplayMiso;
  busConfig.sclk_io_num = kPinDisplaySck;
  busConfig.quadwp_io_num = -1;
  busConfig.quadhd_io_num = -1;
  busConfig.max_transfer_sz = kPanelWidth * 3;  // one scanline of 18bpp data
  ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &busConfig, SPI_DMA_CH_AUTO));

  spi_device_interface_config_t deviceConfig = {};
  // Conservative bring-up clock. The constitution's 40 MHz dirty-tile DMA
  // budget is a target for the real driver, not this write-once-and-look
  // spike; slower first makes a wiring mistake easier to diagnose.
  deviceConfig.clock_speed_hz = 10 * 1000 * 1000;
  deviceConfig.mode = 0;
  deviceConfig.spics_io_num = kPinDisplayCs;
  deviceConfig.queue_size = 1;
  ESP_ERROR_CHECK(
      spi_bus_add_device(SPI2_HOST, &deviceConfig, &g_spiDevice));

  ESP_LOGI(kLogTag, "resetting panel");
  resetPulse();

  sendCommand(kCmdSoftwareReset);
  vTaskDelay(pdMS_TO_TICKS(150));

  sendCommand(kCmdSleepOut);
  vTaskDelay(pdMS_TO_TICKS(120));

  sendCommand(kCmdPixelFormatSet);
  sendDataByte(kPixelFormat18Bit);

  sendCommand(kCmdMemoryAccessControl);
  sendDataByte(kMadctlBgrLandscape);

  sendCommand(kCmdDisplayInversionOff);

  sendCommand(kCmdDisplayOn);
  vTaskDelay(pdMS_TO_TICKS(20));

  ESP_LOGI(kLogTag, "init sequence sent (COLMOD=0x%02x)", kPixelFormat18Bit);
}

void ili9488FillColor(uint8_t r, uint8_t g, uint8_t b) {
  // Landscape (MV=1) swaps the addressable width/height versus the
  // datasheet's native portrait GRAM layout.
  const int width = kPanelHeight;
  const int height = kPanelWidth;
  setAddressWindow(0, 0, width - 1, height - 1);

  sendCommand(kCmdMemoryWrite);

  static uint8_t lineBuffer[/*kPanelHeight landscape width*/ 480 * 3];
  for (int i = 0; i < width; ++i) {
    lineBuffer[i * 3 + 0] = r;
    lineBuffer[i * 3 + 1] = g;
    lineBuffer[i * 3 + 2] = b;
  }

  for (int row = 0; row < height; ++row) {
    sendData(lineBuffer, static_cast<size_t>(width) * 3);
  }

  ESP_LOGI(kLogTag, "filled panel with r=%u g=%u b=%u", r, g, b);
}

}  // namespace steamcore::bringup
