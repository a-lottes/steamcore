# Plan: game-state-management

| | |
|---|---|
| **Phase** | Plan |
| **Owner** | Engineering Manager (`/sprint-plan`) |
| **Input** | `.spark/game-state-management/spec.md` (`approved`) |
| **Status** | `approved` |
| **Date** | 2026-09-02 |

**Handoff**
- **Status:** `approved` — user approved as proposed, including AC-5.2's "detected at that step or later" interpretation (not permanent divergence).
- **Summary:** One `.h`/`.cpp` pair, `steamcore/game_state.{h,cpp}`: an unnumbered `enum class GameState { READY, PLAYING, GAME_OVER }` plus a stateful `GameSession` with exactly one entry point, `advance(const GameInput& input, bool sessionEnded)`, and one reader, `state()`. The previous tick's `start` is a private member, so edge detection is the component's own guarantee rather than the caller's bookkeeping; one call per step means at most one transition per step, evaluated against the state the step began in. 9 tasks, no Makefile edit, no new dependency.
- **Open:** `0 tasks not done` — all 9 tasks `done`, no deviations from the approved architecture. Each task's DoD was verified, not asserted: T2's fourth-enumerator break quoted directly (`error: enumeration value 'PAUSED' not handled in switch [-Werror,-Wswitch]`, on both the type's own switch and the test's); T3/T4/T5/T7 each mutation-tested against the exact defect class their AC exists to catch (level-triggered START, GameInput-coupled game-over, hidden-shared-state determinism break, fire coupled into a transition) with the injected bug observed failing before being reverted — T5(d) specifically confirmed a level-triggered restart passes every other test in the suite and is caught only by the die-while-held case, exactly as planned. T8's three new lint checks each demonstrated failing once; one bug found and fixed during that demonstration (the explicit-value regex was anchored to line-start and silently never matched the header's actual single-line enum declaration — caught by verifying the isolated repro instead of trusting the combined one). `make clean && make test-all` green: 108 C++ tests across 4 configurations, 3 bench binaries all `BENCH OK`, 15 Python + 2 round-trip tests, `sips` cross-check OK, `make lint` OK.
- **Binding ruling:** §3 Task Breakdown for current task status; a plan revision after review/QA findings updates §1/§3 in place, never a new section
- **On conflict:** the numbered body below wins for everything except `Status`; log the mismatch as a finding at the next `/peer-review` and proceed — don't stop on it.

## 1. Architecture Decision

- **Context:** The spec fixes the three states, the initial state, the edge-triggered START rule and the
  caller-signalled game-over, and leaves the *shape* open. Two facts constrain that shape hard. First,
  US-2/US-4 need the previous tick's `start` value to exist somewhere — the whole feature is a comparison
  of consecutive values. Second, A7 requires the thing to be exercisable by feeding it a `GameInput`
  sequence directly, with no `GameLoop<Game>` and no synthetic consumer. Everything else (allocation,
  determinism, no ESP-IDF, C++17, `steamcore` namespace) is inherited unchanged from `game-loop`.

