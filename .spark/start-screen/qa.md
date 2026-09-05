# QA Report: start-screen

| | |
|---|---|
| **Phase** | Review (hands-on) |
| **Owner** | QA Tester (`/demo-day`) |
| **Input** | `.spark/constitution.md` §8 (declared substitute method), `.spark/start-screen/spec.md`, `plan.md`, `review.md` |
| **Status** | `passed` |
| **Round** | 1 |
| **Date** | 2026-09-05 |

**Handoff**
- **Status:** `passed`.
- **Verdict:** Yes — I would demo this now. All 6 Must ACs (US-1) and all 3 Must ACs (US-2) are independently verified by running the host suite myself, reading test bodies to confirm they assert what their names claim, and viewing the rendered PNG. US-3's Should ACs are genuinely captured (not parked): I corroborated the recorded device evidence with a forced, non-cached `idf.py build` today, and cross-checked the harness's documented 150-tick synthetic-start timing against the actual source.
- **Open:** `none` — 0 Blockers, 0 Majors. One Minor (artifact-wording, capped): AC-1.4's Given-clause ("logo element is implemented as a Sprite") no longer literally holds since the plan replaced the Sprite logo with a `drawText` wordmark; the tests and `review.md` correctly reinterpreted its intent ("no decoded PNG/TTF byte, no art asset") rather than the literal premise, and that reinterpretation is itself tested (`title_screen_ready_uses_only_black_and_bright_orange`). No Must AC or verdict changes.
- **Binding ruling:** §5 Verdict and the gate checklist below.
- **On conflict:** the numbered body wins for everything except `Status`.

## 1. Test Environment

- **QA Method:** Per `.spark/constitution.md` §8: `Browser-observable surface: no` (no UI, no browser-drivable surface). Substitute method, performed by me this session: host-compiled unit tests (`make test`/`test-asan`/`test-gcc`/`lint`/`test-python`/`bench`/`view`), the SCFB-over-serial dump path decoded by `tools/scfb_capture.py` + `tools/fb_view.py` (evidence audited, not re-captured — see §2 AC-3.1/3.2), and `idf.py build` re-run today to corroborate the device-side code.
- **Toolchain:** Apple clang 14 (`test`/`test-asan`), Apple `g++` shim (`test-gcc`, honestly nominal per plan §4), Python 3.9 (`test-python`), ESP-IDF v5.4.4 (`idf.py build`, sourced from `~/esp/esp-idf/export.sh`).
- **Test data:** none needed — pure rendering logic, no persisted state.

## 2. Acceptance Criteria Verification

