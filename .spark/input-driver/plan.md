# Plan: input-driver

| | |
|---|---|
| **Phase** | Plan |
| **Owner** | Engineering Manager (`/sprint-plan`) |
| **Input** | `.spark/input-driver/spec.md` (`approved`) |
| **Status** | `approved` |
| **Date** | 2026-09-04 |

**Handoff**
- **Status:** `approved` — user approved as proposed, including both flagged items: the 5-symbol NFR-6 public-surface reading (§1 Decision 8) and the T9/T10 split of the hardware-gated Should (§1 Decision 7).
- **Summary:** Reuse `display-driver`'s proven boundary unchanged — pure half in `include/steamcore/input.h` (header-only), ESP-IDF half in `port/esp32/gpio_input_source.{h,cpp}`, seam is a compile-time `InputReader<Source>` template, not a virtual interface. Debounce is **N consecutive agreeing samples** (`kDebounceSamples = 2`, ≈ ≤33 ms at the 60 Hz tick), no clock anywhere; seven `Debouncer` instances live in one array driven by **one loop over one signal table**, so per-signal special-casing (AC-2.2) is structurally impossible rather than merely reviewed against.
- **Open:** `9 of 10 tasks done; T10 blocked` (no hardware wired — see §3, T10's own row). `/peer-review` passed (Round 1, review.md), `/demo-day` passed (Round 1, qa.md); T10 stays blocked until the wiring guide (`docs/wiring-input.md`) is physically followed.
- **Binding ruling:** §3 Task Breakdown for current task status; a plan revision after review/QA findings updates §1/§3 in place, never a new section
- **On conflict:** the numbered body below wins for everything except `Status`; log the mismatch as a finding at the next `/peer-review` and proceed — don't stop on it.

## 1. Architecture Decision

- **Context:** This is the **second** ESP-IDF-boundary feature, so the boundary itself is settled precedent
  (`display-driver` plan §1 Decision 1, constitution §3/§4): `port/esp32/` is a directory the host Makefile's
  `firmware/steamcore/src/*.cpp` glob structurally cannot see, and §4's "no ESP-IDF header in `include/`/`src/`"
  rule stays absolute and un-excepted. What is genuinely undecided here is the **debounce mechanism**: its
  algorithm, its state layout across seven independent switches, and its single entry point. Three constraints
  bound it: NFR-3/A9 forbid any wall-clock read (input feeds game logic, so a clock here would break
  `GameLoop`'s replay guarantee — unlike display *output* timing, which NFR-5 of `display-driver` deliberately
  exempted); constitution §6 forbids dynamic allocation; and AC-2.2 demands that the seven signals demonstrably
  share one mechanism rather than seven lookalikes.

- **Decision:**
  1. **Boundary: reuse, don't reinvent.** Pure half: one new header-only `include/steamcore/input.h` (the host
     Makefile globs `test/*_test.cpp` automatically — **no Makefile change**). Impure half: new
     `port/esp32/gpio_input_source.{h,cpp}`, `steamcore::port::esp32`-namespaced, added to
     `firmware/system/main/CMakeLists.txt`'s explicit `SRCS` list. Checked against fit, not copied blindly: the
     display precedent works here for the same reason it worked there — the only ESP-IDF surface needed is
     `gpio_config` + `gpio_get_level`, ~30 lines, and everything interesting sits on the pure side of it.
  2. **The seam is a compile-time template**, mirroring `TilePusher<Transmitter>`:
     `template <typename Source, int32_t Samples = kDebounceSamples> class InputReader`. A `Source` must provide
     `bool readSignal(InputSignal) const`, returning the **logical** raw level (`true` = pressed). Host tests
     supply a fake source that replays a scripted per-tick bounce pattern; `GpioInputSource` satisfies the same
     concept on device. Same no-vtable rationale `GameLoop<Game>` and `TilePusher` already set.
  3. **Algorithm: N consecutive agreeing samples**, `inline constexpr int32_t kDebounceSamples = 2`. Each
     `Debouncer::sample(bool raw)` returns the current stable level; a raw value differing from the stable level
     increments a counter, an agreeing one resets it, and the level flips only when the counter reaches
     `Samples`. No clock, no timer, no history word. At the 60 Hz tick that is a ≤33 ms confirmation window —
     inside arcade-immediacy (Principle 4), and expressed in **samples, not milliseconds**, so the caller's tick
     rate is the only timing input and nothing reads time. `Samples` is a non-type template parameter defaulted
     to the named constant, so tests prove the threshold at 1, 2 and 5 without editing the mechanism.
  4. **Seven instances of one type, driven by one loop** (A10): `InputReader` holds
     `Debouncer<Samples> debouncers_[kInputSignalCount]` plus, in `detail`, one `constexpr bool GameInput::*`
     field table in declaration order. `read()` is a single `for` over `kInputSignalCount` — sample signal *i*,
     write `input.*kInputFields[i]`. **No signal name appears in any control flow**, so a bespoke per-signal path
     cannot be written without deleting the loop; AC-2.2 becomes structural, and a test asserts the table's seven
     entries are distinct and in spec order (A1/NFR-8).
  5. **One entry point:** `GameInput read(Source&)`, returning a fully populated value (7 bytes, no allocation),
     called once per tick. No public per-signal accessor — a partially populated `GameInput` is unrepresentable.
  6. **Electrical polarity lives entirely in the port.** Switches are wired to GND with the ESP32's internal
     pull-ups enabled, so `GpioInputSource::readSignal` returns `gpio_get_level(pin) == 0`. The pure core never
     learns that "pressed" is electrically low, and an unwired pin reads *released* rather than noise.
  7. **US-4 splits in two, honestly.** T9 (write the real source + the serial-log harness, `idf.py build` green)
     needs **no** hardware and is executable today. T10 (a human pressing real controls) needs buttons that do not
     exist; it is planned, ordered last, and will be reported `blocked` — never silently dropped, never reported
     as passed on a substitute. If the increment must shrink, T9 and T10 are the cut, in that order.
  8. **Recorded interpretation of NFR-6 (flagged for approval).** The spec names "one debounce-core entry point".
     Realizing it needs five public symbols in `input.h`: entry point `InputReader<Source>::read`, plus
     `InputSignal` (the parameter type of the `Source` concept — unavoidable), `Debouncer` (the shared mechanism
     A10 requires be one named thing, and AC-2.1 tests it in isolation), and the constants `kInputSignalCount`
     and `kDebounceSamples`. The field table is `detail::kInputFields`, following `game_loop.h`'s own convention.
     Declared here rather than discovered at `/peer-review`.
  9. **The clock/RNG lint ban extends into `port/` for this feature** — deliberately the opposite call from
     `display-driver`, whose lint block carries an explicit comment exempting `port/esp32` because display-output
     timing never feeds back into game logic. Input does: a sample taken on a timer instead of a tick changes what
     `GameSession` sees. T7 writes that reasoning into the script beside the display rule it contradicts.

