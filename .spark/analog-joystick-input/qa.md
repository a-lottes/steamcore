# QA Report: analog-joystick-input

| | |
|---|---|
| **Phase** | Review (hands-on) |
| **Owner** | QA Tester (`/demo-day`) |
| **Input** | `.spark/analog-joystick-input/spec.md`, `plan.md`, `review.md` (`passed`, round 3) |
| **Status** | `passed` |
| **Round** | 1 |
| **Date** | 2026-09-06 |

**Handoff**
- **Status:** `passed`
- **Verdict:** Yes — I would demo this. Every Must AC (US-1, US-2) is proven by a test or build step I ran myself; the one Should story (US-3) is correctly `blocked` on unwired hardware, exactly as the plan anticipated, and is recorded here as not-capturable rather than passed.
- **Open:** `none` — 0 Blockers, 0 Majors, 0 Minors found in this pass.
- **Binding ruling:** §5 Verdict and the gate checklist below.
- **On conflict:** the numbered body below wins for everything except `Status`.

## 1. Test Environment

- **QA method:** No browser-observable surface (constitution §8: `Browser-observable surface: no`). This feature additionally has **no framebuffer/rendering surface at all** — `AnalogJoystickSource` is a pure input source, draws nothing — so the framebuffer-dump half of the declared substitute does not apply here, by design, not as a gap. Verification method used: (a) host-compiled unit tests (`make test`/`test-asan`/`test-gcc`/`lint`), run by me from repo root; (b) a genuine, forced full-clean `idf.py build` for the device-buildable half, run by me from `firmware/system/` after `source ~/esp/esp-idf/export.sh` (ESP-IDF v5.4.4 at `~/esp/esp-idf`).
- **App URL / browser / viewport:** N/A — no UI, per §8.
- **Test data:** synthetic ADC integer values driven through `FakeAnalogSource` (`firmware/steamcore/test/fake_analog_source.h`); no real hardware, no real serial transcript (US-3 blocked, see §2).

## 2. Acceptance Criteria Verification

