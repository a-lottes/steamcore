# Review Report: display-driver

| | |
|---|---|
| **Phase** | Review |
| **Owner** | Reviewer (`/peer-review`) |
| **Input** | The diff of `/increment`, `.spark/display-driver/plan.md` |
| **Status** | `passed` |
| **Round** | 2 |
| **Date** | 2026-09-03 |

**Handoff**
- **Status:** mirrors the header table above (authoritative for `Status`).
- **Verdict:** Round 2 confirmed all five round-1 fix claims (F1, F2, F5, F8, F9) against the code and, for F1 and F5, against my own on-device run and my own toolchain probe. No Blocker or Major is open; the gate is closed.
- **Open:** `6 open` (Minors/Nits F10-F15, all judgment calls, none blocking). Confirmed fixed round 2: `F1`, `F2`, `F5`, `F8`, `F9`. Fixed by the reviewer round 1: `F3`, `F4`, `F6`, `F7`. New this round: `F15` (Minor).
- **Binding ruling:** §6 Verdict and the gate checklist below — the only binding location; there is no other round to point to
- **On conflict:** the numbered body below wins for everything except `Status`; log the mismatch as a finding at the next `/peer-review` and proceed — don't stop on it.

## 1. Scope

Same diff as round 1: everything uncommitted against `HEAD` (`f6370a7`) — 6 modified
files, 2 deleted (`firmware/system/main/ili9488_display.{h,cpp}`, the retired spike),
9 new (`panel_format.{h,cpp}`, `tile_pusher.h`, `port/esp32/ili9488_display.{h,cpp}`,
`fixture_pattern.h`, two test files, `harness_consumer.h`, `docs/device-build.md`),
plus the five fix-mode edits. No tool file was passed; scoping was by hand.

**Round 2 re-verified from source, not from round 1's fix descriptions** (condition
(a), verifying a fix, for all five): `app_main.cpp` + `fixture_pattern.h` read in
full and the harness **flashed and run on the real board by me** (F1);
`port/esp32/ili9488_display.h` contract against `.cpp:90-179` (F2);
`firmware/system/CMakeLists.txt` plus **my own** parse of the regenerated
`build/compile_commands.json` and an empirical dialect probe (F5); `plan.md` §2
(F8); `tile_pusher.h` + `tile_pusher_test.cpp` (F9). Host suite re-run by me
(`make clean && make test-all`) since F9 touched test code. Re-read for context:
`harness_consumer.h`, `framebuffer.h`, spec AC-3.1/A11/C12, plan T7's DoD.