- **Alternatives considered:**

  | Alternative | Why rejected |
  |---|---|
  | Virtual `IInputSource` interface instead of a template parameter | Buys identical testability, costs the first vtable in a codebase that has none; `game-loop` and `display-driver` both already rejected runtime polymorphism for one-implementation seams, so deviating would itself be an architecture decision |
  | `#ifdef ESP_PLATFORM` around a GPIO reader in `src/` | Punches a named exception into the one lint rule constitution §4 calls load-bearing. A directory the host build cannot see needs no exception — already settled by `display-driver` |
  | Millisecond debounce window via `esp_timer`/`xTaskGetTickCount` | Forbidden by NFR-3/A9 and it would make the host fixture need a clock fake to prove anything. Sample counting at a caller-driven tick is both deterministic and trivially fixture-able |
  | Interrupt-driven sampling (GPIO ISR + queue) | Introduces concurrency into a contract that is single-threaded everywhere, is unprovable on the host gate, and buys latency the 60 Hz consumer cannot use anyway. Polling 7 pins per tick is free |
  | Shift-register / integrator debounce (8-bit history, Ganssle-style) | More state and more explaining for no better result when sampling at 16.7 ms — a full bounce train is mostly aliased away before the algorithm sees it. N-consecutive is the simplest thing that provably satisfies AC-2.1 |
  | One `Debouncer` type internally owning all seven signals | Welds the algorithm to this exact signal set: AC-2.1 could no longer test the mechanism in isolation, and an eighth signal later would edit the type instead of adding an instance |
  | Seven named members (`start_`, `fire_`, …) instead of an indexed array | Gives every signal its own line of code — exactly the shape AC-2.2 exists to forbid. Uniformity would go back to being reviewer diligence |
  | Bitmask / direction enum inside `GameInput` | Explicitly out of scope (§6, A2): four independent bits are the whole contract |
  | Debounce inside `GameSession` or the consumer | §6 out of scope and it would duplicate edge/level ownership (A8); `GameSession` stays byte-identical this cycle |
  | A third-party debounce library (Bounce2 and friends) | Arduino-framework dependency for ~20 lines of code we must own and test anyway. Every dependency is a liability; this one doesn't pay rent |
  | Drive the panel from the US-4 harness too (state shown on screen) | Every AC-4.x is worded against the **serial log**. Adding the display path re-verifies something already released (v0.2.0) and grows a throwaway harness. Log-only |