| Spec ID | Steps performed | Expected | Observed | Result |
|---|---|---|---|---|
| AC-1.1 | Ran `title_screen_every_pixel_outside_both_rects_is_black` and `title_screen_matches_expected_framebuffer_pixel_exact` (host `make test`); read both bodies — expected buffer built by the test's own independent placement loop, never by calling the code under test. Also viewed `build/title_screen.png` directly. | Title/logo drawn at fixed documented position, every other pixel BLACK except the prompt. | Both tests pass (171/171 overall); PNG shows STEAMCORE centered, rest black except PRESS START. | ✅ pass |
| AC-1.2 | Ran `title_screen_ready_uses_only_black_and_bright_orange` (asserts every lit pixel is `BRIGHT_ORANGE`, none `ORANGE`/`DARK_ORANGE`) and the pixel-exact fixture. Viewed PNG — text is visibly orange. | "PRESS START" drawn in BRIGHT_ORANGE at fixed position via `drawText`. | Test passes; PNG confirms orange ink on black. | ✅ pass |
| AC-1.3 | Ran `make lint` — the `title-screen.cpp draws only through drawText` rule (greps for `setPixel(`/`fillRect(`/`blit(`, excludes tests) fired clean. Read `title_screen.cpp`: exactly two `drawText` calls under a `switch`. | Only `Framebuffer`, `Sprite`/`blit`, `drawText` used; no new primitive. | Lint rule green; source confirms drawText-only, no Sprite/blit used at all. | ✅ pass |
| AC-1.4 | Ran `title_screen_wordmark_uses_only_defined_glyphs` and `title_screen_ready_uses_only_black_and_bright_orange`; ran `make lint`'s asset/PNG/TTF-reference rule. | No decoded PNG/TTF byte anywhere in the logo. | Both tests pass, lint rule green. Note: the AC's literal premise ("logo implemented as a Sprite") is stale — plan §1 Decision 4 replaced the Sprite logo with a `drawText` wordmark — but the substance (no art-asset byte) is verified and the reinterpretation is itself the thing under test. See Handoff for Minor finding. | ✅ pass (with wording caveat, Minor, non-blocking) |
| AC-1.5 | Ran `make test-asan` (171/171, 0 failed) — `title_screen_strings_survive_extreme_positions_under_asan` and `title_screen_fixed_position_stays_in_bounds_under_asan` both included. Isolated with `make test FILTER=title_screen_strings_survive_extreme_positions_under_asan`. | No out-of-bounds access at framebuffer edges under ASan/UBSan. | Both pass under `-fsanitize=address,undefined`; isolated run also green. | ✅ pass |
| AC-1.6 | Ran `title_screen_bounds_do_not_intersect` (isolated via `FILTER`) and `title_screen_bounds_match_independent_computation`. Read `title_screen.h`'s two `static_assert`s (separating-axis expression, not "A above B"). | Logo and prompt bounding rects are disjoint. | Rects `(84,48,72,8)` and `(76,112,88,8)` — y-ranges 48-55 vs 112-119 disjoint; runtime test and static_assert both confirm empty intersection. | ✅ pass |
| AC-2.1 | Ran `title_screen_session_absent_the_tick_after_start` (isolated via `FILTER`), read body: ticks READY then a `start` rising edge through a real `GameLoop<TitleScreenGame>`, asserts framebuffer byte-identical to an untouched (all-BLACK) buffer. | Neither element drawn the tick after START rising edge. | Test passes in isolation and in the full run. | ✅ pass |
| AC-2.2 | Ran `title_screen_session_stays_absent_while_start_held`, read body: 20 further ticks with `start` held true, framebuffer checked every tick, not just once. | No flicker back while start is held. | Test passes; loop asserts every one of 20 ticks individually. | ✅ pass |
| AC-2.3 | Ran `title_screen_replay_is_deterministic_after_every_step` and the negative control `title_screen_replay_comparison_can_detect_a_real_divergence`. Read both bodies. | Two independent runs of the same fixed 8-step input sequence produce byte-identical framebuffers at every step; the comparison can detect a real divergence (proves the test isn't vacuous). | Both pass. Negative control deliberately flips `start` at step 2 and confirms `framebuffersEqual` catches it immediately after that step (not just at sequence end, correctly avoiding false negatives from re-convergence). | ✅ pass |
| AC-3.1 | Cannot re-flash hardware this session. Performed a document/evidence audit instead: (a) re-ran `idf.py build` today with a **forced, non-cached recompile** (touched `title_screen.cpp`, `text.cpp`, `font.cpp`, `app_main.cpp`, `title_screen_harness_game.h`) — confirmed today's source genuinely compiles and links for the device, not just at some earlier point; (b) cross-checked plan.md T12's "150-tick synthetic start pulse" claim against `title_screen_harness_game.h:70` (`kSyntheticStartAtTick = 150`) — matches exactly; (c) read plan.md's recorded transcript evidence: both READY and PLAYING SCFB dumps decoded via `--which first`/`--which last`, byte-exact match to the host PNG, human verbatim confirmation ("funktioniert") recorded 2026-09-04. | Real device dump shows logo+prompt at documented positions; device-side code still compiles today. | Forced device rebuild green (12/12 build steps, all four touched files recompiled). Harness timing matches the record exactly. Recorded transcript/human-confirmation evidence is internally consistent with the host-side proof (same rects, same PNG). **I did not personally flash/capture hardware this session** — this AC rests on independent corroboration of real recorded evidence (build + code cross-check), not a re-run of the physical step, and not on source-reading alone. | ✅ pass (evidence-audit, not a fresh physical capture — see note) |
| AC-3.2 | Read plan.md T12 row and spec AC-3.2's fallback wording. | If hardware unavailable, record "not capturable yet"; here hardware WAS available and T12 ran. | T12 status is `done`, "ran, not blocked" — the not-capturable fallback does not apply. Correctly not invoked. | ✅ pass (N/A path correctly not taken) |
| NFR-1 | Ran `make bench`. | Title screen render well under 16.67ms 60Hz tick budget. | Measured 3.4526 µs/call over 10,000 iterations — ~4,800x margin, not borderline. Other benches (dirty scan 0.0014ms, text render 0.0712ms, game loop 0.0008ms) all comfortably under their 5ms budgets too. | ✅ pass |
| NFR-2 | Ran `make lint`'s allocation-ban rule over the title-screen file set (include/src/test). | Zero dynamic allocation. | Rule green. | ✅ pass |
| NFR-3 | Ran `make lint`'s clock/RNG-ban rule; AC-2.3 above is the behavioral proof. | No wall-clock/unseeded-RNG read; deterministic. | Rule green; AC-2.3 passes with negative control. | ✅ pass |
| NFR-4 | Ran `make test-gcc` (171/171) alongside `make test`/`test-asan` (clang). Ran `make lint`'s glyph-metric-literal rule. | Compiles clean under both compilers, `-Wall -Wextra -Werror`, no resolution/tile-size literal outside constants header. | All three green; no literal violations. | ✅ pass |
| NFR-5 | Read `title_screen.h` public symbols: `drawTitleScreen`, `TitleBounds`, `kTitleWordmarkBounds`, `kTitlePromptBounds` — exactly 4, matches plan §1 Decision 6. `detail::` namespace confirmed not exposed. | Minimal new public surface, no `GameInput`/`GameLoop` contract change. | Confirmed — 4 symbols, no other change (review.md F1/NFR-5 traceability independently spot-checked). | ✅ pass |
| NFR-6 | Read the header's doc comment. | States phase gate, never-clears contract, drawText-only composition, exact strings/rects, one usage example. | All present as described. | ✅ pass |
| NFR-7 | Same evidence as AC-1.2 (single-ink test) plus `panel_format.cpp`'s RGB values underlie the ≈9.9:1 contrast claim (not independently re-measured — cited from spec/review, condition (a)-(d) don't trigger re-derivation here since it's not being contested and isn't itself the fix under test). | BRIGHT_ORANGE on BLACK clears WCAG 4.5:1. | Consistent with AC-1.2's passing test; no contradicting evidence found. | ✅ pass |