**Not re-examined this round** (left untouched, per the caller's scope): F10–F14 and
the round-1 evidence behind them; the lint block (F3/F4), re-run green only.
**Cannot be compiled by any host gate:** `port/esp32/` — judged by reading plus
`idf.py build` and the on-device run.

## 2. Plan Conformance

| Task | Implemented as planned? | Note |
|---|---|---|
| T1 | ✅ | `config.h` derives all panel geometry from `kScreenWidth/Height`/`kTileSize`; `static_assert`s pin 480/320/32/3072. |
| T2 | ✅ r2 | Spike deleted, no stale reference anywhere in the tree (round 1, grep for `bringup`/`ili9488Init`/`ili9488FillColor`). The PLAN GATE's `-std=gnu++17` pin now exists and is *effective*, not just present — F5 fixed r2. |
| T3 | ✅ | One named test per `Color` + pairwise-distinctness + determinism; hex values match `docs/dump-format.md`. |
| T4 | ✅ | Coverage-array test is the real one and it works: an injected `x1 = x0 + kPanelTileSize` off-by-one failed it. |
| T5 | ✅ | I re-ran the plan's own mutant (commit unconditionally) — exactly the 3 named tests failed, 121/124. Not a tautological suite. |
| T6 | ✅ r2 | Two-phase failure policy unchanged in code; the header now documents it accurately — F2 fixed r2. Stack-overflow lesson documented — F6. |
| T7 | ✅ r2 | Fixture extraction clean and round-trip-safe. The DoD's per-anchor log is back and I re-ran it on hardware: 12 anchor lines, once, after tick 1's push — F1 fixed r2 (see F15 for what the log still can't fail on). |
| T8 | ✅ | Traced by hand: tick 1 → 150, tick 2 → 1, ticks 3–21 → 2 each, marker over 20 distinct tiles; matches the recorded transcript exactly. `game_loop.h` untouched. |
| T9 | ⚠️ | 5 rules present. Rule (b) both over- and under-fired (F3) and the block had no missing-path guard (F4); both fixed and re-probed. |
| T10 | ✅ r2 | Doc comments thorough and now consistent with the code (F2 fixed r2); §2's NFR-7 audit names all six `config.h` constants with the A7 justification (F8 fixed r2). |
| T11 | ✅ | 40 MHz in `ili9488_display.cpp:123`; the three measurements are physically consistent (F11). |

## 3. Findings

| # | Severity | Location | Finding | Status |
|---|---|---|---|---|
| F1 | Blocker | `firmware/system/main/app_main.cpp` | AC-3.1's per-anchor serial-log verification is not implemented. T7's DoD requires the first tick to log, per fixture anchor, its source colour, its mapped panel coordinate and the 18bpp bytes; T8 replaced `app_main.cpp` and dropped it without recording a deviation. **Confirmed fixed r2, on hardware by me, not from the fix description:** `app_main.cpp:55-68` defines `logFixtureAnchors()` (colour name, `x*kPanelScale`/`y*kPanelScale`, `toPanelPixel` wire bytes); `app_main.cpp:98` calls it under `if (i == 1)`, after that tick's push. I built, flashed `/dev/cu.usbmodem14101` and captured the log (pyserial, `dtr=False`/`rts=False`; no RESET press needed): exactly **12** `anchor engine=` lines, all 12 distinct, all immediately after `tick 1/21: push sent=150 failed=0`, none on any later tick; values match round 1's quotes and `fixture_pattern.h:92-105` anchor-for-anchor — `(5,5)`→`(10,10)` BLACK … `(239,159)`→`(478,318)` BLACK; wire bytes match `panel_format.cpp:9-16`. Ticks 2–21 unchanged (1 then 2 tiles). | fixed r2 |
| F2 | Major | `port/esp32/ili9488_display.h` | The public contract states init() "logs and reports every step's outcome but **never aborts itself**… the choice of *whether* to abort is left to the caller". It does abort — unconditionally, via three `ESP_ERROR_CHECK`s (`gpio_config`, `spi_bus_initialize`, `spi_bus_add_device`) that run *before* the first `logStep`. **Confirmed fixed r2:** the class contract (`ili9488_display.h:36-50`) and `init()`'s own comment (`:67-72`) now state the two-phase policy — bus/GPIO setup unconditionally fatal via `ESP_ERROR_CHECK`, panel command sequence logs-and-returns-false. I checked the description against the `.cpp`, not just the header: exactly three `ESP_ERROR_CHECK`s, on `gpio_config` (`:98`), `spi_bus_initialize` (`:114`) and `spi_bus_add_device` (`:127`), all before the first `logStep`; the six command steps at `:144-173` each `logStep(...)` and `return false`. Documentation-only, behaviour unchanged — nothing in the abort/return structure moved. | fixed r2 |
| F3 | Major | `tools/check_constraints.sh:247-263` (pre-fix) | The new GPIO rule ran its digit match against `grep -rn`'s whole output line, prefix included, and used `\b(9\|10\|…)\b`. Both directions reproduced: a file whose line 10 read `int spiDummy = 0;` (no pin literal at all) **failed** the gate on the `:10:` prefix, while `gpio_set_level(GPIO_NUM_10, 1);` in `port/esp32/` **passed** clean — ESP-IDF's canonical spelling has no word boundary after `_`. **Why it matters:** this is advertised as the first automation of a constitution §6 non-negotiable. **Fix applied:** match via `awk` on the prefix-stripped content with an `_`-tolerant boundary; all four probes now behave and `make lint` stays green on the clean tree. | fixed r1 |
| F4 | Major | `tools/check_constraints.sh:231-233` (pre-fix) | `PORT_DIR`, `SYSTEM_MAIN_DIR` and `DISPLAY_DRIVER_LITERAL_FILES` were introduced with no existence guard. With `firmware/steamcore/port/` moved away, `make lint` printed **`make lint OK` and exited 0** having scanned none of the three port-scoped rules. **Why it matters:** this is exactly the false-OK class prior review rounds F15 (directory guard) and F6 (per-file guard) already closed for every other check in this script. **Fix applied:** directory + file guards in the script's established style, re-probed. | fixed r1 |
| F5 | Major | `firmware/system/CMakeLists.txt` | The device build compiled **every** source, including the shared engine `.cpp`s, at `-std=gnu++2b` — read directly from `compile_commands.json`, not inferred. Constitution §3 fixes "C++17… one dialect for both the host tests and the target build", and plan.md's PLAN GATE asserted a pin that didn't exist. **Confirmed fixed r2, by my own inspection and an empirical probe:** `CMakeLists.txt:14` has `idf_build_set_property(CXX_COMPILE_OPTIONS "-std=gnu++17" APPEND)`. I parsed the regenerated `build/compile_commands.json` myself (mtime 21:30:46, after the `CMakeLists.txt` edit at 21:30:02) — `panel_format.cpp`, `framebuffer.cpp`, `app_main.cpp` and `ili9488_display.cpp` each carry `['-std=gnu++2b', '-std=gnu++17']` **in that order**, so the pin is last and wins. Flag order alone isn't proof, so I also re-ran `panel_format.cpp`'s exact command on a probe TU: `static_assert(__cplusplus == 201703L)` **passed** and `consteval` was rejected as "a keyword in C++20". The device build is genuinely C++17. | fixed r2 |
| F6 | Minor | `docs/device-build.md` (pre-fix) | T6's silent stack-overflow discovery (~80 KB of engine objects as `app_main` locals vs. `CONFIG_ESP_MAIN_TASK_STACK_SIZE=3584`) lived only in an `app_main.cpp` comment and one plan.md task row — not in the device-build doc, which is where the analogous RESET-button quirk is recorded so it isn't re-discovered. **Fix applied:** new "The silent-stack-overflow trap" section stating the symptom (boot log stops after `Calling app_main()`, no crash message) and the rule (`static`, not local). | fixed r1 |
| F7 | Minor | `firmware/system/main/CMakeLists.txt` (pre-fix) | Plan §5's mitigation for the two-build-systems risk requires "a comment in `main/CMakeLists.txt` states the rule"; the delivered file had none. **Fix applied:** comment added covering both the explicit-`SRCS` rule and why `INCLUDE_DIRS` reaches `firmware/`. `idf.py build` re-verified green. | fixed r1 |
| F8 | Minor | `.spark/display-driver/plan.md` §2 | §2's NFR-7 audit — the one T10 corrected "symbol by symbol" — still omitted the six new consumer-visible constants in `config.h`. They are spec-sanctioned (A7) and the surface is intentional, so this was audit accuracy, not surface bloat. **Confirmed fixed r2:** `plan.md:116-118` now lists all six by name — `kPanelScale`, `kPanelWidth`, `kPanelHeight`, `kPanelTileSize`, `kPanelBytesPerPixel`, `kPanelTileBytes` — as "public, per A7 (Decision 5) and correctly not flagged as drift". Cross-checked against `config.h:30-36`: six constants, same names, nothing else new. | fixed r2 |
| F9 | Minor | `firmware/steamcore/include/steamcore/tile_pusher.h` | `static_assert(sizeof(buffer_) == kPanelTileBytes)` was a tautology — `buffer_` is *declared* that size, so it could never fire — and its message claimed something stronger ("and nothing else") that it didn't check. **Confirmed fixed r2:** `tile_pusher.h` has no `static_assert` left at all (`:91-94` is a comment explaining why it moved); `tile_pusher_test.cpp:52-55` asserts `sizeof(TilePusher<FakeTransmitter>) == kPanelTileBytes` — the whole object, against a concrete instantiation, so padding or an added member would now fire. I re-ran the host suite myself after `make clean`: 124 passed, 0 failed. | fixed r2 |
| F10 | Minor | `port/esp32/ili9488_display.cpp:212-215` | `pushDirty` emits an unconditional `ESP_LOGI` per push. NFR-9 requires it and the harness ticks at 2.5 Hz, so nothing is wrong today — but at the 60 Hz this driver exists to serve that is 60 blocking UART writes/s sitting on the display path. **Fix:** before a real game ships, drop the steady-state line to `ESP_LOGD` (or log only when `failed > 0`, plus a periodic summary) and record the decision. | open |
| F11 | Minor | `.spark/display-driver/plan.md` T6/T11 vs spec NFR-1 | The recorded per-tile times (3386 µs @10 MHz, 2147.9 @20, 1533.4 @40) fit a fixed ~919–928 µs overhead plus exact data time (3072·8/clock) to within 0.3% — strong corroboration that the transcripts are real measurements, not invented. They also mean a 150-dirty-tile frame at 40 MHz takes ~230 ms (~4.3 fps), not the ~92 ms full-frame time constitution §3 documents — so NFR-1's rationale that the all-150 case "is mathematically identical to a full-frame push at the same clock" is false as measured. No Must AC depends on it. **Fix:** `/demo-day` records the measured all-150 figure against NFR-1 instead of repeating "identical"; the ~919 µs is 5 small transactions per tile and is where a future perf pass starts. | open |
| F12 | Minor | `.spark/constitution.md` §3, §4 | Both still say the production display driver "remains future planned work" / "is still unbuilt" — falsified by this diff. **Fix:** `/charter` amendment at `/go-live`, in the same row as the §3 hardware updates. | open |
| F13 | Nit | `README.md:608, 625-629` | Phase-2 note still says the real display driver "ist noch offen". Consistent with prior increments (Game Loop / Text Rendering are also still unchecked despite shipping), so not a regression this feature introduced. **Fix:** one-line refresh at `/go-live`. | open |
| F14 | Nit | `firmware/steamcore/include/steamcore/board_config.h:9, 18` | Comments still describe MISO as "unused by the write-only bring-up test" and the DMA push as something the bus "will carry once the real display driver exists". **Fix:** one-line comment refresh. | open |
| F15 | Minor | `firmware/system/main/app_main.cpp:55-68` (new, round 2) | The restored anchor log is derived entirely from the `kFixtureAnchors` **constants** — it never reads `fb`. It therefore cannot fail: it would print the same 12 lines onto a blank panel. What it does prove on-device is the mapping (`x*kPanelScale`, `toPanelPixel`); what AC-3.1's wording asks ("confirms … each landed at their exact expected coordinate and colour") is what the framebuffer actually held. **Why it's Minor, not Major:** plan T7's approved DoD defines the log exactly this way ("its *source* colour … the 18bpp bytes"), and the anchors are separately asserted bit-exactly against a real `Framebuffer` host-side (`dump_format_test.cpp`, round-trip test), so no Must AC is unverified in aggregate — only the device-specific slice is. **Fix:** log the actual pixel too — `fb.pixel(anchor.x, anchor.y)` already exists and is used in `harness_consumer.h:70` — and print a match/mismatch flag; decide at `/increment` whether a mismatch should `ESP_LOGE` or halt. | open |

## 4. Requirements Traceability

| Spec ID | Implemented at | Verdict |
|---|---|---|
| AC-1.1 | `src/panel_format.cpp:5-19`, `test/panel_format_test.cpp:50-96` | ✅ met |
| AC-1.2 / AC-2.3 | `Makefile:ENGINE_SRCS` globs `src/*.cpp` only; `-I` is `include/` only — `port/` is structurally invisible to the host build | ✅ met |
| AC-1.3 | `test/panel_format_test.cpp:100-112` | ✅ met |
| AC-2.1 | `src/panel_format.cpp:21-25`; `test/panel_format_test.cpp:116,137` (coverage array, mutation-verified) | ✅ met |
| AC-2.2 | `tools/check_constraints.sh` scale-factor + resolution rules; no `*2`/`2*` in the math | ✅ met |
| AC-3.1 | `app_main.cpp:55-68, 94-98`; on-device log re-captured this round: `sent=150 failed=0` + 12 anchor lines | ✅ met r2 (F15: the log restates constants) |
| AC-3.2 | `harness_consumer.h:26-54`, `app_main.cpp:49-56` — counts traced 150/1/2…, 20 distinct tiles | ✅ met |
| AC-3.3 | `tile_pusher.h:70` (one window, `kPanelTileBytes`, per dirty tile); `ili9488_display.cpp:195`; lint call-site rule | ✅ met |
| AC-3.4 | `tile_pusher.h:89` single fixed member buffer; alloc lint over `include/`+`src/`+`port/` | ✅ met |
| AC-3.5 | `app_main.cpp:47,50-51`; `game_loop.h` absent from the diff | ✅ met |
| AC-4.1 / 4.2 / 4.3 | `tile_pusher.h:58-81`; `test/tile_pusher_test.cpp:85,106,130` — mutation-verified | ✅ met |
| AC-4.4 | `ili9488_display.cpp:83-86,144-173` — command sequence logs and returns; the 3 setup aborts are now documented as such, and A6 allows "an abort at this stage… but never silent" | ✅ met r2 |
| AC-5.1 | `ili9488_display.cpp:117-123` (40 MHz + rationale); plan T11 transcript | ✅ met (qa.md record owed to `/demo-day`) |
| NFR-1 | `ili9488_display.cpp:206-215` | ⚠️ measured; premise wrong (F11) |
| NFR-2 / NFR-3 / NFR-5 / NFR-6 | fixed buffers only; no abort path in `TilePusher`; no clock/RNG in the math; `board_config.h` reused, no new pin literal | ✅ met |
| NFR-4 | host gate compiles the pure half ESP-IDF-free; `port/` excluded by construction; both builds now C++17, probe-verified | ✅ met r2 |
| NFR-7 | `panel_format.h`, `tile_pusher.h`, `port/esp32/ili9488_display.h` + `config.h`'s six constants, all audited in `plan.md` §2 | ✅ met r2 |
| NFR-8 | all three headers carry contract + example; the `init()` contract now matches the code | ✅ met r2 |
| NFR-9 | `ili9488_display.cpp:138,177,186-198,212` + `app_main.cpp:95-98`; anchor detail present and re-observed on hardware | ✅ met r2 |
| NFR-10 / NFR-11 | N/A per spec | ✅ |

**Library lens.** *Public API surface:* every new export is intentional and now
named in the plan's audit (F8 fixed r2); nothing leaked that should be internal —
`SpiTransmitter` is private-nested, `HarnessConsumer` and `fixture_pattern.h` stay
test/app-side. *Compatibility:* additions only; the one removal
(`steamcore::bringup`) was an out-of-loop spike in `firmware/system/`, never engine
API, so no deprecation path is owed. *Packaging:* no new dependency; `REQUIRES
driver esp_timer` is minimal. *Contract clarity:* strong overall — F2 is the one
place a doc comment states the opposite of the code.

## 5. What Was Checked

- [x] Correctness: logic does what the acceptance criteria demand — AC-3.1's log half re-observed on hardware this round (F15 bounds what it proves)
- [x] Non-functional: applicable NFRs and constitution quality bars hold — the C++17 dialect is now probe-verified on the device toolchain, not just flag-inspected
- [x] Error handling: failures handled, not swallowed (`sendCommand`/`sendData`/`transmitTile` return values are all checked); the `init()` abort policy is deliberate and documented
- [x] Security: N/A by profile — offline device, no external input, no secrets
- [x] Tests: exist, are meaningful, and pass — `make clean && make test-all` re-run by me this round: 124/124, benches, Python/round-trip, 17 lint blocks, all green; 2 independent mutants (round 1) confirmed the suite is not tautological
- [x] Readability: doc comments are unusually good; no dead code; no copy-paste

## 6. Verdict

**Passed.** All five round-1 fix claims hold up, and the two I was least willing to
take on trust I re-derived from the hardware and the toolchain rather than from the
fix descriptions. For F1 I built, flashed and captured the serial log myself: the
harness emits exactly twelve `anchor engine=…` lines, all twelve distinct, all
immediately after `tick 1/21: push sent=150 failed=0` and on no later tick, with
engine coordinate, panel coordinate, colour name and 18bpp wire bytes matching
`fixture_pattern.h`'s table and `panel_format.cpp`'s palette anchor for anchor —
`(5,5)`→`(10,10)` BLACK through `(239,159)`→`(478,318)` BLACK — and the remaining
twenty ticks unchanged at 1 then 2 tiles. For F5, flag presence was not enough:
`compile_commands.json` (regenerated after the `CMakeLists.txt` edit, confirmed by
mtime) shows `-std=gnu++17` following `-std=gnu++2b` for `panel_format.cpp`,
`framebuffer.cpp`, `ili9488_display.cpp` and `app_main.cpp`, and re-running
`panel_format.cpp`'s own compile command on a probe TU proved the dialect actually
in force — `__cplusplus == 201703L` passed, `consteval` was rejected as C++20. So
constitution §3's one-dialect rule genuinely holds on both sides now. F2 is a
documentation fix that I checked against the `.cpp` rather than the header alone
(three `ESP_ERROR_CHECK`s before the first `logStep`, six command steps that log and
return false); F8's audit names all six constants; F9's tautology is gone and the
replacement assert is on the whole object, with the host suite re-run green from
clean (124/124, 17 lint blocks). One new Minor came out of the F1 re-verification
and is worth naming rather than burying: the restored log is built from the anchor
*constants* and never reads the framebuffer, so it cannot fail — it proves the
mapping on-device but not what was drawn (F15). The approved T7 DoD specifies it
that way and the anchors are asserted bit-exactly against a real `Framebuffer` in
the host suite, so no Must AC is left unverified in aggregate and this does not
block the gate; it is a one-line improvement worth making before someone reads the
log as proof the panel is correct. F10–F15 stay open as judgment calls for the
user. `/demo-day` may start.

---

## ✅ REVIEW GATE

*All boxes checked → `/demo-day` may start. Any box open → back to `/increment`. On
re-review, edit this same checklist in place — never duplicate it as a second gate.*

- [x] No open Blocker findings — F1 confirmed `fixed r2` on real hardware by the reviewer
- [x] No open Major findings (or explicitly waived by the user, with reason recorded here) — F2, F5 confirmed `fixed r2`; F3/F4 fixed by the reviewer round 1. No waiver was needed or used.
- [x] Every Must AC traces to implementing code; no constitution non-negotiable violated — AC-3.1 ✅ (log re-observed on device; F15 bounds it, Minor); constitution §3's C++17 dialect ✅ (probe-verified). §6's non-negotiables (no dynamic allocation, no full-frame push, no GPIO literal, graphics philosophy) all hold.
- [x] All plan deviations documented and accepted — the three named in plan.md are; the two undocumented ones from round 1 (T8's dropped anchor logging, the missing `-std=gnu++17` pin) are now closed in code, not merely explained
- [x] Test suite runs green — `make clean && make test-all` re-run by the reviewer this round: 124/124 C++, benches, Python/round-trip, 17 lint blocks, `make lint OK`; `idf.py build` green; flashed and run on `/dev/cu.usbmodem14101`
- [x] Line budget respected: Ist 165 / Soll ~150 (excluding HTML comments) — 15 over; reason: 15 findings, each a single row, plus the library-lens paragraph §4 owes the active lens; round 2's added evidence sits inside existing rows rather than new sections
- [x] Status set to `passed`
