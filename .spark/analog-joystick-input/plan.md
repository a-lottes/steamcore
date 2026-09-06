# Plan: analog-joystick-input

| | |
|---|---|
| **Phase** | Plan |
| **Owner** | Engineering Manager (`/sprint-plan`) |
| **Input** | `.spark/analog-joystick-input/spec.md` (`approved`) |
| **Status** | `approved` |
| **Date** | 2026-09-05 |

**Handoff**
- **Status:** `approved` — user approved 2026-09-06, no changes requested, including the two items flagged
  for explicit approval: the NFR-6 public-surface reading (§1 Decision 7, 9 symbols) and the deadzone value
  20 % of full scale (§1 Decision 3).
- **Summary:** Same boundary as the two ESP-IDF features before it — pure half header-only in
  `include/steamcore/analog_axis.h`, impure half in `port/esp32/analog_joystick_source.{h,cpp}`. The seam is a
  three-state `AxisLevel` (`kNeutral`/`kLow`/`kHigh`) produced by one `constexpr axisLevel(int32_t raw)`, so an
  ADC read failure is simply "this axis is neutral" instead of a per-signal error branch, and the whole
  raw-ADC → `GameInput` chain is host-proven before a single ESP-IDF line is written.
- **Open:** `none` — T1-T7 `done`, T8 `blocked` (hardware-gated, per its own DoD; see §3). All Musts complete.
- **Binding ruling:** §3 Task Breakdown for current task status; a plan revision after review/QA findings updates §1/§3 in place, never a new section
- **On conflict:** the numbered body below wins for everything except `Status`; log the mismatch as a finding at the next `/peer-review` and proceed — don't stop on it.

## 1. Architecture Decision

- **Context:** The ESP-IDF boundary is settled precedent (`display-driver` plan §1 Decision 1, `input-driver`
  §1 Decision 1): `port/esp32/` is a directory the host Makefile's `src/*.cpp` glob structurally cannot see,
  and constitution §4's "no ESP-IDF header in `include/`/`src/`" stays absolute. `InputReader<Source>` is
  shipped and must not change (A1/NFR-8). What is genuinely undecided is (a) the **shape** of the
  ADC→direction routine and where the pure/impure cut runs through it, (b) the **deadzone constant**, (c)
  five **pin assignments**, and (d) how AC-2.7's ADC-failure fallback is expressed without a per-signal
  error branch. Two constraints bound all four: NFR-3/A9 forbid a clock or RNG anywhere on the input path
  (already lint-enforced *into* `port/` for input specifically), and §6 forbids dynamic allocation.

