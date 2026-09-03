# QA Report: game-loop

| | |
|---|---|
| **Phase** | Review (hands-on) |
| **Owner** | QA Tester (`/demo-day`) |
| **Input** | `.spark/game-loop/spec.md` (`approved`); substitute QA method per `.spark/constitution.md` §8 |
| **Status** | `passed` |
| **Round** | 1 |
| **Date** | 2026-09-02 |

**Handoff**
- **Status:** `passed`.
- **Verdict:** Demoable as a host-verified engine mechanism. `game-loop` has no rendering output of its own (spec §8 N/A, NFR-10 N/A) and its own plan explicitly scopes it to host unit tests only — confirmed from spec.md/plan.md rather than assumed. All 15 Must ACs and all applicable NFRs verified by running the real host test suite, not by reading source. Own mutation testing (call-order swap, wall-clock injection, dynamic-allocation injection, dropped-branch determinism fixture) confirms the gates catch exactly the defect classes they claim to.
- **Open:** `none` — 0 open Blockers/Majors/Minors found this round.
- **Binding ruling:** §5 Verdict and the gate checklist below — the only binding location; there is no other round to point to.
- **On conflict:** the numbered body below wins for everything except `Status`; log the mismatch as a finding at the next `/demo-day` and proceed — don't stop on it.

## 1. Test Environment