- **Decision:**
  1. **A stateful `GameSession` class, not free functions over a plain enum.** A purely functional
     `advance(GameState, bool prevStart, ...)` is only "pure" because it hands the caller the edge-detection
     state to carry — which is precisely the ad-hoc per-game bookkeeping the spec's §1 Problem exists to
     delete, and a caller that forgets to store `prevStart` silently gets level-triggering back. The edge
     state therefore lives inside the type: `GameState state_ = GameState::READY; bool prevStart_ = false;`
     `prevStart_` starting `false` *is* AC-2.2 — the absent prior tick is an implicit "not pressed", so a
     first-ever tick with `start = true` is a rising edge, by construction rather than by a special case.
  2. **Exactly one entry point per step: `void advance(const GameInput& input, bool sessionEnded)`.**
     A5's "one caller-supplied signal **per tick**" binds the signal to a step, and one call is what makes
     "at most one transition per step, evaluated against the state the step began in" structural instead of
     caller discipline — the same species of guarantee as `game-loop` Decision 4. With a separate
     `signalSessionEnded()` the caller's within-tick call order changes the outcome: signalling *before*
     `advance` on a step where `start` also rises would end the session and restart it in the same step.
     `sessionEnded` stays decoupled from `GameInput` (A6/C5: no new field, `game_loop.h` untouched) by being
     its own parameter. Transition rule, complete:
     `READY + startRising → PLAYING`; `PLAYING + sessionEnded → GAME_OVER`; `GAME_OVER + startRising →
     PLAYING`; everything else holds. `sessionEnded` outside PLAYING is ignored (AC-3.3's idempotence falls
     out of that, not out of a separate flag). `fire` is never read at all. `prevStart_ = input.start` runs
     on **every** step regardless of state — that invariant is what keeps a START held *across* a state
     change from firing an edge in the new state (see T5's die-with-START-held test).
  3. **`enum class GameState { READY, PLAYING, GAME_OVER };` with no explicit values and no fixed
     underlying type.** Unlike `Color`, this value is never stored in a byte-per-pixel buffer or a file, so
     nothing needs a wire number — and with no `= 0` anywhere, NFR-4's "no integer literal standing in for a
     state" is true even inside the type's own definition. "Exactly three values" (AC-1.2) is pinned by
     `-Wswitch -Werror`: `advance`'s `switch` and one test's `switch` are exhaustive with **no `default`
     label**, so adding a fourth enumerator is a build break — the same trick `game-loop` used `sizeof` for,
     and cheaper than a public `kStateCount` constant NFR-6 would flag.
  4. **`.h` + `.cpp` in `include/steamcore/` and `src/`, i.e. the constitution §5 norm, not the
     `sprite.h`/`game_loop.h` exception.** Those two are header-only for stated reasons that do not apply
     here: a POD with no out-of-line code, and a class template. `GameSession` is a concrete non-template
     class whose `advance` is real branching logic, so it gets a `.cpp`. Bonus, not decoration:
     `check_constraints.sh` already greps all of `include/` and `src/` for allocation and ESP-IDF headers,
     so AC-1.3 is covered by the existing lint the moment the file lands there. `ENGINE_SRCS`/`TEST_SRCS`
     are wildcards — **the whole feature needs zero Makefile edits.**
  5. **`game_state.h` includes `steamcore/game_loop.h` for `GameInput`.** §6 forbids touching
     `game_loop.h`, so `GameInput` cannot be moved to a header of its own, and A7's "no `GameLoop<Game>`"
     is about instantiation, not about an `#include`. Transitively pulling in `Framebuffer` costs nothing
     at runtime and every real consumer includes both anyway.

- **Alternatives considered:**

  | Alternative | Why rejected |
  |---|---|
  | Free functions over a bare enum: `GameState nextState(GameState, bool prevStart, bool start, bool ended)` | Maximally testable and stateless, but the caller must store and correctly update `prevStart` every tick — re-creating per game exactly the ad-hoc button bookkeeping §1 names as the risk. It reads as pure only by exporting the impurity |
  | Separate `signalSessionEnded()` method alongside `advance(input)` | Two entry points whose within-step order changes the result (die-and-restart in one step if signalled first); "one signal per tick" (A5) becomes unenforced; NFR-6 counts a second entry point as surface. Its one advantage — signalling mid-`update` with no latency — is available anyway by calling `advance` at the end of the consumer's own `update` |
  | `sessionEnded` as a second small enum (`SessionEnd::kEnded`) instead of `bool` | Type-safer at the call site, but NFR-6 allows exactly one new state type plus the component; a second public enum is a review finding by definition |
  | `bool sessionEnded = false` as a defaulted parameter | Makes the "explicit caller-supplied signal" (US-3) implicit and creates a second effective signature for one saved word per call site |
  | `GameState advance(...)` returning the new state, with no `state()` | `render()` and any mid-tick branch must read the state *without* advancing, so `state()` is needed regardless; a returning `advance` would then be a second way to read the same value |
  | Adding a `GameState` field to `GameInput`, or having `GameLoop<Game>` own/branch on a session | Both are explicitly out of scope (§6, A6/C5) and would change the shipped engine↔game contract |
  | Header-only `game_state.h` like `game_loop.h` | Neither stated reason for that deviation applies (no template, not a POD), and it would drop the mechanism out of the existing `src/`+`include/` lint sweep for no gain |
  | A `gameStatesEqual`-style comparison oracle mirroring `fb_compare.h` | `fb_compare.h` exists because comparing 38,400 pixels is hand-written code that can be wrong. Here the comparison is `==` on a scoped enum — a language primitive — and `make test-negative` already proves `CHECK_EQ` fails loudly. A wrapper would add a thing to be wrong, not a proof (see §4 for where that layer's effort goes instead) |
  | A new `check_constraints.sh` rule grepping for state integers generally | No named constant is introduced and no digit stands for a state, so a numeric grep over `game_state.*` would be pure noise. T8 instead greps the two shapes that *would* re-introduce one: an enumerator with an explicit value, and `static_cast<GameState>` |

- **Consequences:** *Easier* — a future `Game::update` composes in two lines
  (`session_.advance(input, died); switch (session_.state()) {...}`), with no bookkeeping of its own; the
  component is testable with a plain local variable and no fixture; the transition table is one switch a
  reviewer reads in ten seconds. *Harder* — a caller who wants a game-over to take effect within the same
  `update` must call `advance` after its own simulation rather than before (documented with a usage example
  in T9), and `game_state.h` now depends on `game_loop.h`, so a future Input Abstraction story that moves
  `GameInput` must update one include. *Deliberately not decided here* — what ends a session (A5), how a
  `Game` branches on the state (§6), and any rendering of it (NFR-10).

## 2. Affected Components

Scoped by hand — no tool file was passed with this task, so no blast-radius query was run and none is
cited here.

- **New:** `firmware/steamcore/include/steamcore/game_state.h`, `firmware/steamcore/src/game_state.cpp`;
  tests `firmware/steamcore/test/game_state_test.cpp`,
  `firmware/steamcore/test/game_state_determinism_test.cpp`, test-only fixture
  `firmware/steamcore/test/session_replay_fixture.h`.
- **Modified:** `tools/check_constraints.sh` (one new scoped block: AC-5.3's clock/RNG grep and NFR-2's
  allocation grep over the new test files, plus NFR-4's two enum-literal shapes), `docs/host-tests.md`.
- **Untouched, by design:** `Makefile` (both source lists are wildcards), `game_loop.h`, `framebuffer.*`,
  `sprite.h`, `color.h`, `config.h`, `font.*`, `text.*`, `dirty_tracker.*`, `dump_format.*`, `tools/*.py`,
  `README.md`, `games/` (§6, A6).
- **New dependencies: none.** Standard library only, and only what `game_loop.h` already pulls in;
  NFR-11 stays N/A. No new bench binary: NFR-1 is N/A per the spec (an enum compare at 60 Hz).
- **Public API surface added (NFR-6), with its named consumer:** `steamcore::GameState` with its three
  values (US-1), `steamcore::GameSession` with `advance(const GameInput&, bool)` (US-2/3/4) and `state()`
  (every consumer). Nothing else — no count constant, no `reset()`, no `isPlaying()`, no fourth state.

## 3. Task Breakdown

| # | Task | Story | Covers (AC / NFR) | Depends on | Status | Definition of Done |
|---|---|---|---|---|---|---|
| T1 | Walking skeleton: `GameState`, `GameSession`, initial state, one transition end to end | US-1, US-2 | AC-1.1, NFR-6 | – | `done` | `game_state.h` declares `enum class GameState { READY, PLAYING, GAME_OVER }` (no explicit values) and `class GameSession` with exactly `void advance(const GameInput&, bool sessionEnded)` and `GameState state() const`; `game_state.cpp` implements the full transition table of §1 Decision 2 as one `switch` with no `default` label; two named tests prove a default-constructed `GameSession` reads READY and that one `advance(GameInput{true, false}, false)` reaches PLAYING; `make test`, `make test-gcc`, `make test-asan` and `make lint` are green with no Makefile edit and no new `-I` path — files: firmware/steamcore/include/steamcore/game_state.h, firmware/steamcore/src/game_state.cpp, firmware/steamcore/test/game_state_test.cpp |
| T2 | Pin "exactly three states" at compile time and confirm the existing lint reaches the new files | US-1 | AC-1.2, AC-1.3, NFR-2, NFR-4 | T1 | `done` | A test contains a `switch` over `GameState` covering all three enumerators with **no `default`**, so a fourth value is a `-Werror=switch` build break; this is demonstrated once by temporarily adding a fourth enumerator, with the exact compiler error quoted in the task note and both the enumerator and the note's claim reverted; separately, `make lint`'s existing `include/`+`src/` allocation and ESP-IDF rules are demonstrated to actually reach the new files by temporarily inserting a `new` in `game_state.cpp` and observing lint fail; the header's enumerators carry no `= <digit>` and no ESP-IDF include — files: firmware/steamcore/test/game_state_test.cpp, firmware/steamcore/include/steamcore/game_state.h |
| T3 | US-2 battery: READY→PLAYING fires on the rising edge only | US-2 | AC-2.1, AC-2.2, AC-2.3, NFR-3 | T1 | `done` | Three named tests: (a) AC-2.1 — `start` false on step 1 then true on steps 2–10 leaves the state READY after step 1, PLAYING after step 2, and PLAYING after every one of steps 3–10, asserted after **each** step, not only at the end; (b) AC-2.2 — a freshly constructed session whose very first `advance` carries `start = true` is PLAYING after that step; (c) AC-2.3 — from READY, steps with `start = false` and `fire` both true and false leave the state READY. `make test-asan` reports zero findings for the file — files: firmware/steamcore/test/game_state_test.cpp |
| T4 | US-3 battery: caller-signalled game over, decoupled and idempotent | US-3 | AC-3.1, AC-3.2, AC-3.3 | T3 | `done` | Three named tests, each driven from a session first walked to PLAYING: (a) AC-3.1 — one `advance(input, /*sessionEnded=*/true)` yields GAME_OVER on that same step; (b) AC-3.2 — all four `start`/`fire` combinations, each including a rising `start`, with `sessionEnded = false` leave the state PLAYING; (c) AC-3.3 — two consecutive steps with `sessionEnded = true` leave GAME_OVER after the first and GAME_OVER after the second, with no transition to READY or PLAYING in between. A fourth test asserts `sessionEnded = true` in READY is ignored (state stays READY), since §6 defines no such transition — files: firmware/steamcore/test/game_state_test.cpp |
| T5 | US-4 battery: one-press restart, plus the die-with-START-held case that is the only place edge beats level | US-4 | AC-4.1, AC-4.2, AC-4.3, NFR-3 | T4 | `done` | Four named tests from a session walked to GAME_OVER: (a) AC-4.1 — `start` false then true restarts directly to PLAYING on the rising step, and the state is never observed as READY at any step of the whole sequence; (b) AC-4.2 — `start` held true for five further steps after the restart leaves PLAYING throughout with no second transition; (c) AC-4.3 — `fire = true, start = false` leaves GAME_OVER; (d) **the level-vs-edge test**: a session that reaches GAME_OVER via `advance(GameInput{true, false}, true)` — i.e. the session ends while START is *already held* — stays GAME_OVER for every subsequent step START stays held, and only returns to PLAYING after START is released for one step and pressed again. The task note records why this case is planned beyond the literal ACs: with `start` held, a level-triggered implementation is observationally identical to a correct one in every other sequence in T3–T5 — files: firmware/steamcore/test/game_state_test.cpp |
| T6 | Replay fixture with proven coverage of the states and shapes the sensitivity proof depends on | US-5 | AC-5.1 | T5 | `done` | `session_replay_fixture.h` defines `kSessionReplaySteps` (≥ 20), a test-only `ReplayStep { GameInput input; bool sessionEnded; }` and `sessionReplayStepAt(int32_t)` returning a fixed, hand-written sequence — no loop-generated pattern, no RNG, no clock; a coverage test independently re-derives from the fixture (by replaying it, not by reading its literals) that the sequence visits **all three** states, contains at least one held-`start` run of ≥ 3 consecutive steps, at least one `start` rising edge in READY and one in GAME_OVER, at least one `sessionEnded = true` step, and at least one step of each `fire` value in each state it visits. Without these assertions T7's sweeps could pass vacuously — files: firmware/steamcore/test/session_replay_fixture.h, firmware/steamcore/test/game_state_determinism_test.cpp |
| T7 | Two-layer determinism proof: lockstep replay compared after every step, plus a whole-input-surface sensitivity sweep | US-5 | AC-5.1, AC-5.2, NFR-3, NFR-5 | T6 | `done` | (a) AC-5.1 — two freshly constructed `GameSession`s are advanced in lockstep through the whole fixture and their `state()` is compared after **every individual step**, with the first differing step index printed on failure. (b) AC-5.2 — a sensitivity sweep runs, for each step index *i* in the fixture and independently for each of the three input fields (`start`, `fire`, `sessionEnded`), a replay in which run B has that one field flipped at step *i* only, recording whether the two runs' states ever differ at step *i* or later. The test asserts: **at least one** *i* is detected for `start`, **at least one** for `sessionEnded`, and **not a single** *i* is ever detected for `fire` — the last being AC-2.3/3.2/4.3's inertness proven across the whole replay rather than at three hand-picked steps. The `fire` case must be asserted as a non-detection, not folded into the same "must diverge" loop as the other two — a blind copy of `game-loop`'s three-way sweep would either fail or provoke a "fix" that gives `fire` an effect. The recorded reading of AC-5.2's "diverge from that step onward" is *detected at that step or a later one*, not *differing at every subsequent step*: a state machine can legitimately re-converge (run A reaches PLAYING by a later rising edge of its own), and asserting permanence would be asserting something false. `make test`, `make test-gcc` and `make test-asan` all green — files: firmware/steamcore/test/game_state_determinism_test.cpp, firmware/steamcore/test/session_replay_fixture.h |
| T8 | Lint: no clock/RNG in the game-state file set, no allocation in its test files, no integer standing in for a state | US-5, US-1 | AC-5.3, NFR-2, NFR-4, NFR-5 | T7 | `done` | `check_constraints.sh` gains one block over an explicitly named game-state file set (`game_state.h`, `game_state.cpp`, `game_state_test.cpp`, `game_state_determinism_test.cpp`, `session_replay_fixture.h`) with the same missing-file-is-a-failure posture the game-loop block already uses: the clock/RNG token pattern and the allocation pattern are each lifted into one shell variable shared by the existing game-loop block and this one, so the two rules cannot drift apart, and the game-loop block's file sets, messages and behaviour are otherwise unchanged; plus two NFR-4 greps over `game_state.h` rejecting an enumerator declared with an explicit value (`^\s*(READY|PLAYING|GAME_OVER)\s*=`) and any `static_cast<GameState>`. Each of the three new checks is demonstrated failing once against a temporarily inserted violation and the observation recorded; `make lint` is green on the real tree afterwards, proving the shared-variable refactor did not disarm the game-loop rules — files: tools/check_constraints.sh, docs/host-tests.md |
| T9 | NFR-7 contract doc comment and the NFR-6 surface audit | US-1, US-2, US-3, US-4, US-5 | NFR-6, NFR-7 | T8 | `done` | `game_state.h`'s doc comment states, each explicitly: the exact three states and that a fresh instance is READY; that START is edge-triggered (was-false-now-true) and *why* — a held button reads `true` every tick, so a level check would re-fire — including that the first tick ever counts as a rising edge; that GAME_OVER returns **directly** to PLAYING on one press, citing constitution Principle 4; that PLAYING→GAME_OVER comes only from the caller's `sessionEnded` argument and is never derived from `GameInput`, and that it is ignored outside PLAYING; that at most one transition happens per `advance`, evaluated against the state the step began in; that the type is usable and testable standalone, without `GameLoop<Game>`; and the inherited single-threaded, nothing-throws, no-error-code, no-allocation contract. One compilable usage example shows a consumer calling `advance` once per `update` and branching on `state()`. The header is then audited symbol by symbol against NFR-6 (`GameState` + its three values, `GameSession`, `advance`, `state` — and nothing else, with the audit written down); `docs/host-tests.md` records the new tests and lint rules; `make test-all` green from a clean checkout — files: firmware/steamcore/include/steamcore/game_state.h, docs/host-tests.md |

## 4. Test Strategy

- **Everything is host unit tests** (`STEAMCORE_TEST`, `make test`), as in all four prior increments: all
  15 ACs are provable with `clang++` alone, no board, no ESP-IDF. One named test per AC (or per AC case
  group) so `make test FILTER=...` lets QA record criteria individually (constitution §8). Test files are
  picked up by the existing `test/*_test.cpp` wildcard — no Makefile change.
- **US-1 (T1/T2)** — AC-1.1 is one assertion on a default-constructed value. AC-1.2 is *not* a runtime
  assertion: exhaustive `switch`es with no `default` plus `-Werror` make a fourth state a build failure,
  which is stronger than a test nobody may run, and the build failure is demonstrated once rather than
  assumed. AC-1.3 needs no new grep — the mechanism lives in `include/`+`src/`, which `make lint` already
  sweeps; T2 proves that reach instead of trusting it.
- **US-2/US-4 (T3/T5)** — asserted **after every step** of each held-button sequence, not at the end,
  because the defect these ACs exist to catch (a level-triggered check) is a transition happening on a step
  where none should. The load-bearing test is T5(d): in every other sequence in the plan a level-triggered
  implementation produces the *same* state trace as a correct one, and only "the session ends while START
  is still held" separates them. Planning US-2/US-4 without it would produce a green suite that cannot fail
  for the exact bug §1 of the spec names.
- **US-3 (T4)** — the decoupling claim is proven positively (a `sessionEnded` step ends the session) and
  negatively (no `start`/`fire` combination ever does), and idempotence is asserted as "the second signal
  changes nothing", not merely "the state is still GAME_OVER".
- **US-5 (T6/T7)** — this is where the false-green risk lives, so the proof stays two-layered, but the
  layers are placed where this feature's risk actually is. `game-loop` needed layer one to prove its
  38,400-pixel comparator could detect a difference; here the comparison is `==` on a scoped enum, so that
  effort moves to **proving the fixture is not vacuous** (T6: it must be shown to visit all three states and
  contain held runs, rising edges in both READY and GAME_OVER, and an end-of-session signal) and to
  **widening the sensitivity check to the whole input surface** (T7: all three fields, every step index,
  swept — `game-loop`'s Round-1 review found exactly the one-field version of this gap). The asymmetry
  matters and is deliberate: `start` and `sessionEnded` must be shown *detectable*, `fire` must be shown
  *undetectable*, since an effect from `fire` would be a bug, not sensitivity.
- **Sanitizers** — `make test-asan` (`-fsanitize=address,undefined -fno-sanitize-recover=all`) over the
  held-button sequences and the sweep is where NFR-3 is actually proven; the sweep alone is
  `kSessionReplaySteps² × 3` replays, still microseconds, so it stays inside the correctness gate.
- **Grep gates (T8)** carry AC-5.3 and NFR-2/NFR-4/NFR-5's static half, automated in `make lint` rather
  than left to reviewer diligence, and each new rule is demonstrated failing once — an unexercised gate is
  an untested one.
- **Deliberately not covered, and why:** no benchmark — NFR-1 is N/A in the spec (an enum comparison, not a
  framebuffer replay), and a timing assertion with no budget to defend would be noise. No `make view` PNG
  and no framebuffer dump: this feature draws nothing (NFR-10 N/A). No `/demo-day` in a browser —
  constitution §8 records no browser-observable surface. No device or 60 Hz claim: no ESP-IDF, no board,
  nothing here may be reported as hardware-verified. Real GCC — `/usr/bin/g++` is clang on this host, so
  NFR-4's two-compiler claim stays honestly reported as nominal, unchanged from `game-loop`.

## 5. Risks & Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| A green US-2/US-4 suite that a level-triggered implementation would also pass — with START held, edge and level produce identical state traces in every sequence except one | Highest risk here: the feature's entire reason for existing (§1 Problem) would be certified by tests that cannot fail for that bug | T5(d) is a named task DoD, not a nice-to-have: the session must end *while START is held* and stay GAME_OVER until START is released and pressed again. T3(a)/T5(b) additionally assert after every step, not only at the end |
| The sensitivity sweep is mirrored from `game-loop` without noticing that `fire` must *not* diverge | Medium-High — either a permanently failing test, or a "fix" that gives `fire` an effect and breaks AC-2.3/3.2/4.3 outright | The asymmetry is written into T7's DoD and §4 explicitly: `start`/`sessionEnded` assert detection, `fire` asserts non-detection across every step index |
| AC-5.2's "diverge from that step onward" read literally as "differs at every subsequent step" | Medium — a reviewer-visible mismatch, or an assertion that is simply false: two runs can re-converge when the lagging one gets its own rising edge | Interpretation recorded in T7 and here rather than buried: divergence is asserted as *detected at that step or later*. Flagged for the approval conversation |
| The fixture is deterministic but shallow — never reaches GAME_OVER, or never holds START — making both AC-5.1 and the sweeps vacuously green | Medium — the exact second half of `game-loop`'s Round-1 finding | T6 is its own task, before T7, and its DoD is a coverage test that re-derives the visited states and shapes *by replaying* the fixture rather than by reading its literals |
| Sharing the clock/RNG and allocation token patterns between the game-loop and game-state lint blocks disturbs a proven rule | Medium — a silently disarmed gate on already-shipped code | T8 changes only the pattern's storage, never the game-loop file sets or messages, requires each new check to be observed failing, and requires `make lint` green on the real tree afterwards as the regression check |
| `advance(input, sessionEnded)` forces a consumer that detects game-over mid-`update` to call `advance` after its own simulation, or accept one tick of latency | Low-Medium — an ergonomics complaint from the first real game author, not a correctness bug | The recommended composition is the NFR-7 usage example in T9; the cost of reversing to a second entry point later is one method on one class, with no stored data to migrate |
| `game_state.h` includes `game_loop.h` for `GameInput`, coupling this feature to a header §6 forbids changing | Low | The dependency is one include in one direction; a future Input Abstraction story that relocates `GameInput` updates one line here. Splitting `GameInput` out now would itself be the §6 violation |
| Inherited from the spec (A9/A10): no persistence, no threading, no allocation — none of which is exercised by anything that exists yet | Low, accepted by the spec | Nothing here allocates, stores or shares state across threads; `make lint` and `make test-asan` are the standing checks, and the honest-status rule (constitution §4) applies to every report about this feature |

---

## ✅ PLAN GATE

*All boxes checked → `/increment` may start. Any box open → back to `/sprint-plan`.*

- [x] Spec status is `approved` (never plan against a draft)
- [x] Architecture decision includes rejected alternatives (9 recorded, §1)
- [x] Architecture respects the constitution's technical constraints (§3 no dynamic allocation — one enum and two members, no container anywhere; no wall-clock or RNG read, enforced by `make lint`; C++17; `steamcore` namespace, `snake_case` file names, `.h`/`.cpp` pair per §5; no ESP-IDF header; one `-I` path unchanged; no resolution/tile literal introduced; English throughout) — no conflict found
- [x] Every task maps to a user story — no orphan tasks, no story without tasks
- [x] Every Must AC and every applicable NFR is covered by at least one task (AC-1.1…1.3, AC-2.1…2.3, AC-3.1…3.3, AC-4.1…4.3, AC-5.1…5.3, NFR-2…NFR-7; NFR-1/8/9/10/11 are N/A per the spec)
- [x] Every task has a checkable definition of done
- [x] Task order respects dependencies (walking skeleton T1 first: type, transition table and one end-to-end transition before any AC battery; fixture T6 before the determinism proof T7)
- [x] Test strategy covers every Must story
- [x] Line budget respected: Ist 194 / Soll ~300 (excluding HTML comments)
- [x] Status set to `approved` by the user
