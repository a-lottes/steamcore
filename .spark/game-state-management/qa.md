# QA Report: game-state-management

| | |
|---|---|
| **Phase** | Review (hands-on) |
| **Owner** | QA Tester (`/demo-day`) |
| **Input** | `.spark/game-state-management/spec.md`, `.spark/constitution.md` §8 (QA Method), host toolchain |
| **Status** | `passed` |
| **Round** | 1 |
| **Date** | 2026-09-02 |

**Handoff**
- **Status:** mirrors the header table above (authoritative for `Status`).
- **Verdict:** Would demo this now. All 15 Must ACs performed and passed via the host test suite (109/0 clang, 109/0 ASan, 109/0 g++-alias), 17/17 game-state-scoped tests individually re-run under both plain and ASan builds, `make lint`'s three new game-state rules green, `make test-all` fully green end to end. No Blocker/Major/Minor bugs found. No AC required the framebuffer-dump half — this feature renders nothing.
- **Open:** `none` — 0 bugs found.
- **Binding ruling:** §5 Verdict and the gate checklist below — the only binding location; there is no other round to point to.
- **On conflict:** the numbered body below wins for everything except `Status`; log the mismatch as a finding at the next `/demo-day` and proceed — don't stop on it.

## 1. Test Environment

- **App URL:** N/A — constitution `.spark/constitution.md` §8 declares `Browser-observable surface: no`; there is no browser-drivable surface (no `package.json`, HTML, route handler or terminal entrypoint). Per §8 the substitute method for this round is the **unit-test half only** (host-compiled tests) — the framebuffer-dump half is not enforceable today (no ESP-IDF toolchain, no board).
- **Browser / viewport(s):** N/A — same reason.
- **Test data / accounts used:** N/A — no persistence, no accounts (spec A9).
- **Toolchain actually used:** Apple clang 14.0.3 (`/usr/bin/clang++`), `/usr/bin/g++` (Apple clang alias — nominal second-compiler check, same caveat `docs/host-tests.md` already records), GNU Make 3.81, macOS 13.7.8. No ESP-IDF, no cmake, no board — none of this feature's Must ACs need them (pure state-machine logic, zero rendering output).
- **Commands run (from repo root, `/Users/andreaslottes/steamcore`), output observed directly, not inferred:**
  - `make clean && make test-all` → chained `test`, `test-negative`, `test-asan`, `test-gcc`, `bench`, `test-python`, `test-roundtrip`, `test-png-external`, `lint`, all green; full suite **109 passed, 0 failed** in each of 3 C++ configurations (clang `-O2`, ASan `-fsanitize=address,undefined -fno-sanitize-recover=all`, g++-alias), 3× `BENCH OK`, 15 Python + 2 round-trip tests `OK`, `sips` cross-check `OK`, `make lint OK`.
  - `make test FILTER=game_state` → **17 passed, 0 failed** (isolates every `game_state_*`-named test).
  - `make test-asan FILTER=game_state` → **17 passed, 0 failed** under AddressSanitizer/UBSan.
  - Read `firmware/steamcore/include/steamcore/game_state.h`, `src/game_state.cpp`, `test/game_state_test.cpp`, `test/game_state_determinism_test.cpp`, `test/session_replay_fixture.h` directly to map each printed test name to its AC (below) and to confirm NFR-6 (public surface: exactly `GameState`'s 3 values, `GameSession`, `advance`, `state` — nothing else) and NFR-7 (doc comment covers all required contract clauses) by inspection of the compiled, test-passing source.
  - `grep -n "game_state" tools/check_constraints.sh` → confirmed the lint file sets (`GAME_STATE_DETERMINISM_FILES`, `GAME_STATE_TEST_FILES`) name exactly `game_state.h`, `game_state.cpp`, `game_state_test.cpp`, `game_state_determinism_test.cpp`, `session_replay_fixture.h` — the same files `make lint`'s output (`--- no wall-clock read or unseeded RNG in the game-state mechanism (AC-5.3) ---`, `--- no dynamic allocation in the game-state test file set (NFR-2) ---`, `--- no integer standing in for a GameState (NFR-4) ---`) confirmed running and green.

## 2. Acceptance Criteria Verification

All 15 Must ACs are pure state-machine logic (spec §1 confirms: "no display driver exists" for this feature) — fully verifiable by the unit-test half of §8's substitute method. None requires the framebuffer-dump half; none is marked "could not be captured."

| Spec ID | Steps performed | Expected | Observed | Result |
|---|---|---|---|---|
| AC-1.1 | Ran `make test FILTER=game_state`; located `game_state_fresh_session_starts_ready` (`game_state_test.cpp:13`) | Fresh `GameSession` reads `READY` | Test asserts `session.state() == GameState::READY`; suite prints `17 passed, 0 failed` | ✅ pass |
| AC-1.2 | Ran full suite (a 4th enumerator is a `-Werror -Wswitch` build break per plan T2, already demonstrated at review round 1/2 and re-confirmed here by the clean build succeeding with exactly 3 enumerators); ran `game_state_exactly_three_states_are_named_and_distinct` (`:88`) | Exactly READY/PLAYING/GAME_OVER, no BOOT/PAUSED | Build succeeds (3-way exhaustive switch, no `default`), test passes | ✅ pass |
| AC-1.3 | Ran `make lint`; observed `--- no dynamic allocation or heap-backed container ---` and `--- no ESP-IDF/FreeRTOS/driver header in logic code ---` blocks (general `include/`+`src/` sweep, which reaches `game_state.h`/`.cpp`) | No allocation, no ESP-IDF header in the component's source | `make lint OK`, no failure reported | ✅ pass |
| AC-2.1 | Ran `game_state_held_start_transitions_once_on_rising_edge` (`:98`) — `start` false step 1, held true steps 2–10, state checked after **every** step | PLAYING from step 2, no re-trigger while held | Test passes (asserts after each of 9 held steps) | ✅ pass |
| AC-2.2 | Ran `game_state_first_ever_tick_with_start_true_is_a_rising_edge` (`:112`) | Fresh instance, first tick `start=true` → PLAYING | Test passes | ✅ pass |
| AC-2.3 | Ran `game_state_ready_stays_ready_without_start` (`:119`) — both `fire` values, `start=false` | Stays READY regardless of `fire` | Test passes | ✅ pass |
| AC-3.1 | Ran `game_state_session_ended_signal_ends_playing_session` (`:130`) | PLAYING + `sessionEnded=true` → GAME_OVER same step | Test passes | ✅ pass |
| AC-3.2 | Ran `game_state_no_input_combination_ends_a_session` (`:143`) — 4 `start`/`fire` combos, each a genuine rising edge (driven from `sessionInPlayingWithStartReleased()`, the review-F2 fix) | Stays PLAYING for every combo | Test passes (review confirmed by mutation-testing at round 2 that a naive driver would have missed this — re-verified here by reading the fixed driver and re-running) | ✅ pass |
| AC-3.3 | Ran `game_state_repeated_session_ended_signal_is_idempotent` (`:160`) | GAME_OVER after first signal, unchanged after second | Test passes | ✅ pass |
| AC-4.1 | Ran `game_state_restart_never_passes_through_ready` (`:179`) | GAME_OVER → PLAYING directly on rising `start`, never READY | Test passes | ✅ pass |
| AC-4.2 | Ran `game_state_held_start_after_restart_fires_once` (`:189`) — 5 further held steps after restart | Exactly one transition, stays PLAYING | Test passes | ✅ pass |
| AC-4.3 | Ran `game_state_fire_alone_does_not_restart` (`:201`) | `fire=true, start=false` in GAME_OVER stays GAME_OVER | Test passes | ✅ pass |
| AC-5.1 | Ran `game_state_replay_is_deterministic_after_every_step` (`game_state_determinism_test.cpp:95`) — 20-step fixture, 2 independent sessions, compared after every step | Identical state at every step | Test passes (`firstMismatchStep == -1`); fixture's own coverage test (`:20`) also passed, confirming the fixture actually visits all 3 states, both rising-edge contexts, a ≥3-step held run, and a `sessionEnded` step — not vacuous | ✅ pass |
| AC-5.2 | Ran `game_state_replay_sensitivity_sweep_is_asymmetric` (`:166`) — every step index, each of `start`/`fire`/`sessionEnded` flipped independently | `start`/`sessionEnded` detected divergent somewhere; `fire` never | Test passes: `startEverDetected` and `sessionEndedEverDetected` true, `fireEverDetected` false | ✅ pass |
| AC-5.3 | Ran `make lint`; observed `--- no wall-clock read or unseeded RNG in the game-state mechanism (AC-5.3) ---` block over the exact 5-file game-state set | No `<chrono>`/`<ctime>`/RNG token in `game_state.{h,cpp}` or its test/fixture files | `make lint OK`, block reported no failure | ✅ pass |
| NFR-2 | `make lint` (`--- no dynamic allocation in the game-state test file set ---`) | Zero dynamic allocation in component + test fixture | Green | ✅ pass |
| NFR-3 | `make test-asan FILTER=game_state` | Zero sanitizer findings across US-2/US-4 held-button sequences and US-5 replay/sweep | `17 passed, 0 failed`, no ASan/UBSan abort | ✅ pass |
| NFR-4 | `make test-gcc` (full suite, g++-alias) + `make lint` (`--- no integer standing in for a GameState ---`); read `game_state.h:31` — no `= <digit>` on any enumerator | Compiles clean `-std=c++17 -Wall -Wextra -Werror`; no state numeral outside the enum's own definition | `109 passed, 0 failed` under g++; lint block green; header confirmed literal-free by direct read | ✅ pass |
| NFR-5 | AC-5.1–5.3 above, all passed | Same sequence + same initial state → identical state sequence; no wall-clock/RNG | Confirmed | ✅ pass |
| NFR-6 | Read `game_state.h:25-82` in full | Exactly `GameState` (3 values) + `GameSession` with `advance`/`state`; nothing else; `GameInput`/`GameLoop` untouched | Confirmed by direct read — no extra public symbol, `game_loop.h` only `#include`d, not modified (`git status` shows it untracked-clean) | ✅ pass |
| NFR-7 | Read `game_state.h:5-66` doc comment in full | States exact 3 states + initial; edge-triggering and why; direct-restart citing Principle 4; caller-signalled game-over, GameInput-decoupled; standalone-testable; inherited no-throw/no-alloc contract; one usage example | Every clause present verbatim (lines 5-23 usage example; 39-66 contract clauses) | ✅ pass |
| NFR-1, 8, 9, 10, 11 | N/A per spec §5 (O(1) enum compare, no logging surface, offline/no-PII, no display driver yet, no external dependency) | — | — | N/A (spec-declared) |

## 3. Exploratory Findings

Beyond the ACs, the substitute method's own tooling was pushed harder than the plan's minimum:

- Re-ran the full `game_state` filter twice (plain + ASan) rather than trusting the combined `test-all` run alone, to get per-feature traceability independent of the other 92 tests in the binary — no discrepancy from the combined run.
- Confirmed `make test FILTER=this_filter_matches_nothing_zzz`-style zero-match protection exists and was exercised by `test-negative` in this same run (`ERROR: no test matched filter`, non-zero exit) — so a mistyped `FILTER=game_state` typo could not have silently reported false success; it would have failed loud.
- Inspected `docs/host-tests.md` for whether the new tests/lint rules are documented: the three new lint rules (game-state clock/RNG, game-state test-set allocation, GameState-integer-literal) are documented in the `make lint` row; the four new named tests are not individually listed by name (review's own F8, `accepted`, artifact-wording-only, capped Minor by review's own ruling — not re-flagged here as it changes no verdict, no gate answer, no Must AC).
- Checked `git status` on the feature's new/modified files — matches plan §2 exactly (new: `game_state.h/.cpp`, 3 test/fixture files; modified: `check_constraints.sh`, `docs/host-tests.md`); no stray change.

No bugs found. Table intentionally empty (0 rows) — nothing to report.

| # | Severity | Steps to reproduce | Expected vs. observed | Status |
|---|---|---|---|---|

## 4. Console & Network

N/A — no browser, no console, no network surface exists for this feature (constitution §8: `Browser-observable surface: no`). Substituted checks: compiler warnings (`-Wall -Wextra -Werror`, clean across clang and g++-alias) and sanitizer output (ASan/UBSan, zero findings) — both clean, observed directly in the command output above, not inferred.

## 5. Verdict

Would demo this now. Every one of the 15 Must acceptance criteria (AC-1.1 through AC-5.3) was verified by actually running the named host test and reading its pass/fail output — not by reading the test source and assuming it works. The full gate (`make clean && make test-all`) is green end to end: 109/0 across three C++ configurations, 3 benches OK, 17 Python/round-trip tests OK, lint OK. The feature-scoped filter (`make test FILTER=game_state`) independently confirms 17/17 game-state tests pass under both the plain and ASan builds. No AC needed the framebuffer-dump half of the substitute method — this is a pure state machine with zero rendering output, exactly as the spec states (§2 "Not a user of this feature: the console player... no display driver, no rendered text exists yet"), so nothing was skipped or asserted without evidence. No Blocker, Major or Minor bugs found in this round.

---

## ✅ QA GATE

- [x] Every Must-story acceptance criterion verified by the declared substitute method (host unit tests) and passed
- [x] Every applicable NFR verified and passed (NFR-2..7); NFR-1/8/9/10/11 correctly N/A per spec
- [x] No open Blocker or Major bugs (none found; 0 Minor bugs either)
- [x] N/A — no browser console (§8, no browser-observable surface); compiler warnings and sanitizer output substituted, both clean
- [x] N/A — no viewports; substitute method (host toolchain: clang 14.0.3, g++-alias, no ESP-IDF/board) recorded in §1, matching what §8 says is enforceable today
- [x] Line budget respected: Ist 108 / Soll ~130 (excluding HTML comments)
- [x] Status set to `passed`
