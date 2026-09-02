# Review Report: game-state-management

| | |
|---|---|
| **Phase** | Review |
| **Owner** | Reviewer (`/peer-review`) |
| **Input** | The diff of `/increment`, `.spark/game-state-management/plan.md` |
| **Status** | `passed` |
| **Round** | 2 |
| **Date** | 2026-09-02 |

**Handoff**
- **Status:** mirrors the header table above (authoritative for `Status`).
- **Verdict:** Passed at round 2. F2, F3 and F4 are confirmed fixed — each re-verified by me with the mutant it claims to catch, injected and reverted, not taken from the fix description. F1/F7 stayed fixed. F5, F6, F8 are user-accepted. No open Blocker or Major; gate closed.
- **Open:** `0 open` — Blockers: `none`; Majors: `none` (`F2` fixed r2); Minors: `none` open (`F3`, `F4` fixed r2; `F5`, `F9` closed); Nits: `F6`, `F8` accepted (see §3)
- **Binding ruling:** §6 Verdict and the gate checklist below — the only binding location; there is no other round to point to
- **On conflict:** the numbered body below wins for everything except `Status`; log the mismatch as a finding at the next `/peer-review` and proceed — don't stop on it.

## 1. Scope

Round 2 (re-review). Scope: the round-1 fix delta in `firmware/steamcore/test/game_state_test.cpp` (the `sessionInPlayingWithStartReleased` helper, the rewritten AC-3.2 combo test, `labelOfState`, the new `game_state_playing_ignores_rising_start_when_session_also_ends`), read in full against `game_state.h`/`.cpp`; plus a confirmation pass over the rest of the round-1 scope (`game_state_determinism_test.cpp`, `session_replay_fixture.h`, `tools/check_constraints.sh`, `docs/host-tests.md` — all unchanged since round 1) and `git diff HEAD -- README.md` for F5.

Re-derived from source this round rather than cited (condition (a), verifying a fix to that very fact, for all three; also (b) for F2, a Must-AC verification): the F2, F3 and F4 fix claims — each mutation-tested by me, injected and reverted, see §3. Everything round 1 established and this round did not touch (the `-Wswitch` break, the three lint rules, the six earlier `advance` mutants) is cited from round 1's own §2/§4, not re-derived. `make clean && make test-all` re-run green after every mutant was reverted: 109 passed / 0 failed in each of the four C++ configurations, 3× `BENCH OK`, 15 Python + 2 round-trip, `make lint` OK.

Not reviewed: `assets/**` and `.spark/game-state-management/` (excluded by the caller). No tool file was passed, so scoping was done by hand; no blast-radius query is cited.

## 2. Plan Conformance

| Task | Implemented as planned? | Note |
|---|---|---|
| T1 | ✅ | `game_state.h:31,67-80`, `game_state.cpp:5-20`; one `switch`, no `default`; no Makefile edit, no new `-I`. |
| T2 | ✅ | Re-verified myself: a fourth enumerator breaks the build in three places (`game_state.cpp:9`, `game_state_test.cpp:55`, `game_state_determinism_test.cpp:44`) with the quoted `-Werror,-Wswitch` error; injected `new` in the test set fails `make lint`. |
| T3 | ✅ | `game_state_test.cpp:79-108`; assertions after every step, as required. |
| T4 | ✅ (r2) | (b) now meets its DoD: the four combos are driven from `sessionInPlayingWithStartReleased()` (`game_state_test.cpp:57-61,151`), so each `start = true` combo is a genuine rising edge — F2 closed, mutation-confirmed. (a), (c) and the READY case unchanged. |
| T5 | ✅ | Including (d): I mutated `advance` to level-triggering and `game_state_ending_while_start_held_does_not_auto_restart` (`:190`) was the only one of 16 tests that failed — exactly as §4 predicted. |
| T6 | ✅ | `session_replay_fixture.h`, 20 hand-written steps; coverage test re-derives by replaying (`game_state_determinism_test.cpp:20-89`). |
| T7 | ✅ | Lockstep (`:95`) and the asymmetric sweep (`:166`). Mutating `fire` into a GAME_OVER restart fails both `!fireEverDetected` and AC-4.3's test. |
| T8 | ✅ | `check_constraints.sh:131-232`; all three new checks and the game-loop regression re-verified failing/green by me. |
| T9 | ⚠️ | Doc comment covers every NFR-7 clause and the usage example; `docs/host-tests.md` records the new lint rules but not the new tests (F8) — deviation accepted by the user. |
| — | ⚠️ | `README.md` is still modified in the working tree although plan §2 lists it "Untouched, by design" — deviation ruled on by the user: the change belongs to `rendering-core` and is excluded from this feature's commit (F5). |

