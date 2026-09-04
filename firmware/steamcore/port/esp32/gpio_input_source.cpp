#include "steamcore/port/esp32/gpio_input_source.h"

#include "driver/gpio.h"
#include "steamcore/board_config.h"

namespace steamcore::port::esp32 {
namespace {

// InputSignal's own declaration order (input.h) -- the one place a
// signal's identity is connected to its board_config.h pin.
constexpr int kPinsInSignalOrder[kInputSignalCount] = {
    kPinInputStart, kPinInputFire, kPinInputSelect, kPinInputUp,
    kPinInputDown,  kPinInputLeft, kPinInputRight,
};

}  // namespace

void GpioInputSource::init() {
  uint64_t pinBitMask = 0;
  for (int32_t i = 0; i < kInputSignalCount; ++i) {
    pinBitMask |= (1ULL << kPinsInSignalOrder[i]);
  }

  gpio_config_t config = {};
  config.pin_bit_mask = pinBitMask;
  config.mode = GPIO_MODE_INPUT;
  config.pull_up_en = GPIO_PULLUP_ENABLE;
  config.pull_down_en = GPIO_PULLDOWN_DISABLE;
  config.intr_type = GPIO_INTR_DISABLE;
  ESP_ERROR_CHECK(gpio_config(&config));
}

bool GpioInputSource::readSignal(InputSignal signal) const {
  const int pin = kPinsInSignalOrder[static_cast<int32_t>(signal)];
  return gpio_get_level(static_cast<gpio_num_t>(pin)) == 0;
}

}  // namespace steamcore::port::esp32
