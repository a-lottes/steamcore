# Review Report: start-screen

| | |
|---|---|
| **Phase** | Review |
| **Owner** | Reviewer (`/peer-review`) |
| **Input** | The uncommitted working-tree diff of `/increment`, `.spark/start-screen/plan.md` |
| **Status** | `passed` |
| **Round** | 2 |
| **Date** | 2026-09-05 |

**Handoff**
- **Status:** mirrors the header table above (authoritative for `Status`).
- **Verdict:** `passed` at round 2. All nine round-1 findings independently re-verified in the working tree — not taken from their `fixed` labels. `fixed r2` in §3 means *the reviewer reproduced the fix this round*; `fixed r1` means confirmed when it was made. One new Minor (F10) was found and reviewer-fixed.
- **Open:** `0 open` — Blockers: none; Majors: none (F1, F2 `fixed r2`); Minors/Nits: F3 `fixed r1`, F4-F9 `fixed r2`, F10 `fixed r2`. Nothing awaits `/increment`; `/demo-day` may start.
- **Binding ruling:** §6 Verdict and the gate checklist below — the only binding location.
- **On conflict:** the numbered body below wins for everything except `Status`; log the mismatch as a finding at the next `/peer-review` and proceed — don't stop on it.

## 1. Scope

Round 2 re-review of the fixes to round 1's nine findings, over the same uncommitted working tree
against `HEAD` (`c164c75`): 8 modified files (`Makefile`, `docs/{device-build,dump-format,host-tests}.md`,
`firmware/steamcore/src/font.cpp`, `firmware/system/main/{app_main.cpp,CMakeLists.txt}`,
`tools/check_constraints.sh`) and 10 new files (`title_screen.{h,cpp}`, 6 test/fixture/bench files,
`title_screen_harness_game.h`, `scfb_capture.py` + `test_scfb_capture.py`). `git status` is unchanged
from round 1 — no file appeared or vanished. Since "fixed code is new code", every fix was
re-derived from the working tree rather than read off its `fixed` label (condition (a): this round
verifies fixes to those very facts). Re-ran myself: `make test` / `test-asan` / `test-gcc`
(**171/171 each**), `make lint` (clean), `make test-python` (**31/31**), `make bench`
(5.50 µs/call), and `idf.py build` (**green**, after `touch`ing `app_main.cpp`,
`gpio_input_source.cpp`, `font.cpp`, `title_screen.cpp` so no cached object could stand in).
Four mutation probes were run and reverted (F1, F6, F7, F9) — details in §3.

**Not reviewed:** the on-device execution of T12 (no board attached to this session) — its
flash/capture evidence and the human's panel confirmation are taken from plan.md T12 as recorded,
per constitution §4 honest-status reporting. The device *build* I did run myself. The untracked
`assets/Buttons.png`, `assets/fonts/`, `assets/sprites/` are referenced by no code and are outside
this feature's file list — see §6 open question. No tool file was passed, so no blast-radius query
is cited; scoping was done by hand from `git status`/`git diff`.

## 2. Plan Conformance