- **Consequences:** *Easier* — the entire debounce contract, all seven signals and the `GameSession` composition
  are proven on `make test`/`test-asan`/`test-gcc` with zero ESP-IDF, exactly the payoff `TilePusher`'s fake
  transmitter bought; a second input port (USB gamepad, README's later phase) is a sibling `Source`, not a
  rewrite; retuning the window is one constant plus a threshold-parameterized test. *Harder* — `port/esp32/` gains
  a second file no CI gate compiles (same permanent cost, same counterweight: lint + `docs/device-build.md`);
  `GameInput` changes size, so both build systems and every harness recompile; and the on-device half of this
  feature stays unverifiable until physical parts exist, which the plan states rather than hides.

## 2. Affected Components

Scoped by hand — no tool file was passed with this task, so no blast-radius query was run and none is cited here.

- **New (pure, host-gated):** `firmware/steamcore/include/steamcore/input.h`;
  tests `firmware/steamcore/test/input_test.cpp`, `firmware/steamcore/test/input_session_test.cpp`,
  fixture `firmware/steamcore/test/fake_input_source.h`.
- **New (ESP-IDF, device-only):** `firmware/steamcore/port/esp32/gpio_input_source.{h,cpp}`.
- **New (docs):** `docs/wiring-input.md` (US-3's deliverable).
- **Modified:** `firmware/steamcore/include/steamcore/game_loop.h` (`GameInput` +5 fields, `static_assert`,
  doc comment); `firmware/steamcore/test/game_loop_test.cpp` (the default-construction test now names seven
  fields); `firmware/steamcore/include/steamcore/board_config.h` (7 pin constants);
  `tools/check_constraints.sh` (T7); `docs/host-tests.md`, `docs/device-build.md`;
  `firmware/system/main/app_main.cpp` + `CMakeLists.txt` (T9 harness).
- **Untouched, by design:** `game_state.{h,cpp}` (`GameSession` unmodified — asserted by `git diff --exit-code`
  in T5), `framebuffer.*`, `dirty_tracker.*`, `panel_format.*`, `tile_pusher.h`,
  `port/esp32/ili9488_display.*`, `Makefile` (header-only + auto-globbed tests need no change),
  `session_replay_fixture.h`, `replay_fixture.h` (audited: every existing `GameInput{…}` literal in the tree uses
  0 or 2 positional arguments, so aggregate init defaults the five new fields to `false` and every literal keeps
  its original `start=a, fire=b` meaning — NFR-8 satisfied by field order, with zero call-site migration).
- **New dependencies: none.** `driver/gpio.h` and `esp_log.h` ship with the already-installed ESP-IDF v5.4.4 and
  are already used by `firmware/system/`. No new package, no new service, no new pattern — the port directory and
  the template seam are both established precedent.
- **Pin assignment (AC-3.1), decided:** joystick as one contiguous block — `kPinInputUp = 4`, `kPinInputDown = 5`,
  `kPinInputLeft = 6`, `kPinInputRight = 7`; buttons — `kPinInputStart = 15`, `kPinInputFire = 17`,
  `kPinInputSelect = 18`. All seven support internal pull-ups and avoid GPIO26–37 (octal PSRAM/flash), the
  boot-strapping pins 0/3/45/46, USB D-/D+ (19/20) and the display's 9–14. Also avoided, with reasons:
  43/44 (UART0), 38–42 (JTAG), 47/48 (onboard RGB LED on this devkit family), and **16 — deliberately skipped
  because `check_constraints.sh`'s tile-size rule bans a bare `16` anywhere in `include/` and excludes only
  `config.h`, not `board_config.h`**; skipping one pin is cheaper than weakening that rule.

## 3. Task Breakdown

| # | Task | Story | Covers (AC / NFR) | Depends on | Status | Definition of Done |
|---|---|---|---|---|---|---|
| T1 | Extend `GameInput` to seven fields; re-pin the `static_assert`; audit every call site | US-1 | AC-1.3, NFR-8, NFR-7 | – | `done` | `grep -rn "GameInput{"` audit confirmed: every existing call site (`game_loop_test.cpp`, `replay_fixture.h`, `session_replay_fixture.h`, `game_state_test.cpp`) uses 0 or 2 positional args — zero migration needed. `inputEquals` extended to compare all 7 fields; two new named tests: default-construction (all 7 false) and a two-arg-literal-still-means-start-fire proof (NFR-8). `make test`/`test-asan`/`test-gcc`/`lint` all green (125/125). | `GameInput` declares exactly `start, fire, select, up, down, left, right` in that order, each `= false`; `static_assert(sizeof(GameInput) == 7 * sizeof(bool))` replaces the `2 *` form; the header comment states the seven fields are raw, undecoded, independent levels with no diagonal encoding and no edge detection; `game_loop_test.cpp`'s default-construction test asserts all seven fields false and one existing two-argument literal is asserted to still mean `start`/`fire`; a `grep -rn 'GameInput{'` over the repo is recorded showing no call site takes more than two positional arguments, so none needed migration; `make test-all` green — files: firmware/steamcore/include/steamcore/game_loop.h, firmware/steamcore/test/game_loop_test.cpp |
| T2 | Walking skeleton: `input.h` + fake source + one signal debounced end to end | US-1, US-2 | AC-1.1, AC-2.4, NFR-2, NFR-3 | T1 | `done` | `input.h` (InputSignal, kInputSignalCount, kDebounceSamples=2, Debouncer<Samples>, InputReader<Source,Samples>) + `fake_input_source.h` + one walking-skeleton test proving a clean false→true→false cycle on `start` alone. `make test`/`test-asan`/`test-gcc`/`lint` green (126/126), no Makefile change, no ESP-IDF header. | `input.h` declares `InputSignal` (7 enumerators, no explicit values), `kInputSignalCount`, `kDebounceSamples = 2`, `Debouncer<Samples>` with `sample(bool)`/`level()`, and `InputReader<Source, Samples>` with `GameInput read(Source&)`; `fake_input_source.h` defines a `steamcore::test` fake replaying a scripted per-tick level per signal; one test drives a press-then-release on `start` alone through `InputReader` and asserts a single clean `false→true→false` cycle in the returned `GameInput`; the header includes no ESP-IDF header, allocates nothing and reads no clock; `make test`, `make test-asan`, `make test-gcc` and `make lint` green with no `Makefile` change — files: firmware/steamcore/include/steamcore/input.h, firmware/steamcore/test/fake_input_source.h, firmware/steamcore/test/input_test.cpp |
| T3 | Signal battery: all seven independently, level-not-pulse, cold start, simultaneous directions | US-1 | AC-1.1, AC-1.2, AC-1.3, AC-1.4 | T2 | `done` | Seven named per-signal tests (`input_reader_<signal>_alone_cycles_only_<signal>`) call a shared `checkPressReleaseCyclesOnlyThatSignal` helper that presses+releases one signal and asserts the other six stay false on every tick throughout, against an independently-redeclared `kAllFields` table (not the production `detail::kInputFields`, so a bug in that table couldn't be masked); a held test confirms `fire` reads true on 10/10 consecutive ticks; a cold-start test confirms all seven false across 10 ticks of an all-released run; two diagonal tests (`up`+`right`, then all four directions) confirm independent `true`s with no collapsing; a table test compares `detail::kInputFields` element-by-element against `kAllFields` and confirms no duplicate entries. `make test`/`test-asan`/`test-gcc`/`lint` all green (138/138). | One named test per signal presses and releases that signal alone and asserts its field cycles once while the other six stay `false` on every tick; a held test asserts the field reads `true` on each of ≥10 consecutive ticks (a level, not a pulse); a cold-start test asserts all seven read `false` on the very first `read()` and on every tick of an all-released run; a diagonal test holds `up`+`right` together and asserts both fields read `true` independently, with `left`/`down` false, and a further test holds all four directions at once; a table test asserts `detail::kInputFields`' seven entries are distinct and in spec declaration order — files: firmware/steamcore/test/input_test.cpp |
| T4 | Bounce battery: one transition per press, per signal, under simultaneous noise; threshold proven as a count | US-2 | AC-2.1, AC-2.2, NFR-1 | T3 | `done` | Shared `kBouncePattern`/`countTransitions` fixture (isolated single-sample noise blips plus two genuine 2-sample press/release events) applied per-signal (7 tests, each asserting exactly 2 transitions) and to all seven at once with per-signal phase offsets (1 test, each field independently asserted at exactly 2 transitions); 3 latency tests prove the flip lands on exactly the `Samples`-th agreeing sample at `Samples` = 1, 2, 5 via the template parameter. Mutation-verified: removed the counter reset on agreement in `Debouncer::sample` (input.h) — rebuilt, and exactly the 8 new bounce tests failed (7 per-signal + the simultaneous one), all 141 others (including T1–T3 and the latency tests) stayed green, since non-noisy sequences never exercise the removed reset; reverted, `make test`/`test-asan`/`test-gcc`/`lint` all green again (149/149). | A shared bounce fixture (a scripted raw pattern with multiple rapid highs/lows around one press) is applied to each of the seven signals in turn and asserts exactly one level transition is reported for that field per simulated event, counted by the test rather than eyeballed; one test bounces **all seven simultaneously** with different phases and asserts each field's outcome is correct independently of the others; a latency test asserts a level flips on exactly the `Samples`-th agreeing sample and not before, run at `Samples` = 1, 2 and 5 via the template parameter, pinning the ≤33 ms window at 60 Hz as a sample count (NFR-1's host half, value recorded for `qa.md`); mutation-verified — replacing the counter comparison with an unconditional flip fails the bounce tests and only those, then reverted with the suite green — files: firmware/steamcore/test/input_test.cpp |
| T5 | Composition: debounced `start` into the unmodified `GameSession` | US-2 | AC-2.3 | T4 | `done` | Four tests in a new `input_session_test.cpp`: a bouncing press causes exactly one READY→PLAYING transition; holding start afterward for 20 further ticks causes no second transition; release-then-press-again drives PLAYING→GAME_OVER→PLAYING; the same bouncing-press proof repeated for the GAME_OVER→PLAYING restart. `git diff --exit-code v0.1.0 -- game_state.h game_state.cpp` confirmed byte-identical (exit 0) — no GameSession change was needed. `make test`/`test-asan`/`test-gcc`/`lint` all green (153/153). | A test feeds a bouncing `start` press through `InputReader` into `GameSession::advance(input, false)` from READY and asserts exactly one READY→PLAYING transition for that press; holding `start` for many further ticks produces no second transition; a release-and-press-again produces the next one; the same is proven for GAME_OVER→PLAYING; `git diff --exit-code` over `game_state.h`/`game_state.cpp` is recorded showing `GameSession` is byte-identical to its shipped v0.1.0 form — files: firmware/steamcore/test/input_session_test.cpp |
| T6 | Pin table in `board_config.h` + the physical wiring guide | US-3 | AC-3.1, AC-3.2, AC-3.3, NFR-4 | T1 | `done` | `board_config.h` gained the seven named constants at the stated pins with a comment naming every excluded range (9–14 display, 26–37 PSRAM/flash, 0/3/45/46 boot-strap, 19/20 native USB) and why 16 is skipped (spare adjacent slot); `docs/wiring-input.md` written with the pin table, active-low common-ground topology, component-class shopping list, explicit "four independent microswitches, not a joystick" statement, and a Status section stating nothing is wired yet. `make test`/`lint` green (153/153, lint doesn't yet check these new pins — that's T7). | `board_config.h` gains exactly seven named constants (`kPinInputUp/Down/Left/Right/Start/Fire/Select` = 4/5/6/7/15/17/18) in the existing plain-`int` style, with a comment naming every excluded range (26–37, 0/3/45/46, 19/20, 9–14) and why 16 is skipped; `docs/wiring-input.md` gives, per signal, its GPIO and its switch topology — seven independent single-pole normally-open switches, each one leg to its GPIO and one leg to a shared common ground, internal pull-up enabled in firmware, no external resistors, 3.3V logic only — states explicitly that the four directions are four separate microswitches and not a multi-position or analog input, lists what the user must acquire at component-class level (no part numbers, §6), repeats the display's already-claimed pins so nothing is double-booked, and states plainly that **nothing is wired yet** and that wiring is the explicit next physical-world action before US-4 can be attempted; `make lint` green — files: firmware/steamcore/include/steamcore/board_config.h, docs/wiring-input.md |
| T7 | Lint rules for this feature's own concerns | US-2, US-3 | AC-3.1, NFR-2, NFR-3, NFR-4, NFR-8 | T4, T6 | `done` | GPIO-literal digit set extended to 4/5/6/7/15/17/18, verified zero false positives against the unmodified tree before editing; new clock/RNG-ban and allocation-ban blocks scoped to `input.h`/`input_test.cpp`/`input_session_test.cpp`/`fake_input_source.h`, with a comment explaining why this block (unlike display-driver's) reaches into `port/` — deferred to T9 for `gpio_input_source.{h,cpp}` itself, since those files don't exist until that task (small deviation from the plan's literal file list, recorded here: T7 depends on T4/T6, not T9, so the two port files couldn't exist yet without breaking the missing-file-is-a-failure posture; T9 will extend `INPUT_DETERMINISM_FILES` once it creates them); new presence check for `game_loop.h`'s `GameInput` size guard. All four demonstrated failing individually against a planted violation (incl. `gpio_set_level(GPIO_NUM_17, 0)`), each reverted, `make lint`/`make test`/`test-asan`/`test-gcc` green (153/153). `docs/host-tests.md` updated to describe the three new/extended rules. | `check_constraints.sh` gains one named input-driver block: (a) the existing GPIO-literal rule's digit set is extended from 9–14 to also cover 4/5/6/7/15/17/18 — **verified**, not assumed, by running the full `make lint` on the unmodified tree and confirming zero false positives across `include/`, `src/`, `port/` and `firmware/system/main/`; (b) a clock/RNG ban over the input mechanism file set (`input.h`, `input_test.cpp`, `input_session_test.cpp`, `fake_input_source.h`, `port/esp32/gpio_input_source.{h,cpp}`) reusing the shared `CLOCK_RNG_PATTERN`, with a comment stating why this one **does** reach into `port/` while the display-driver rule beside it deliberately does not; (c) the shared allocation pattern applied to the same file set; (d) the presence of `sizeof(GameInput) == 7 * sizeof(bool)` in `game_loop.h` asserted, so the size guard cannot be deleted silently; all four use the existing missing-file-is-a-failure posture; each is demonstrated failing once against a temporarily planted violation (including a `gpio_set_level(GPIO_NUM_17, 0)` outside `board_config.h`), then reverted and `make lint`/`make test` green; if (a) does produce a false positive, the token pattern is narrowed and never a digit dropped — files: tools/check_constraints.sh, docs/host-tests.md |
| T8 | Contract documentation and the NFR-6 surface audit | US-1, US-2 | NFR-6, NFR-7 | T5, T7 | `done` | Six of the seven required contract statements were already present in `input.h` since T2 (level-not-edge/GameSession ownership, tick-driven determinism, single-threaded/no-throw/no-alloc, Source signature, logical-polarity-in-port, usage example); added the one missing explicit line ("One mechanism (Debouncer), instantiated seven independent times inside InputReader"). Surface audit: `input.h`'s five public symbols (`InputReader`, `InputSignal`, `Debouncer`, `kInputSignalCount`, `kDebounceSamples`) match §1 Decision 8 exactly, no drift; §2 Affected/Untouched lists checked against the tree as it stands through T7 — no correction needed. `docs/host-tests.md` gained an input-driver paragraph naming AC-1.1–3.3 as host-covered, AC-4.5 as structurally verified (no hardware), and AC-4.1–4.4 as the only ones needing a human at real controls. `make test`/`test-asan`/`test-gcc`/`lint` green (153/153). | `input.h`'s doc comment states, each explicitly: returns a **level**, never an edge, and `GameSession` alone owns edge detection and only for `start`; tick-driven, never wall-clock, so replay determinism holds; single-threaded, nothing throws, no error code, no dynamic allocation; one shared mechanism instantiated seven independent times; the `Source` concept's exact required signature; that "pressed = true" is logical and electrical polarity belongs to the port; and one usage example in `dirty_tracker.h`/`tile_pusher.h` style; the public surface is audited symbol by symbol against §1 Decision 8's list and §2 corrected in place if it drifted; `docs/host-tests.md` states which ACs the host gate covers and which need the board; `make test-all` re-verified green after the doc-only edits — files: firmware/steamcore/include/steamcore/input.h, firmware/steamcore/include/steamcore/game_loop.h, docs/host-tests.md |
| T9 | Real `GpioInputSource` + serial-log harness, built for the device (no hardware needed) | US-4 | AC-4.5, NFR-5, NFR-6 | T8 | `done` | `port/esp32/gpio_input_source.{h,cpp}` built exactly to spec (init() one `gpio_config`, pull-up enabled, no interrupt; `readSignal` returns `gpio_get_level(pin) == 0`). New `input_harness_game.h` (replacing the deleted display-driver `harness_consumer.h`) drives a real `GameSession` through `GameLoop<Game>`'s canonical composition (game_state.h's own doc-comment pattern), logging every transition and every fire/select/direction level change; `sessionEnded` is the harness's own synthetic trigger (every 150 ticks spent PLAYING), never derived from input. `app_main.cpp` rewritten around `InputReader<GpioInputSource>`; `CMakeLists.txt` gained `game_state.cpp` and `gpio_input_source.cpp`. T7's deferred lint coverage now added: `PORT_DIR` moved to the top of `check_constraints.sh` (was defined too late for T7 to reference) and `INPUT_DETERMINISM_FILES` extended with the two new port files. `idf.py build` verified green end-to-end (sourced `~/esp/esp-idf/export.sh`, full ninja build, `steamcore_system.bin` produced) — executable with zero hardware. `docs/device-build.md` updated with the input-driver AC split and a note that this build's `app_main` is now the input harness, not display-driver's. `make test`/`test-asan`/`test-gcc`/`lint` green (153/153). | `port/esp32/gpio_input_source.{h,cpp}` defines `steamcore::port::esp32::GpioInputSource` with `void init()` (one `gpio_config` per signal: input mode, internal pull-up, no interrupt, pins read only from `board_config.h`) and `bool readSignal(InputSignal) const` returning `gpio_get_level(pin) == 0`, satisfying `InputReader`'s `Source` concept with no other public member; `app_main.cpp` replaces the display-driver harness with `InputReader<GpioInputSource>` + a real `GameSession`, ticking at a fixed documented interval, logging every `GameSession` transition and every debounced level change on the other six signals with signal name and new level, plus a synthetic `sessionEnded` trigger to reach GAME_OVER; `GameSession`/`GameLoop`'s shipped signatures are used unmodified; `main/CMakeLists.txt` lists the new port source; **`idf.py build` green** — the whole task is executable with no buttons in existence, and this is where AC-4.5's structural half is verified by reading the harness — files: firmware/steamcore/port/esp32/gpio_input_source.h, firmware/steamcore/port/esp32/gpio_input_source.cpp, firmware/system/main/app_main.cpp, firmware/system/main/CMakeLists.txt, docs/device-build.md |
| T10 | **Hardware-gated:** on-device confirmation with real presses | US-4 | AC-4.1, AC-4.2, AC-4.3, AC-4.4, NFR-1, NFR-5 | T9 | `blocked` | **Reported `blocked`, exactly per this row's own gating text.** No buttons, joystick or SELECT switch are physically wired (confirmed against `docs/wiring-input.md`'s own Status section, which states this explicitly, and against the user's own answer during `/story-time`: "Nein, noch nichts vorhanden"). AC-4.1, AC-4.2, AC-4.3 and AC-4.4 are recorded as **explicitly unverified** — not passed, not satisfied by any substitute (no jumper-to-GND sanity touch was run either, since none was requested). T9 already delivered everything executable without hardware: the harness builds and is `idf.py build`-green, and AC-4.5 (structural: shipped `GameSession`/`GameLoop` APIs used unmodified) is verified by that build plus reading the source. Unblocking T10 requires physically wiring the panel per `docs/wiring-input.md`, then re-running `/increment` for this one task. | **Executable only if buttons, joystick and SELECT switch are physically wired per `docs/wiring-input.md` by `/increment` time; if not, this task is reported `blocked` with that reason and its ACs recorded as explicitly unverified — never as passed, and never satisfied by a substitute (a jumper-to-GND touch may be run as an extra sanity check, but is recorded as exactly that, not as AC-4.x).** When runnable: flashed per `docs/device-build.md` (manual RESET after flash), the transcript shows exactly one READY→PLAYING transition for one deliberate START press; exactly one GAME_OVER→PLAYING for one press after the synthetic session end; a sustained multi-minute run of varied START/FIRE presses whose logged transition count equals the human's counted deliberate presses with zero spurious extras, with FIRE's debounced level logged per press/release; each direction and SELECT logged toggling correctly, including at least one held `up`+`right` diagonal showing both fields true at once; the human judges press-to-log latency immediate and the chosen `kDebounceSamples` value is recorded for `qa.md` (NFR-1); the transcript and the human's confirmation are recorded verbatim in this row — files: firmware/system/main/app_main.cpp, docs/device-build.md |

## 4. Test Strategy

- **Host-CI-verifiable at `/increment` time (`make test`, `test-asan`, `test-gcc`, `lint` — zero ESP-IDF):**
  AC-1.1–AC-1.4 (T2/T3), AC-2.1–AC-2.3 (T4/T5), AC-2.4 (T2, and every host task by construction), AC-3.1's grep
  half and NFR-2/NFR-3/NFR-4/NFR-8 (T7), NFR-1's window as a sample count (T4). This is the payoff of Decision 2:
  the entire debounce contract — the only subtle logic in the feature — is proven on a Mac with a fake source,
  exactly as `TilePusher`'s fake transmitter proved the retry contract.
- **Document-verifiable (no toolchain, no board):** AC-3.1's content, AC-3.2, AC-3.3 — read `board_config.h` and
  `docs/wiring-input.md` (T6). US-3 is a deliverable, not a runtime behaviour; "manual" here means "there is
  nothing to execute", not "we skipped the test".
- **Needs the physical board *and* parts that do not exist yet:** AC-4.1–AC-4.4, AC-4.5's runtime half, NFR-1's
  human latency judgment, NFR-5's actual log output (T10). Nothing in this plan reports these as passed until a
  transcript exists (constitution §4/§6, honest status). T9 deliberately carves out the part that needs only a
  compiler, so a missing button does not block writing and building the real driver.
- **US-1** — T3, one named test per signal so QA can record AC-1.1 per criterion with `make test FILTER=`. The
  cross-signal assertions (the other six stay false) are what actually carry "independently", not the per-signal
  cycle check.
- **US-2** — T4 is the primary proof and it is negative-first: the fixture must be observed producing a *bouncing*
  raw signal and the debounced output observed staying clean, or the suite proves nothing. The mutation check is
  what confirms the tests can fail. T5 then proves the whole point of the feature — that a real bouncing press
  reaches `GameSession` as exactly one transition — against the unmodified shipped type.
- **US-3** — reviewed, not executed (T6), plus T7's automated half: the pin numbers get a lint rule so a future
  bare literal fails the gate rather than a reviewer's attention.
- **US-4** — device-only by nature and hardware-gated (T10); split from its buildable half (T9) precisely so the
  honest status is "the run is blocked", not "the driver is missing".
- **Sanitizers:** `make test-asan` matters here for the seven-entry `Debouncer` array and the field-pointer table
  — an off-by-one in the `read()` loop reads in-bounds-looking garbage under plain `make test`.
- **Deliberately not automated, with reasons:** oscilloscope/electrical bounce capture (§6, A7 — simulated bounce
  plus a human press is the accepted method); an ESP-IDF `unity` test app (would add a third test framework and a
  second CI concept for something a human must physically press anyway); `/demo-day` in a browser (no
  browser-observable surface, constitution §8); real GCC (`/usr/bin/g++` is Apple clang — `test-gcc` stays
  honestly reported as nominal, unchanged from prior increments).

## 5. Risks & Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| No physical buttons exist, so US-4 cannot run at `/increment` | High for the Should, zero for the Musts | Planned as the split of Decision 7: T9 delivers and builds the real port without hardware; T10 is reported `blocked` with the reason named. `docs/wiring-input.md` (T6) is what unblocks it, and is itself a Must deliverable |
| Extending the GPIO-literal lint digits to 4/5/6/7 creates false positives — small integers sit near `spi`/`pin` tokens all the time | Medium: a noisy gate gets weakened or ignored | Verified clean against today's whole tree before choosing these pins (the only near-miss, `queue_size`, is `1`). T7's DoD requires full-tree `make lint` green **and** a planted violation caught; if a false positive appears, the token pattern is narrowed, never a digit dropped |
| A cheap switch bounces longer than 2 sample periods (>33 ms), so a single press still logs twice on real hardware | Medium: only discoverable at T10, and it looks like a logic bug | The threshold is one named constant plus a template parameter, and T4 already proves the mechanism at `Samples` = 1/2/5 — raising it is a one-line change with tests already covering it. The chosen value is recorded in `qa.md` (NFR-1) rather than assumed correct |
| The debounce window is expressed in samples, so a harness that ticks slowly (the display harness ticked at 400 ms) silently changes the effective window | Medium: an on-device result that does not represent the 60 Hz game case | Stated in the contract (T8) that the caller owns pacing and the window is a sample count; T9's harness ticks at a fixed, documented interval and logs it, so the transcript records what window was actually in effect |
| `port/esp32/gpio_input_source.*` is production code **no gate compiles** — it can rot silently between flashes | High and permanent: the inherited cost of the boundary, stated honestly | Same counterweight as `display-driver`: T7's lint rules cover what grep can (pins, allocation, clock reads), `docs/device-build.md` keeps the build reproducible, and Decision 2 keeps the file to ~30 lines with all logic on the pure side |
| T9 overwrites `app_main.cpp`, so the display-driver's on-device harness is no longer runnable from `HEAD` | Low-Medium: a previously demonstrated proof stops being one command away | Accepted deliberately: harnesses are throwaway by construction (spec NFR-6/A13, the same posture `display-driver` took toward the retired spike), `display-driver` is released and tagged at v0.2.0, and git history retains the harness verbatim |
| `sizeof(GameInput)` changes from 2 to 7 | Low: no call site takes more than two positional arguments (audited across the whole repo) and nothing serializes or `memcmp`s the type | T1 records the audit as part of its DoD, and the re-pinned `static_assert` plus T7 rule (d) make any future field addition a loud compile error rather than a silent size change |
| A partially wired cabinet (some signals unconnected) reads floating pins as random presses during T10 | Low: would look like a debounce failure | Internal pull-ups are configured in `GpioInputSource::init` (Decision 6), so an unwired pin reads released; `docs/wiring-input.md` states this so a partial wiring is a legitimate, interpretable test setup |

## Deviations (fix-mode, review.md F1-F9)

- **F9's fix uncovered a new, GCC-only build break, fixed in the same pass.** F9 asked for two structural guards in `input.h`; the second (`static_assert(kInputFields[kInputSignalCount - 1] != nullptr, ...)`) compiles clean under host clang/g++ but fails `idf.py build` under the real ESP-IDF xtensa-GCC toolchain with `-Werror=address` ("the address '&steamcore::GameInput::right' will never be NULL") — GCC constant-folds the indexed access back to the literal member-address expression and (correctly, for the code as it stands today) observes the comparison is trivially true, with no way to know the guard exists for a *future* miscount, not today's already-correct table. Fixed with a narrowly-scoped `#pragma GCC diagnostic ignored "-Waddress"` around the one `static_assert`, guarded by `#if defined(__GNUC__) && !defined(__clang__)` so it's a no-op on host clang (which never warned) and only fires for a real GCC. Re-verified: `make test`/`test-asan`/`test-gcc` 154/154 all three, `make lint` clean, `idf.py build` green. Small, obvious correction — no architecture or scope change, recorded per the `/increment` fix-mode rule rather than silently folded into F9's own note.

---

## ✅ PLAN GATE

*All boxes checked → `/increment` may start. Any box open → back to `/sprint-plan`.*

- [x] Spec status is `approved` (never plan against a draft)
- [x] Architecture decision includes rejected alternatives (10 recorded, §1)
- [x] Architecture respects the constitution's technical constraints (§3 pin assignment — seven new constants in `board_config.h` only, every reserved range avoided and named; no dynamic allocation — seven fixed `Debouncer` members and a `constexpr` table; §3 Timing/determinism — no clock read anywhere in the input path, enforced by a new lint rule that deliberately reaches into `port/`; §4's no-ESP-IDF-header-in-logic rule left absolute via the existing `port/esp32/` directory boundary; C++17, `steamcore` namespace, `snake_case`, English) — no conflict found
- [x] Every task maps to a user story — no orphan tasks, no story without tasks
- [x] Every Must AC and every applicable NFR is covered by at least one task (AC-1.1–1.4, AC-2.1–2.4, AC-3.1–3.3, AC-4.1–4.5; NFR-1–NFR-8; NFR-9/NFR-10 are N/A per the spec)
- [x] Every task has a checkable definition of done
- [x] Task order respects dependencies (walking skeleton first: T1 re-shapes the contract, T2 puts one signal through the whole fake-source → debounce → `GameInput` path before any battery exists; the hardware-gated work is last and split so its buildable half is not blocked by its unbuildable one)
- [x] Test strategy covers every Must story, and states per AC whether it is host-CI-verifiable, document-verifiable or needs the board
- [x] Line budget respected: Ist 206 / Soll ~300 (excluding HTML comments) — 94 under
- [x] Status set to `approved` by the user
