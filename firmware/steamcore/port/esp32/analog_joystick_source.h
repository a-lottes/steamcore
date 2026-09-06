#pragma once

#include <cstdint>

#include "esp_adc/adc_oneshot.h"
#include "steamcore/analog_axis.h"
#include "steamcore/input.h"

// The real device-side Source for InputReader<Source> reading a 5-pin
// analog joystick module plus two discrete pushbuttons (analog-joystick-
// input plan §1 Decisions 1/4-8) -- ESP-IDF/ADC-dependent, therefore
// outside the host `make test` gate by construction, the same boundary
// GpioInputSource already established. A SECOND, mutually exclusive
// Source: the console is wired with either this or GpioInputSource,
// never both at once (docs/wiring-analog-joystick.md).
//
// Example (illustrative -- see firmware/system/main/app_main.cpp for the
// real on-device harness):
//   steamcore::port::esp32::AnalogJoystickSource source;
//   if (!source.init()) { /* directions read neutral forever; buttons
//                            keep working -- log and decide whether to
//                            continue */ }
//   steamcore::InputReader<steamcore::port::esp32::AnalogJoystickSource>
//       reader;
//   const steamcore::GameInput input = reader.read(source);  // once per tick
//
// Contract:
//  - init() configures the three digital pins (board_config.h) as
//    inputs with the internal pull-up enabled, no interrupt -- exactly
//    GpioInputSource's own pattern -- and initializes one ADC oneshot
//    unit with both VRX/VRY channels, whose unit and channel are
//    DERIVED from the configured GPIO pins via
//    adc_oneshot_io_to_channel(), never written down as a second
//    literal. If ADC init fails for any reason, it is logged, init()
//    returns false, and every direction signal reads false forever
//    (the same neutral a failed per-read also produces) -- the three
//    buttons keep working regardless, since a dead ADC must not brick
//    the board. Call once before the first readSignal().
//  - Button mapping (spec A3, resolved by the user -- the one part of
//    this Source a caller cannot guess from a pin name, unlike
//    GpioInputSource's 1:1 kPinInput* scheme): Taster 1 (the first
//    discrete pushbutton, kPinJoystickStart) -> `start`, Taster 2
//    (kPinJoystickFire) -> `fire`, and the joystick's own integrated
//    click SW (kPinJoystickSw) -> `select`, since SW sits under the
//    thumb already resting on the stick.
//  - readSignal() returns the LOGICAL level InputReader expects
//    ("pressed" == true). For start/fire/select this is
//    `gpio_get_level(pin) == 0` (active-low, pull-up, same inversion
//    convention GpioInputSource uses). For up/down/left/right this calls
//    `steamcore::directionActive(signal, axisLevel(rawX), axisLevel(rawY))`
//    (steamcore/analog_axis.h) -- all of the actual decision logic lives
//    there, host-tested; this file is ESP-IDF glue with none of its own.
//    Both axes are sampled exactly ONCE per tick, on the `kUp` call --
//    the first direction signal in InputSignal's declared order, which
//    InputReader::read() always queries before kDown/kLeft/kRight (the
//    same fixed-order assumption GpioInputSource's own kPinsInSignalOrder
//    table already relies on) -- and the other three direction calls
//    reuse that one sample pair (review F9). This makes all four
//    booleans in one tick consistent with each other and with the single
//    rawX/rawY pair lastRawX()/lastRawY() report, and halves ADC traffic
//    again versus reading only the needed axis per call.
//  - A raw ADC read that fails (a non-ESP_OK esp_err_t) or an ADC that
//    never initialized maps to AxisLevel::kNeutral for that axis, so a
//    read failure never fabricates a direction (steamcore/analog_axis.h
//    proves directionActive(signal, kNeutral, kNeutral) is false for
//    every direction, on the host).
//  - The deadzone is a fixed percentage of full scale, centred on the
//    ADC's nominal midpoint -- no calibration step, no persisted
//    calibration state (spec A4/A13). A joystick resting outside that
//    fixed band may read a spurious direction at rest or lose usable
//    range on one side; this is an accepted, documented limitation
//    (spec A14, docs/wiring-analog-joystick.md), not corrected here.
//  - Powered from the board's 3.3V rail only -- see
//    docs/wiring-analog-joystick.md. No firmware logic depends on or
//    checks the supply voltage; a wrong voltage is a wiring error to
//    catch before power-on, not a runtime condition.
//  - lastRawX()/lastRawY() are diagnostic accessors only, existing
//    solely so an on-device harness can log each raw sample beside its
//    derived direction boolean -- not part of InputReader's Source
//    concept.
//  - Single-threaded, nothing throws, no error code (besides init()'s
//    bool), no dynamic allocation -- same inherited contract as every
//    other steamcore type.
namespace steamcore::port::esp32 {

class AnalogJoystickSource {
 public:
  AnalogJoystickSource() = default;

  // Configures the three digital pins and the ADC oneshot unit/channels.
  // Returns false if ADC setup fails; the three digital signals still
  // work in that case, but every direction reads false forever. Call
  // once before the first readSignal().
  bool init();

  // Satisfies InputReader's Source concept.
  bool readSignal(InputSignal signal) const;

  // The raw ADC count from the single sample pair taken this tick (0
  // before the first tick's kUp call), so both normally come from the
  // same sample pair -- see the class contract above (review F9). The one
  // exception: an axis whose read failed this tick keeps its previous
  // value here while its level falls back to kNeutral, so a stale raw can
  // sit beside two false booleans (docs/device-build.md says how to read
  // that). Diagnostic only.
  int32_t lastRawX() const { return lastRawX_; }
  int32_t lastRawY() const { return lastRawY_; }

 private:
  // Value-initialized to 0, never a named ADC_CHANNEL_* literal: neither
  // is ever read before init() overwrites both with the real,
  // pin-derived channel (adcReady_ guards every read in readSignal()),
  // so naming a channel here would be a meaningless second literal, not
  // a real assignment (T6's own lint rule bans exactly that).
  adc_oneshot_unit_handle_t adcUnit_ = nullptr;
  adc_channel_t adcChannelX_{};
  adc_channel_t adcChannelY_{};
  bool adcReady_ = false;
  mutable int32_t lastRawX_ = 0;
  mutable int32_t lastRawY_ = 0;
  // The one sample pair taken this tick (on the kUp call), reused by the
  // other three direction signals -- review F9.
  mutable AxisLevel cachedX_ = AxisLevel::kNeutral;
  mutable AxisLevel cachedY_ = AxisLevel::kNeutral;
};

}  // namespace steamcore::port::esp32
