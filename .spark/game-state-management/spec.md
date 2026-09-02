# Spec: game-state-management

| | |
|---|---|
| **Phase** | Specify |
| **Owner** | Product Owner (`/story-time`), Designer (`/look-and-feel`) |
| **Status** | `approved` |
| **Date** | 2026-09-02 |
| **Ticket** | `none` |

**Handoff**
- **Status:** `approved` — user approved as proposed, including all four judgment calls (no-BOOT slice, edge-detection owned here, GAME_OVER restarts directly to PLAYING, no GameInput/GameLoop change) and Design Review N/A.
- **Summary:** `GameLoop<Game>` calls a consumer's `update`/`render` every tick regardless of session phase, and nothing in the engine represents "has the player pressed START yet" or "has this run ended." Deliver a small, directly-testable session-phase component — exactly READY/PLAYING/GAME_OVER — with edge-triggered (not level-triggered) START handling and a one-press restart from GAME_OVER straight back to PLAYING, proven deterministic the same way `game-loop` proved its own replay guarantee.
- **Open:** `none` — the architecture-shape questions the orchestrator flagged (BOOT in/out, who owns edge-detection, restart destination) are resolved below, not deferred to `/sprint-plan`.
- **Binding ruling:** §4 User Stories US-1…US-5; §7 Clarifications for what was decided and why.
- **On conflict:** the numbered body below wins for everything except `Status`; log the mismatch as a finding at the next `/peer-review` and proceed.

## 1. Problem & Goal

- **Problem:** `game-loop` deliberately shipped no session-phase concept (its own spec, C1) — `GameLoop<Game>` ticks `update`/`render` unconditionally from the moment it exists. Every future game would otherwise invent its own ad hoc "have we started yet / has the run ended" bookkeeping, and — the sharper risk — its own ad hoc button check, which a naive `if (input.start)` gets wrong: `start` reads `true` on every tick a physical button stays held, not once per press, so a level-triggered check would flip READY/PLAYING back and forth every tick the button is down. Constitution Product Principle 4 — "arcade immediacy... restart always one button away" — has zero implementing code today.
- **Goal:** One small session-phase component, exactly three states, with an edge-triggered START transition into play and a one-press restart out of game-over — testable directly against a `GameInput` sequence, without requiring a full `GameLoop<Game>`/synthetic-game fixture to exercise it.
- **Success signal:** one documented host test command shows (1) READY→PLAYING and GAME_OVER→PLAYING each fire exactly once per rising edge of `start`, even across many consecutive ticks where `start` stays `true`, (2) PLAYING→GAME_OVER fires only on an explicit caller signal, never derived from `GameInput`, and (3) two freshly constructed instances replaying the same ordered sequence produce an identical state at every single step — 0 failures, 0 sanitizer findings. Deferred signal: the first Phase-3 game or a system-menu story composes against this without editing a file delivered here.
- **Why now:** README's Phase-2 checklist lists "Game State Management" as its own unchecked item, directly after Game Loop, which just shipped; `game-loop`'s own spec explicitly deferred it (C1/C2 there). Principle 4 is the constitution's fourth-ranked product principle and currently unimplemented anywhere in the codebase.

## 2. Target Users

- **Engine developer (primary, today):** needs one proven session-phase container instead of re-deriving READY/PLAYING/GAME_OVER logic — and its edge-detection — per future game.
- **Future game-module author (primary, near future, same person):** will call into this component from their own `update(const GameInput&)` to decide what to draw/simulate this tick, and will signal "session ended" from their own game logic (collision, lives, timer — none of which exist yet).
- **Future system-menu / registry story (consumer, not built):** will decide how a player reaches a game and what happens on return to a menu — explicitly not this feature's concern (§6).
- *Not a user of this feature:* the console player. No display driver, no rendered "PRESS START"/"GAME OVER" text exists yet — same posture as every prior engine story.

## 3. Assumptions & Open Questions

