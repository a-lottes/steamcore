# Spec: analog-joystick-input

| | |
|---|---|
| **Phase** | Specify |
| **Owner** | Product Owner (`/story-time`), Designer (`/look-and-feel`) |
| **Status** | `approved` |
| **Date** | 2026-09-05 |
| **Ticket** | `none` |

**Handoff**
- **Status:** `approved` — user approved 2026-09-05. All three previously-open forced product/hardware choices (§3 A3, A4, A5) resolved by the user; zero open items remain (§7 C1–C3).
- **Summary:** add exactly one new `Source` type, `AnalogJoystickSource`, alongside the already-shipped `GpioInputSource` — never replacing or modifying it — so the physical hardware already on the breadboard (a 5-pin analog joystick + two discrete pushbuttons) has a software path into the unmodified `InputReader<Source>`/`GameInput`. The four direction signals come from ADC axis reads run through deadzone/threshold math (host-testable, pure integer); the three button signals come from three digital active-low reads — Taster 1 = `start`, Taster 2 = `fire`, joystick `SW` = `select` — exactly `GpioInputSource`'s existing pattern. Powered from the board's 3.3V rail, never 5V.
- **Open:** `0 open` — A3 (button mapping), A4 (threshold approach) and A5 (power rail) all resolved; see §7 C1–C3 and the follow-on judgment calls C7–C9 the resolutions surfaced.
- **Binding ruling:** §4 User Stories for the current stories; §7 Clarifications for what changed since the last round and why.
- **On conflict:** the numbered body below wins for everything except `Status`; log the mismatch as a finding at the next `/peer-review` and proceed — don't stop on it.

## 1. Problem & Goal