## 3. Findings

| # | Severity | Location | Finding | Status |
|---|---|---|---|---|
| F1 | Minor | `firmware/steamcore/test/game_state_test.cpp:29,41,55` | `sessionInPlaying`, `sessionInGameOver` and `nameOfState` had external linkage, unlike every sibling test file (`clipping_test.cpp:10`, `framebuffer_test.cpp:17`, `game_loop_test.cpp:11`, and this feature's own `game_state_determinism_test.cpp:118`). All `test/*_test.cpp` link into one binary (`Makefile:37,65`); `allPixelsEqual` already exists twice and only links because both copies are anonymous. A future test file reusing one of these names is a link error at best, an ODR violation at worst. Fix applied: wrapped the three helpers in an anonymous namespace; no assertion or logic touched, suite re-run green. | fixed r1 |
| F2 | Major | `firmware/steamcore/test/game_state_test.cpp:119-136` | AC-3.2 names two cases — "**pressing** or holding either button mid-play never ends or restarts a session". Only *holding* is tested: all four combos are driven from `sessionInPlaying()`, which advanced with `start = true`, so `prevStart_` is already `true` and none of the `start = true` combos is a rising edge. Plan T4(b) explicitly required "each including a rising `start`". Proven, not suspected: I mutated `advance`'s PLAYING arm to `if (startRising) state_ = READY` and, separately, to `if (sessionEnded \|\| startRising) state_ = GAME_OVER` — both violate AC-3.2 outright and **both pass all 16 game-state tests**, including the lockstep replay and the sensitivity sweep (the sweep only asserts "at least one step detected", already satisfied elsewhere). This is the false-green class spec A8/C7 exists to prevent. Fix applied: added `sessionInPlayingWithStartReleased()` (releases `start` for one step after reaching PLAYING) and drive the AC-3.2 combos from it, so each `start = true` combo is a genuine rising edge. **Confirmed r2, re-derived not trusted:** I injected that same mutant (`if (startRising) state_ = READY;` in the PLAYING arm, `game_state.cpp:14`) — the fixed test fails at `game_state_test.cpp:153` (108/1); with the *old* `sessionInPlaying()` driver restored and the mutant still live, the whole suite goes 109/0 green, which is the round-1 gap reproduced exactly. A third mutant (`if (input.start && !startRising) state_ = READY;`, the *holding* half) fails 3 other tests, so the fix bought "pressing" without losing "holding". All reverted; 109/0 green. | fixed r2 |
| F3 | Minor | `firmware/steamcore/test/game_state_test.cpp:55-72` | NFR-4 requires "no integer literal standing in for a state anywhere outside the type's own definition", yet `nameOfState` maps the three states to `0`/`1`/`2` and lines 70-72 assert exactly those literals — `0` stands in for `READY`. It also pins an enumerator *ordering* that plan §1 Decision 3 deliberately left unspecified, and the name says "name" while the return type is `int32_t` (with an unreachable `return -1`). The T8 lint only greps `game_state.h`, so it cannot see this. Fix applied: renamed to `labelOfState`, returns `const char*` (`"READY"`/`"PLAYING"`/`"GAME_OVER"`), test asserts via `std::strcmp`. Exhaustive `default`-less switch kept — that's the AC-1.2 mechanism, untouched. **Confirmed r2:** `nameOfState` no longer exists anywhere under `firmware/`, `tools/`, `docs/`; `labelOfState` (`game_state_test.cpp:74-84`) returns `const char*` and the assertions (`:89-91`) compare strings, no state ordinal left in the file; `make lint`'s NFR-4 GameState block green. | fixed r2 |
| F4 | Minor | `firmware/steamcore/include/steamcore/game_state.h:60-61` | The header promises "at most one transition happens per call, evaluated against the state the call began in" and that `sessionEnded` is ignored outside PLAYING — a public contract clause (NFR-7) with no test. Fix applied: new test `game_state_playing_ignores_rising_start_when_session_also_ends` — from PLAYING with `start` released for one step, `advance({true,false}, sessionEnded=true)` must land GAME_OVER (only `sessionEnded` decides from PLAYING). **Confirmed r2, both halves re-derived:** I replaced the switch in `game_state.cpp:9-19` with the literal 3-statement if-chain (`READY→PLAYING`, `PLAYING→GAME_OVER`, `GAME_OVER→PLAYING`, each re-reading `state_`). (a) A temporary probe starting from a fresh READY with rising `start` + `sessionEnded=true` **passes** under the chain — the 3-way flip does cancel, so round 1's originally-proposed mutant really is a dead end. (b) Starting from PLAYING the chain fails exactly and only `game_state_test.cpp:249` (108/1). Probe deleted, mutant reverted, 109/0 green. | fixed r2 |
| F5 | Minor | `README.md:610-628` | The diff ticks "Framebuffer" and "Sprite-System" and appends a note about them — content belonging to the `rendering-core` increment (last README commit: `e5d4be3`), not to this feature; it does **not** tick "Game State Management". Plan §2 lists `README.md` under "Untouched, by design", so this is an undocumented deviation that would misattribute another increment's doc update to this feature's commit. Fix: leave `README.md` out of this feature's commit and land it separately. **r2 ruling (user):** accepted as `rendering-core` work, excluded from this feature's commit. `git diff HEAD -- README.md` still shows those two hunks, since nothing in this feature is committed yet — the exclusion is a `git add` discipline at commit time, not something git can show as done today. Nothing to re-flag; not this feature's content. | accepted |
| F6 | Nit | `firmware/steamcore/test/session_replay_fixture.h:62` | `return kSteps[stepIndex];` has no bounds handling; an out-of-range or negative index is UB. The existing precedent this file mirrors, `replay_fixture.h:31`, is total by construction (`tickIndex % 4`). Current callers are all in-range and ASan covers the suite, but this is a shared header future tests will call. Fix: state the precondition `0 <= stepIndex < kSessionReplaySteps` in the doc comment above the function. | accepted |
| F7 | Nit | `firmware/steamcore/test/game_state_test.cpp:49-54` | The AC-1.2 explanatory comment sat directly above `sessionInPlaying()`, a helper it has nothing to do with; the switch it describes is `nameOfState`, 20 lines further down. Fix applied: moved the comment above `nameOfState`. | fixed r1 |
| F8 | Nit | `docs/host-tests.md:25` | T9's DoD says the doc "records the new tests and lint rules"; only the lint rules were added. The four new test names (and the fixture) appear nowhere. Artifact-wording only, changes no verdict. Fix: one sentence in the `make test` row, or accept and amend T9. | accepted |
| F9 | Minor | `.spark/game-state-management/review.md:15` (round-1 revision) | The round-1 Handoff block claimed `0 open` and "`F5` accepted-excluded" while §3's F5 Status cell still read `open` and the gate had three unchecked boxes. The block's own conflict rule makes the body authoritative, so I proceeded on `F5 = open` and reviewed it — but a consumer reading only the block would have seen a closed report where the body said otherwise. Artifact wording only: changes no verdict, no gate answer, no Must AC, so capped at Minor. Fix applied: this round's Handoff and the F5 cell now agree, and F5 carries the user's ruling explicitly. | fixed r2 |

## 4. Requirements Traceability

| Spec ID | Implemented at | Verdict |
|---|---|---|
| AC-1.1 | `game_state.h:78`; test `game_state_test.cpp:11` | ✅ met |
| AC-1.2 | `game_state.h:31` + `default`-less switches (`game_state.cpp:9`, `game_state_test.cpp:56`); build break re-verified | ✅ met |
| AC-1.3 | `game_state.h`/`.cpp` under `include/`+`src/`; `check_constraints.sh` general sweep (injected `new` observed failing) | ✅ met |
| AC-2.1 | `game_state.cpp:6,10-12`; test `:79-88` (assert after every step) | ✅ met |
| AC-2.2 | `game_state.h:79` (`prevStart_ = false`); test `:93-97` | ✅ met |
| AC-2.3 | test `:100-108` | ✅ met |
| AC-3.1 | `game_state.cpp:13-15`; test `:111-115` | ✅ met |
| AC-3.2 | `game_state.cpp:13-15`; test `:143-155` (pressing, via `sessionInPlayingWithStartReleased`) + `:98-108` and the fixture's held run (holding) | ✅ met r2 — both halves mutation-confirmed (F2) |
| AC-3.3 | `game_state.cpp:14`; test `:136-143` | ✅ met |
| AC-4.1 | `game_state.cpp:16-18`; test `:155-162` | ✅ met |
| AC-4.2 | test `:165-174` | ✅ met |
| AC-4.3 | test `:177-181`; sweep `game_state_determinism_test.cpp:179` | ✅ met |
| AC-5.1 | `game_state_determinism_test.cpp:95-116` + fixture coverage test `:20-89` | ✅ met |
| AC-5.2 | `game_state_determinism_test.cpp:128-180` (asymmetric: `start`/`sessionEnded` detected, `fire` never) | ✅ met |
| AC-5.3 | `check_constraints.sh:186-200`; injected `<ctime>` observed failing | ✅ met |
| NFR-2 | `check_constraints.sh:131-132,202-216`; injected `new` observed failing | ✅ met |
| NFR-3 | `make test-asan` green over the held-button batteries and the 20×3×20 sweep | ✅ met |
| NFR-4 | `-std=c++17 -Wall -Wextra -Werror` (`Makefile:16`), `make test-gcc` green (nominal — `/usr/bin/g++` is Apple clang); `check_constraints.sh:218-232` | ✅ met r2 — no state ordinal left anywhere outside the enum (F3) |
| NFR-5 | `game_state.cpp:5-20` (pure function of prior state + step); AC-5.1/5.2 + clock/RNG grep | ✅ met |
| NFR-6 | `game_state.h:31,67-80` — audited symbol by symbol: `GameState` + 3 values, `GameSession`, `advance`, `state`. Nothing else; `GameInput`/`GameLoop` untouched (`git diff` clean on `game_loop.h`) | ✅ met |
| NFR-7 | `game_state.h:5-23` (usage example) and `:39-66` — every clause the NFR enumerates is present, including the Principle 4 citation | ✅ met |

## 5. What Was Checked

- [x] Correctness: logic does what the acceptance criteria demand — traced AC by AC; six mutants at round 1, four more at round 2 (F2's original, F2's old-driver counterfactual, F2's holding-half, F4's if-chain in both its READY and PLAYING starting positions), each injected and reverted
- [x] Non-functional: applicable NFRs and constitution quality bars hold (no allocation, no ESP-IDF header, no clock/RNG, C++17, `steamcore` namespace, `.h`/`.cpp` pair, English, no hardware claim)
- [x] Error handling: no failure mode exists — `advance` cannot fail, returns void, throws nothing; nothing is swallowed
- [x] Security: N/A per NFR-9 — offline device, no input persisted, no secret, no untrusted parsing in the diff
- [x] Tests: exist, pass, and were probed for sensitivity — round 1's two gaps (F2, F4) re-probed at round 2 and both now close on the mutant they claim to catch
- [x] Readability: transition table is one 12-line switch; contract doc is complete; the round-2 test delta reads cleanly — the new helper's comment says why it releases `start`, and the F4 test's comment spells out the double-transition it exists to catch
- [x] Library lens: public surface minimal and intentional (NFR-6); purely additive, no export removed/renamed/re-signed, so no compatibility break; zero new dependencies; contract and error behaviour documented (NFR-7). Packaging/semver are constitution-declared no-ops for a statically-linked image

## 6. Verdict

**Round 2 — passed.** The one thing that held this back at round 1 was a Must criterion certified by a suite that could not fail for it, and that is now genuinely closed: I re-injected the exact mutant round 1 caught nothing with (`if (startRising) state_ = READY;` in the PLAYING arm) and the rewritten AC-3.2 combo test fails at `game_state_test.cpp:153`; to be sure it is the *helper* doing the work and not luck, I put the old `sessionInPlaying()` driver back with the same mutant still live and watched the whole suite return 109/0 green — round 1's false green, reproduced on demand. A third mutant reacting to a *held* `start` mid-play fails three other tests, so the fix did not trade one half of "pressing or holding" for the other. F4's claim survives the same treatment, including its awkward part: the literal if-chain rewrite really does cancel back to the correct answer when the call starts in READY (I probed it and it passed), and really does double-transition to the wrong state when the call starts in PLAYING, where the new test catches it and nothing else in the suite does — so the fix targets the only starting position that distinguishes them, and round 1's own note about the dead end was accurate rather than an excuse. F3 is the cheap one to confirm: `nameOfState` is gone from the tree, `labelOfState` returns a `const char*`, and no state ordinal survives outside the enum. Every mutant was reverted and `make clean && make test-all` is green afterwards — 109/0 in four C++ configurations, three benches, 17 Python tests, `make lint` OK. What remains unfixed is `accepted`, not open, and all of it was ruled on by the user: F5 (the `README.md` hunks still sit in the working tree and must simply not be staged with this feature — they are `rendering-core` content), F6 and F8. The one new thing I found is bookkeeping, not code: round 1's Handoff block and its own body disagreed about whether anything was open (F9, fixed here). Gate closed.

The mechanism itself is unchanged since round 1 and still holds up: `advance` is a pure function of its own prior state and the step's arguments, the transition table matches plan §1 Decision 2 exactly, the two-layer determinism proof is real (round 1's level-triggered-restart, `fire`-driven-restart and `prevStart_` mutants each caught by a named test), and the three lint rules each fail on an injected violation with the game-loop block still armed after the shared-pattern refactor.