| # | Assumption / Question | Resolution |
|---|---|---|
| A1 | The idea arrived via `/next-steps`'s own sketch: `BOOT→READY→PLAYING→GAME_OVER`. Treated as background to verify, not transcribe — see A2. | Accepted, partly overridden |
| A2 | **No `BOOT` state.** There is no boot sequence, system menu or game registry to represent yet — those are a future "System-Menü"/registry story's own concern. The honest minimal slice is **READY → PLAYING → GAME_OVER**, three states, entered at READY on construction (mirrors `GameLoop`'s own "constructed once, no separate boot phase" shape). | **Overridden `/next-steps` framing** (C1) |
| A3 | **This feature owns START edge-detection** (was-`false`-now-`true`), because it is pure software comparison of consecutive tick values — fully testable today with synthetic `GameInput` sequences, unlike physical-switch debouncing (filtering real electrical bounce), which genuinely needs hardware and stays deferred, mirroring `game-loop`'s own hardware-vs-logic line (its A7). Without this, the whole component is broken by construction: a held button would flip states every tick. | **Resolved here, not deferred** (C2) |
| A4 | **GAME_OVER restarts directly to PLAYING** on the next START rising edge — not to an intermediate READY requiring a second press. Reads constitution Principle 4's "restart always one button away" literally: one press, one transition, back in play. | **Resolved here, not deferred** (C3) |
| A5 | **This feature does not decide what ends a session.** PLAYING→GAME_OVER fires on one explicit caller-supplied signal per tick, decoupled from `GameInput` entirely — the *reason* (collision, lives, timer, score) belongs to Collision System / Score System, both separate, unstarted Phase-2 items. | Accepted (C4) |
| A6 | **`GameInput` gains no new field and `GameLoop<Game>` is not modified.** This is a new, separate, directly-composable component a future `Game`'s own `update` calls into — game-loop's shipped public surface (`GameLoop`, `tick`, `GameInput{start, fire}`) is untouched. | Accepted (C5) |
| A7 | **Directly unit-testable without a `GameLoop<Game>` instantiation or a synthetic `ReplayGame`-style fixture.** A plain, small value whose transitions are exercised by feeding it a `GameInput` sequence (and, separately, an end-of-session signal) — not something requiring the full tick machinery to observe. | Accepted (C6) |
| A8 | **Replay determinism gets its own explicit AC, proven the same two-layer way `game-loop`'s US-4 proved it** (independent runs compared after every step, plus a sensitivity check) — not left to "falls out of pure logic," because that exact false-green shape dominated two prior review rounds on this codebase. | **Resolved here, not deferred** (C7) |
| A9 | No persisted-data format-version concern (constitution §6): this is transient runtime/session state, reset to READY on every fresh construction — never written to or read from Flash. | Accepted (C8) |
| A10 | Threading/allocation/no-throw contracts are unchanged from `game-loop`: single-threaded, zero dynamic allocation, no exception, no error code. | Accepted |

## 4. User Stories

### US-1 (Must): A minimal three-state session container, correct initial state

> As the engine developer, I want a small component holding a game session's current phase — exactly READY, PLAYING or GAME_OVER, starting at READY — so future games and system code share one authoritative session-phase value instead of each inventing their own.

**Acceptance criteria:**

- [ ] AC-1.1: Given a freshly constructed instance, when its current state is read, then it is READY.
- [ ] AC-1.2: Given the state type, when inspected, then exactly three values are defined — READY, PLAYING, GAME_OVER — no BOOT, no PAUSED, no other value (A2).
- [ ] AC-1.3: Given the component's source, when grepped, then it performs no dynamic allocation and includes no ESP-IDF header (A10).

### US-2 (Must): READY→PLAYING fires exactly once per START press, not once per tick held

> As the engine developer, I want the READY→PLAYING transition to fire only on the tick where `start` newly becomes `true`, not on every subsequent tick it stays `true`, so a player holding the button down never re-triggers it (A3).

**Acceptance criteria:**