| Task | Implemented as planned? | Note |
|---|---|---|
| T1 | ✅ | `drawTitleScreen` is a `switch` on `GameState` calling `drawText` twice; header carries `TitleBounds` + both rects + all four `static_assert` groups. |
| T2 | ✅ | Width-tracks-string `static_assert`s present (`title_screen.h:101-108`); `glyphCharAt` coverage test present. |
| T3 | ✅ r2 | Pixel-exact fixture, outside-is-BLACK, independent-width and rect-intersection tests all present, and the strings are now restated as local literals (`title_screen_test.cpp:39-40`) exactly as the DoD and §1 Decision 6 require. F1 closed. |
| T4 | ✅ | 7×7 extreme grid incl. `INT32_MIN/MAX`, green under `-fsanitize=address,undefined`. |
| T5 | ✅ | I re-ran `git diff --exit-code HEAD` over `game_state.{h,cpp}`, `game_loop.h`, `input.h` myself (condition (b): Must-AC verification) — empty, all four byte-identical. |
| T6 | ✅ | 8-step replay, per-step comparison, working negative control. |
| T7 | ✅ | I planted a violation for each of the 5 rules and confirmed each fires and exits 1 (`setPixel`, `assets/…png`, deleted disjointness message, `std::time`, `new`). Also confirmed the inherited glyph-metric rule reaches `title_screen.h` via its `font.h` include, as §1 Decision 5 claims. |
| T8 | ✅ r2 | Public surface is exactly the four symbols of §1 Decision 6. The `CMakeLists.txt` removal that drove F4 has been reverted, so there is no longer an undocumented component change to record. |
| T9 | ✅ | `TITLE_DUMP`/`clean-title-dump`/`bench` wiring mirrors `TEXT_DUMP`; `make bench` reproduces 5.50 µs/call at r2 (plan records 3.26 — same order, all far under budget). |
| T10 | ✅ | I viewed `build/title_screen.png` myself: STEAMCORE centred on row 6, PRESS START on row 14, all else black, one ink. Constitution §6 and AC-1.6 hold in the image. |
| T11 | ✅ r2 | Harness, capture tool and its tests are sound. All three defects around it are closed: F2 (`--which` selector + corrected walkthrough), F3 (stale docs), F4 (`SRCS` entry restored, fresh `.obj` observed). F10 (one further stale doc count) found and fixed this round. |
| T12 | ✅ | Ran rather than blocked. Watchdog deviation is honestly recorded; `vTaskDelay(1)` at `app_main.cpp:83-85` fires every 20 of ~1,201 lines — live code, ~60 yields per dump, not dead. |

**Deviations (plan §Deviations).** All three are recorded and all three are sound. I re-derived the
`font.cpp` fix from scratch rather than citing it — trigger condition (d), an explicit reason to
doubt: it edits already-released `text-rendering` code. Findings: (1) `reportInvalidGlyphArt` is
reachable **only** at compile time — its sole caller `glyphPixel` (`font.cpp:519`) is called only
from `buildAtlas` (`:536`), called only from the `constexpr` global `kAtlas` (`:551`); `nm` on an
`-O2` object shows the symbol is never emitted at all. (2) The "bad art fails the build" contract
survives: planting `"#X     #"` yields `error: constexpr variable 'kAtlas' must be initialized by a
constant expression` naming the function and the offending row — a *better* diagnostic than the
throw gave. (3) Glyph pixels are unchanged: `font_test.cpp` and `text_fixture_test.cpp` (which
compares against the committed `reference_pattern.scfb`) pass unmodified. The one questionable part,
the `for (;;) {}` body, is now `std::abort()` — F6. Re-derived at r2 (condition (a)): I re-planted
`"#X     #"` at `font.cpp:46` and the build still fails with `constexpr variable 'kAtlas' must be
initialized by a constant expression` naming `reportInvalidGlyphArt` and the offending row, so the
compile-time guard survives the change; `font_test.cpp`, `text_fixture_test.cpp` and
`reference_pattern.scfb` are all still byte-identical to `HEAD` and green, so no glyph pixel moved.

## 3. Findings

