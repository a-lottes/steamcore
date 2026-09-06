#pragma once

// GPIO pin assignment for the ESP32-S3-N16R8 prototype board (constitution
// §3: "every GPIO number lives in one central board_config.h. No GPIO
// literal anywhere else"). Fixed 2026-09-02 when wiring the KMRTM35018-SPI
// (ILI9488) panel: avoids GPIO26-37, reserved on this chip variant for the
// octal PSRAM/flash bus, and the boot-strapping pins 0, 3, 45, 46. The SPI
// pins land on ESP32-S3's SPI2_HOST IOMUX set (CS0/MOSI/SCK/MISO) for the
// lowest-overhead path, ahead of the dirty-tile DMA push this same bus will
// carry once the real display driver exists.
//
// Plain ints, not gpio_num_t: this header must stay includable from
// hardware-independent code without pulling in an ESP-IDF driver header.

namespace steamcore {

inline constexpr int kPinDisplayMosi = 11;       // SDI
inline constexpr int kPinDisplaySck = 12;        // SCK
inline constexpr int kPinDisplayMiso = 13;       // SDO -- wired, unused by the write-only bring-up test
inline constexpr int kPinDisplayCs = 10;         // CS
inline constexpr int kPinDisplayDc = 14;         // DC/RS
inline constexpr int kPinDisplayReset = 9;  // RESET

// LED (backlight) is wired directly to 3V3, not to a GPIO: the panel's
// datasheet gives a backlight *voltage* range (3.0-3.6V), implying an
// onboard current-limiting resistor already sized for a 3.3V rail, so no
// external resistor or GPIO control was needed for this bring-up test.
// Revisit if the real driver wants PWM dimming later.

// input-driver (T6): seven single-pole normally-open momentary switches,
// each one leg to its GPIO and the other to a shared common ground, with
// the internal pull-up enabled in firmware (active-low: a raw read of 0
// means pressed) -- no external resistors, no analog joystick. Chosen to
// avoid: GPIO9-14 (already claimed by the display, above), GPIO26-37
// (reserved for octal PSRAM/flash on this chip variant), the
// boot-strapping pins 0/3/45/46, and GPIO19/20 (native USB D-/D+, needed
// for the serial-log capture workaround documented in docs/device-build.md).
// GPIO16 is deliberately skipped, not just unused: tools/check_constraints.sh's
// tile-size-literal rule bans a bare `16` anywhere in include/ except
// config.h, and this file is not config.h -- `kPinInputX = 16` here would
// fail `make lint` (review F4). Cheaper to skip one pin than weaken that
// rule.
inline constexpr int kPinInputUp = 4;
inline constexpr int kPinInputDown = 5;
inline constexpr int kPinInputLeft = 6;
inline constexpr int kPinInputRight = 7;
inline constexpr int kPinInputStart = 15;
inline constexpr int kPinInputFire = 17;
inline constexpr int kPinInputSelect = 18;

// analog-joystick-input (T4): a SECOND, mutually exclusive control
// scheme -- this project wires either the seven kPinInput* switches
// above or these five pins, never both at once (docs/wiring-analog-
// joystick.md). Two discrete digital buttons plus the joystick's own
// integrated click, and an analog joystick's two potentiometer axes.
// kPinJoystickVrx/Vry must sit on an ADC1-capable pin (GPIO1-10 on this
// chip); only 1, 2 and 8 are free there once GPIO9/10 (display) and
// GPIO4-7 (kPinInput* above) are excluded -- GPIO3 is boot-strapping.
// The three digital pins (Sw/Start/Fire) can be any free GPIO: chosen
// to avoid GPIO9-14 (display), GPIO4-7/15/17/18 (kPinInput* above, so
// the two schemes never double-book a pin), GPIO26-37 (octal PSRAM/
// flash on this chip variant), the boot-strapping pins 0/3/45/46,
// GPIO19/20 (native USB D-/D+), GPIO43/44 (UART0), GPIO38/48 (the
// onboard RGB LED, which pin depends on the DevKitC-1 revision), GPIO
// 39-42 (JTAG, already avoided by kPinInput* above), and GPIO16
// (tools/check_constraints.sh's tile-size-literal rule bans a bare `16`
// in include/ outside config.h). The ADC unit/channel for Vrx/Vry is
// derived at init time via ESP-IDF's adc_oneshot_io_to_channel(), never
// written down as a second literal here.
inline constexpr int kPinJoystickVrx = 1;    // ADC1_CH0
inline constexpr int kPinJoystickVry = 2;    // ADC1_CH1
inline constexpr int kPinJoystickSw = 21;    // -> select
inline constexpr int kPinJoystickStart = 47;  // Taster 1 -> start
inline constexpr int kPinJoystickFire = 8;    // Taster 2 -> fire

}  // namespace steamcore
