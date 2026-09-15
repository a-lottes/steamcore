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

As of 2026-09-15, the module and both discrete buttons have been rewired
directly to the board (breadboard removed from the signal path) per the
pin table above, to rule out the breadboard as the suspected common-cause
fault. This was re-tested the same day (T7's harness, restored from commit
`c5c01b0` and reflashed for this manual check only — not a source change,
`app_main.cpp` reverted to `highscore-system`'s harness immediately after):

- **`select` (joystick `SW`, GPIO21): confirmed working.** One clean
  `pressed`/`released` pair was logged for a single deliberate press
  (~180 ms apart), exactly the expected behaviour.
- **`start` (Taster 1, GPIO47) and `fire` (Taster 2, GPIO8): still
  faulty**, but with a new, more specific symptom than the breadboard
  attempt — both logged `pressed` within the first 344 ms of boot and
  never once logged `released` over the full ~200 s session, i.e. they
  read permanently pressed regardless of the physical button. This is
  the signature of a short rather than a flaky connection, and matches a
  known 4-pin-tactile-switch mistake: if the two GPIO/GND wires land on
  the *same* internally-bridged pin pair (rather than diagonal pins from
  the two separate pairs), the switch reads permanently closed
  independent of whether it's pressed. **Next step: re-check both
  buttons' pin pairs with a multimeter continuity test** (unpressed
  state should read open, not closed) before re-testing.
- **`rawX`/`rawY`: still not usable**, and no longer intermittent like
  the breadboard attempt — `rawX` stayed at a constant `0` for the
  entire session (never tracked stick movement), `rawY` mostly sat at
  `0` too with one brief transient up to `~188`. Neither axis approached
  the expected ~2047 centred rest value or the 0–4095 sweep a moved
  stick should produce. Since `select`'s clean result rules out a
  shared-GND fault this time, this now looks like an independent problem
  on `VRX`/`VRY` specifically (module wiring order, a bent/miswired pin,
  or GPIO1/2 continuity) rather than the common-cause hypothesis from
  the first attempt — not yet confirmed with a multimeter.

A same-day follow-up fixed both discrete buttons (the 4-pin pin-pair
mistake above was the actual cause) and added a second, no-multimeter
diagnostic: a throwaway on-panel harness (not committed — see this
section's history in git for its content if it needs re-creating) that
draws each of the seven `GameInput` fields plus `rawX`/`rawY` live on the
ILI9488 screen instead of the serial log, so a stuck or dead signal is
visible without a second window. Result with this harness, module still
on the documented 3.3V rail:

- **Both discrete buttons: confirmed working.**
- **The joystick module's `SW` and its two analog axes were never
  observed working at the same time** — `SW` registered presses only
  while the module was (experimentally, against this doc's own guidance)
  powered from 5V instead of 3.3V, and the axes only produced a plausible
  centred/sweeping reading while powered from 3.3V. **This is not being
  adopted as "SW needs 5V"** — running the module at 5V risks exactly the
  ADC damage this doc's Power rail section already warns about, and on
  most 5-pin modules `SW`'s idle level is sourced from the same shared
  VCC pin as the axes, so a 5V idle level on `SW` risks GPIO21 the same
  way. The working hypothesis is that the axis behaviour at 5V is the
  ADC clipping/saturating on out-of-range voltage (i.e. confirms *not* to
  use 5V, rather than requiring it), and that `SW`'s failure at 3.3V is
  an independent bad connection on that one leg — not yet isolated.
  **Open, next step:** re-seat/re-check `SW`'s own wire at GPIO21 while
  the module stays on 3.3V, then re-run the on-panel diagnostic; do not
  re-apply 5V to chase this.

A second same-day follow-up re-seated `SW`'s wire at GPIO21 (module still
on 3.3V) and re-ran the on-panel diagnostic:

- **Both discrete buttons and both analog axes: confirmed working
  together at 3.3V** — `rawX`/`rawY` move plausibly with the stick and
  the two buttons toggle cleanly. This resolves AC-3.1/AC-3.4's button
  half and rules out a wiring-wide fault; the earlier 5V/3.3V split was
  specific to `SW`, not shared by the rest of the circuit.
- **`SW`: still not registering, unchanged by the re-seat.** Since a
  reseat of the same physical wire made no difference, this is no longer
  a "bad connection" hypothesis — the fault is more likely GPIO21 itself
  or the module's click switch, not the joint between them.

**The swap test (no multimeter needed) was run and is conclusive.** The
physical Taster button swapped cleanly across both GPIO8 and GPIO21 —
confirmed working on either pin. The joystick module's `SW` switch,
swapped the same way across both pins, registered on **neither**. Since
the same two GPIOs that just proved themselves good (with the Taster)
fail identically with `SW`, the board side (both candidate pins, and by
extension the internal pull-up path) is cleared — **the fault is
isolated to the joystick module's `SW` switch or its own wire**, not to
GPIO21 or to anything else already ruled out (breadboard, shared GND,
5V/3.3V rail).

**Next steps**, roughly in order of effort: (1) try swapping the actual
jumper wire between the module's `SW` pin and the board for a
known-good spare — a single broken strand inside a jumper wire is a
common failure mode this test can't distinguish from a dead switch; (2)
visually inspect the module's `SW` solder pad for a cracked/cold joint
under good light or magnification; (3) if both come back clean, treat
the module's built-in click switch as defective — at that point `SW` as
a signal source is a hardware dead end on this specific module, and
re-litigating A3's mapping (`SW` → `select`) against a spare discrete
button becomes a product decision, not a wiring one.

**A second, different joystick module was substituted and showed the
identical failure** — `SW` still never registers, on either GPIO, while
the plain Taster continues to work on both. Two independently defective
switches is unlikely; this is now treated as a structural incompatibility
between this class of module's `SW` output and this circuit (most likely
the ESP32-S3's internal pull-up being too weak against some leakage path
specific to the module's `SW` line — not confirmed, not worth further
multimeter-less chasing) rather than a per-unit hardware defect.

**Decision: `SW` is abandoned as a signal source.** The wiring reverts to
exactly the pin table above minus `SW` — Taster 2 goes back to GPIO8
(`fire`), Taster 1 stays on GPIO47 (`start`), `VRX`/`VRY` stay on GPIO1/2
as documented, and the joystick module's `SW` wire is left disconnected
rather than connected to any GPIO. `select` therefore has no physical
input on this hardware and will always read `false` — this is not a crash
or an unhandled case: `AnalogJoystickSource`/`InputReader` already treat
an unpressed signal as the default steady state, the same fail-safe
posture the doc's own ADC-failure case already documents for the axes.

This is a hardware limitation being recorded, not a code change — no
source in `firmware/` needs to change for `select` to simply stay
unpressed. **Open product question, not decided here:** A3's mapping
(`SW` → `select`) can no longer be fulfilled by this hardware; whether
`select` gets reassigned to a third discrete button, or the product
accepts running without a `select` input, is a spec-level call for
`/story-time` or a plan amendment, not resolved by this wiring doc.

T8: both discrete buttons and both analog axes are confirmed working at
the documented, safe 3.3V supply (AC-3.1, AC-3.4's start/fire half).
`SW`/`select` (the rest of AC-3.4) is not achievable with either tested
module and is recorded here as a hardware limitation rather than left
open-ended — `qa.md` should capture this exact split rather than a bare
`blocked`.
