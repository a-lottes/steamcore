# QA Report: input-driver

| | |
|---|---|
| **Phase** | Review (hands-on) |
| **Owner** | QA Tester (`/demo-day`) |
| **Input** | `.spark/input-driver/spec.md`, `.spark/constitution.md` §8 (declared substitute QA method) |
| **Status** | `passed` |
| **Round** | 1 |
| **Date** | 2026-09-04 |

**Handoff**
- **Status:** `passed`.
- **Verdict:** Yes — I'd demo this. Every Must AC (US-1/US-2/US-3) is verified, host gates are 153/153 three ways plus a clean lint, and the ESP-IDF firmware I rebuilt myself compiles green and uses `GameSession`/`GameLoop`'s shipped APIs unmodified. US-4's four hardware ACs are honestly parked, not claimed, exactly as the spec pre-negotiated.
- **Open:** `none` — no new bugs found. Review's original 9 Minors/1 Nit (F1-F9) were fixed and independently verified at `/peer-review` Round 2 (2026-09-04); one new artifact-currency Minor there (F10, this note) is closed by this line. **Post-QA note (2026-09-04):** the F1-F9 fix diff (one added host test, corrected comments/logging in `app_main.cpp`/`input_harness_game.h`, doc wording fixes, two new `static_assert`s in `input.h` plus a scoped GCC pragma) landed after this report's Round 1 run. None of it touches a Must AC's behavior — Round 2's reviewer independently re-ran `make test`/`test-asan`/`test-gcc` (154/154), `make lint`, and `idf.py build` (green, zero warnings) and confirmed `GameSession` still byte-identical to v0.1.0. This report's AC verdicts (§2) stand unchanged; not re-run as a full QA round since nothing Must-AC-relevant changed.
- **Binding ruling:** §5 Verdict and the gate checklist below.
- **On conflict:** the numbered body below wins for everything except `Status`.

## 1. Test Environment

- **QA Method:** `.spark/constitution.md` §8 declares `Browser-observable surface: no` with a performable substitute: host-compiled unit tests for hardware-independent logic, plus framebuffer dump/serial transcript for device output (this feature is log-only, no framebuffer touched). I performed the host-test half myself; the device half beyond a clean build is not capturable this round (no hardware wired — see §2 AC-4.1–4.4).
- **Host toolchain:** Apple clang 14.0.3, `/usr/bin/g++` (Apple clang), `/usr/bin/make`, macOS 13.7.8.
- **Device toolchain:** ESP-IDF v5.4.4, sourced via `~/esp/esp-idf/export.sh`; `idf.py build` run against `firmware/system/` after touching all four T9 source files to force a real recompile (not a cached artifact).
- **Test data:** `FakeInputSource`/simulated GPIO source (host tests), no physical device.

## 2. Acceptance Criteria Verification

