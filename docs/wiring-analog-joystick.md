# Analog Joystick Wiring

Physical wiring guide for the `analog-joystick-input` feature: a five-pin
analog joystick module plus two discrete pushbuttons feeding
`steamcore::port::esp32::AnalogJoystickSource`. This is a **second, mutually
exclusive** control scheme — the console is wired with either this or the
seven-microswitch scheme in `docs/wiring-input.md`, never both at once.
**Nothing is wired yet (as of 2026-09-06)** — this is the reference for the
next physical-world action, before US-3's on-device confirmation (plan.md
T8) can be attempted.

## What you need

At the component-class level (no part numbers):

- One 5-pin analog joystick module (`VRX`, `VRY`, `SW`, `+5V`/`VCC`, `GND`) —
  two potentiometer axes plus an integrated momentary pushbutton.
- Two single-pole, normally-open momentary microswitches (Taster 1, Taster
  2) — the same component class `docs/wiring-input.md` already uses.
- Hookup wire to a shared ground rail.

No external pull-up or pull-down resistors on the three digital signals —
the ESP32-S3's internal pull-ups, enabled in firmware, are used instead. The
two analog axes need no external circuitry either — the module's own
potentiometers already form the voltage divider the ADC reads.

## Power rail — 3.3V, never 5V

**The module is powered from the board's own 3.3V (3V3) rail, never from a
5V source**, even though the module's own silkscreen may label that pin
`+5V`. The module is a plain resistive divider with no active logic on it,
so it runs fine at 3.3V. This matters because the ESP32-S3's ADC and GPIO
pins are referenced to 3.3V and **are not 5V-tolerant** — feeding 5V into
`VRX`/`VRY` risks permanently damaging the ADC input. Confirm the module's
supply pin before connecting it, every time, not just once.

## Topology

The three digital signals (`SW`, and the two discrete buttons) are wired
exactly like `docs/wiring-input.md`'s switches: one leg to its GPIO pin, the
other leg to a shared common ground. With the internal pull-up enabled, a
released switch reads logic 1 and a pressed switch pulls the pin to ground
(logic 0) — active-low, inverted back to "pressed == true" in exactly one
place, `AnalogJoystickSource::readSignal`.

`VRX`/`VRY` connect directly to their ADC1-capable GPIO pins — no inversion,
no pull-up: the ESP32-S3's ADC reads the raw analog voltage the module's
potentiometers produce, 0V to 3.3V mapped to roughly 0-4095 counts at this
project's chosen 12-bit resolution. All signal levels are 3.3V logic.

## Pin table

| Signal | GPIO | Constant (`board_config.h`) | Maps to |
|---|---|---|---|
| VRX | 1 | `kPinJoystickVrx` | left/right (via threshold, `analog_axis.h`) |
| VRY | 2 | `kPinJoystickVry` | up/down (via threshold, `analog_axis.h`) |
| SW (joystick click) | 21 | `kPinJoystickSw` | `select` |
| Taster 1 | 47 | `kPinJoystickStart` | `start` |
| Taster 2 | 8 | `kPinJoystickFire` | `fire` |

`VRX`/`VRY` must land on an ADC1-capable pin (GPIO1–10 on this chip
variant); GPIO1, 2 and 8 are the only ones free once the display (9–10) and
the other control scheme's own pins (4–7, above) are excluded.

## What's excluded, and why

- **GPIO9–14** — already claimed by the display (`board_config.h`).
- **GPIO4–7, 15, 17, 18** — already claimed by `docs/wiring-input.md`'s
  seven-microswitch scheme. The two schemes must never double-book a pin,
  even though only one is ever wired at a time.
- **GPIO26–37** — reserved for the octal PSRAM/flash bus on this chip
  variant (ESP32-S3-N16R8); using them for anything else corrupts
  flash/PSRAM access.
- **GPIO0, 3, 45, 46** — boot-strapping pins.
- **GPIO19, 20** — the native USB D-/D+ pair (serial-log capture,
  `docs/device-build.md`).
- **GPIO43, 44** — UART0 TX/RX.
- **GPIO38, 48** — drives the onboard RGB LED on some ESP32-S3-DevKitC-1
  revisions; which pin depends on the board revision, so both are skipped
  rather than checked per-unit.
- **GPIO39–42** — the classic JTAG pins, already avoided by
  `docs/wiring-input.md`'s own scheme for the same reason.
- **GPIO16** — skipped deliberately: `tools/check_constraints.sh`'s
  tile-size-literal rule bans a bare `16` anywhere in `include/` except
  `config.h`, so a pin constant assigned to 16 would fail `make lint`.
  Cheaper to skip one pin than weaken that rule (the same reasoning
  `docs/wiring-input.md` already recorded for its own scheme).

## Known limitation: off-centre rest reading

This feature ships a **fixed** deadzone threshold centred on the ADC's
*nominal* midpoint (2047 of 4095 counts) — there is no per-unit calibration
step. If the physical joystick's true resting voltage falls outside that
fixed band (manufacturing tolerance), it may read a spurious direction as
active at rest, or lose usable range on one side. This is an accepted,
documented trade-off (spec A13/A14), not a bug — US-3's on-device pass
(plan.md T8) records the module's actual observed resting values so this is
measured, not guessed.

## Status

A first wiring attempt (2026-09-06) did not yet produce a confirmable
result: on the flashed harness (plan.md T7), `rawX`/`rawY` were unstable
(observed jumping between 0, 4095, and briefly a plausible ~2600 centred
value), and none of the three digital signals (start/fire/select) ever
registered a press. Both symptoms together point at a common-cause fault,
most likely an unreliable GND connection between the board and the
breadboard rather than three independent pin failures — but this was not
confirmed with a multimeter and remains a hypothesis. Next step before
retrying: verify continuity on every connection, especially the shared
ground, then re-run the harness. T8 stays `blocked` until a stable reading
is observed.