| Spec ID | Steps performed | Expected | Observed | Result |
|---|---|---|---|---|
| AC-1.1 | Ran `make test FILTER=analog_axis`; inspected `analog_axis_classifies_centre_and_both_thresholds` and the `axisLevel(kAxisAdcCenter)` static_assert | Centre value (inside deadzone) → both direction booleans false | `axisLevel(kAxisAdcCenter) == kNeutral`; `analog_axis_walking_skeleton_...` shows `centered.{left,right,up,down}` all false | ✅ pass |
| AC-1.2 | Ran the same filter; inspected `analog_axis_each_cardinal_drives_only_its_own_field` and boundary static_asserts (`center ± counts + 1`) | Past threshold → matching direction true, opposite false | Test drives each of 4 cardinals alone through `InputReader`; only the matching field is true each time | ✅ pass |
| AC-1.3 | Inspected `analog_axis_diagonal_drives_both_adjacent_fields` | Both axes past threshold → both adjacent fields true, no snapping | `input.left==true && input.up==true`, `right`/`down` false | ✅ pass |
| AC-1.4 | Inspected `analog_axis_is_idempotent_over_repeated_calls` (100x) and `analog_axis_opposite_directions_never_both_active` (sweep -1000..full+1000, step 37) | Same input → identical result every time; no hidden state | All 100 repeats identical; opposite pair never both true across full sweep | ✅ pass |
| AC-1.5 | Ran `make test`, `make test-asan`, `make test-gcc` from repo root; grepped `analog_axis.h`/`analog_axis_test.cpp` for ESP-IDF headers | Builds/passes under host toolchains, zero ESP-IDF header | `206 passed, 0 failed` on all three; `grep` for `esp_adc\|driver/gpio\|freertos` in `include/`/`src/` returns nothing | ✅ pass |
| AC-1.6 | Read `analog_axis.h`; inspected `kAxisDeadzonePercent==20`/`kAxisDeadzoneCounts==819` static_asserts; grepped port files for bare `819`/`4095`/`2047` | Deadzone centred on nominal midpoint, named constant, no bare literal at call sites | `kAxisAdcCenter = kAxisAdcFullScale/2`; grep for bare deadzone literals in port files returns nothing (only named constants used) | ✅ pass |
| AC-2.1 | Diffed `AnalogJoystickSource::readSignal` declaration against `GpioInputSource::readSignal` | Identical `bool readSignal(InputSignal) const` signature | Both declare exactly `bool readSignal(InputSignal signal) const;` | ✅ pass |
| AC-2.2 | Read `analog_joystick_source.cpp` `readSignal()` start/fire/select branch | Active-low digital read of mapped pin, pull-up convention matching `GpioInputSource` | `gpio_get_level(pin) == 0`, pull-up enabled in `init()`'s single `gpio_config_t`, matches A3 mapping (Sw→select, Start→start, Fire→fire) | ✅ pass |
| AC-2.3 | Read `readSignal()` direction branch | Direction signals go through US-1's `axisLevel`/`directionActive` on a fresh ADC read | `directionActive(signal, cachedX_, cachedY_)` fed by `adc_oneshot_read` samples taken on the `kUp` call each tick | ✅ pass |
| AC-2.4 | Ran `idf.py fullclean` then a genuine forced `idf.py build` from `firmware/system/` (not a cached/incremental build — verified no stale `.obj` survived the fullclean) | `idf.py build` succeeds with both Sources compiled in | `[1060/1060]`, `Project build complete`, exit code 0; confirmed both `analog_joystick_source.cpp.obj` and `gpio_input_source.cpp.obj` present under `build/esp-idf/main/CMakeFiles/__idf_main.dir/` after the build | ✅ pass |
| AC-2.5 | Read `board_config.h`; ran `make lint` | All 5 new GPIO/ADC constants live in `board_config.h`, none as bare literals elsewhere | `kPinJoystickVrx/Vry/Sw/Start/Fire` all in `board_config.h`; lint rules "no VRX/VRY GPIO-pin literal outside board_config.h" and the feature-scoped literal block both pass | ✅ pass |
| AC-2.6 | Read `docs/wiring-analog-joystick.md` in full | States 5-signal pin table, digital/SW topology, ADC axis wiring, 3.3V-never-5V power rail | All present: pin table, topology section, "Power rail — 3.3V, never 5V" section with damage-risk explanation | ✅ pass |
| AC-2.7 | Inspected the 4 `static_assert(!directionActive(..., kNeutral, kNeutral))` in `analog_axis_test.cpp`, and `readSignal()`'s failure path (`adc_oneshot_read != ESP_OK` → level stays `kNeutral`) | ADC read failure → `readSignal` returns false for directions | Host-proven for all 4 directions via static_assert; port code confirmed to leave `cachedX_/cachedY_` at `kNeutral` default on a failed read | ✅ pass |
| NFR-2 | Ran `make lint`; grepped new files for `new`/`malloc`/`std::vector`/`std::string` | Zero dynamic allocation | Lint rule "no dynamic allocation in the analog-joystick file set" passes; grep confirms no allocation calls in any new file | ✅ pass |
| NFR-3 | Ran `make lint`; read `analog_axis.h`/port files for clock/RNG use | No wall-clock read, no unseeded RNG, tick-driven only | Lint rule "no wall-clock read or unseeded RNG in the analog-joystick mechanism" passes | ✅ pass |
| NFR-4 | Ran `make lint`; read `board_config.h`'s ADC-derivation comment and `analog_joystick_source.cpp` | No GPIO/ADC-channel literal outside `board_config.h`; ADC channel structurally derived, never written | Two dedicated lint rules pass ("no ADC channel/unit literal", "no VRX/VRY GPIO-pin literal outside board_config.h"); `adc_oneshot_io_to_channel()` derives the channel at init, no `ADC_CHANNEL_n` literal anywhere | ✅ pass |
| NFR-5 | Read `app_main.cpp`'s harness loop and `docs/device-build.md`'s log-format section | Harness logs raw ADC sample beside derived direction boolean for all 4 axes | `ESP_LOGI(...,"axes: rawX=%d rawY=%d up=%d down=%d left=%d right=%d", ...)` every tick; button changes logged via `logLevelChange` on debounced transition only | ✅ pass |
| NFR-6 | Grepped `analog_axis.h`/`analog_joystick_source.h` for top-level public declarations outside `detail::` | Exactly 9 public symbols (1 type + threshold entry point + supporting names) | Counted: `AxisLevel`, `axisLevel`, `directionActive`, `kAxisAdcBitWidth`, `kAxisAdcFullScale`, `kAxisAdcCenter`, `kAxisDeadzonePercent`, `kAxisDeadzoneCounts`, `AnalogJoystickSource` = 9; `detail::DirectionPolarity`/`kDirectionPolarities` correctly excluded | ✅ pass |
| NFR-7 | Read `analog_joystick_source.h`'s header comment | States button mapping, deadzone/no-calibration approach, centre behaviour, ADC-failure fail-safe, 3.3V assumption, no-throw/no-alloc/single-threaded contract | All six elements present verbatim in the header's `Contract:` block | ✅ pass |
| NFR-8 | Ran `git diff --exit-code v0.3.0 -- firmware/steamcore/port/esp32/gpio_input_source.{h,cpp} firmware/steamcore/include/steamcore/input.h firmware/steamcore/include/steamcore/game_loop.h` | All four files byte-identical to v0.3.0 | Exit code 0 — no diff output, confirmed byte-identical | ✅ pass |
| NFR-9 | Read `docs/wiring-analog-joystick.md`'s "Power rail" section | States 3.3V rail explicitly, before any pin is energized | "The module is powered from the board's own 3.3V (3V3) rail, never from a 5V source" stated plainly with the ADC-damage rationale | ✅ pass |
| NFR-10 | N/A per spec (no personal data/network path) | — | — | N/A |
| NFR-11 | N/A per spec (no visual/UI surface) | — | — | N/A |
| AC-3.1 | Not capturable — nothing physically wired | Real stick pushed to 4 cardinal extremes toggles matching booleans | `docs/wiring-analog-joystick.md`'s own Status section: "Nothing is physically wired yet" (as of 2026-09-06) | ⛔ blocked / not capturable |
| AC-3.2 | Not capturable — same reason | Diagonal push shows both adjacent booleans true on real hardware | Not attempted — no wiring exists to attempt it on | ⛔ blocked / not capturable |
| AC-3.3 | Not capturable — same reason | Sustained centre hold on real unit shows no chatter/spurious true | Not attempted | ⛔ blocked / not capturable |
| AC-3.4 | Not capturable — same reason | Real button/SW presses toggle start/fire/select in serial log | Not attempted | ⛔ blocked / not capturable |
| AC-3.5 | Read `plan.md` T8 row and `docs/wiring-analog-joystick.md` Status section | A not-yet-wired state is reported as `blocked`, never passed, never substituted | Confirmed: plan T8 is `blocked` with reason stated; wiring doc's Status section states nothing is wired; this row itself satisfies AC-3.5 by recording the block plainly rather than omitting or faking it | ✅ pass |