| Spec ID | Steps performed | Expected | Observed | Result |
|---|---|---|---|---|
| AC-1.1 | Ran `make test FILTER=input_reader_start_alone_cycles_only_start` etc. (7 tests, one per signal); read `input_test.cpp:123-155` | Each signal cycles false→true→false alone, other six unaffected | 7/7 pass; read `checkPressReleaseCyclesOnlyThatSignal` — asserts the other 6 fields false every tick during the cycle | ✅ pass |
| AC-1.2 | Ran `make test FILTER=input_reader_held_signal`; read `input_test.cpp:159-172` | Held signal reads true on every one of many ticks (level, not pulse) | 1/1 pass; asserts `fire` true on 10/10 consecutive ticks | ✅ pass |
| AC-1.3 | Ran `make test FILTER=input_reader_cold_start`; read `input_test.cpp:177-187` | All 7 fields false on first read and every tick of an all-released run | 1/1 pass; loops 10 ticks, checks all 7 fields via `kAllFields` | ✅ pass |
| AC-1.4 | Ran `make test FILTER=input_reader_up_and_right_held` and `input_reader_all_four_directions`; read `input_test.cpp:191-223` | Simultaneous direction bits read true independently, no collapsing | 2/2 pass; up+right both true, down/left false; all-four also both proven | ✅ pass |
| AC-2.1 | Ran `make test FILTER=bounce_yields_exactly_two` (7 tests); read `input_test.cpp:249-289` and the shared bounce fixture (`:64-90`) | Noisy raw bounce around one press yields exactly one clean transition per event | 7/7 pass, each asserts exactly 2 transitions (press+release) against a genuinely bouncing raw fixture | ✅ pass |
| AC-2.2 | Ran `make test FILTER=seven_simultaneous_bounces`; read `input_test.cpp:295-324` and `input.h:116-124` (single `for` loop, no signal name in control flow) | All 7 signals debounce correctly and independently, phase-offset, in one run | 1/1 pass; each of 7 fields independently asserted at exactly 2 transitions with different phase offsets | ✅ pass |
| AC-2.3 | Ran `make test FILTER=input_session_bouncing_start_press_causes_one` and `..._restarts_from_game_over`; read `input_session_test.cpp` in full | A bouncing `start` press through `InputReader` into unmodified `GameSession` causes exactly one READY→PLAYING (and GAME_OVER→PLAYING) transition, never a second while held | 4/4 tests in the file pass; confirmed held-after-transition causes no 2nd transition, and release-then-press drives the next one correctly | ✅ pass |
| AC-2.4 | Ran `make test`, `make test-asan`, `make test-gcc` (153/153 all three); grepped `input.h`/`input_test.cpp` for ESP-IDF headers | Debounce core builds/passes on host with zero ESP-IDF header | 153/153 x3, zero ESP-IDF include found; `make lint`'s ESP-IDF-header rule also green | ✅ pass |
| AC-3.1 | Read `board_config.h:38-44` directly | 7 distinct named GPIO constants, all reserved ranges avoided, no bare literal elsewhere | Confirmed: `kPinInputUp/Down/Left/Right/Start/Fire/Select` = 4/5/6/7/15/17/18; `make lint`'s GPIO-literal rule (extended digit set) green, confirming no bare literal elsewhere | ✅ pass |
| AC-3.2 | Read `docs/wiring-input.md` in full | States per-signal GPIO + switch topology; four independent switches, not a joystick | Confirmed: pin table (§"Pin table"), explicit "four entirely independent microswitches... not a multi-position joystick switch and not an analog stick" | ✅ pass |
| AC-3.3 | Read `docs/wiring-input.md` §"Status" | States plainly nothing is wired yet, names it as the next physical action before US-4 | Confirmed verbatim: "Nothing is physically wired yet... the explicit next step before plan.md's T10... can be attempted" | ✅ pass |
| AC-4.1 | N/A | — | **Not capturable.** No physical hardware wired (T10 `blocked`, confirmed against `docs/wiring-input.md` §Status and plan.md T10 row). No substitute claimed. | not capturable |
| AC-4.2 | N/A | — | **Not capturable.** Same reason as AC-4.1. | not capturable |
| AC-4.3 | N/A | — | **Not capturable.** Same reason as AC-4.1. | not capturable |
| AC-4.4 | N/A | — | **Not capturable.** Same reason as AC-4.1. | not capturable |
| AC-4.5 | Rebuilt firmware myself: `source ~/esp/esp-idf/export.sh && cd firmware/system && idf.py build` (forced fresh recompile of `app_main.cpp`, `input_harness_game.h`, `gpio_input_source.{h,cpp}` by touching them first). Read all four files. | Build green; `GameSession`/`GameLoop` shipped public APIs used unmodified — no new parameter, no new method | Build green (`steamcore_system.bin`, 0x2fd80 bytes, 81% partition free). Confirmed `GameSession::advance(const GameInput&, bool)` and `GameLoop(Game&, Framebuffer&)`/`tick(const GameInput&)` used exactly as declared in `game_state.h:73` and `game_loop.h:121,124` — no new symbol added to either | ✅ pass |
| NFR-1 (host half) | Ran `make test FILTER=flips_on_exactly` (3 tests, `Samples`=1/2/5); read `input.h:63` (`kDebounceSamples = 2`) | Level flips on exactly the Samples-th agreeing sample, window expressed as a sample count | 3/3 pass. Chosen window: `kDebounceSamples = 2` → ≤33 ms at the 60 Hz game tick. **Device half not capturable** (no hardware); note the on-device harness (T9, not run on hardware) ticks at 20 ms, so its *effective* window would be 40 ms, not ≤33 ms — already flagged as review F2, does not affect the host-verified Must ACs | ✅ pass (host half); device half not capturable |
| NFR-2 | Grepped `input.h`, `input_test.cpp`, `gpio_input_source.{h,cpp}` for `new`/`malloc`/container types; `make lint`'s allocation rule | No dynamic allocation anywhere in the driver | Confirmed clean; `make lint` green | ✅ pass |
| NFR-3 | Grepped for `<chrono>`/`time(`/`rand(` etc. in the input file set; `make lint`'s clock/RNG rule (extended into `port/`) | No wall-clock read, uniformly across all 7 signals including the port | Confirmed clean; lint rule explicitly reaches into `port/esp32/gpio_input_source.{h,cpp}` and passed | ✅ pass |
| NFR-4 | Read `board_config.h`; `make lint`'s GPIO-literal rule | No GPIO literal outside `board_config.h` | Confirmed; lint green | ✅ pass |
| NFR-8 | Ran `make test FILTER=game_loop_test` implicitly via full suite; read `game_loop.h` `static_assert` | `static_assert(sizeof(GameInput) == 7 * sizeof(bool))`; positional 2-arg literals keep `start`/`fire` meaning | Confirmed size guard present; full suite green including `game_loop_test.cpp`'s 2-arg-literal-still-means-start-fire test | ✅ pass |