## 3. Exploratory Findings

| # | Severity | Steps to reproduce | Expected vs. observed | Status |
|---|---|---|---|---|
| — | — | Ran full host suite 3x consecutively (`make test`) to check for flakiness; ran `make test FILTER=zzz_this_test_does_not_exist_zzz` to confirm the negative-filter path fails loudly rather than silently passing. | Expected: consistent pass count, and a clear non-zero exit + error message on a bogus filter. Observed: 171/171 all three runs, no variance; bogus filter correctly printed `ERROR: no test matched filter "..."` and exited non-zero (make reported `Error 1`). No bug found. | n/a — no finding |
| — | — | Checked whether any of `review.md`'s 10 closed findings (F1-F10, all `fixed r2`) could still threaten a Must AC. Read F1 (test string restatement) and F2 (`--which` selector) in detail since they were the two Majors. | F1: mutation-tested by the reviewer themselves (`"PRES  START"` fails the pixel-exact test) — re-confirmed structurally sound by reading the current test file, which uses plain literals `kExpectedWordmarkText`/`kExpectedPromptText`, not `detail::` imports. F2: `--which first`/`last` present in `scfb_capture.py`, used correctly in `docs/device-build.md`'s walkthrough. Neither threatens a Must AC today. | n/a — no finding, reviewer's conclusion independently confirmed |

No Blocker, Major, or reproducible Minor bugs found in this feature's own code during exploration.

## 4. Console & Network

N/A — no browser, no network surface (constitution §8). Substitute check: no ASan/UBSan violations across 171 tests (`make test-asan` clean, no sanitizer trap output), no lint failures, no Python test failures (31/31), no unexpected `idf.py build` warnings/errors on a forced recompile.

## 5. Verdict

**Passed.** All 9 US-1/US-2 Must ACs are independently verified against test bodies I read myself (not just names), the host suite is green three-for-three across two compilers plus ASan/UBSan, lint is clean, benches clear their budget by three orders of magnitude, and I personally viewed the rendered title screen (STEAMCORE centered above PRESS START, both orange, rest black). US-3's Should ACs are genuinely captured, not parked: I could not re-flash hardware this session, but I corroborated the recorded evidence with my own forced device rebuild today and a direct code cross-check of the harness's timing claim — this is an honest evidence audit, distinct from either a fresh physical capture or from reading source and calling it tested. I would demo this to a stakeholder right now.

---

## ✅ QA GATE

- [x] Every Must-story acceptance criterion verified (host-substitute method per §8) and passed
- [x] Every applicable NFR verified and passed
- [x] No open Blocker or Major bugs (one Minor artifact-wording note on AC-1.4, non-blocking)
- [x] Console/network N/A, substitute checks (ASan/UBSan, lint, Python tests) all clean
- [x] Tested on all agreed surfaces: host (both compilers + sanitizers), Python tooling, device build
- [x] Line budget respected: Ist 96 / Soll ~130 (excluding HTML comments)
- [x] Status set to `passed`