- **Problem:** `GpioInputSource` (`input-driver`, v0.3.0) satisfies `InputReader`'s `Source` concept for seven purely-digital microswitch reads — but the parts actually sitting on the breadboard today are not seven microswitches. They are one 5-pin analog joystick module (two potentiometer axes, `VRX`/`VRY` — analog voltages, not digital levels — plus an integrated `SW` pushbutton) and two separate discrete pushbuttons. No `Source` implementation today can read an ADC axis, so this hardware has zero software path into `InputReader`/`GameInput`, even though it's already wired.
- **Goal:** a second `Source` type, `AnalogJoystickSource`, satisfying `bool readSignal(InputSignal) const` exactly as `GpioInputSource` does — so `InputReader<Source>` and `GameInput` are used completely unmodified — deriving the four direction booleans from ADC reads plus deadzone/threshold logic, and start/fire/select from three digital active-low reads (two discrete buttons + the joystick's own `SW` pin).
- **Success signal:** host-side, the threshold/deadzone function proves correct center/threshold/diagonal behavior against synthetic ADC values with zero ESP-IDF header touched. `idf.py build` stays green for all of `firmware/steamcore/port/esp32/`, including `GpioInputSource` unmodified. Once wired end to end: a human moving the real stick to each of the four cardinal extremes, a deliberate diagonal, and back to center, plus pressing each of the three buttons, produces exactly the corresponding `GameInput` field toggle in the serial log — center reads all-four-false with no chatter over a sustained hold.
- **Why now:** the hardware is already on the breadboard, unused, and the engineering gap (ADC peripheral + threshold math) is genuinely new — `GpioInputSource` cannot be reused or trivially extended to cover it, so it needs its own `/story-time` pass rather than a docs-only wiring update.

## 2. Target Users

- **Engine developer (primary, today):** owns the breadboard and needs the analog joystick's two axes and the two discrete buttons to reach `InputReader`/`GameInput` without touching anything already shipped.
- **Future game-module author (indirect beneficiary):** eventually consumes the same `GameInput` regardless of which `Source` populated it — this feature must never make that swap visible above the `Source` boundary.
- *Not a user of this feature:* the console player — no game, menu or registry exists yet, and this feature does not decide which `Source` a shipped game ultimately uses (§6).

## 3. Assumptions & Open Questions

| # | Assumption / Question | Resolution |
|---|---|---|
| A1 | **Structural, additive-only change.** `AnalogJoystickSource` is a brand-new type in `firmware/steamcore/port/esp32/`, mirroring `GpioInputSource`'s file layout (`analog_joystick_source.{h,cpp}`). `InputReader<Source>`, `Debouncer`, `GameInput`, the `Source` concept itself, and `GpioInputSource` are all **untouched** — this feature does not edit any of those five files. Multiple port-level `Source` implementations coexisting is intentional. | Accepted |
| A2 | **Axis-to-direction convention.** `VRX` maps to left/right, `VRY` maps to up/down — the joystick module's own pin labels' conventional meaning. Which raw extreme (high vs. low ADC reading) corresponds to which named direction is confirmed empirically during US-3's on-device pass, not fixed by this spec — a trivial, low-risk implementation detail, not a product decision. | Accepted |
| A3 | **Button-to-signal mapping.** Taster 1 → `start`, Taster 2 → `fire`, joystick `SW` → `select`. Chosen because `SW` naturally sits under the thumb already resting on the stick, suited to `select` as the most frequently-used of the three during normal play (menu confirm / in-game action, depending on the eventual consumer). | **Resolved by the user** |
| A4 | **Threshold/deadzone approach.** A fixed percentage-of-range threshold, symmetric around the ADC's nominal midpoint (a compile-time constant derived from the ADC's resolution, e.g. half of full-scale) — no calibration step, no calibration UI, no persisted calibration state. The exact percentage is an implementation-tuning constant, deferred to `/sprint-plan`/`/increment` and validated empirically on real hardware (US-3, AC-3.1–3.3) — mirrors A8's precedent of deferring exact GPIO numbers rather than fixing an implementation detail in the spec. See A13/A14 for what this does and does not cover. | **Resolved by the user** |
| A5 | **Power rail.** 3.3V, from the board's own 3V3 rail — never 5V. The module is a plain resistive divider with no active ICs, so it works fine at 3.3V; this also matches the ESP32-S3's own 3.3V-referenced I/O/ADC (not 5V-tolerant), avoiding any risk of ADC pin damage from over-volting. See A16 for what this does and does not require in code. | **Resolved by the user** |
| A6 | **Diagonal handling.** Both adjacent direction booleans may read `true` simultaneously on a diagonal push — the same "four independent bits, no cardinal-snapping" philosophy `input.h` and `input-driver` (AC-1.4, A2) already shipped for `GpioInputSource`. Resolved by precedent; flagged for the user's sanity-check, not re-litigated. | Resolved (PO judgment, by precedent) |
| A7 | **MoSCoW split.** Host-testable threshold math (US-1) and the buildable device-side `Source` (US-2) are Musts, fully verifiable without the physical joystick attached (`idf.py build` needs the toolchain, not the board). On-device human confirmation (US-3) is a **Should**, hardware-gated per `CLAUDE.md`'s established "buildable half vs. blocked half" pattern and `input-driver`'s own US-4 precedent. | Resolved (PO judgment, by precedent) |
| A8 | New GPIO/ADC-channel constants (2 discrete buttons, joystick `SW`, `VRX`, `VRY` — 5 new pins) land in `board_config.h`, avoiding already-claimed pins (display 9–14; `GpioInputSource`'s 4,5,6,7,15,17,18), reserved ranges (26–37 PSRAM/flash, boot-strap 0/3/45/46, USB D-/D+ 19/20), and GPIO16 (lint-skipped, `board_config.h`'s own note). `VRX`/`VRY` additionally need an ADC1-capable pin (GPIO1–10 on this chip) — exact numbers are a `/sprint-plan` decision, not fixed here. | Accepted |
| A9 | No dynamic allocation, no wall-clock read, no unseeded RNG anywhere the threshold decision depends on; tick-driven only — inherited unchanged from the constitution and every prior feature. | Accepted |
| A10 | The threshold/deadzone logic is exposed as a pure function/routine taking raw ADC-shaped integer input, separable from the actual ADC peripheral read — the same pure/impure seam `TilePusher<Transmitter>` and `Debouncer` already established — so it is host-testable with zero ESP-IDF header touched. | Accepted |
| A11 | The joystick's `SW` pin is an integrated momentary switch with the same active-low, internal-pull-up digital behavior as the two discrete buttons and `GpioInputSource`'s existing seven signals — same topology, same inversion convention. | Accepted |
| A12 | This feature does not wire either `Source` into any consumer, menu, registry or game — mirrors `input-driver`'s own boundary. The on-device harness (US-3) is throwaway/test-only. | Accepted |
| A13 | **Deadzone basis, follow-on from A4.** The deadzone band is defined around the ADC's *nominal* midpoint (a fixed compile-time constant), not a value measured or calibrated per physical unit — there is no per-device calibration step to derive it from (A4). This is the simplest-slice consequence of "no calibration," not an oversight. | Resolved (PO judgment, follows directly from A4) |
| A14 | **Asymmetric/non-centered rest reading, explicit limitation.** Because A13 fixes the deadzone to the *nominal* midpoint rather than a per-unit measured one, a physical joystick whose true resting ADC value falls outside that nominal deadzone band (e.g. due to manufacturing tolerance) will read a spurious direction as `true` at rest, or lose effective range on one side — and this feature ships no software correction for it. Accepted as a known, explicit limitation of the fixed-threshold/no-calibration approach the user chose (A4), not silently glossed over. If real hardware shows this in US-3's on-device pass, it becomes a candidate for a future calibration feature, not a bug in this one (§6). | Resolved (PO judgment, explicit trade-off of A4) |
| A15 | **ADC read failure, fail-safe default.** If the underlying ADC peripheral read reports an error (a real ESP-IDF `adc_oneshot` possibility, e.g. driver-not-ready), `readSignal` for that direction signal returns `false` — the same neutral value as a centered stick — rather than throwing or propagating the error, matching `GpioInputSource`'s existing no-throw contract and the constitution's no-exceptions constraint. | Resolved (PO judgment, extends A9/A11's inherited no-throw contract) |
| A16 | **3.3V requires documentation, not a code non-negotiable.** `board_config.h` holds no voltage-dependent constant today (voltage is a wiring fact, not a GPIO number or code branch) — mirroring the display's own 3.3V-only backlight precedent (constitution §3), which is likewise recorded as a comment, not a compile-time value. A5's resolution is therefore satisfied by explicit documentation (US-2/AC-2.6, NFR-9) stating 3.3V plainly; no firmware logic branches on supply voltage, and none should — a voltage mistake is a wiring error to be checked before power-on, not a runtime condition to detect in software. | Resolved (PO judgment, answers the caller's explicit question) |

## 4. User Stories

### US-1 (Must): Deadzone/threshold logic converts raw ADC readings into four independent direction booleans, host-tested

> As the engine developer, I want the ADC→direction-boolean math proven correct against synthetic values before it ever touches a real ADC peripheral, so the one genuinely new piece of logic this feature adds is trustworthy on the host gate.

**Acceptance criteria:**

- [ ] AC-1.1: Given a synthetic raw axis value at the resting/center position (inside the deadzone), when converted, then both direction booleans for that axis read `false`.
- [ ] AC-1.2: Given a synthetic raw axis value past the configured threshold toward one extreme, when converted, then the corresponding direction boolean reads `true` and the opposite direction on that same axis reads `false`.
- [ ] AC-1.3: Given synthetic raw values on both axes simultaneously past their thresholds (a diagonal), when converted, then both corresponding direction booleans read `true` independently — no cardinal-snapping, no collapsing to one direction (A6).
- [ ] AC-1.4: Given the same synthetic value fed twice in a row, when converted both times, then both results are identical — pure function, no hidden state, no chatter from an unchanging input.
- [ ] AC-1.5: Given the threshold/deadzone routine's source, when compiled and run under host `make test`, then it builds and passes with zero ESP-IDF header included.
- [ ] AC-1.6: Given the routine's deadzone band, when inspected, then it is centered on the ADC's *nominal* midpoint (a compile-time constant, not a runtime-measured or per-unit-calibrated value — A13), and the threshold's exact percentage-of-range width is a single named compile-time constant, not a bare literal scattered across call sites.

### US-2 (Must): AnalogJoystickSource satisfies the Source concept, unmodified, alongside GpioInputSource

> As the engine developer, I want a new Source type that reads the real hardware (ADC axes + three digital buttons) through the exact same `bool readSignal(InputSignal) const` shape, so `InputReader<Source>` never has to know or care which physical control scheme is plugged in.

**Acceptance criteria:**

- [ ] AC-2.1: Given `AnalogJoystickSource`'s declaration, when read, then it provides `bool readSignal(InputSignal) const` with the exact signature `GpioInputSource` already provides — no change required to `InputReader`, `GameInput`, `Debouncer`, or the `Source` concept itself.
- [ ] AC-2.2: Given `readSignal` called for `start`/`fire`/`select`, when read, then it returns the active-low digital read of the physical button/`SW` pin assigned to that signal per A3's resolved mapping (Taster 1 = `start`, Taster 2 = `fire`, joystick `SW` = `select`), using the same pull-up-enabled inversion convention `GpioInputSource` already uses.
- [ ] AC-2.3: Given `readSignal` called for `up`/`down`/`left`/`right`, when read, then it returns US-1's threshold/deadzone routine applied to a fresh ADC read of the corresponding axis (A2).
- [ ] AC-2.4: Given the whole `firmware/steamcore/port/esp32/` directory (both `GpioInputSource` and the new `AnalogJoystickSource`), when built with `idf.py build`, then it compiles cleanly — proving `GpioInputSource` is untouched and still buildable alongside the new type.
- [ ] AC-2.5: Given `board_config.h`, when read, then every new GPIO/ADC-channel constant this feature adds lives there, none as a bare literal elsewhere (A8).
- [ ] AC-2.6: Given a written wiring reference (`docs/`), when read, then it states the pin table for all five new signals, the digital button/`SW` topology, the ADC axis wiring, and the power rail explicitly as **3.3V, from the board's 3V3 rail, never 5V** (A5/A16) — never silently assuming a voltage.
- [ ] AC-2.7: Given the underlying ADC peripheral read reports a failure, when `readSignal` is called for a direction signal, then it returns `false` rather than throwing or otherwise propagating the error (A15).

### US-3 (Should, hardware-gated on wiring): Real stick and button presses reach GameInput, confirmed on the physical board

> As the engine developer, I want a minimal on-device harness (mirrors `input-driver`'s own US-4 pattern) proving the real analog joystick and both discrete buttons drive `GameInput` correctly, so the pipeline is proven end to end, not just in simulation. If the wiring isn't complete and the module isn't confirmed running safely at 3.3V by `/increment` time, this story's ACs are reported `blocked`, never attempted, never silently passed.

**Acceptance criteria:**

- [ ] AC-3.1: Given `AnalogJoystickSource` driving the harness, when a human pushes the stick to each of the four cardinal extremes in turn, then the serial log shows exactly the corresponding direction boolean toggle `true`, with the other three staying `false`.
- [ ] AC-3.2: Given the same harness, when a human pushes the stick to a deliberate diagonal, then the log shows both adjacent direction booleans `true` simultaneously — confirms AC-1.3 on real, noisy analog hardware.
- [ ] AC-3.3: Given the stick released to center, when held there for a sustained multi-second period, then all four direction booleans read `false` throughout — no chatter from real ADC noise, and no spurious `true` from A14's known asymmetric-rest limitation on this particular unit (if one is observed, it is logged as a finding, not silently ignored).
- [ ] AC-3.4: Given a human pressing each of the two discrete buttons and the joystick's `SW` pin in turn, when read, then the log shows exactly the corresponding `start`/`fire`/`select` field toggle `true`, matching A3's resolved mapping.
- [ ] AC-3.5: If the wiring is not complete by `/increment` time, this story is reported `blocked` with the reason, and every one of its ACs is recorded in `qa.md` as not-capturable — never passed, never satisfied by a substitute (`CLAUDE.md` hardware-gated pattern).

## 5. Non-Functional Requirements

| # | Category | Requirement (measurable) | How it's verified |
|---|---|---|---|
| NFR-1 | Performance | One ADC read + threshold compare per axis per tick adds no perceptible input lag beyond `GpioInputSource`'s own digital read — "arcade immediacy" (constitution Principle 4), human-judged on-device the same way `input-driver`'s NFR-1 was. | AC-3.1–3.4 (on-device, human-judged) |
| NFR-2 | Reliability / memory | Zero dynamic allocation: no `new`/`malloc`/`std::vector`/`std::string`, including any per-axis threshold state, which is fixed-size. | `/peer-review` (grep) |
| NFR-3 | Determinism | Threshold/deadzone logic is pure integer math: no wall-clock read, no unseeded RNG, tick-driven only. | `/peer-review` (grep); AC-1.4 |
| NFR-4 | Constitution literals | No new GPIO or ADC-channel literal outside `board_config.h` (A8) — extends the existing GPIO-literal rule to ADC channels too. | `/peer-review` (grep; `tools/check_constraints.sh` pattern extension is a `/sprint-plan`/review task) |
| NFR-5 | Observability / ops | The on-device harness logs each raw ADC sample alongside its derived direction boolean, for all four direction signals, matching `input-driver`'s NFR-5 pattern. | AC-3.1–3.3 |
| NFR-6 | **Library lens — public API surface** | Exactly one new public type (`AnalogJoystickSource`) plus its threshold/deadzone entry point exposed for host testing (A10). Zero change to `Source`, `InputReader`, `GameInput`, `Debouncer`, or `GpioInputSource`'s public shape. The on-device harness (US-3) is throwaway/test-only, not counted against this surface. | `/peer-review` |
| NFR-7 | **Library lens — contract clarity** | `AnalogJoystickSource`'s header states, mirroring `GpioInputSource`'s own comment style: which physical control maps to which of start/fire/select (A3), the deadzone/threshold approach (fixed percentage of range, symmetric around the nominal midpoint, no calibration — A4/A13) and its guaranteed all-false center behavior, the fail-safe behavior on an ADC read error (A15), the power-rail assumption (3.3V, A5/A16), and the inherited no-throw/no-alloc/single-threaded contract. | `/peer-review` |
| NFR-8 | **Library lens — contract stability** | `GpioInputSource`'s file is byte-for-byte untouched; adding `AnalogJoystickSource` is purely additive — no existing call site, test, or public signature changes (A1). | `/peer-review` (diff review) |
| NFR-9 | Hardware safety | The power rail is **resolved to 3.3V** (A5) and recorded in writing (US-2/AC-2.6) *before* any ADC pin is energized on real hardware — a 5V mistake risks permanent ADC damage, so this is verified as a documentation gate, not assumed satisfied by silence. | `/peer-review` (doc present); gates US-3 |
| NFR-10 | Security & privacy | N/A — offline device, no personal data, no network path touched by an input read. | — |
| NFR-11 | Accessibility | N/A — no new visual/UI surface; physical control ergonomics/final layout is README's enclosure/Phase-4 decision. | — |

*Lens note: `library` active **scoped** (constitution §2) — semver/packaging are no-ops for this statically-linked firmware image; Public API surface, Contract clarity and Contract stability land as NFR-6/7/8.*

## 6. Out of Scope

- **Any calibration UI, persisted calibration data, or calibration flow** — the resolved fixed-threshold approach (A4) ships the smallest slice: a fixed threshold around the nominal midpoint, no calibration state.
- **Per-unit correction for a joystick that doesn't rest exactly at the ADC's nominal midpoint** — an explicit, accepted limitation of "no calibration" (A13/A14), not something this feature's software works around; a unit that shows this in US-3 is a hardware-tolerance finding, not a bug to fix here.
- **Any voltage-dependent code path or runtime detection of the supply rail** — 3.3V (A5) is a wiring and documentation fact only (A16); no firmware logic ever branches on supply voltage.
- **Modifying `GameInput`, `InputReader`, `Debouncer`, the `Source` concept, or `GpioInputSource`** — all five stay exactly as shipped (A1).
- **Wiring `AnalogJoystickSource` (or `GpioInputSource`) into any real consumer, menu, game registry, or a runtime choice between the two Sources** — that decision belongs to whichever future feature first needs a control scheme (A12).
- **A detailed electrical schematic or exact resistor/part numbers** — US-2/AC-2.6's wiring reference names pins, topology, and the power-rail resolution; component-level circuit design is a `/sprint-plan` engineering decision.
- **Diagonal-to-cardinal snapping, or any other interpretation of the four direction booleans beyond raw independent levels** — matches A6/`input-driver`'s already-shipped philosophy.
- **Any oscilloscope or electrical measurement of ADC noise/bounce** — visual/log verification only, same as `input-driver`'s A7.
- **Final enclosure control layout, spacing, or which physical button a player will eventually see labeled what** — README Phase-4 concern.

## 7. Clarifications

| # | Date | Question | Resolution |
|---|---|---|---|
| C1 | 2026-09-05 | Which physical button (discrete #1, discrete #2, joystick `SW`) maps to which of `start`/`fire`/`select`? | **Resolved by the user.** Taster 1 = `start`, Taster 2 = `fire`, joystick `SW` = `select` (A3) — picked the recommended option: `SW` sits under the thumb already at the stick, suited to the most frequently-used signal. |
| C2 | 2026-09-05 | Fixed percentage-of-range threshold with no calibration, or a calibration step? | **Resolved by the user.** Fixed percentage-of-range threshold, no calibration step, no calibration UI, no persisted calibration state (A4) — picked the recommended option: simplest implementation. |
| C3 | 2026-09-05 | Power the joystick module from 3.3V or 5V? | **Resolved by the user.** 3.3V, from the board's own 3V3 rail (A5) — picked the recommended option: matches the ESP32-S3's own 3.3V-referenced I/O/ADC; the module is a plain resistive divider with no active logic, so 3.3V works fine, and it's the safer default since the ADC is not 5V-tolerant. |
| C4 | 2026-09-05 | Does a diagonal push set both adjacent direction booleans simultaneously, or snap to one cardinal direction? | **Resolved by precedent (PO judgment).** Both true simultaneously — matches `input.h`/`input-driver` AC-1.4's already-shipped philosophy (A6). Flagged for sanity-check, not re-asked. |
| C5 | 2026-09-05 | Is the hardware-gated on-device confirmation a Must or a Should? | **Resolved by precedent (PO judgment).** Should, hardware-gated — mirrors `CLAUDE.md`'s documented "buildable half vs. blocked half" split and `input-driver`'s own US-4 (A7). |
| C6 | 2026-09-05 | Does this feature modify `GpioInputSource`, `InputReader`, `GameInput`, or `Debouncer` in any way? | **No.** Purely additive; all four stay untouched (A1, NFR-8). |
| C7 | 2026-09-05 | Now that A4 fixes "no calibration," what does the deadzone center on, and is a joystick that rests off-center handled? | **Resolved (PO judgment), follows directly from A4.** The deadzone centers on the ADC's *nominal* midpoint, a compile-time constant — there's no calibration step to derive a per-unit value from (A13). An off-center rest reading outside that band is an explicit, accepted limitation, not corrected in software (A14, §6). |
| C8 | 2026-09-05 | What does `readSignal` return if the underlying ADC peripheral read itself fails (an `adc_oneshot` error)? | **Resolved (PO judgment).** Returns `false`, the same neutral value as a centered stick — no throw, no propagated error — matching `GpioInputSource`'s existing no-throw contract (A15, AC-2.7). |
| C9 | 2026-09-05 | Does the 3.3V decision (A5) need a corresponding constitution or `board_config.h` code non-negotiable? | **Resolved (PO judgment).** No — `board_config.h` holds no voltage-dependent constant (voltage is a wiring fact, mirroring the display's own 3.3V-only precedent, recorded as a comment). A5 is satisfied by explicit documentation (AC-2.6, NFR-9), not a new code guard (A16). |

## 8. Design Review

- **Overall impression:** N/A — no new visual/UI surface. This feature reads physical buttons/ADC axes and populates `GameInput`; it changes no rendering, menu, or on-screen element.
- **Heuristics findings:** N/A — no user-facing visual interaction surface exists in this increment.
- **Accessibility notes:** N/A — see NFR-11.
- **Design risks & required changes:** none for a visual surface. The equivalent risk here is the *API* contract, covered by the `library` lens (NFR-6/7/8).

---

## ✅ SPEC GATE

*All boxes checked → `/sprint-plan` may start. Any box open → back to `/story-time` or `/look-and-feel`.*

- [x] Problem, goal and success signal are concrete (no buzzwords, no "everyone")
- [x] Every story has testable Given/When/Then acceptance criteria
- [x] Stories are prioritized (MoSCoW) and at least one is a Must
- [x] Non-functional requirements are stated and measurable (or marked N/A with reason)
- [x] Clarify pass done: no ambiguity left unresolved or unparked — functional scope (A1/A2/A13), data (A8, pin table; A13/A14, deadzone basis and its explicit limitation), roles/permissions (N/A, no UI), error/edge cases (A6, diagonal; A15, ADC-read-failure fail-safe; AC-1.4, boundary consistency), NFRs (NFR-6/7/8/9 for the library lens + hardware safety, now stating 3.3V explicitly), integrations (A8, board_config.h; A16, no voltage-dependent code path), UX flows (N/A, no UI), out-of-scope (§6, now covering per-unit drift and voltage-dependent code) — all swept twice, once before and once after the three forced choices resolved
- [x] Open questions are resolved or explicitly accepted as risk — **0 remain open**; A3/A4/A5 resolved by the user (C1–C3), their follow-on ambiguities resolved by PO judgment and logged (C7–C9)
- [x] Out-of-scope section is filled (something was consciously cut)
- [x] Constitution (`.spark/constitution.md`) respected, or conflicts recorded as open questions — no conflict found; A16 explicitly checked the 3.3V decision against the constitution's existing display-rail precedent
- [x] Design review done for UI-facing features (or marked N/A with reason) — marked N/A (§8): no new visual/UI surface
- [x] Line budget respected: Ist 160 / Soll ~250 (excluding HTML comments)
- [x] Status set to `approved` by the user — 2026-09-05
