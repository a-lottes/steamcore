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

}  // namespace steamcore