- **App URL:** N/A — no browser-drivable surface (`.spark/constitution.md` §8: `Browser-observable surface: no`). Confirmed complete declaration (surface `no`, method named, performable) before proceeding — see reasoning below.
- **Browser / viewport(s):** N/A. `game-loop` is a pure timing/call-order mechanism (`GameLoop<Game>`, fixed `update`→`render` per tick) with no rendering output of its own — spec §8 Design Review and NFR-10 both record this explicitly ("nothing in this increment reaches a display driver or panel"), and the plan's own Test Strategy states "`/demo-day` in a browser — no browser-observable surface... a fourth dump would be surface without a consumer." Confirmed from spec.md/plan.md, not assumed. The device-side `Framebuffer`/rendering integration also does not exist yet (today's only device work is the unrelated write-only color-test spike recorded in constitution §3) — so the framebuffer-dump-over-USB-CDC half of §8's substitute method is not enforceable for any feature yet, this one included, and nothing here is claimed as hardware-verified.
- **Substitute method used:** host-compiled unit tests (constitution §8, enforceable today), run for real from a clean checkout — `make clean && make test-all` and targeted `make test`/`make test-asan` filters — never source reading. Toolchain: Apple clang 14.0.3 (`clang++`) and `/usr/bin/g++` (itself clang on this host, honestly reported as such, matching plan §4/review's own caveat), `-std=c++17 -Wall -Wextra -Werror`, from repo root `/Users/andreaslottes/steamcore` via the top-level `Makefile`.
- **Test data / accounts used:** N/A — no accounts, no persisted data. Test fixtures: the in-repo `ReplayGame`/`CountingGame`/`LoggingGame` synthetic consumers in `firmware/steamcore/test/{replay_fixture.h, game_loop_test.cpp}` (spec A10), not a real game.

## 2. Acceptance Criteria Verification

All commands run today from `/Users/andreaslottes/steamcore`. Full clean run: `make clean && make test-all` → 109 C++ tests × 4 configurations (`test`, `test-negative`, `test-asan`, `test-gcc`) all green, 3 bench binaries `BENCH OK`, 15 Python + 2 round-trip tests OK, `sips` cross-check OK, `make lint` OK — exit 0, log captured. Targeted: `make test FILTER=game_loop` → 14 passed, 0 failed; `make test-asan FILTER=game_loop` → 14 passed, 0 findings; `make test FILTER=fb_compare` → 5 passed, 0 failed.

| Spec ID | Steps performed | Expected | Observed | Result |
|---|---|---|---|---|
| AC-1.1 | Ran `game_loop_n_one_strict_interleaving`, `game_loop_n_hundred_strict_interleaving` (N=1,100) plus `game_loop_one_tick_calls_update_then_render_once` under `make test FILTER=game_loop`; own mutation swapped `tick()`'s call order (`game_loop.h`) | update/render called exactly N times, strictly alternating update(i)→render(i)→update(i+1) | 14/14 pass on unmodified code; swapped-order mutation immediately failed `game_loop_n_one_strict_interleaving`, `game_loop_n_hundred_strict_interleaving` + 2 others (10 passed, 4 failed), naming `game_loop_test.cpp:56` | ✅ pass |
| AC-1.2 | Ran `game_loop_n_zero_never_calls_update_or_render` | N=0 → neither called, no crash | Passed; asserts both counters are 0 | ✅ pass |
| AC-1.3 | Ran `make lint`; own mutation injected `std::chrono::steady_clock::now()` into `game_loop.h` and re-ran `make lint` | No wall-clock/system-time call in the mechanism; grep gate enforces it | `make lint OK` on unmodified tree; injected clock read caused `make lint FAILED`, naming `game_loop.h:3` and `:114` exactly | ✅ pass |
| AC-1.4 | Ran `make test-asan FILTER=game_loop` (N=0/1/100 cases included) | Zero ASan/UBSan findings | 14 passed, 0 findings | ✅ pass |
| AC-2.1 | Ran `game_loop_each_tick_observes_exactly_its_own_input` | update(i) observes exactly index i's GameInput, never i-1/i+1 | Passed | ✅ pass |
| AC-2.2 | Ran `game_loop_render_always_receives_the_same_framebuffer_instance` | render draws into the same Framebuffer instance every tick (address identity) | Passed — 100 ticks, all recorded addresses equal | ✅ pass |
| AC-2.3 | Ran `game_loop_never_implicitly_clears_between_ticks`; observed in call-order mutation above that this test also fails when order is swapped | Two consecutive un-cleared draws both still present after tick 2 | Passed on unmodified code; failed under order-swap mutation, confirming it actually exercises the contract | ✅ pass |
| AC-2.4 | Ran `game_loop_render_observes_this_ticks_update_state` | render observes exactly the state update left for that same tick | Passed; also failed under order-swap mutation (see AC-1.1) | ✅ pass |
| AC-3.1 | Ran `game_loop_input_default_constructs_both_false`; inspected `static_assert(sizeof(GameInput) == 2 * sizeof(bool))` at `game_loop.h:36` | GameInput carries exactly start/fire, no third field | Passed; a third field breaks the build per the static_assert (confirmed present, not re-triggered — build-break class, not a runtime test) | ✅ pass |
| AC-3.2 | Ran `game_loop_single_tick_input_is_raw_pass_through` | Both fields pass through unmodified, no debounce/edge-detect/default substitution | Passed | ✅ pass |
| AC-3.3 | Ran `game_loop_each_tick_observes_exactly_its_own_input` (covers all four combos) plus `game_loop_replay_sequence_covers_all_four_combinations` | AC-2.1 holds individually for all four start/fire combinations | Passed | ✅ pass |
| AC-4.1 | Ran `game_loop_replay_is_deterministic_after_every_tick` (53 ≥ 50 ticks); own mutation dropped the `start`-handling branch in `ReplayGame::update` (`replay_fixture.h`) and re-ran | Two independent runs byte-identical after every individual tick; a consumer that ignores input cannot pass | Passed on unmodified code (framebuffers compared via `fb_compare.h` after each of 53 ticks); dropped-branch mutation caused exactly `game_loop_replay_fixture_is_sensitive_to_start_changes` to fail (13 passed, 1 failed) — the sensitivity control is armed for `start`, closing review's F1 as re-verified independently this round | ✅ pass |
| AC-4.2 | Read `replay_fixture.h` for randomness use (none present); ran `make lint` (AC-4.4 rule also covers RNG tokens) | No unseeded/time-derived RNG anywhere in the fixture | Confirmed by construction (no RNG call exists) and by the passing lint gate | ✅ pass |
| AC-4.3 | Ran `make test-asan FILTER=game_loop` (includes the 53-tick replay test) | Zero ASan/UBSan findings on the full replay run | 14 passed, 0 findings | ✅ pass |
| AC-4.4 | Ran `make lint`; own mutations (clock read, `std::vector` injection) both into `game_loop.h` | No wall-clock read, no unseeded RNG anywhere in the delivered game-loop files | `make lint OK` on unmodified tree; both injected mutations caused `make lint FAILED`, naming the exact file/line under the "no wall-clock read or unseeded RNG in the game-loop mechanism" and "no dynamic allocation in the game-loop file set" rules | ✅ pass |
| NFR-1 | Ran `make bench` (`steamcore_bench_game_loop`) twice (full clean run + standalone) | 53-tick replay (both runs combined) < 5 ms at `-O2` | Observed 0.0008–0.0015 ms across runs, well under budget; separate non-gating comparison-cost line (0.17–0.31 ms per `framebuffersEqual` call) correctly labeled "not part of the budget" (review F4/F12 fix confirmed still in place) | ✅ pass |
| NFR-2 | Ran `make lint`; own mutation injected `std::vector<int> scratch; scratch.push_back(1);` into `game_loop.h::tick()` | Zero dynamic allocation in mechanism, GameInput, test fixture | `make lint OK` on unmodified tree; injected `std::vector` caused `make lint FAILED` on both the general include/src rule and the game-loop-specific test-set extension, naming `game_loop.h:114` | ✅ pass |
| NFR-3 | Ran `make test-asan` (full suite, 109 tests) and `make test-asan FILTER=game_loop` | Zero sanitizer findings across every AC | 109 passed / 0 findings (full); 14 passed / 0 findings (game-loop only) | ✅ pass |
| NFR-4 | Ran `make test` (clang) and `make test-gcc` (g++, itself clang on this host — honestly reported nominal per plan/review) | Compiles clean, no ESP-IDF header, no new resolution/tile literal | Both green, 109/109; grepped `game_loop.h`/test files for ESP-IDF headers and resolution/tile literals — none found outside `config.h` | ✅ pass |
| NFR-5 | Ran `game_loop_replay_is_deterministic_after_every_tick`; own mutation as AC-4.1 | Same input sequence + same initial state → byte-identical framebuffer sequence; no clock/unseeded-RNG code | Passed; mutation testing (AC-4.1, AC-1.3/AC-4.4) confirms both the positive proof and its negative controls | ✅ pass |
| NFR-6/7 (library lens, scoped) | Read `game_loop.h` public surface: `GameInput{start,fire}`, `GameLoop<Game>`, `tick()`; read the doc comment | Exactly one tick-driving entry point; doc states call order, no-implicit-clear, no-real-time-pacing, inherited contract, GameInput fields, one usage example | Confirmed — matches review's NFR-6/NFR-7 audit (F7/F10 fixed, `detail::kGameHasUpdate`/`kGameHasRender` named explicitly as the two reachable-but-conventionally-private symbols); usage example present and compiles as part of `make test` | ✅ pass |
| NFR-8/9/10/11 | N/A per spec (no runtime failure mode/logging surface, no persisted/personal data, no display driver reached yet, no external dependency) | — | Confirmed N/A by reading spec §5 justification against the actual delivered file set — no contradiction found | N/A |

## 3. Exploratory Findings

Beyond the AC battery, this round's exploration *is* mutation testing (the substitute method's equivalent of clicking around off the happy path — deliberately breaking the mechanism's own invariants to see whether the gates that exist to catch them actually do): call-order swap, wall-clock injection, dynamic-allocation injection, and a dropped-branch determinism-fixture mutation, all detailed in §2 and all reverted (confirmed via `git status`/`git checkout` back to a clean working tree matching the committed baseline before the final `make clean && make test-all` re-run). No mutation produced a false green — every one was caught by exactly the test/gate its own AC names, and no unrelated test was disturbed.

No new defects found. Nothing to report in this table.

| # | Severity | Steps to reproduce | Expected vs. observed | Status |
|---|---|---|---|---|
| — | — | — | — | — |

## 4. Console & Network

N/A — no browser, no console, no network surface for this feature (constitution §8). Checked instead: compiler warnings (`-Wall -Wextra -Werror` — zero warnings across `test`, `test-gcc`, `test-asan`, `bench` builds) and sanitizer output (`test-asan` — zero ASan/UBSan findings, full suite and game-loop-filtered). Both clean.

## 5. Verdict

Would I demo this to a stakeholder right now? For what it actually is — a hardware-independent tick mechanism with a determinism proof — yes. Every Must AC (15/15) and every applicable NFR is verified by running the real test suite from a clean checkout, not by reading the header. The centerpiece the task asked me to stress — the determinism guarantee — held up under my own mutation testing: swapping the tick() call order breaks four tests immediately and by name; injecting a wall-clock read into the mechanism breaks the lint gate by name and line; injecting a `std::vector` breaks the allocation gate the same way; and dropping the `start`-handling branch from the determinism fixture is caught by exactly the one test built to catch it (`game_loop_replay_fixture_is_sensitive_to_start_changes`), independently re-confirming review's F1 fix rather than trusting the review report's own claim. Nothing here is claimed as hardware-verified, and nothing needed to be — the feature's own spec (§8 Design Review, NFR-10) and plan (Test Strategy) establish, and this round independently confirmed from those documents rather than assumed, that game-loop reaches no display driver and has no browser-observable or framebuffer-dump-observable surface of its own.

---

## ✅ QA GATE

- [x] Every Must-story acceptance criterion verified in the real browser and passed — substituted per constitution §8: verified by running the real host test suite (all 15 ACs, US-1..US-4)
- [x] Every browser-observable NFR verified and passed — N/A here (no browser-observable surface); the applicable host-verifiable NFRs (NFR-1..7) were verified by running the real bench/lint/compiler gates
- [x] No open Blocker or Major bugs (Minor bugs listed and accepted by the user) — 0 open findings
- [x] Browser console free of errors on the tested flows — N/A (§8); substituted: compiler warnings and sanitizer output both clean (§4)
- [x] Tested on all agreed viewports — N/A (§8, no UI surface); substituted: tested under both compiler configurations (clang, g++) and both build modes (`-O2`, ASan/UBSan)
- [x] Line budget respected: Ist 108 / Soll ~130 (excluding HTML comments)
- [x] Status set to `passed`