## 3. Exploratory Findings

| # | Severity | Steps to reproduce | Expected vs. observed | Status |
|---|---|---|---|---|
| — | — | `make test` run 3x consecutively (flakiness check) | Expected: consistent 153/153 each run. Observed: 153/153, 153/153, 153/153 — no flakiness | n/a, no bug |
| — | — | `make test FILTER=this_matches_nothing_xyz123` | Expected: "no test matched filter", non-zero exit. Observed: `ERROR: no test matched filter "this_matches_nothing_xyz123"`, `make: *** [test] Error 1` — the negative path works correctly | n/a, no bug |

No new bugs found. I independently reviewed review.md's 9 open Minor findings (F1–F9) and confirm the reviewer's own judgment: none touches a Must AC's correctness — F1 (unused `Debouncer::level()`), F2 (harness tick-rate window mismatch, device-only), F3 (unlabelled synthetic game-over in log, device-only), F4/F5 (wiring-doc completeness), F6 (doc wording overstates automation, doesn't change what's actually tested), F7/F8 (artifact wording), F9 (defensive asserts, not currently exercised) all sit outside US-1/US-2/US-3's host/document-verified surface.

## 4. Console & Network

N/A — no browser, no network surface (constitution §8). Build output reviewed for warnings: `idf.py build` produced no warnings on the touched/recompiled objects; `make test`/`test-asan`/`test-gcc`/`lint` all compiled with `-Wall -Wextra -Werror` and zero diagnostics.

## 5. Verdict

Yes, I'd demo this. All three Must stories (US-1/US-2/US-3) are independently verified — I ran every mapped test in isolation via `FILTER`, read every test body against its AC's wording rather than trusting the test name, read the two document-verifiable ACs directly, and rebuilt the ESP-IDF firmware myself (forcing a real recompile, not relying on a cached binary) to confirm AC-4.5's structural claim. Host gates are unanimously green: `make test`/`test-asan`/`test-gcc` all 153/153, `make lint` clean, no flakiness across 3 repeated runs, and the negative-filter path fails correctly. US-4's four hardware-only ACs (AC-4.1–4.4) are honestly recorded as not capturable — no physical hardware is wired, confirmed against `docs/wiring-input.md`'s own Status section and plan.md's T10 row — and since US-4 is a Should explicitly pre-negotiated in the spec (A3/C7) as gated on hardware existing by `/increment` time, this does not block the verdict. No new bugs found; review's 9 open Minors/1 Nit are confirmed not to threaten any Must AC.

---

## ✅ QA GATE

- [x] Every Must-story acceptance criterion verified (AC-1.1–1.4, AC-2.1–2.4, AC-3.1–3.3) — all host-test or document verified, performed myself
- [x] Every browser-observable NFR verified and passed — N/A surface (constitution §8); host-observable NFR-1 (host half), NFR-2, NFR-3, NFR-4, NFR-8 all verified
- [x] No open Blocker or Major bugs (Minor bugs listed and accepted by the user) — 9 Minor + 1 Nit from review, none new, none blocking
- [x] Browser console free of errors on the tested flows — N/A, no browser; build/test output clean of warnings
- [x] Tested on all agreed viewports — N/A, no visual surface
- [x] Line budget respected: Ist 71 / Soll ~130
- [x] Status set to `passed`