**User routing (round 1, 2026-09-02):** F2 → fix in `/increment`, then re-review. F3, F4 → fix alongside F2 in the same `/increment` pass (both cheap, both close real contract/NFR gaps). F5 → `README.md` excluded from this feature's commit; it belongs to and lands with the already-shipped `rendering-core` increment, not here. F6, F8 → `accepted` — cosmetic, left open for a future pass.

---

## ✅ REVIEW GATE

*All boxes checked → `/demo-day` may start. Any box open → back to `/increment`. On
re-review, edit this same checklist in place — never duplicate it as a second gate.*

- [x] No open Blocker findings — none at either round
- [x] No open Major findings — F2 confirmed fixed at round 2 by re-injecting its own mutant; no waiver needed
- [x] Every Must AC traces to implementing code; no constitution non-negotiable violated — AC-3.2 now verified for both "pressing" and "holding" (§4), and no non-negotiable is violated
- [x] All plan deviations documented and accepted — T4(b) closed (F2 fixed r2); T9 (F8) and the `README.md` change (F5) both ruled on by the user and recorded in §2/§3
- [x] Test suite runs green — `make clean && make test-all` after every round-2 mutant was reverted: 109 passed / 0 failed in each of the four C++ configurations, 3× `BENCH OK`, 15 Python + 2 round-trip, `sips` cross-check OK, `make lint` OK
- [x] Line budget respected: Ist 113 / Soll ~150 (excluding HTML comments)
- [x] Status set to `passed`