- [ ] AC-2.1: Given state READY and a `GameInput` sequence where `start` is `false` on tick 1, then `true` on ticks 2–10 (a held press), when each tick is fed in order, then the state transitions to PLAYING on tick 2 and stays PLAYING for ticks 3–10 — no second transition fires while `start` stays `true`.
- [ ] AC-2.2: Given a freshly constructed instance in READY, when the very first tick it ever receives carries `start = true`, then it transitions to PLAYING on that first tick — the absence of any prior tick counts as an implicit "not pressed," so this is a rising edge, not a no-op.
- [ ] AC-2.3: Given state READY and a tick where `start` is `false`, when processed, then the state remains READY regardless of `fire`'s value.

### US-3 (Must): PLAYING→GAME_OVER on an explicit end-of-session signal, decoupled from GameInput

> As a future game-module author, I want PLAYING→GAME_OVER to fire only when I explicitly signal the session ended — never derived from `GameInput` — so my own game logic decides *when*, without this feature guessing a rule or `GameInput` growing a new field (A5, A6).

**Acceptance criteria:**

- [ ] AC-3.1: Given state PLAYING, when the caller signals the session has ended, then the state becomes GAME_OVER on that same step.
- [ ] AC-3.2: Given state PLAYING, when a tick is processed with any `start`/`fire` combination but no end-of-session signal, then the state remains PLAYING — pressing or holding either button mid-play never ends or restarts a session by itself.
- [ ] AC-3.3: Given state PLAYING, when the caller signals session-ended twice in a row with no intervening restart, then the state is GAME_OVER after the first signal and the repeated signal has no further effect (idempotent).

### US-4 (Must): GAME_OVER→PLAYING restarts directly on one fresh START press

> As a future game-module author, I want a fresh START press in GAME_OVER to return directly to PLAYING — not to an intermediate READY — so restarting a run costs the player exactly one button press, per constitution Principle 4 (A4).

**Acceptance criteria:**

- [ ] AC-4.1: Given state GAME_OVER and a sequence where `start` is `false` then becomes `true`, when processed, then the state transitions directly to PLAYING on the tick `start` becomes `true` — never to READY, never needing a second press.
- [ ] AC-4.2: Given state GAME_OVER and `start` held `true` for several consecutive ticks after restarting, when processed, then exactly one transition fires, on the rising edge, and the state stays PLAYING for the rest of the held duration.
- [ ] AC-4.3: Given state GAME_OVER and a tick where `fire` is `true` and `start` is `false`, when processed, then the state remains GAME_OVER — only `start` restarts, never `fire`.

### US-5 (Must): Replaying the same sequence is provably deterministic

> As the engine developer, I want two independently constructed instances driven by the identical ordered sequence of ticks and end-of-session signals to reach an identical state at every step, so this feature provably keeps constitution §4's determinism guarantee that `game-loop` already proved (A8).

**Acceptance criteria:**

