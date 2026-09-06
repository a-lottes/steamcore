#include "steamcore/port/esp32/analog_joystick_source.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "steamcore/analog_axis.h"
#include "steamcore/board_config.h"

namespace steamcore::port::esp32 {
namespace {

constexpr const char* kLogTag = "analog_joystick_source";

// Ties the ADC channel configuration below to analog_axis.h's own
// kAxisAdcBitWidth: ESP-IDF's ADC_BITWIDTH_* enumerators are numerically
// equal to the bit width they name (ADC_BITWIDTH_12 == 12), so this
// catches either constant drifting from the other, not just a typo in
// the cast below.
static_assert(static_cast<int>(ADC_BITWIDTH_12) == kAxisAdcBitWidth,
              "kAxisAdcBitWidth must match the ADC_BITWIDTH_* enumerator "
              "actually configured below");

// readSignal() samples both axes only on the kUp call and serves
// kDown/kLeft/kRight from that cache, which is correct only because kUp is
// the first of the four directions InputReader::read() reaches in its
// ascending 0..kInputSignalCount sweep (input.h). Reordering InputSignal's
// enumerators so another direction preceded kUp would leave those three
// reading one tick stale -- silently, and only for one tick before
// self-correcting, which is the hardest kind of bug to see on a device.
// Pinned here so that reorder is a build failure instead (review F11).
static_assert(static_cast<int>(InputSignal::kUp) <
                      static_cast<int>(InputSignal::kDown) &&
                  static_cast<int>(InputSignal::kUp) <
                      static_cast<int>(InputSignal::kLeft) &&
                  static_cast<int>(InputSignal::kUp) <
                      static_cast<int>(InputSignal::kRight),
              "AnalogJoystickSource caches one sample pair on the kUp call; "
              "kUp must stay the first direction InputSignal declares");

}  // namespace

bool AnalogJoystickSource::init() {
  const uint64_t pinBitMask = (1ULL << kPinJoystickSw) |
                              (1ULL << kPinJoystickStart) |
                              (1ULL << kPinJoystickFire);

  gpio_config_t config = {};
  config.pin_bit_mask = pinBitMask;
  config.mode = GPIO_MODE_INPUT;
  config.pull_up_en = GPIO_PULLUP_ENABLE;
  config.pull_down_en = GPIO_PULLDOWN_DISABLE;
  config.intr_type = GPIO_INTR_DISABLE;
  ESP_ERROR_CHECK(gpio_config(&config));

  adc_unit_t unitX;
  adc_unit_t unitY;
  if (adc_oneshot_io_to_channel(kPinJoystickVrx, &unitX, &adcChannelX_) !=
          ESP_OK ||
      adc_oneshot_io_to_channel(kPinJoystickVry, &unitY, &adcChannelY_) !=
          ESP_OK) {
    ESP_LOGE(kLogTag, "failed to derive ADC unit/channel for VRX/VRY pins");
    return false;
  }
  if (unitX != unitY) {
    ESP_LOGE(kLogTag, "VRX and VRY must share one ADC unit (got %d and %d)",
             static_cast<int>(unitX), static_cast<int>(unitY));
    return false;
  }

  adc_oneshot_unit_init_cfg_t unitConfig = {};
  unitConfig.unit_id = unitX;
  if (adc_oneshot_new_unit(&unitConfig, &adcUnit_) != ESP_OK) {
    ESP_LOGE(kLogTag, "failed to create ADC oneshot unit");
    return false;
  }

  adc_oneshot_chan_cfg_t chanConfig = {};
  chanConfig.bitwidth = static_cast<adc_bitwidth_t>(kAxisAdcBitWidth);
  chanConfig.atten = ADC_ATTEN_DB_12;
  if (adc_oneshot_config_channel(adcUnit_, adcChannelX_, &chanConfig) !=
          ESP_OK ||
      adc_oneshot_config_channel(adcUnit_, adcChannelY_, &chanConfig) !=
          ESP_OK) {
    ESP_LOGE(kLogTag, "failed to configure ADC channels for VRX/VRY");
    // review F7: the unit was already created above -- delete it rather
    // than leaking the handle on this failure path, which matters the
    // moment anything ever retries init().
    adc_oneshot_del_unit(adcUnit_);
    adcUnit_ = nullptr;
    return false;
  }

  adcReady_ = true;
  return true;
}

bool AnalogJoystickSource::readSignal(InputSignal signal) const {
  switch (signal) {
    case InputSignal::kStart:
      return gpio_get_level(static_cast<gpio_num_t>(kPinJoystickStart)) == 0;
    case InputSignal::kFire:
      return gpio_get_level(static_cast<gpio_num_t>(kPinJoystickFire)) == 0;
    case InputSignal::kSelect:
      return gpio_get_level(static_cast<gpio_num_t>(kPinJoystickSw)) == 0;
    default:
      break;
  }

  // Both axes are sampled exactly once per tick, on the kUp call -- the
  // first direction signal InputReader::read() ever queries, per
  // InputSignal's declared order (review F9). kDown/kLeft/kRight reuse
  // that one cached pair instead of re-reading hardware, so all four
  // booleans in a tick are consistent with each other and with the one
  // rawX/rawY pair lastRawX()/lastRawY() report.
  //
  // A read that fails, or an ADC that never initialized, leaves the
  // level at its AxisLevel::kNeutral default -- directionActive() then
  // reports every direction false for a neutral pair, proven on the host
  // (analog_axis_test.cpp) without a per-signal error branch here.
  if (signal == InputSignal::kUp) {
    cachedX_ = AxisLevel::kNeutral;
    cachedY_ = AxisLevel::kNeutral;
    if (adcReady_) {
      int rawX = 0;
      if (adc_oneshot_read(adcUnit_, adcChannelX_, &rawX) == ESP_OK) {
        lastRawX_ = rawX;
        cachedX_ = axisLevel(rawX);
      }
      int rawY = 0;
      if (adc_oneshot_read(adcUnit_, adcChannelY_, &rawY) == ESP_OK) {
        lastRawY_ = rawY;
        cachedY_ = axisLevel(rawY);
      }
    }
  }
  return directionActive(signal, cachedX_, cachedY_);
}

}  // namespace steamcore::port::esp32
