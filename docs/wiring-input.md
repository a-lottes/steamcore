# Input Wiring

Physical wiring guide for the input-driver feature: seven buttons/switches
feeding `steamcore::port::esp32::GpioInputSource`. **Nothing is wired yet
(as of 2026-09-04)** — this is the reference for the next physical-world
action, wiring the panel, before US-4's on-device confirmation (plan.md
T10) can be attempted. `docs/device-build.md` covers building and flashing
once the wiring below is in place.

## What you need

At the component-class level (no part numbers):

- Seven single-pole, normally-open momentary microswitches — four for the
  directions, one each for start/fire/select. **Not** a multi-position
  joystick switch and **not** an analog stick: the four directions are
  four entirely independent microswitches, wired and read exactly like
  the other three buttons.
- Hookup wire to a shared ground rail.

No external pull-up or pull-down resistors — the ESP32-S3's internal
pull-ups, enabled in firmware, are used instead.

## Topology

Every switch is wired the same way: one leg to its GPIO pin, the other leg
to a shared common ground. With the internal pull-up enabled, a released
switch reads logic 1 and a pressed switch pulls the pin to ground (logic
0) — **active-low**. `steamcore::port::esp32::GpioInputSource` is the one
place that inverts this back to the logical "pressed == true" that
`InputReader`/`GameInput` expect; nowhere else in the engine sees the
inversion. All signal levels are 3.3V logic — the ESP32-S3's own I/O
voltage, not 5V.

## Pin table

| Signal | GPIO | Constant (`board_config.h`) |
|---|---|---|
| Up | 4 | `kPinInputUp` |
| Down | 5 | `kPinInputDown` |
| Left | 6 | `kPinInputLeft` |
| Right | 7 | `kPinInputRight` |
| Start | 15 | `kPinInputStart` |
| Fire | 17 | `kPinInputFire` |
| Select | 18 | `kPinInputSelect` |

These are independent of, and do not conflict with, the display's already-
claimed pins (`board_config.h`): CS=10, RESET=9, DC=14, MOSI=11, SCK=12,
MISO=13.

## What's excluded, and why

- **GPIO9–14** — already claimed by the display (above).
- **GPIO26–37** — reserved for the octal PSRAM/flash bus on this chip
  variant (ESP32-S3-N16R8); using them for anything else corrupts flash/PSRAM
  access.
- **GPIO0, 3, 45, 46** — boot-strapping pins; a switch resting in the wrong
  state here can prevent the board from booting at all.
- **GPIO19, 20** — the native USB D-/D+ pair, needed for the serial-log
  capture workaround documented in `docs/device-build.md`.
- **GPIO16** — skipped deliberately, even though it is otherwise free:
  `tools/check_constraints.sh`'s tile-size-literal rule bans a bare `16`
  anywhere in `include/` except `config.h`, so a pin constant assigned to
  16 here would fail `make lint`. Cheaper to skip one pin than weaken
  that rule (review F4).
- **GPIO15 and GPIO16 both carry alternate IOMUX functions** on this chip
  (`XTAL_32K_P`/`U0RTS` on 15, `XTAL_32K_N`/`U0CTS` on 16) — they're free
  to use as plain GPIO here only because no 32.768 kHz crystal is fitted
  on this board and UART0 hardware flow control is unused. Worth knowing
  *before* soldering START (GPIO15) to them, not after (review F5).

## Status

Nothing is physically wired yet. Wiring this table up on the real board is
the explicit next step before plan.md's T10 (hardware-gated on-device
confirmation) can be attempted; until then T10 is reported `blocked`.