- [ ] AC-5.1: Given a fixed, non-trivial ordered sequence (≥ 20 steps) mixing held/rising `start` values and at least one end-of-session signal, run twice from two freshly constructed instances, when the state is compared after **every individual step**, then the two runs match at each one — not only the last.
- [ ] AC-5.2: Given the same fixture, when a single step's `start` value is deliberately changed in one of the two runs, then the two runs' state sequences are proven to diverge from that step onward — a comparator or fixture that could never detect a difference would falsely certify determinism (mirrors `game-loop`'s T5/T6 sensitivity proof).
- [ ] AC-5.3: Given the component's and its test fixture's source, when grepped, then no wall-clock read and no unseeded RNG call appears anywhere (constitution §4).

## 5. Non-Functional Requirements

| # | Category | Requirement (measurable) | How it's verified |
|---|---|---|---|
| NFR-1 | Performance | N/A — every transition is an O(1) enum comparison/assignment; nothing here approaches a measurable cost at 60 Hz, unlike a 38,400-pixel framebuffer replay. | — |
| NFR-2 | Reliability / memory | Zero dynamic allocation anywhere in the delivered component and its test fixture (grep gate): no `new`/`malloc`/`std::vector`/`std::string`. | `/peer-review` (grep) |
| NFR-3 | Reliability / bounds | Full suite (US-2/US-4's held-button sequences, US-5's replay) runs under `-fsanitize=address,undefined` with zero findings. | AC-2.1, AC-4.2, AC-5.1/5.2 |
| NFR-4 | Portability / toolchain | Compiles clean under `clang++` and `g++`, `-std=c++17 -Wall -Wextra -Werror`; no ESP-IDF header; the three states are named symbolically, no integer literal standing in for a state anywhere outside the type's own definition. | `/peer-review` (grep + both compilers) |
| NFR-5 | Determinism | Same ordered sequence + same initial state → identical state sequence, both compilers (constitution §4's exact wording); no wall-clock or unseeded-random read. | AC-5.1–5.3 + `/peer-review` |
| NFR-6 | **Library lens — public API surface** | Exactly one new state type (three named values) plus the transition-driving component; no new field on `GameInput`, no change to `GameLoop<Game>` (A6); no speculative fourth state or unused accessor. Any other new public symbol is a review finding. | `/peer-review` |
| NFR-7 | **Library lens — contract clarity** | Doc comment states: the exact three states and initial state (US-1); that START is edge-triggered, not level-triggered, and why (A3); that GAME_OVER→PLAYING is direct, citing Principle 4 (A4); that PLAYING→GAME_OVER is caller-signaled, never `GameInput`-derived (A5); that it's testable standalone without `GameLoop<Game>` (A7); the inherited single-threaded/no-throw/no-alloc contract. One usage example. | `/peer-review` |
| NFR-8 | Observability / ops | N/A — pure in-memory state, no runtime failure mode, no logging surface yet (inherited pattern). | — |
| NFR-9 | Security & privacy | N/A — offline device, no input persisted, no personal data (constitution §2). | — |
| NFR-10 | Accessibility | N/A — nothing in this increment reaches a display driver; rendering "PRESS START"/"GAME OVER" text is a future consumer's concern, not decided here. | — |
| NFR-11 | Integrations & dependencies | N/A — no external dependency; composes with a future `Game`'s own `update`, does not modify `GameLoop`/`GameInput` (A6). | — |

*Lens note: `library` active **scoped** (constitution §2); semver/packaging are no-ops. Public API surface and Contract clarity land as NFR-6/NFR-7.*

## 6. Out of Scope

- **`BOOT` state, boot sequence, system menu, game registry.** No such machinery exists; owned by a future "System-Menü"/registry story (A2).
- **`PAUSED` or any state beyond the three named here.** Not asked for by any named consumer (NFR-6).
- **Any real game-over *condition*** (collision, lives, timer, score threshold). This feature provides the transition mechanism only; the reason is Collision System / Score System's future concern (A5).
- **Physical switch/button debouncing** (electrical bounce filtering). Requires real hardware to characterize; logical rising-edge detection on the raw `bool` is in scope and built here (A3) — the two are deliberately not the same thing.
- **Any change to `GameInput`, `GameLoop<Game>`, or the engine↔game contract** shipped by `game-loop`. This is a new, separate, composable type (A6).
- **Rendering any on-screen text or visual for READY/PLAYING/GAME_OVER** ("PRESS START", "GAME OVER"). No display driver exists; a future consumer's concern (NFR-10).
- **Score, highscore, persistence.** Unchanged from every prior spec — still future stories; no format-version concern applies here (A9).
- **Multiplayer or multiple concurrent session-phase instances.** One session at a time, mirrors `game-loop`'s "one consumer at a time."
- **SELECT/joystick input to this component.** Only `start` drives a transition; `fire` is explicitly proven inert to every transition (AC-2.3, AC-3.2, AC-4.3).
- **`GameLoop<Game>` calling this component automatically, or skipping `update`/`render` based on state.** How a `Game` internally branches on the state value is the `Game`'s own responsibility, not built or wired here.
- **Display driver, ILI9488, SPI, tile push.** No board wired, no ESP-IDF installed — unchanged from every prior increment.

## 7. Clarifications

| # | Date | Question | Resolution |
|---|---|---|---|
| C1 | 2026-09-02 | Does `BOOT` belong in this first slice's state set? | **No.** No boot sequence/menu/registry exists to represent; slice is READY→PLAYING→GAME_OVER, three states (A2, §6). |
| C2 | 2026-09-02 | Does this feature own START edge-detection, or is it deferred to a future Input Abstraction story? | **Owned here.** It's pure logic (consecutive-value comparison), fully testable without hardware, unlike electrical debouncing which stays deferred. Without it the component is broken by construction (A3). |
| C3 | 2026-09-02 | Does GAME_OVER restart to READY (needing a second press) or directly to PLAYING? | **Directly to PLAYING**, on the next START rising edge — reads Principle 4's "one button away" literally (A4). |
| C4 | 2026-09-02 | Does this feature decide *what* ends a session? | **No.** One caller-supplied signal, decoupled from `GameInput`; the real condition is Collision/Score System's future job (A5). |
| C5 | 2026-09-02 | Does `GameInput` grow a field for "session ended," or does `GameLoop<Game>` change? | **Neither.** New, separate, composable component; game-loop's shipped surface is untouched (A6). |
| C6 | 2026-09-02 | Does this feature need its own `ReplayGame`-style fixture and full `GameLoop<Game>` to be tested? | **No.** Directly testable as a standalone value fed a `GameInput` sequence and a session-ended signal (A7). |
| C7 | 2026-09-02 | Does determinism need its own explicit AC, or does it "fall out" of pure logic? | **Explicit AC (US-5), two-layer proof** — same shape as `game-loop`'s US-4, because that false-green risk dominated two prior review rounds here (A8). |
| C8 | 2026-09-02 | Half-size version, if the story must shrink? | US-1 + US-2 + US-3 alone (state exists, one-way START, one-way game-over) still deliver core value — a session can begin and can end, deterministically. US-4 (direct restart) is cut first despite Principle 4, as the honest interim cost of a smaller slice; US-5's determinism proof is the last thing cut, mirroring `game-loop`'s own C8 — the constitution's guarantee outranks convenience. |

## 8. Design Review

- **N/A, user-confirmed.** This story delivers a pure three-state transition component — no color, layout, visual shape, or interaction surface is decided here; NFR-10 already records that no display driver exists to render READY/PLAYING/GAME_OVER onto. Same posture as `game-loop`, which also had no design review. If a future story renders on-screen state (e.g. "PRESS START"), that story gets its own design pass — not retrofitted here.

---

## ✅ SPEC GATE

*All boxes checked → `/sprint-plan` may start. Any box open → back to `/story-time` or `/look-and-feel`.*

- [x] Problem, goal and success signal are concrete (no buzzwords, no "everyone")
- [x] Every story has testable Given/When/Then acceptance criteria
- [x] Stories are prioritized (MoSCoW) and at least one is a Must
- [x] Non-functional requirements are stated and measurable (or marked N/A with reason)
- [x] Clarify pass done: functional scope (C1/C2/C3), data (A9, no persistence), roles/permissions (N/A, no UI), error/edge cases (AC-2.1/2.2, AC-4.2, AC-3.3), NFRs (NFR-1…11), integrations (NFR-11, A6), UX flows (N/A, no UI), out-of-scope (§6) — all swept
- [x] Open questions are resolved or explicitly accepted as risk — 0 open
- [x] Out-of-scope section is filled (something was consciously cut)
- [x] Constitution (`.spark/constitution.md`) respected — Principle 4 and §4 Determinism directly grounded (A4, A8); no conflict found
- [x] Design review done for UI-facing features (or marked N/A with reason) — N/A, user-confirmed (§8): pure state-transition logic, no visual/UI surface
- [x] Line budget respected: Ist 174 / Soll ~250 (excluding HTML comments)
- [x] Status set to `approved` by the user
