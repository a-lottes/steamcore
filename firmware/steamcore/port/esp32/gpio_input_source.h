#pragma once

#include "steamcore/input.h"

// The real device-side Source for InputReader<Source> (input-driver plan
// §1 Decision 6) -- ESP-IDF/GPIO-dependent, therefore outside the host
// `make test` gate by construction, the same boundary display-driver's
// Ili9488Display already established (constitution §4).
//
// Example (illustrative -- see firmware/system/main/app_main.cpp for the
// real on-device harness):
//   steamcore::port::esp32::GpioInputSource source;
//   source.init();
//   steamcore::InputReader<steamcore::port::esp32::GpioInputSource> reader;
//   const steamcore::GameInput input = reader.read(source);  // once per tick
//
// Contract:
//  - init() configures all seven pins (board_config.h) as inputs with
//    the internal pull-up enabled, no interrupt -- called once, before
//    the first readSignal() call.
//  - readSignal() returns the LOGICAL level InputReader expects
//    ("pressed" == true): every switch is wired to a shared ground
//    (docs/wiring-input.md), so a raw low reads as pressed --
//    `gpio_get_level(pin) == 0`. This is the one place that inversion
//    happens; nowhere else in the engine learns that "pressed" is
//    electrically low (plan §1 Decision 6).
//  - Single-threaded, nothing throws, no error code, no dynamic
//    allocation -- same inherited contract as every other steamcore
//    type.
namespace steamcore::port::esp32 {

class GpioInputSource {
 public:
  GpioInputSource() = default;

  // Configures all seven pins: input mode, internal pull-up enabled, no
  // interrupt. Call once before the first readSignal().
  void init();

  // Satisfies InputReader's Source concept.
  bool readSignal(InputSignal signal) const;
};

}  // namespace steamcore::port::esp32