- **Decision:**
  1. **Pure half: one header-only `include/steamcore/analog_axis.h`.** No `.cpp` (nothing to link, mirroring
     `input.h`), so no `Makefile` change and no `CMakeLists.txt` entry; the host suite picks up
     `test/analog_axis_test.cpp` through the existing `test/*_test.cpp` glob.
  2. **The routine returns a named three-state level, not two booleans:**
     `enum class AxisLevel { kNeutral, kLow, kHigh };` and
     `constexpr AxisLevel axisLevel(int32_t raw)`. One potentiometer axis physically cannot be at both
     extremes, and a `{bool low; bool high;}` pair makes that impossible state representable. Naming the
     state also matches `game_state.h`'s "a state is named, never numbered" convention. AC-1.3's diagonal is
     unaffected: the two axes are two independent calls.
  3. **Deadzone: a fixed band of ±20 % of full scale around the nominal midpoint** —
     `kAxisAdcBitWidth = 12`, `kAxisAdcFullScale = (1 << kAxisAdcBitWidth) - 1` (4095),
     `kAxisAdcCenter = kAxisAdcFullScale / 2` (2047), `kAxisDeadzonePercent = 20`,
     `kAxisDeadzoneCounts = kAxisAdcFullScale * kAxisDeadzonePercent / 100` (819). Integer math, no float,
     no literal at any call site (AC-1.6). **Why 20:** a cheap resistive module's rest scatter is typically
     ±5 % of full scale and ESP32-S3 ADC noise ±20–50 counts, so 819 counts is roughly a 3× margin against
     A14's known off-centre-rest limitation (which is exactly what AC-3.3's sustained-centre hold tests),
     while still only requiring ~40 % of each half-travel to register a direction. It is one named constant:
     retuning after T8's real-hardware reading is a one-line change, and the host tests assert *relative to*
     the constant so they survive that change (one deliberately concrete test pins today's value).
  4. **The ADC-failure fallback is a level, not a branch (AC-2.7/A15).** The port maps a non-`ESP_OK`
     `adc_oneshot_read` to `AxisLevel::kNeutral` at the single point where the axis is read, and
     `directionActive(signal, kNeutral, kNeutral) == false` for all four directions is proven on the *host*.
     So the fail-safe is one substitution, not four `if (err) return false;` copies, and its consequence is
     covered by the host gate even though the failing call site is device-only.
  5. **The direction mapping is pure too.** `constexpr bool directionActive(InputSignal, AxisLevel x,
     AxisLevel y)` owns which axis and which extreme each of `up`/`down`/`left`/`right` reads, against a
     `detail::` polarity table (the `detail::kInputFields` precedent). A2's empirical "which extreme is
     left" answer is therefore one table entry, flipped after T8 without touching the port. Non-direction
     signals return `false` and a host test pins that. This leaves the port file as ~60 lines of ESP-IDF glue
     with no decision in it.
  6. **`board_config.h` holds GPIO numbers only; the ADC unit and channel are *derived*, never written down.**
     `init()` calls ESP-IDF's own `adc_oneshot_io_to_channel(pin, &unit, &channel)` on `kPinJoystickVrx`/`Vry`,
     so no ADC channel number exists as a literal anywhere in the codebase and a mis-typed pin fails loudly at
     init instead of silently reading the wrong channel. NFR-4's "no ADC-channel literal" becomes structural.
     Pins (§2 has the exclusion reasoning): `Vrx = 1`, `Vry = 2` (ADC1), `Sw = 21` → `select`,
     `Start = 47` (Taster 1), `Fire = 8` (Taster 2).
  7. **Recorded interpretation of NFR-6 (flagged for approval).** "One new public type plus its threshold
     entry point" realizes as **9 public symbols**: `AnalogJoystickSource` and, in `analog_axis.h`,
     `AxisLevel`, `axisLevel`, `directionActive` and the five constants above (`kAxisAdcBitWidth`,
     `kAxisAdcFullScale`, `kAxisAdcCenter`, `kAxisDeadzonePercent`, `kAxisDeadzoneCounts`). The polarity table
     is `detail::`. The type itself carries **four** members: `init()`, `readSignal()`, and `lastRawX()` /
     `lastRawY()` — diagnostic accessors returning the value cached by the most recent read, existing solely
     because NFR-5 requires the harness to log each raw sample beside its derived boolean, and `readSignal`
     is `const` (AC-2.1 pins that signature). Declared here rather than discovered at `/peer-review`.
  8. **Init failure policy, split like `Ili9488Display`'s:** `bool init()`. The three digital pins use one
     `gpio_config` with `ESP_ERROR_CHECK`, byte-for-byte `GpioInputSource`'s pattern. The ADC half never
     aborts: each failing step is logged and `init()` returns `false`, leaving `adcReady_` false so all four
     directions read `false` forever (Decision 4's same neutral) while the three buttons keep working — a
     dead ADC must not brick a board.

- **Alternatives considered:**

  | Alternative | Why rejected |
  |---|---|
  | Extend `GpioInputSource` with an ADC mode (flag or subclass) | Edits a shipped, released file — forbidden by A1/NFR-8 — and welds two unrelated electrical topologies into one type. Two `Source`s coexisting is the whole point of the concept |
  | `AnalogJoystickSource<AdcReader>` templated on a fake ADC so the *type* is host-testable | Tempting, and rejected: the only thing left inside it after Decisions 4–5 is `gpio_get_level` + `adc_oneshot_read`. A template seam here would buy a test of ESP-IDF glue while doubling the surface `/peer-review` reads. The pure side already carries every decision |
  | Virtual `IInputSource` so both Sources share a base | First vtable in a codebase that has none; rejected twice already (`game-loop`, `input-driver`). `InputReader` binds at compile time |
  | Return `{bool low; bool high;}` (or two `bool` out-params) per axis | Makes "both extremes at once" representable on a single potentiometer, and every caller must then re-derive which is which. See Decision 2 |
  | Hysteresis (separate enter/exit thresholds) around the deadzone edge | Real, but speculative: `Debouncer<2>` already sits downstream of every signal and absorbs edge dither, and no evidence of a problem exists before T8. YAGNI — and it would double the tuning constants |
  | Averaging / median-of-N ADC samples per tick | Same: unmeasured noise, and it multiplies the per-tick ADC cost against NFR-1's "arcade immediacy" for a signal that is then debounced anyway. T8 logs raw values; if the log shows dither, this becomes an evidenced follow-up |
  | Calibration at boot (read rest position, offset from it) | §6 out of scope, A4/A13 resolved by the user against it |
  | `adc_continuous` (DMA) driver instead of `adc_oneshot` | Buys a sample buffer and an ISR concept for two axes read twice per tick; oneshot is the boring choice and the one whose failure mode is a plain `esp_err_t` (AC-2.7) |
  | Store `kAdcChannelVrx`/`Vry` constants in `board_config.h` beside the pins | A second, hand-maintained representation of the same fact that can silently drift from the pin. Decision 6 derives it instead |
  | Float or percentage-of-3.3V math in the threshold | No FPU need, no unit conversion, and the ADC gives counts — integer counts are what the hardware actually produces |
  | GPIO38/39–42 for the discrete buttons | 38 drives the onboard RGB LED on ESP32-S3-DevKitC-1 v1.1; 39–42 are the classic JTAG pins `input-driver`'s plan already chose to avoid. GPIO8/21/47 need no such caveat |

- **Consequences:** *Easier* — the entire new logic of this feature (threshold, diagonal, fail-safe, polarity)
  runs on `make test`/`test-asan`/`test-gcc` with zero ESP-IDF and zero hardware, so a blocked T8 costs the
  Musts nothing; a third `Source` (USB gamepad, README's later phase) is a sibling file; retuning the deadzone
  or flipping an axis after real-hardware evidence is a one-line edit with tests already written around it.
  *Harder* — `port/esp32/` gains a third file no gate compiles (the boundary's permanent, known cost —
  counterweighted by lint and by T5/T7's mandatory real `idf.py build`); `board_config.h` now serves two
  mutually exclusive control schemes, so the wiring docs must say plainly that only one is wired at a time;
  and the raw-value diagnostics (Decision 7) put two members on the public type that exist purely for NFR-5.

## 2. Affected Components

Scoped by hand — no blast-radius tool file was passed with this task (`aspark-graph` has no built graph for
this repo and does not index C++), so no query result is cited and nothing below is inferred from one.

- **New (pure, host-gated):** `firmware/steamcore/include/steamcore/analog_axis.h`; tests
  `firmware/steamcore/test/analog_axis_test.cpp`, fixture
  `firmware/steamcore/test/fake_analog_source.h`.
- **New (ESP-IDF, device-only):** `firmware/steamcore/port/esp32/analog_joystick_source.{h,cpp}`.
- **New (docs):** `docs/wiring-analog-joystick.md` (AC-2.6/NFR-9's deliverable).
- **Modified:** `firmware/steamcore/include/steamcore/board_config.h` (5 pin constants);
  `tools/check_constraints.sh` (T6); `docs/host-tests.md`, `docs/device-build.md`;
  `firmware/system/main/CMakeLists.txt` (new source + `esp_adc` in `REQUIRES`);
  `firmware/system/main/app_main.cpp` (T7 harness, replacing `start-screen`'s — harnesses are throwaway by
  construction, A12/NFR-6, the same posture every prior feature took).
- **Untouched, by design (asserted with `git diff --exit-code` in T5):**
  `include/steamcore/input.h`, `include/steamcore/game_loop.h` (`GameInput`),
  `port/esp32/gpio_input_source.{h,cpp}`, plus `game_state.*`, `framebuffer.*`, `title_screen.*`,
  `ili9488_display.*`, and the `Makefile` (header-only + auto-globbed tests need no change).
- **New dependencies: none new to the project.** `esp_adc/adc_oneshot.h` ships with the already-installed
  ESP-IDF v5.4.4; it needs `esp_adc` added to the existing `REQUIRES` line, which is a build-file edit, not a
  third-party package. No new pattern: the port directory, the pure/impure split and the compile-time seam are
  all established precedent.
- **Pin assignment, decided (AC-2.5/A8):** `kPinJoystickVrx = 1` (ADC1_CH0), `kPinJoystickVry = 2`
  (ADC1_CH1), `kPinJoystickSw = 21` (→ `select`), `kPinJoystickStart = 47` (Taster 1 → `start`),
  `kPinJoystickFire = 8` (Taster 2 → `fire`). VRX/VRY must be ADC1 (GPIO1–10 on this chip) and only 1, 2 and 8
  are free there — 3 is boot-strapping, 4–7 are `GpioInputSource`'s, 9/10 the display's. Avoided and why:
  9–14 (display), 4–7/15/17/18 (`GpioInputSource` — the two schemes must not double-book), 26–37 (octal
  PSRAM/flash), 0/3/45/46 (boot-strap), 19/20 (native USB), 43/44 (UART0), 38/48 (onboard RGB LED depending on
  DevKitC-1 revision), 39–42 (JTAG), 16 (`check_constraints.sh`'s tile-size rule bans a bare `16` in
  `include/`).

## 3. Task Breakdown

| # | Task | Story | Covers (AC / NFR) | Depends on | Status | Definition of Done |
|---|---|---|---|---|---|---|
| T1 | Walking skeleton: raw ADC value → `GameInput`, host-side, end to end | US-1, US-2 | AC-1.1, AC-1.5, NFR-2, NFR-3 | – | `done` | `analog_axis.h` declares `AxisLevel`, `constexpr AxisLevel axisLevel(int32_t)`, `constexpr bool directionActive(InputSignal, AxisLevel, AxisLevel)` and the five named constants of §1 Decision 3; `fake_analog_source.h` defines a `steamcore::test` fake `Source` that replays scripted raw X/Y values plus three scripted button levels through `axisLevel`/`directionActive`; one test feeds a centred pair and a past-threshold low-X pair through the **unmodified** `InputReader<FakeAnalogSource>` and asserts `GameInput.left` goes `false → true` while `up`/`down`/`right` stay false throughout; the header includes no ESP-IDF header, allocates nothing, reads no clock; `make test`, `make test-asan`, `make test-gcc` and `make lint` green with no `Makefile` change — files: firmware/steamcore/include/steamcore/analog_axis.h, firmware/steamcore/test/fake_analog_source.h, firmware/steamcore/test/analog_axis_test.cpp |
| T2 | Threshold battery: centre, both extremes, exact boundary, idempotence | US-1 | AC-1.1, AC-1.2, AC-1.4, AC-1.6 | T1 | `done` | Named tests assert: `axisLevel(kAxisAdcCenter)` is `kNeutral`; `kAxisAdcCenter ± kAxisDeadzoneCounts` is still `kNeutral` and `± (kAxisDeadzoneCounts + 1)` is `kLow`/`kHigh` (the exact boundary, asserted from both sides); `axisLevel(0)`/`axisLevel(kAxisAdcFullScale)` are `kLow`/`kHigh`; values outside `[0, kAxisAdcFullScale]` still classify without UB; the same value fed twice returns the identical result and calling the routine 100× on one value never changes it (AC-1.4, purity); at least three assertions are `static_assert`s, proving the routine is usable at compile time and therefore stateless; one deliberately concrete test pins today's numbers (`kAxisDeadzonePercent == 20`, `kAxisDeadzoneCounts == 819`) so a retune after T8 is a visible, intentional edit; every other assertion is written relative to the constants, never to a literal — files: firmware/steamcore/test/analog_axis_test.cpp |
| T3 | Direction battery: diagonal, opposition, non-direction signals, ADC-failure neutral | US-1, US-2 | AC-1.3, AC-1.4, AC-2.7, NFR-3 | T2 | `done` | Named tests assert: each of the four cardinals alone drives exactly its own field true through `InputReader<FakeAnalogSource>` with the other three false on every tick; a diagonal (both axes past threshold) drives both adjacent fields true simultaneously with the two opposite fields false — no cardinal-snapping (AC-1.3); the opposite direction on the same axis is never true at the same time as its partner, for both axes; `directionActive` returns `false` for `kStart`/`kFire`/`kSelect` (a non-direction signal is never satisfied by axis state); `directionActive(signal, kNeutral, kNeutral)` is false for all four directions, which is exactly the ADC-read-failure fallback of §1 Decision 4 (AC-2.7's host-provable half); a sustained centred hold over ≥60 ticks reports all four false on every tick with zero transitions counted by the test (the host analogue of AC-3.3); the fake's three button levels pass through unchanged to `start`/`fire`/`select` — files: firmware/steamcore/test/analog_axis_test.cpp, firmware/steamcore/test/fake_analog_source.h |
| T4 | Pin constants + the physical wiring reference | US-2 | AC-2.5, AC-2.6, NFR-9 | – | `done` | `board_config.h` gains exactly five named constants (`kPinJoystickVrx = 1`, `kPinJoystickVry = 2`, `kPinJoystickSw = 21`, `kPinJoystickStart = 47`, `kPinJoystickFire = 8`) in the existing plain-`int` style, with a comment naming every excluded range of §2 and stating that these are a **second, mutually exclusive** control scheme that does not double-book `GpioInputSource`'s pins; `docs/wiring-analog-joystick.md`, in `docs/wiring-input.md`'s structure, gives the five-row pin table with constant names, the active-low/internal-pull-up topology for the two discrete buttons and the joystick's `SW`, the VRX/VRY analog wiring, the A3 mapping (Taster 1 = start, Taster 2 = fire, SW = select), the statement that the module is powered from **3.3V, the board's own 3V3 rail, never 5V — a 5V feed can permanently damage the ADC pin** (AC-2.6/NFR-9), a note that ADC1 is required and why only 1/2/8 were available, the known A14 limitation in plain words, a component-class shopping list with no part numbers, and a Status section stating whether anything is wired yet; `make lint` green — files: firmware/steamcore/include/steamcore/board_config.h, docs/wiring-analog-joystick.md |
| T5 | The real `AnalogJoystickSource`, built for the device | US-2 | AC-2.1, AC-2.2, AC-2.3, AC-2.4, AC-2.7, NFR-6, NFR-7, NFR-8 | T3, T4 | `done` | `port/esp32/analog_joystick_source.{h,cpp}` defines `steamcore::port::esp32::AnalogJoystickSource` with exactly `bool init()`, `bool readSignal(InputSignal) const` (the identical signature `GpioInputSource` declares) and the two `lastRawX()`/`lastRawY()` diagnostic accessors of §1 Decision 7 — no other public member; `init()` configures the three digital pins with one `gpio_config` (input, internal pull-up, no interrupt, `ESP_ERROR_CHECK`) and creates the ADC oneshot unit and both channels **derived** via `adc_oneshot_io_to_channel` from the `board_config.h` pins, logging and returning `false` on any ADC failure without aborting; `readSignal` returns `gpio_get_level(pin) == 0` for start/fire/select per A3's mapping and, for the four directions, `directionActive(signal, axisLevel(rawX), axisLevel(rawY))` where a read whose `esp_err_t` is not `ESP_OK` (or a false `adcReady_`) yields `AxisLevel::kNeutral`; a `static_assert` ties the configured ADC bit width to `kAxisAdcBitWidth`; the header's doc comment states, each explicitly, the A3 button mapping, the fixed-percentage/no-calibration deadzone and its all-false centre, the A14 off-centre-rest limitation, the ADC-failure fail-safe, the 3.3V rail assumption, and the inherited single-threaded/no-throw/no-alloc contract (NFR-7); no ADC channel number and no GPIO number appears as a literal in either file; the file is added to `main/CMakeLists.txt`'s `SRCS` with `esp_adc` added to `REQUIRES`, and **`idf.py build` is run and green** (AC-2.4); `git diff --exit-code` over `port/esp32/gpio_input_source.h`, `gpio_input_source.cpp`, `include/steamcore/input.h` and `include/steamcore/game_loop.h` against `v0.3.0` is recorded showing all four byte-identical (NFR-8); the public surface is audited symbol by symbol against §1 Decision 7 and §2 corrected in place if it drifted (NFR-6) — files: firmware/steamcore/port/esp32/analog_joystick_source.h, firmware/steamcore/port/esp32/analog_joystick_source.cpp, firmware/system/main/CMakeLists.txt |
| T6 | Lint rules for this feature's own literals and determinism | US-1, US-2 | AC-2.5, NFR-2, NFR-3, NFR-4 | T5 | `done` | All four rules implemented and mutation-verified: **(a)** global GPIO digit set extended with 8/21/47, re-verified zero false positives on the unmodified tree, then a planted `= 47;` near a `Pin`-named identifier in `analog_axis.h` correctly failed both rule (a) and (b) simultaneously (47 is in both sets), reverted. **(b)** feature-scoped 1/2/8/21/47 block over `analog_axis.h` + both port files; isolated with a planted `= 2;` (not in the global set) — only rule (b) fired, confirmed rule (a) silent, reverted. **(c)** `ADC_CHANNEL_[0-9]`/`ADC_UNIT_[0-9]` ban caught a **real, non-synthetic violation** during development: the header's `adcChannelX_`/`adcChannelY_` members defaulted to `ADC_CHANNEL_0` as a placeholder — fixed to value-initialization (`adc_channel_t adcChannelX_{};`) since neither is ever read before `init()` overwrites both with the pin-derived real channel. A second, synthetic `ADC_CHANNEL_3` was then planted and reverted to confirm the rule still fires clean. **(d)** shared `CLOCK_RNG_PATTERN`/`SCOPED_ALLOC_PATTERN` over the full new file set; a planted `#include <chrono>` and, separately, a planted `std::vector` each failed correctly, both reverted. `idf.py build` re-run clean after the real `ADC_CHANNEL_0` fix. `make test`/`lint` green throughout (206 passed, 0 failed). `docs/host-tests.md` updated — files: tools/check_constraints.sh, docs/host-tests.md | `check_constraints.sh` gains one named analog-joystick block: (a) the existing GPIO-literal digit set is extended with `8`, `21` and `47`, **verified** by running full `make lint` on the otherwise-unmodified tree first and confirming zero false positives across `include/`, `src/`, `port/` and `firmware/system/main/` — if one appears the token pattern is narrowed, never a digit dropped; (b) a feature-scoped literal block over `analog_axis.h` and `analog_joystick_source.{h,cpp}` covering the digits `1`, `2`, `8`, `21`, `47`, with a comment recording honestly that `1`/`2` cannot join the global set (they would fire on ordinary small integers near any `pin`/`spi` token) and naming the residual gap that leaves; (c) a ban on `ADC_CHANNEL_[0-9]`/`ADC_UNIT_[0-9]` anywhere under `port/` and `firmware/system/main/`, which is what makes §1 Decision 6's "the channel is derived, never written" structural; (d) the shared `CLOCK_RNG_PATTERN` and `SCOPED_ALLOC_PATTERN` applied to the new file set (`analog_axis.h`, `analog_axis_test.cpp`, `fake_analog_source.h`, both port files), extending input-driver's precedent that the input path's clock ban reaches into `port/`; all four use the existing missing-file-is-a-failure posture; each is demonstrated failing once against a temporarily planted violation, then reverted with `make lint` and `make test` green; `docs/host-tests.md` describes the new rules — files: tools/check_constraints.sh, docs/host-tests.md |
| T7 | On-device harness — buildable half of US-3, needs no hardware | US-3 | AC-2.4, NFR-5, NFR-6 | T5, T6 | `done` | First step confirmed `start-screen`'s harness safely committed (`a8e590f` in `git log`, `app_main.cpp` absent from `git status`) before overwriting it. `app_main.cpp` rewritten around `AnalogJoystickSource` + unmodified `InputReader<Source>`: every tick logs `rawX`/`rawY` beside all four direction booleans (NFR-5); `start`/`fire`/`select` log only on debounced change, mirroring `input_harness_game.h`'s `logLevelChange` precedent. `idf.py build` real and green (verified twice, once before and once after a docs-only edit). `docs/device-build.md` updated: top paragraph now names this build's harness, plus a new "Reading the analog-joystick harness log" section with format-accurate illustrative examples (not captured — nothing is wired; review F6). `make test`/`lint` unaffected (206 passed, 0 failed; lint OK) — files: firmware/system/main/app_main.cpp, firmware/system/main/CMakeLists.txt (already done in T5), docs/device-build.md | Before touching `app_main.cpp`, `git log`/`git status` is checked and recorded to confirm `start-screen`'s release commit exists in history (its harness must not be overwritten while uncommitted — see §5); `app_main.cpp` is rewritten around `AnalogJoystickSource` + the unmodified `InputReader<AnalogJoystickSource>`, ticking at a fixed, documented interval, logging on every tick or on change: each axis' raw ADC sample beside its derived direction boolean for all four directions (NFR-5), and every debounced level change of `start`/`fire`/`select` by name; the shipped `InputReader`/`GameInput` signatures are used unmodified; **`idf.py build` green** end to end with the harness linked — the whole task is executable with nothing wired; `docs/device-build.md` records that this build's `app_main` is now the analog-joystick harness and how to read its log lines — files: firmware/system/main/app_main.cpp, firmware/system/main/CMakeLists.txt, docs/device-build.md |
| T8 | **Hardware-gated:** on-device confirmation with the real stick and buttons | US-3 | AC-3.1, AC-3.2, AC-3.3, AC-3.4, AC-3.5, NFR-1, NFR-5 | T7 | `blocked` | **Blocked, per this task's own Definition of Done.** `docs/wiring-analog-joystick.md`'s own Status section still reads "Nothing is physically wired yet" — the user has the components (two 4-pin pushbuttons, one 5-pin analog joystick) sitting on a breadboard, but there is no confirmation they are connected to the assigned pins (GPIO1/2/8/21/47) or that the module's supply is actually the board's 3.3V rail rather than 5V. AC-3.1–AC-3.4 are therefore explicitly unverified, not capturable, and must be recorded as such in `qa.md` — never as passed, never satisfied by a substitute. AC-3.5 itself (the requirement that a not-yet-wired state gets reported exactly this way) is thereby satisfied. Nothing here holds back `/peer-review`, `/demo-day` or `/go-live`, since this story is a Should (spec A7/C5) — files: none (no code change; blocked state recorded here and will be echoed in `qa.md`) | **Executable only if the joystick module and both discrete buttons are physically wired per `docs/wiring-analog-joystick.md` AND the module's supply rail is confirmed to be 3.3V, not 5V, by `/increment` time; if either is not true, this task is reported `blocked` with that reason and AC-3.1–AC-3.4 are recorded in `qa.md` as explicitly unverified / not capturable — never as passed, never satisfied by a substitute (a bench-supply or jumper sanity touch may be run, but is recorded as exactly that, not as an AC-3.x).** When runnable: flashed per `docs/device-build.md` (manual RESET after flash), the transcript shows each of the four cardinal extremes toggling exactly its own direction boolean with the other three false (AC-3.1); a deliberate diagonal showing both adjacent booleans true at once (AC-3.2); a sustained ≥5 s centred hold with all four false throughout and zero transitions, with the observed resting raw values recorded so A14's off-centre limitation is measured rather than assumed (AC-3.3); each of Taster 1, Taster 2 and `SW` toggling exactly `start`, `fire`, `select` (AC-3.4); the human judges press-to-log latency immediate (NFR-1) and the effective `kAxisDeadzonePercent` is recorded for `qa.md`, with any retune applied as the one-line change §1 Decision 3 anticipates and T2's tests re-run; the transcript and the human's confirmation are recorded verbatim in this row — files: firmware/system/main/app_main.cpp, docs/wiring-analog-joystick.md |

## 4. Test Strategy

- **Host-CI-verifiable at `/increment` time (`make test`, `test-asan`, `test-gcc`, `lint` — zero ESP-IDF,
  zero hardware):** AC-1.1–AC-1.6 (T1–T3), AC-2.7's fallback semantics (T3), AC-2.5's grep half and
  NFR-2/NFR-3/NFR-4 (T6). This is the payoff of §1 Decisions 2/4/5: every decision this feature makes lives on
  the pure side, so a blocked T8 costs the Musts nothing.
- **US-1** — T2 is the arithmetic battery (centre, both extremes per axis, the exact threshold boundary from
  both sides, idempotence, `static_assert` purity), T3 the behavioural one (four cardinals, diagonal,
  opposition, sustained centre). Same rigour `collision-system` established for its own pure routine, for the
  same reason: an off-by-one at the boundary is invisible to a "roughly works" test. Assertions are written
  against the named constants so a T8 retune does not invalidate the suite; exactly one test pins today's
  literal values on purpose.
- **US-2** — mostly *compiler*-verifiable, deliberately: AC-2.1's signature identity, AC-2.3's composition and
  AC-2.4's coexistence are proven by a real `idf.py build` (T5, T7), not by a claim; AC-2.7's device-side
  branch is reviewed in source while its *consequence* is host-tested (T3); NFR-8 is proven by
  `git diff --exit-code` against `v0.3.0`, not by inspection. AC-2.5/AC-2.6 are document-verifiable (T4):
  US-2's wiring half is a deliverable, not a runtime behaviour — "manual" here means there is nothing to
  execute, not that a test was skipped.
- **US-3** — device-only by nature, split per `CLAUDE.md`: T7 is the buildable half and runs today; T8 needs a
  human at real, wired controls and is expected to report `blocked`. Nothing in this plan reports AC-3.1–3.4
  as passed until a transcript exists (constitution §4/§6).
- **Sanitizers:** `make test-asan` matters for the fake source's scripted per-tick arrays — an off-by-one in
  the script index reads in-bounds-looking garbage under plain `make test`.
- **Deliberately not automated, with reasons:** oscilloscope/electrical ADC-noise capture (§6 — log
  observation is the accepted method); an ESP-IDF `unity` app for the port file (a second test framework for
  ~60 lines of glue a human must physically exercise anyway); `/demo-day` in a browser (no browser-observable
  surface, constitution §8); real GCC (`/usr/bin/g++` is Apple clang — `test-gcc` stays honestly reported as
  nominal, unchanged from prior increments).

## 5. Risks & Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| The real unit rests outside the ±20 % band, so a direction reads true at rest (A14, explicitly accepted) | Medium, and only discoverable at T8 — it looks like a logic bug | T8's DoD requires the *observed resting raw values* to be logged and recorded, so this is measured, not guessed; the threshold is one named constant and T2's assertions are written relative to it, so a retune is a one-line change with the suite still valid. A per-unit fix stays out of scope (§6) |
| `T7` overwrites `app_main.cpp` before a prior feature's harness there is safely committed | Low today, verified: `start-screen`'s release commit `a8e590f` already contains `app_main.cpp`, `title_screen_harness_game.h` and every other start-screen file byte-for-byte (confirmed 2026-09-06 via `git show --stat a8e590f` and a clean `git status`); the risk is real only for a *future* feature that lands between now and T7 without being committed first | T7's DoD keeps the `git log`/`git status` check as its first step regardless — cheap insurance against a future regression, not a fix for a problem that exists today. Overwriting a *committed* harness is established, accepted precedent (A12/NFR-6) |
| GPIO1/2 cannot join the global GPIO-literal lint digit set — small integers sit near `pin`/`spi` tokens constantly | Medium: a rule that looks complete but is not | T6 (b) covers them in a feature-scoped block over the three files where a drifting pin copy would actually appear, and writes the residual gap into the script itself rather than leaving the reader to assume full coverage |
| ESP-IDF API drift: `adc_oneshot_io_to_channel`'s signature, or `ADC_ATTEN_DB_12` vs the deprecated `DB_11` spelling, differs in the installed v5.4.4 headers | Medium: T5 is written from memory of the API, not from the headers | T5's DoD is a real `idf.py build`, never a claim of one; whatever the installed headers actually declare wins, and any deviation from this plan's wording is recorded in T5's row. No pure-side code depends on the spelling |
| At 12 dB attenuation the S3's ADC saturates around ~3.1V, so the top of the stick's travel clips before the mechanical end, and the ADC is non-linear near both rails | Low: harmless for a threshold, misleading if read as a position | Threshold-only use is stated in the header contract (T5) and the wiring doc (T4); T8's raw-value logging makes the actual usable span visible instead of assumed |
| GPIO8/21/47 availability differs across ESP32-S3 devkit revisions (38/48 already ruled out for exactly this reason) | Low-Medium: a button that never reads, or a fought-over pin | T4's wiring doc names the exclusion reasoning and instructs a check against the board's own silkscreen before soldering; a wrong pin is a one-constant change in `board_config.h` and nothing else |
| `port/esp32/analog_joystick_source.*` is production code **no gate compiles** between flashes | High and permanent — the inherited cost of the boundary, stated rather than hidden | Same counterweight as the two ports before it: T6's lint rules cover what grep can (pins, ADC literals, allocation, clock reads), `docs/device-build.md` keeps the build reproducible, and §1 Decisions 4/5 keep the file down to glue with no decision in it |
| Nothing is wired, so T8 reports `blocked` | Zero for the Musts, by design | Anticipated by A7/C5 and `CLAUDE.md`'s split: T7 delivers everything a compiler can verify; T8's `blocked` is a plan-anticipated terminal state, not a failure, and does not hold `/peer-review`, `/demo-day` or `/go-live` |
| Two `Source` types now exist and nothing selects between them | Low: a reader may assume this feature settles the console's control scheme | Explicitly out of scope (A12, §6); T5's header comment and T4's wiring doc both state that only one scheme is wired at a time and that the choice belongs to a later feature |

---

## ✅ PLAN GATE

*All boxes checked → `/increment` may start. Any box open → back to `/sprint-plan`.*

- [x] Spec status is `approved` (never plan against a draft) — verified 2026-09-05, zero open items
- [x] Architecture decision includes rejected alternatives (11 recorded, §1)
- [x] Architecture respects the constitution's technical constraints — §3 pin assignment (5 new constants in `board_config.h` only, every reserved range avoided and named; ADC channel derived, never written); no dynamic allocation (a `constexpr` routine and two `int32_t` caches); §3 Timing/determinism (no clock, no RNG on the input path, lint-enforced into `port/`); §4's no-ESP-IDF-header rule left absolute via the existing `port/esp32/` boundary; C++17, `steamcore` namespace, `snake_case`, zero vtables (compile-time seam), English — no conflict found
- [x] Every task maps to a user story — no orphan tasks, no story without tasks
- [x] Every Must AC and every applicable NFR is covered by at least one task (AC-1.1–1.6 → T1–T3; AC-2.1–2.7 → T3–T5, T7; AC-3.1–3.5 → T8; NFR-1 → T8; NFR-2/3/4 → T1, T3, T6; NFR-5 → T7, T8; NFR-6/7/8 → T5; NFR-9 → T4; NFR-10/11 are N/A per the spec)
- [x] Every task has a checkable definition of done
- [x] Task order respects dependencies — walking skeleton first (T1 puts a raw ADC value through the unmodified `InputReader` to `GameInput` before any battery exists), the hardware-gated work last and split so its buildable half (T7) is not blocked by its unbuildable one (T8)
- [x] Test strategy covers every Must story, and states per AC whether it is host-CI-verifiable, compiler-verifiable, document-verifiable or needs the board
- [x] Line budget respected: Ist 214 / Soll ~300 (excluding HTML comments; this file contains none) — 86 under
- [x] Status set to `approved` by the user — 2026-09-06
