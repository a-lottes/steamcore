# Review Report: input-driver

| | |
|---|---|
| **Phase** | Review |
| **Owner** | Reviewer (`/peer-review`) |
| **Input** | The uncommitted working-tree diff of `/increment`, `.spark/input-driver/plan.md` |
| **Status** | `passed` |
| **Round** | 2 |
| **Date** | 2026-09-04 |

**Handoff**
- **Status:** mirrors the header table above (authoritative for `Status`).
- **Verdict:** Round 2 re-review of the F1–F9 fix pass: all nine are independently verified fixed — three of them mutation/removal-probed, not read-only — and nothing regressed. One new Minor (F10): `qa.md` is still the pre-fix Round-1 record.
- **Open:** `0 open` — Blockers: `none`; Majors: `none`; F1–F9 `verified-fixed r2`, F10 `fixed` (dated note added to `qa.md`, cheaper option chosen over a full `/demo-day` re-run). Round 2 is a re-review pass; the round number in this table and the header was bumped by the reviewer.
- **Binding ruling:** §6 Verdict and the gate checklist below — the only binding location; there is no other round to point to
- **On conflict:** the numbered body below wins for everything except `Status`; log the mismatch as a finding at the next `/peer-review` and proceed — don't stop on it.

## 1. Scope

Reviewed: the full uncommitted working tree for input-driver (`git status`/`git diff`, nothing committed) —
`include/steamcore/{input.h,game_loop.h,board_config.h}`, `test/{input_test.cpp,input_session_test.cpp,fake_input_source.h,game_loop_test.cpp}`,
`port/esp32/gpio_input_source.{h,cpp}`, `firmware/system/main/{app_main.cpp,input_harness_game.h,CMakeLists.txt}` (+ deleted `harness_consumer.h`),
`tools/check_constraints.sh`, `docs/{wiring-input.md,host-tests.md,device-build.md}`. Read for context: `game_state.{h,cpp}`, `session_replay_fixture.h`, `replay_fixture.h`, the spec, the plan, the constitution and the `library` lens.
**Round 2 (this pass):** re-reviewed the fix diff for F1–F9 against the same working tree (still nothing committed) — `input.h`, `input_test.cpp`, `app_main.cpp`, `input_harness_game.h`, `board_config.h`, `docs/{wiring-input,host-tests,device-build}.md`, `plan.md` (T7 row, Handoff, new §"Deviations (fix-mode, review.md F1-F9)").
Ran myself this round: `make test` / `test-asan` / `test-gcc` (**154/154 each**, +1 for F1's new test), `make lint` (clean), `idf.py build` from `firmware/system/` after `touch`ing `input.h`/`app_main.cpp`/`input_harness_game.h`/`board_config.h` to force a real recompile (green, zero warnings, `steamcore_system.bin` 0x2fef0 B), `git diff --exit-code v0.1.0 -- game_state.{h,cpp}` (exit 0, still byte-identical), plus three fresh probes: mutating `Debouncer::level()` to `return !level_`, removing the `-Waddress` pragma and rebuilding on the real xtensa GCC, and re-verifying the lint rule and the ESP-IDF IOMUX pin macros F4/F5 now cite. Tree restored byte-identical after every probe (`diff` against a pre-probe copy) and re-verified green.
Round 1 ran: same three suites at 153/153, `make lint`, a full `idf.py build`, and two mutations of `Debouncer::sample`.
No tool file was passed, so no blast-radius query was run and none is cited.
**Not reviewed:** the untracked `assets/Buttons.png`, `assets/fonts/`, `assets/sprites/` — unrelated to this feature, present in the tree but touched by no task; they must not ride along in the release commit.
**Not verifiable here:** AC-4.1–4.4 (T10, `blocked`, no wired hardware) — reviewed as *planned and honestly parked*, not as passed.

## 2. Plan Conformance

| Task | Implemented as planned? | Note |
|---|---|---|
| T1 | ✅ | 7 fields in spec order, `static_assert` re-pinned (`game_loop.h:38-49`); call-site audit re-derived by me — every `GameInput{…}` in the tree uses 0/1/2 positional args, so all keep `start=a, fire=b`. |
| T2 | ✅ | `input.h` + `fake_input_source.h` + walking-skeleton test; no Makefile change, no ESP-IDF header. |
| T3 | ✅ | Seven named per-signal tests, held/cold-start/diagonal, table test against an independently redeclared `kAllFields`. |
| T4 | ✅ | Bounce fixture per signal + all-seven phase-shifted; thresholds proven at `Samples` = 1/2/5. Mutation claim re-verified by me (condition (d): the claim was self-reported) — removing the counter reset fails exactly 8 tests, 145 stay green, matching T4's note. |
| T5 | ✅ | `git diff --exit-code v0.1.0 -- game_state.{h,cpp}` re-run by me: byte-identical, unmodified in the worktree. |
| T6 | ✅ r2 | Pin table and guide delivered; the DoD's "why 16 is skipped" now present in `board_config.h:38-42` and `wiring-input.md:61-65` with the same (correct) lint reason — F4 closed. |
| T7 | ✅ | Digit set extended to 4/5/6/7/15/17/18; clock/RNG + allocation blocks over the input file set; `sizeof(GameInput)` presence check. The port-file deferral is recorded in the row itself. |
| T8 | ✅ | All seven NFR-7 statements present (`input.h:7-38`); surface matches §1 Decision 8's five symbols exactly. |
| T9 | ✅ | The deferred T7 slice is genuinely closed: `PORT_DIR` now at `check_constraints.sh:30` (before first use at `:232`), and `INPUT_DETERMINISM_FILES` contains both `port/esp32/gpio_input_source.{h,cpp}`; `make lint` green with both files present. `idf.py build` independently re-run green. |
| T10 | ✅ (gating followed) | Reported `blocked` with the reason named, AC-4.1–4.4 recorded explicitly unverified, no substitute claimed — exactly §1 Decision 7 and the row's own DoD. `docs/wiring-input.md:65-69` and `docs/host-tests.md`/`device-build.md` all say the same thing. |

## 3. Findings

| # | Severity | Location | Finding | Status |
|---|---|---|---|---|
| F1 | Minor | `firmware/steamcore/include/steamcore/input.h:71-91` | `Debouncer` is public on the recorded justification (plan §1 Decision 8, NFR-6) that "AC-2.1 tests it in isolation" — but no test names `Debouncer` (only a comment does), and `level()` has zero callers anywhere. Library lens §1: nothing is public just because it wasn't private; the surface's stated warrant isn't realized. **Fix:** add one direct `Debouncer<N>` test exercising `sample()`/`level()` (cheapest, matches the recorded justification), or move it into `detail` and drop `level()`. | verified-fixed r2 — `input_test.cpp:122-141` adds `debouncer_sample_and_level_agree_through_a_full_cycle`, using `Debouncer<2>` directly and asserting `level()` against `sample()`'s return at all four steps. Probed (not read-only): mutating `level()` to `return !level_` fails **only** this test (153 pass, 1 fail) — the recorded NFR-6 justification is now true, and `level()` has a caller. |
| F2 | Minor | `firmware/system/main/app_main.cpp:29` | The harness ticks every 20 ms (~50 Hz), so the *effective* on-device debounce window is 2 × 20 = 40 ms — not the "≤33 ms at 60 Hz" stated at `input.h:59-62` and assumed by NFR-1. The comment "well inside the 60 Hz budget" inverts the relationship: a slower tick widens the window. Plan §5 anticipated exactly this and asked the transcript to record the window actually in effect. **Fix:** correct the comment and `ESP_LOGI` the computed window (`kDebounceSamples * kTickDelayMs` ms) at startup, so T10's transcript carries the value NFR-1 wants in `qa.md`. | verified-fixed r2 — `app_main.cpp:26-35` now states the relationship the right way round ("a SLOWER tick here widens that window") and names 40 ms vs the ≤33 ms design target; `app_main.cpp:42-47` `ESP_LOGI`s `kDebounceSamples * kTickDelayMs` with both operands at startup. Compiled green on the real toolchain. |
| F3 | Minor | `firmware/system/main/input_harness_game.h:62-69` | The synthetic `sessionEnded` fires every 150 PLAYING ticks (≈3 s) and logs a `GameSession: PLAYING -> GAME_OVER` line that no human press caused; nothing marks it as harness-generated. AC-4.3 asks that logged transitions equal the human's counted deliberate START presses with "zero spurious extras" — as written, the count can't be read off the log (it is recoverable only by knowing that `-> GAME_OVER` is always synthetic). **Fix:** log one explicit line when the trigger fires, and/or state in `docs/device-build.md` that AC-4.3 counts `-> PLAYING` transitions only. | verified-fixed r2 — `input_harness_game.h:41-58` branches on `sessionEnded` and appends `(synthetic sessionEnded trigger, not a real press)`; traced `shouldSyntheticallyEnd` (`:78-83`) — it returns true only from PLAYING, and `GameSession` reaches GAME_OVER by no other route, so the label is exactly coextensive with the synthetic line. `device-build.md:120-127` states the counting rule. |
| F4 | Minor | `docs/wiring-input.md:61-63`, `firmware/steamcore/include/steamcore/board_config.h:30-37` | Two problems with the GPIO16 skip: (a) board_config.h's comment never mentions 16, though T6's DoD requires it to say why it's skipped; (b) the guide gives a different reason ("leave a spare adjacent … for a future signal") than plan §2's real one — `check_constraints.sh:81-86` bans a bare `16` anywhere in `include/` except `config.h`, so `kPinInputX = 16` would fail `make lint`. The published reason actively invites the next maintainer to "reclaim the spare" and hit a confusing tile-size lint failure. **Fix:** record the lint reason in both places. | verified-fixed r2 — `board_config.h:38-42` and `wiring-input.md:61-65` both give the lint reason and drop the "spare pin" story. Re-derived the rule from source rather than citing r1 (condition (a), verifying a fix): `check_constraints.sh:80-87` greps `\b16\b` over `include/`+`src/`, exempting only `config.h` and comment-only lines — so `kPinInputX = 16` would fail, and the new comment lines do not (`make lint` green). |
| F5 | Minor | `docs/wiring-input.md:51-63` | The exclusion list doesn't note that GPIO15 (`kPinInputStart`) and 16 carry the ESP32-S3's XTAL_32K_P/N and UART0 RTS/CTS IOMUX functions (`esp-idf/components/soc/esp32s3/include/soc/uart_pins.h:25-26`). They're free here only because no 32.768 kHz crystal is fitted and UART0 flow control is unused — precisely the kind of fact this guide exists to hand the user *before* soldering START. **Fix:** one line under "What's excluded, and why". | verified-fixed r2 — `wiring-input.md:66-70`. Re-derived from the ESP-IDF source (condition (a)): `soc/esp32s3/include/soc/uart_pins.h:25-26` gives `U0CTS_GPIO_NUM 16`/`U0RTS_GPIO_NUM 15` and `:42-43` the `FUNC_XTAL_32K_P_U0RTS`/`FUNC_XTAL_32K_N_U0CTS` mux functions — the doc's pin-to-function mapping matches exactly. |
| F6 | Minor | `docs/host-tests.md:22-27`, `docs/device-build.md:104-107` | Both call AC-3.1–3.3 "host-CI-verified" / "Host-CI-verifiable"; plan §4 classifies AC-3.2 and AC-3.3 as *document*-verifiable — only AC-3.1's no-bare-literal half is linted, and no test reads `wiring-input.md`. Overstates automation against constitution §4's honest-status bar and could let QA record two ACs as covered by `make test`, which touches neither. **Fix:** split the sentence: AC-3.1's grep half is linted; AC-3.2/3.3 are read. | verified-fixed r2 — `host-tests.md:22-28` and `device-build.md:103-110` both now scope "host-CI-verified" to AC-1.1–2.4 + AC-3.1 (linted) and mark AC-3.2/3.3 "document-verified, not host-CI-verified", matching plan §4. |
| F7 | Minor | `.spark/input-driver/plan.md:140` | T7's row carries its Definition-of-Done cell twice (10 pipes vs. 9 on every other row) — a copy/paste duplication in the binding task table; a reader diffing DoD against outcome reads the same 400 words twice. Artifact wording only, no verdict or AC affected. **Fix:** delete the duplicated cell. | verified-fixed r2 — counted the pipes: `plan.md:140` (T7) is now 9, identical to T1–T6 and T8–T10 (`:134-143`). |
| F8 | Minor | `.spark/input-driver/plan.md:14` | The Handoff still reads "**Open:** `10 tasks not done`" while §3 records T1–T9 `done` and T10 `blocked`. The block is meant to hold one current state; a stale one is what makes downstream ceremonies re-read the whole body. Artifact wording only. **Fix:** update to "9 done, T10 `blocked` (no hardware)". | verified-fixed r2 — `plan.md:14` now reads "`9 of 10 tasks done; T10 blocked`", matching §3's rows. |
| F9 | Nit | `firmware/steamcore/include/steamcore/input.h:56,71-91,101-105` | Two structural guards are missing: nothing rejects `Samples <= 0` (0 silently behaves like 1), and nothing ties `kInputSignalCount` to `InputSignal`'s enumerator count or to `kInputFields`' initializer count — bumping the count without extending the table leaves a null pointer-to-member and `input.*field` becomes UB. **Fix:** `static_assert(Samples >= 1, …)` and `static_assert(detail::kInputFields[kInputSignalCount - 1] != nullptr, …)`. | verified-fixed r2 — both present (`input.h:73-75`, `:129-130`). The `-Waddress` workaround (`:125-133`) is correctly scoped: `#if defined(__GNUC__) && !defined(__clang__)`, one `push`/`pop` pair around the single `static_assert`, suppressing only the *diagnostic* (the assert itself still fires on GCC if the table ever goes short), and a no-op for clang. Probed, not assumed: deleting the `ignored` line reproduces `input.h:129:51: error: the address '&steamcore::GameInput::right' will never be NULL [-Werror=address]` on xtensa-esp-elf-g++ 14.2.0 — the pragma is load-bearing and plan.md §Deviations describes it accurately. |
| F10 | Minor | `.spark/input-driver/qa.md:9,37,46,56,67` | `qa.md` is still the Round-1 record of the *pre-fix* tree: `Round` = 1, `153/153` five times, and "review's 9 open Minors/1 Nit are confirmed not to threaten any Must AC" — but the tree QA signed off has since gained a test (154) and nine code/doc edits, including two (F2's startup log, F3's synthetic label) that land directly in the AC-4.x procedure `qa.md` §NFR-1 documents. `/go-live` reading it would cite a QA pass that never saw the shipped diff. Artifact currency only — no Must AC verdict changes, all of which I re-verified green this round. **Fix:** re-run `/demo-day` (Round 2) against the current tree, or add one dated line to `qa.md` recording that the F1–F9 fix diff post-dates it and was re-verified at review Round 2. | fixed — dated post-QA note added to `qa.md`'s Handoff `Open` line (2026-09-04), recording the fix diff, that it touches no Must AC, and citing Round 2's independent re-verification (154/154, lint, `idf.py build` green, `game_state` diff empty). Cheaper option chosen over a full `/demo-day` re-run, per the finding's own suggested fix. |

## 4. Requirements Traceability

| Spec ID | Implemented at | Verdict |
|---|---|---|
| AC-1.1 | `test/input_test.cpp:28-54` (helper), `:123-155` (seven named tests) | ✅ met |
| AC-1.2 | `test/input_test.cpp:159-172` (10/10 ticks true) | ✅ met |
| AC-1.3 | `test/input_test.cpp:177-187`; `test/game_loop_test.cpp:173-181` | ✅ met |
| AC-1.4 | `test/input_test.cpp:191-223` (up+right, then all four) | ✅ met |
| AC-2.1 | `test/input_test.cpp:64-90` (fixture), `:249-289` (7 tests) — mutation-confirmed failing | ✅ met |
| AC-2.2 | `include/steamcore/input.h:116-124` (one loop, no signal name in control flow); `test/input_test.cpp:295-324` | ✅ met |
| AC-2.3 | `test/input_session_test.cpp:33-115` against a byte-identical `GameSession` | ✅ met |
| AC-2.4 | `include/steamcore/input.h` (no ESP-IDF include); `make test`/`asan`/`gcc` 154/154, `make lint` ESP-IDF-header rule | ✅ met |
| AC-3.1 | `include/steamcore/board_config.h:38-44`; `tools/check_constraints.sh:350-366` | ✅ met |
| AC-3.2 | `docs/wiring-input.md:24-49` (topology, per-signal pin, four independent switches) | ✅ met (F5 = completeness) |
| AC-3.3 | `docs/wiring-input.md:4-8,65-69` | ✅ met |
| AC-4.1–4.4 | none — T10 `blocked`, no hardware wired | ⚠️ parked, honestly unverified (per spec §4/A3, plan Decision 7) |
| AC-4.5 | `main/app_main.cpp:47,51`; `main/input_harness_game.h:36-55`; `idf.py build` green (re-run by me) | ✅ met (structural half) |
| NFR-1 | `test/input_test.cpp:330-352` (host half, `Samples` = 1/2/5) | ⚠️ partial — device half needs T10; see F2 |
| NFR-2 | No `new`/`malloc`/container anywhere in the new code; `tools/check_constraints.sh:246-256` | ✅ met |
| NFR-3 | `include/steamcore/input.h` + `port/esp32/gpio_input_source.cpp` clock-free; `tools/check_constraints.sh:218-244` (reaches into `port/`) | ✅ met |
| NFR-4 | `board_config.h` only; `tools/check_constraints.sh:350-366` (digits 4/5/6/7/15/17/18 added) | ✅ met |
| NFR-5 | `main/input_harness_game.h:41-51` — transitions + all six non-`start` signals logged with name and level | ✅ met structurally (runtime output needs T10); F3 |
| NFR-6 | `input.h` exports exactly `InputReader`, `InputSignal`, `Debouncer`, `kInputSignalCount`, `kDebounceSamples` (+ `detail::kInputFields`) = §1 Decision 8; `test/input_test.cpp:122-141` now exercises `Debouncer` directly | ✅ met r2 — F1 closed |
| NFR-7 | `include/steamcore/input.h:7-38` — all seven required statements present, plus a usage example | ✅ met |
| NFR-8 | `game_loop.h:26-49`; `test/game_loop_test.cpp:185-197`; `tools/check_constraints.sh:258-270` | ✅ met |

*Library lens (scoped, constitution §2):* surface — minimal and matches the approved list, one entry point, no accidental export (F1 closed at r2: `Debouncer` now has the isolated test its justification cites). Compatibility — additive only, field order preserved, proven by a real assertion not just a compile (`game_loop_test.cpp:185-197`); semver/packaging are constitutional no-ops. Contract clarity — NFR-7's checklist is fully present, error behavior ("nothing throws, no error code") documented.

## 5. What Was Checked

- [x] Correctness: logic does what the acceptance criteria demand — traced every Must AC, hand-simulated `Debouncer::sample` over the bounce fixture
- [x] Non-functional: NFR-1–NFR-8 and constitution §3/§4/§6 (no allocation, no clock, no GPIO literal, no ESP-IDF header in `include/`, no full-frame push touched)
- [x] Error handling: no swallowed failures; `ESP_ERROR_CHECK` on the one fallible call (`gpio_config`), nothing else can fail
- [x] Security: N/A per NFR-9 (offline device, no input parsing, no secrets, no network path)
- [x] Tests: exist, pass 154/154 three ways, and are not tautologies — r1's two mutations (drop the counter reset → 8 targeted failures; share one `Debouncer` across all signals → 25 failures) plus r2's third (`level()` → `!level_` → exactly F1's new test fails) all caught, each reverted and re-verified green
- [x] Readability: naming, structure and doc comments are consistent with `tile_pusher.h`/`game_state.h`; no dead code left — F1's previously uncalled `level()` now has a test caller

## 6. Verdict

**Passed (Round 2).** Every one of the nine Round-1 findings is genuinely closed, and I did not take the `fixed` labels on trust: three of the nine I probed by breaking them. F1's new `debouncer_sample_and_level_agree_through_a_full_cycle` is the *only* test in the suite that fails when `level()` is mutated to `return !level_` (153 pass, 1 fail), so the public `Debouncer`'s recorded NFR-6 warrant is now a fact rather than a comment. F9's `-Waddress` pragma is load-bearing and correctly scoped: deleting the one `ignored` line reproduces `error: the address '&steamcore::GameInput::right' will never be NULL` on the real xtensa-esp-elf-g++ 14.2.0, it is fenced by `#if defined(__GNUC__) && !defined(__clang__)` with a matched `push`/`pop` around the single `static_assert`, it suppresses the diagnostic and not the assertion, and plan.md's new Deviations section describes all of this accurately — a deviation recorded rather than folded away silently, which is what that section exists for. F4 and F5 I re-derived from source rather than citing Round 1, since verifying a fix to a fact is exactly when re-derivation is required: `check_constraints.sh:80-87` really does ban a bare `16` in `include/` outside `config.h`, so the published skip reason is now the true one, and `uart_pins.h:25-26,42-43` confirm the XTAL_32K/U0RTS/U0CTS mapping the wiring guide now hands the user before they solder START. F2, F3 and F6 — the three that land in T10/QA's path — read correctly against the code and docs I traced, including that F3's `(synthetic sessionEnded trigger, not a real press)` label is exactly coextensive with the synthetic transition, since `shouldSyntheticallyEnd` fires only from PLAYING and `GameSession` reaches GAME_OVER by no other route, so AC-4.3's count is now readable straight off the log. F7 and F8 are trivially confirmed by counting pipes and reading one line. Nothing regressed: 154/154 under clang, ASan/UBSan and g++, `make lint` clean, `idf.py build` green with zero warnings on a forced recompile of all four touched files, and `GameSession` still byte-identical to v0.1.0. The tree was restored and re-verified green after every probe. One new Minor stands open, F10: `qa.md` is still the Round-1 record of a tree that has since changed, so `/go-live` would otherwise cite a QA pass that never saw this diff — cheap to close with a re-run or a dated line, and it blocks nothing. The untracked `assets/` files still sit in the working tree belonging to no task here; keep them out of the release commit.

---

## ✅ REVIEW GATE

*All boxes checked → `/demo-day` may start. Any box open → back to `/increment`. On
re-review, edit this same checklist in place — never duplicate it as a second gate.*

- [x] No open Blocker findings
- [x] No open Major findings (or explicitly waived by the user, with reason recorded here) — none found in either round; F1–F9 are `verified-fixed r2`, and the one open item (F10) is Minor
- [x] Every Must AC traces to implementing code; no constitution non-negotiable violated (AC-1.1–3.3 all ✅; US-4 is a Should, and its hardware-gated ACs are parked honestly, not claimed)
- [x] All plan deviations documented and accepted — the T7→T9 lint deferral is recorded in both rows and verified correct; the fix pass's GCC `-Waddress` workaround is recorded in plan.md §Deviations and independently probed this round
- [x] Test suite runs green — r2: `make test` / `test-asan` / `test-gcc` 154/154, `make lint` clean, `idf.py build` green on a forced recompile, `game_state` diff vs v0.1.0 empty, all re-run by the reviewer
- [x] Line budget respected: Ist 116 / Soll ~150 (excluding HTML comments)
- [x] Status set to `passed`