**On US-3 (Should, hardware-gated):** per `CLAUDE.md`'s "Hardware-gated Should" convention and this feature's own plan T8, AC-3.1–AC-3.4 are recorded above as `blocked`/not capturable — never passed, never approximated by reading source. This is the plan-anticipated terminal state for an unwired Should story and does not affect the verdict below; only AC-3.5 (which is about correctly reporting the block) is itself verifiable and passes.

## 3. Exploratory Findings

None found this round. Beyond the AC table, I additionally checked: `InputSignal`'s declared enum order (`kStart, kFire, kSelect, kUp, kDown, kLeft, kRight` — `input.h:46-54`) actually puts `kUp` first among the four directions, which is the load-bearing assumption behind `analog_joystick_source.cpp`'s single-sample-per-tick caching and its own `static_assert` pinning that order — confirmed correct, not just asserted. No dead code, no bare literals outside named constants, no double-booked pins between the two `Source` schemes (`board_config.h`'s two pin blocks: 4-7/15/17/18 vs. 1/2/8/21/47, disjoint).

| # | Severity | Steps to reproduce | Expected vs. observed | Status |
|---|---|---|---|---|
| — | — | none found | — | — |

## 4. Console & Network

N/A — no browser console, no network requests (offline device, no UI). The device-side equivalent (serial log) is not capturable this round since nothing is wired (§2, US-3); `docs/device-build.md` documents the expected log format for when it becomes capturable.

## 5. Verdict

Yes, I would demo this right now — for what this feature actually claims to deliver. Every Must-story acceptance criterion (US-1's host-tested threshold math, US-2's device-buildable `AnalogJoystickSource`) is verified by a test or build I ran myself: `make test`/`test-asan`/`test-gcc`/`lint` all green (206/206, three toolchains), a genuine forced-clean `idf.py build` succeeded end to end with both `AnalogJoystickSource` and the untouched `GpioInputSource` compiling into the same image, and `git diff --exit-code` against `v0.3.0` proves the four files this feature promised not to touch are byte-identical. The one Should story, US-3's real-hardware confirmation, is correctly `blocked` — nothing is wired yet, exactly as `docs/wiring-analog-joystick.md` and plan.md's T8 both say — and I have recorded its five ACs as not-capturable rather than passed or faked. This feature has no rendering/framebuffer surface at all, so no visual QA applies here, which is the correct outcome for an input-only feature, not a gap in this report.

---

## ✅ QA GATE

- [x] Every Must-story acceptance criterion verified (AC-1.1–1.6, AC-2.1–2.7) — verified by host test/build/read, not browser (N/A per §8)
- [x] Every applicable NFR verified and passed (NFR-2–9); NFR-10/11 N/A per spec
- [x] No open Blocker or Major bugs (none found)
- [x] N/A: browser console — no browser surface (§8)
- [x] N/A: viewports — no UI (§8); device-buildable half verified via genuine `idf.py build`
- [x] Line budget respected: Ist 78 / Soll ~130 (excluding HTML comments)
- [x] Status set to `passed`