| # | Severity | Location | Finding | Status |
|---|---|---|---|---|
| F1 | Major | `firmware/steamcore/test/title_screen_test.cpp:26-27`, `:147-177`, `:205-221` | The tests import `steamcore::detail::kWordmarkText`/`kPromptText` instead of restating the literals, contrary to plan §1 Decision 6 ("the tests restate both strings") and T3's own DoD. Result: **no test anywhere asserts the screen reads "STEAMCORE"/"PRESS START"** — `grep -rn 'PRESS START' firmware/steamcore/test` is empty. Verified: changing the header constant to `"PRES  START"` leaves all 171 tests green, so AC-1.2's named string can regress silently; the expected buffer and the "independent" width are both derived from the same constant the code uses. The comment at `:145-146` ("its own restated strings") is therefore factually wrong. Fix: restate both literals in the test file (and assert them equal to `detail::`), or drop the `detail::` using-declarations entirely. fixed r2 — the `detail::` using-declarations are gone (`grep -rn 'detail::' firmware/steamcore/test/` now hits only `input_test.cpp` and F1's own explanatory comment); `kExpectedWordmarkText`/`kExpectedPromptText` at `:39-40` are plain literals, used at `:118`, `:163-165`, `:221-229`, `:258-259`. **Mutation re-run by me, not taken from the note:** setting `kPromptText` to `"PRES  START"` fails `title_screen_matches_expected_framebuffer_pixel_exact` (`15 pixel(s) differ, first at (110,112)`, 170 passed / 1 failed); header restored byte-identical, 171/171 green. |
| F2 | Major | `tools/scfb_capture.py:104-108`, `docs/device-build.md:150-186` | `main()` always writes `valid_blocks[-1]` and offers no selector. The documented AC-3.1 walkthrough tells the reader to capture *past* the READY→PLAYING change (step 2) and then to confirm STEAMCORE/PRESS START in the produced PNG (step 5) — but the last block is the all-black PLAYING frame. Reproduced against a synthetic two-block transcript: the written `.scfb` payload contains only palette index 0. So the documented procedure cannot produce AC-3.1's evidence, and T12's "both dumps decoded via `scfb_capture.py`" is not reproducible as written. Fix: add `--index N` / `--all` (writing `out.0.scfb`, `out.1.scfb`), or correct step 5 to say which block is written and how to get the READY one. fixed r2 — `--which {first,last}` at `scfb_capture.py:88-96`, default `last` (backward compatible), consumed at `:118`. `docs/device-build.md:167-190` now decodes both blocks into `title_screen_ready.scfb` / `title_screen_playing.scfb` and step 5 names which image must show what. **Probed by me against a fresh synthetic two-block log** (first block payload `7`, last block payload `0`): `--which first` wrote payload `[7]×8`, bare and `--which last` both wrote `[0]×8` — three distinct invocations, each the correct block. The documented AC-3.1 procedure now yields the READY frame it asks the reader to inspect. |
| F3 | Minor | `docs/host-tests.md:56`, `:58`, `:61` | Three command-table rows left stale by this same diff: `make bench` said "three budgets" (the Makefile diff adds a fourth), `make test-python` said "`test_fb_view.py` *and* `test_roundtrip.py` (15 tests)" (now 25 across three files), `make view` said "two PNGs" (now three). Fixed directly by the reviewer — doc-only, low risk. | fixed r1 — re-read at r2: the `make bench` and `make view` rows are still accurate. The `make test-python` row had drifted *again* (F10). |
| F4 | Minor | `firmware/system/main/CMakeLists.txt:21` | `gpio_input_source.cpp` was removed from `SRCS`. The host `Makefile` globs `firmware/steamcore/src/*.cpp` only, so `port/esp32/` is structurally invisible to it — this released v0.3.0 file is now compiled by **no** build at all and has lost its only compile gate. The removal is recorded only in T11's completion note, not in plan §2 or §Deviations. Fix: keep it in `SRCS` (it compiles unused at zero cost) or record the lost coverage explicitly. fixed r2 — back in `SRCS` at `CMakeLists.txt:31`. **Verified against a forced recompile, not a cached object:** after `touch`ing the source, `idf.py build` reported `Building CXX object …/gpio_input_source.cpp.obj` and completed green, and the `.obj` exists on disk (18,796 bytes, current timestamp). The v0.3.0 file has its compile gate back. |
| F5 | Nit | `firmware/steamcore/test/title_screen_test.cpp:261-273` | `title_screen_fixed_position_stays_in_bounds_under_asan` builds a `ready` framebuffer and asserts nothing about it, and its comment claims it covers "PLAYING/GAME_OVER" while only PLAYING is exercised. Fix: assert on `ready`, and either add the GAME_OVER case or drop it from the comment. fixed r2 — `title_screen_test.cpp:277-278` now asserts `anyNonBlackInside(ready, …)` on both rects; `:288-291` adds a real GAME_OVER case compared against an untouched framebuffer; the comment at `:267-272` states exactly what is covered and cites the finding. No dangling unasserted framebuffer remains in the test. Green under `make test-asan` (171/171). |
| F6 | Nit | `firmware/steamcore/src/font.cpp:514-517` | `reportInvalidGlyphArt`'s body is `for (;;) {}` — an empty infinite loop with no side effect is UB under C++17 [intro.progress]/1, so it is free to be optimised away; combined with `[[noreturn]]` it is not actually a guard if the (today correct) unreachability analysis ever stops holding. Fix: `std::abort()` or `__builtin_trap()`, both defined-behaviour and available on the host and ESP-IDF toolchains alike. fixed r2 — `font.cpp:524` is now `[[noreturn]] void reportInvalidGlyphArt() { std::abort(); }` with `#include <cstdlib>` at `:5`; no `for (;;)` remains in the file. The UB is gone and the guard is real. Verified not to change any glyph: see §2 Deviations — bad art still fails the build, and the three unmodified font/fixture artefacts still pass. |
| F7 | Nit | `firmware/system/main/app_main.cpp:102` | `std::abort()` with no `#include <cstdlib>` — the exact latent-transitive-include class plan §Deviations #2 just fixed in `font.cpp`. (`size_t`/`uint8_t` here are safe: `dump_format.h` guarantees them.) Left open rather than reviewer-fixed because I cannot re-run `idf.py build` in this session. Fix: add `#include <cstdlib>`. fixed r2 — `#include <cstdlib>` at `app_main.cpp:19`; `idf.py build` green with a forced recompile of `app_main.cpp.obj`. **One honest correction to the record:** I probed this by *deleting* the include again and rebuilding — the device build still succeeded (exit 0, `app_main.cpp.obj` rebuilt). So the xtensa toolchain also supplies `std::abort` transitively; the omission was latent, exactly as this finding's own wording ("latent-transitive-include class") said, and was never an active break on either toolchain. The fix is correct include-what-you-use hygiene that removes a real fragility — but no build, host or device, would have caught it. Include restored byte-identical. |
| F8 | Nit | `tools/test_scfb_capture.py` | `extract_blocks`/`validate_and_decode` are covered well, but `main()` is untested — argv arity, missing file, the no-valid-block→exit 1 path, and the "last valid block wins" selection that F2 lives in. `scfb_capture.py:40-43`'s "a second BEGIN discards the open block" branch is also untested. Fix: two `main()`-level tests plus one double-BEGIN case. | fixed r2 — `test_scfb_capture.py:114-179` adds `MainTest` with the five promised cases, and `:72-86` adds `test_a_second_begin_discards_the_open_incomplete_block`. I read the assertions rather than counting them: the two `--which` tests build blocks with *distinct* payloads (`[0,0,0,0]` vs `[3,3,3,3]`) and assert the written file equals the specific expected one, so they genuinely discriminate — not tautologies; the two failure tests assert both the return code *and* that no output file was created. `main()` at `scfb_capture.py:99-104` now turns a missing log into a clean `return 1` instead of an uncaught `OSError`. `make test-python`: 31/31, re-run by me. |
| F9 | Nit | `tools/check_constraints.sh:335-341` | The AC-1.6 presence rule is `grep -qF 'must not overlap'` over the whole file, so commenting the `static_assert` out while leaving its text keeps the rule green. Fix: also require `static_assert` on a nearby line, or anchor on the separating-axis expression instead of the message. fixed r2 — `check_constraints.sh:348-349` reads `grep -nF 'must not overlap' … | grep -qvE '^[0-9]+:[[:space:]]*//'`. **The corrected anchored regex is what is in the file**, not the broken first attempt — I checked this specifically because the fix note flags a silent no-op, and confirmed the difference matters by feeding both patterns the same commented-out input: `:[0-9]+:…` still matches a line (rule stays green, no-op) while `^[0-9]+:…` excludes every line (rule fires). **Mutation reproduced by me:** commenting out `title_screen.h:130-136` while leaving the message text in the comment makes `bash tools/check_constraints.sh` exit 1 with "the kTitleWordmarkBounds/kTitlePromptBounds disjointness static_assert was not found…"; header restored byte-identical and `make lint` OK. |

| F10 | Minor | `docs/host-tests.md:58` | Found at r2. F8's fix added 6 Python tests, so the `make test-python` row's "(25 tests)" is stale again — the *same* drift F3 recorded (it read "15 tests" then), reintroduced by the very change that closed F8, and its enumeration of `test_scfb_capture.py`'s coverage omitted the new `main()` and double-BEGIN cases entirely. It matters because this row is the only place a reader learns what that command proves; a count that silently lags reality is how the doc stopped being trusted the first time. Fix: state 31 and list the new cases. | fixed r2 — fixed directly by the reviewer (doc-only, low risk, F3 precedent): the row now reads 31 and names the `main()` arity/missing-file/no-valid-block/`--which` cases and the second-`BEGIN` case. `make lint`, `make test`, `make test-python` re-run green afterwards. |

## 4. Requirements Traceability

| Spec ID | Implemented at | Verdict |
|---|---|---|
| AC-1.1 | `title_screen.cpp:11-12`, bounds `title_screen.h:80-87`; test `title_screen_test.cpp:182-200` | ✅ met |
| AC-1.2 | `title_screen.cpp:13-14` (`Color::BRIGHT_ORANGE`); tests `:126-137`, `:156-186` against restated literals `:39-40` | ✅ met r2 — position, ink *and* the exact wording are now each a failing assertion (F1; mutation-proven) |
| AC-1.3 | `title_screen.cpp:11-14` (only `drawText`); lint rule verified firing | ✅ met |
| AC-1.4 | No art at all; test `:108-112`; `assets/`/`.png`/`.ttf` lint rule verified firing | ✅ met |
| AC-1.5 | Test `:237-256` + `:261-273`; `make test-asan` 171/171 | ✅ met |
| AC-1.6 | `title_screen.h:130-136`; test `:226-228`. Re-derived by hand (Must AC, condition (b)): wordmark `(84,48,72,8)`, prompt `(76,112,88,8)`, both from `(sizeof(s)-1)*kGlyphAdvance` with `kGlyphAdvance == kGlyphHeight == 8` — separating axis holds on y (`48+8 = 56 ≤ 112`); no hardcoded width | ✅ met |
| AC-2.1 | `title_screen.cpp:16-18`; `title_screen_session_test.cpp:46-61` | ✅ met |
| AC-2.2 | `title_screen_session_test.cpp:64-81` (20 held ticks) | ✅ met |
| AC-2.3 | `title_screen_determinism_test.cpp` (8 steps, per-step compare, negative control) | ✅ met |
| AC-3.1 | `app_main.cpp:52-88`, `title_screen_harness_game.h`, `tools/scfb_capture.py`; `docs/device-build.md:167-195`; hardware run per plan T12 | ✅ met r2 — the documented decode path now produces both the READY and PLAYING frames (F2, probed end-to-end); the recorded human panel confirmation stands |
| AC-3.2 | Not needed: hardware was available and T12 ran | ✅ met |
| NFR-1 | `bench_title_screen.cpp`; reproduced 4.39 µs/call vs a 16.67 ms tick | ✅ met |
| NFR-2 | No allocation, no ESP-IDF header in `include/`/`src/`; lint rules verified firing | ✅ met |
| NFR-3 | AC-2.3 above + clock/RNG lint rule verified firing | ✅ met |
| NFR-4 | `clang++` and `g++`, `-std=c++17 -Wall -Wextra -Werror`, both 171/171; glyph-metric rule confirmed reaching `title_screen.h` | ✅ met |
| NFR-5 (library lens) | `title_screen.h:56`, `:80`, `:89`, `:141` — exactly four public symbols, matching §1 Decision 6; no `GameInput` field, no `GameLoop` contract change | ✅ met r2 — `detail::` has no consumer outside `title_screen.h` itself (grep-confirmed); the internal namespace is genuinely internal again (F1) |
| NFR-6 (library lens) | `title_screen.h:9-50` — phase gate, never-clears/never-erases, composes-only-`drawText`, exact strings/rects, no-throw/no-alloc, one usage example | ✅ met |
| NFR-7 | Single ink `BRIGHT_ORANGE`; test `:117-128` makes it a failing assertion | ✅ met |
| NFR-8/9/10 | N/A per spec (offline, no runtime failure mode, no new dependency — `scfb_capture.py` is stdlib-only, allowlist-checked) | ✅ met |

*Library lens, remaining rows:* semver/packaging are declared no-ops for a statically-linked
firmware image (constitution §2), and this diff adds no export to any released header — the four
new symbols all live in a new file. Nothing breaking, nothing removed, no new dependency.

## 5. What Was Checked

- [x] Correctness: logic does what the acceptance criteria demand
- [x] Non-functional: applicable NFRs and constitution quality bars hold
- [x] Error handling: failures are handled, not swallowed
- [x] Security: no injected input trusted, no secrets in code
- [x] Tests: exist, are meaningful, and pass — r1's F1 exception is closed and mutation-proven
- [x] Readability: the next developer will understand this

## 6. Verdict

`passed`. Both round-1 Majors are genuinely closed, and I know that because I re-broke them rather
than reading their labels. F1: with the `detail::` imports replaced by restated literals, mutating
`kPromptText` to `"PRES  START"` now takes the pixel-exact fixture down with a precise diagnostic
(15 pixels, first at `(110,112)`) where at round 1 all 171 tests shrugged — AC-1.2's wording is a
real assertion at last. F2: `--which first`/`--which last` were probed against a synthetic two-block
transcript and returned the two different, correct blocks, so `docs/device-build.md`'s own
walkthrough now hands the next verifier the READY frame step 5 tells them to inspect. The six
Minors/Nits all landed too, and two of them are worth naming because the fixes were done honestly
rather than plausibly: F9's author reused a grep pattern that silently made the lint rule a no-op,
caught it only by re-running the mutation, and wrote that failure into the comment — I reproduced
both the corrected rule firing and the broken pattern's no-op, and the note is accurate. F6 swapped
UB for `std::abort()` without moving a single glyph pixel, which I confirmed by re-planting bad art
(still a compile error) against three untouched, still-green font artefacts. Two corrections to the
record, neither changing the verdict: F7's missing include was latent on the *device* toolchain too
— I deleted it again and `idf.py build` still passed — so it was good hygiene, not a repair, and no
build would have caught it; and F8's fix quietly restaled the very doc row F3 existed to fix
(F10, reviewer-fixed). Everything else re-runs green: 171/171 on clang, ASan/UBSan and g++, lint
clean, 31/31 Python, `idf.py build` green with the four touched sources force-recompiled and
`gpio_input_source.cpp.obj` observed rebuilt, and the four released `GameSession`/`GameLoop`/
`GameInput` files still byte-identical to `HEAD`. The remainder of the r1 verdict stands: the
engine change is the best kind of boring — `drawTitleScreen` is 13 lines composing nothing but the
shipped `drawText`, with derived rather than hardcoded layout constants — and the verification is
unusually thorough for a feature whose hardware story is only a `Should`. The fixes did not disturb
any of that; they were surgical, stayed inside the findings' scope, and nobody took the opportunity
to refactor. This is ready for `/demo-day`. **Open question for the user (not a
finding):** `assets/Buttons.png`, `assets/fonts/` and `assets/sprites/` are untracked, referenced by
no code, and belong to no task in this plan — whether `/go-live` commits them is a release decision
this review cannot make.

---

## ✅ REVIEW GATE

*All boxes checked → `/demo-day` may start. Any box open → back to `/increment`. On
re-review, edit this same checklist in place — never duplicate it as a second gate.*

- [x] No open Blocker findings
- [x] No open Major findings — F1 and F2 both `fixed r2`, each re-verified by a mutation/probe I ran myself, not by their labels. No waiver was needed or granted.
- [x] Every Must AC traces to implementing code; no constitution non-negotiable violated — AC-1.2, AC-3.1 and NFR-5 upgraded from ⚠️ partial to ✅ met at r2
- [x] All plan deviations documented and accepted — the three in §Deviations are; T3's string restatement and the `CMakeLists.txt` removal are both reverted/implemented, so neither is an undocumented deviation any more
- [x] Test suite runs green — `make test`/`test-asan`/`test-gcc` **171/171** each, `make lint` clean, `make test-python` **31/31**, `make bench` green (5.50 µs/call), `idf.py build` **green** with the four touched sources force-recompiled; all re-run by the reviewer at r2 after every probe was reverted (working tree confirmed byte-identical to pre-probe state)
- [x] Line budget respected: Ist 171 / Soll ~150 (excluding HTML comments) — 21 over, from the per-finding r2 verification evidence in §3; accepted as the point of a re-review
- [x] Status set to `passed`
