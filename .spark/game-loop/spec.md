# Spec: game-loop

| | |
|---|---|
| **Phase** | Specify |
| **Owner** | Product Owner (`/story-time`), Designer (`/look-and-feel`) |
| **Status** | `approved` |
| **Date** | 2026-09-02 |
| **Ticket** | `none` |

**Handoff**
- **Status:** `approved` — user approved as proposed, including all three PO judgment calls (no state machine, narrowed call-diagram, all four stories as Must) and Design Review N/A.
- **Summary:** rendering-core and text-rendering give SteamCore a tested drawing surface, but nothing yet calls into it on a schedule. Deliver a hardware-independent fixed-timestep tick mechanism — a fixed `update`/`render` call order once per tick, a minimal two-signal `GameInput`, wired to the existing `Framebuffer` — with host-test proof that replaying the same input sequence twice is byte-identical, verifiable today with clang++ alone.
- **Open:** `none`.
- **Binding ruling:** §4 User Stories US-1…US-4; §7 Clarifications for what was decided and why.
- **On conflict:** the numbered body below wins for everything except `Status`; log the mismatch as a finding at the next `/peer-review` and proceed.

## 1. Problem & Goal

- **Problem:** SteamCore has a tested rendering substrate but no `core` module at all: no fixed timestep, no defined boundary between "a game" and "the engine," no proof that repeated ticks behave the same way twice. Constitution §3 states a binding constraint — "game logic runs a fixed, deterministic 60 Hz step... Logic never reads wall-clock time or frame duration" — that today is asserted nowhere in code and verified by nothing. Every later Phase-2 item that depends on a running loop (Input Abstraction, Collision System, Audio API, Score, Game State Management) and every Phase-3 game sits behind this.
- **Goal:** A tested, hardware-independent tick mechanism that calls a fixed `update`/`render` contract once per logical tick, wired to the existing `Framebuffer`, driven by a minimal per-tick input value — with host-test proof that the same input sequence replayed twice produces byte-identical results.
- **Success signal:** one documented host test command shows (1) a requested tick count drives exactly that many update-then-render pairs, in order, and (2) replaying the same `GameInput` sequence from a fresh start twice produces byte-identical framebuffers after **every** tick — 0 failures, 0 sanitizer findings. Deferred signal: when the first Phase-3 game or the game-state-management/collision-system stories land, they build against this contract without editing a file delivered here.
- **Why now:** Phase 2's own checklist lists "Game Loop" directly after the two shipped rendering items and before Input Abstraction, Collision System, Audio API, Score, Highscore and Game State Management — each either calls into or is called by a running loop. Building any of them before the loop exists means guessing at a contract that gets redesigned later.

## 2. Target Users

- **Engine developer (primary, today):** needs one proven tick mechanism instead of inventing call-order/timing logic per future module.
- **Future game-module author (primary, near future, same person):** will implement `update(GameInput)`/`render()` once a real game exists; this story fixes the contract shape, not their game logic.
- **Future engine stories — Input Abstraction, Collision System, Game State Management, Audio API (consumers, not built):** each hangs off this tick in a defined call slot, without this feature guessing their internals.
- *Not a user of this feature:* the console player. No display driver or panel exists; nothing here is human-visible yet — same posture as rendering-core and text-rendering.

## 3. Assumptions & Open Questions

| # | Assumption / Question | Resolution |
|---|---|---|
| A1 | The idea arrived with a sketch (from `/next-steps`, not the user directly): a `BOOT→READY→PLAYING→GAME_OVER` state machine, `onCollision(Entity&, Entity&)`, and a claim about a determinism clause. Treated as background to verify, not to transcribe — see A4–A6. | Accepted, partly overridden |
| A2 | Everything is verifiable with the host toolchain (constitution §4): no board, no ESP-IDF, no cmake required for any AC. | Accepted |
| A3 | Constitution §3 Timing and §4 Determinism are confirmed, binding, and directly on point: "game logic runs a fixed, deterministic 60 Hz step... Logic never reads wall-clock time" and "the same input sequence and the same seed must produce the same framebuffer sequence." Quoted verbatim, not paraphrased, so §4/§5's ACs and NFRs can cite it exactly. | Accepted |
| A4 | **No `BOOT/READY/PLAYING/GAME_OVER` state machine in this slice.** README's own Phase 2 checklist lists "Game Loop" and "Game State Management" as two separate, still-unchecked items — conflating them here builds ahead of the smallest slice. Deferred to a future `game-state-management` story. | **Overridden `/next-steps` framing** (C1) |
| A5 | **No `Entity`/`onCollision` surface.** No `entity.h` exists in the repo; README lists "Collision System" as its own separate, unstarted Phase-2 item. Deferred. | **Overridden `/next-steps` framing** (C2) |
| A6 | README's Game Loop diagram lists five per-frame steps (`INPUT→UPDATE→COLLISION→RENDER→AUDIO`). This story narrows the call contract to `UPDATE→RENDER` only — `INPUT` is folded into the `GameInput` value handed to `update`; `COLLISION` and `AUDIO` have no engine support yet (separate unstarted Phase-2 items: Collision System, Audio API) and are not silently added here. | Accepted (C3) |
| A7 | **`GameInput` carries exactly two boolean signals: START and FIRE** — the two buttons README's own Controller section has already committed to by name, independent of joystick/SELECT. Raw pass-through only: no decoding, debouncing or edge-detection, because no hardware exists to decode from yet (README Phase 1: buttons and joystick both unwired). | Accepted (C4) |
| A8 | **No real-time / wall-clock pacing is built, tested or claimed here.** The tick is a discrete, caller-driven step count, not a measured 60 Hz interval — an actual scheduler needs ESP-IDF/FreeRTOS, absent per constitution §4. That story comes once hardware and toolchain exist. | Accepted (C5) |
| A9 | **The mechanism never implicitly clears the framebuffer between ticks.** Clearing, if wanted, is `render()`'s own explicit call, mirroring rendering-core's explicit `clear()` — no hidden side effect a future game must discover by reading engine source. | Accepted (C6) |
| A10 | **A synthetic, in-repo test double stands in for "a game"** to exercise `update`/`render` — not a Phase-3 game, ships under `test/`, not `games/`. Same posture as rendering-core/text-rendering's own fixtures: no real downstream consumer is required for testable ACs. | Accepted |
| A11 | Threading, allocation and coordinate contracts are unchanged from rendering-core: single-threaded, zero dynamic allocation, no throw/no error code. | Accepted |
| A12 | "No dynamic allocation" and "one virtual-resolution constant" are inherited constraints this feature does not newly exercise — the mechanism owns no entities and adds no screen-size logic; it trivially holds them rather than requiring new grep surface. | Accepted |

## 4. User Stories

### US-1 (Must): A fixed tick mechanism drives update-then-render, in order

> As the engine developer, I want a mechanism that, when driven for N logical ticks, calls a consumer's `update` once and then `render` once per tick, in that fixed order, so future game and engine code shares one call sequence instead of each inventing its own loop.

**Acceptance criteria:**

- [ ] AC-1.1: Given a test consumer and a requested tick count N (tested for N = 0, 1, and 100), when the mechanism runs, then `update` and `render` are each called exactly N times, and for every tick *i*, `update(i)` completes before `render(i)` starts, and `render(i)` completes before `update(i+1)` starts.
- [ ] AC-1.2: Given N = 0, when the mechanism runs, then neither `update` nor `render` is called and no crash occurs.
- [ ] AC-1.3: Given the mechanism's own source, when grepped, then it calls no wall-clock/system-time function and derives no tick from measured elapsed time (constitution §3 Timing) — a tick is a discrete, caller-supplied step.
- [ ] AC-1.4: Given AC-1.1's N = 0/1/100 cases, when run under `-fsanitize=address,undefined`, then zero findings occur.

### US-2 (Must): update/render share a stable, tick-scoped contract with the framebuffer

> As a future game-module author, I want each tick's `update` to receive exactly that tick's input and each tick's `render` to draw into the engine's one framebuffer instance, so game code depends on a fixed data-flow instead of guessing when input applies or which buffer it draws to.

**Acceptance criteria:**

- [ ] AC-2.1: Given a `GameInput` sequence of length N with a distinct value at each index, when the mechanism runs, then `update` at tick *i* observes exactly the value supplied for index *i* — never the previous or next tick's.
- [ ] AC-2.2: Given a test consumer whose `render` draws a distinguishing mark, when it runs across every tick of one call to the mechanism, then it draws into the same `Framebuffer` instance every time (same instance identity, mirrors rendering-core A7) — never a fresh or copied buffer per tick.
- [ ] AC-2.3: Given a test consumer whose `render` never calls `clear()`, when two consecutive ticks each draw one distinct pixel, then after the second tick both pixels are still present — the mechanism never implicitly clears between ticks (A9).
- [ ] AC-2.4: Given a test consumer whose `update` mutates its own state and whose `render` draws based on that state, when one tick runs, then `render` observes exactly the state `update` left for that same tick — no reordering, no stale read.

### US-3 (Must): A minimal, two-signal GameInput

> As the engine developer, I want each tick's input reduced to START and FIRE as plain boolean pass-through values, so the loop's contract is provably exercised without pre-building a joystick/button decode layer that has no wired hardware to decode from yet.

**Acceptance criteria:**

- [ ] AC-3.1: Given a `GameInput` value, when inspected, then it carries exactly two fields — `start`, `fire` — and no third field, no joystick direction, no raw GPIO state.
- [ ] AC-3.2: Given a `GameInput` value for one tick, when `update` observes it, then both fields carry exactly the values the caller supplied — no debouncing, no edge-detection, no default substitution by the mechanism.
- [ ] AC-3.3: Given a sequence covering all four start/fire combinations (both false, start only, fire only, both true) across separate ticks, when the run completes, then AC-2.1 holds individually for each of the four combinations.

### US-4 (Must): Replaying the same input sequence is provably deterministic

> As the engine developer, I want two independent runs of the same ordered `GameInput` sequence, each from a freshly constructed instance, to produce byte-identical framebuffer contents after every tick, so constitution §4's determinism guarantee is a tested property of this mechanism, not an unverified claim.

**Acceptance criteria:**

- [ ] AC-4.1: Given a fixed, non-trivial ordered `GameInput` sequence (at least 50 ticks, mixing all four start/fire combinations) run twice against a freshly constructed test consumer whose own `update`/`render` logic is a deterministic function of its prior state and that tick's input (no time/rand reads), when the two runs' framebuffers are compared after **every individual tick**, then they are byte-identical at each one — not only the final tick.
- [ ] AC-4.2: Given AC-4.1's fixture uses randomness anywhere, then it is only ever exercised with an explicitly supplied seed, never an unseeded or time-derived one (constitution §4).
- [ ] AC-4.3: Given the full AC-4.1 run, when executed under `-fsanitize=address,undefined`, then zero findings occur.
- [ ] AC-4.4: Given the mechanism's and the test fixture's source, when grepped, then no wall-clock read and no unseeded RNG call appears anywhere in the delivered files (mirrors rendering-core NFR-5's grep gate).

## 5. Non-Functional Requirements

| # | Category | Requirement (measurable) | How it's verified |
|---|---|---|---|
| NFR-1 | Performance | AC-4.1's 50-tick replay (both runs combined) completes in < 5 ms with `-O2` on the reference host (Apple clang 14). Target-device 60 Hz real-time pacing is explicitly **not** measured or claimed this cycle (A8). | host benchmark test + `/peer-review` |
| NFR-2 | Reliability / memory | Zero dynamic allocation anywhere in the delivered mechanism, `GameInput` type and test fixture (grep gate): no `new`/`malloc`/`std::vector`/`std::string`. | `/peer-review` (grep) |
| NFR-3 | Reliability / bounds | Full suite runs under `-fsanitize=address,undefined` with zero findings across every AC above. | AC-1.4, AC-4.3 |
| NFR-4 | Portability / toolchain | Compiles clean under `clang++` and `g++`, `-std=c++17 -Wall -Wextra -Werror`; no ESP-IDF header in any delivered file; no new resolution/tile literal is introduced — this story adds no new screen-size logic and inherits `config.h` unchanged (A12). | `/peer-review` (grep + both compilers) |
| NFR-5 | Determinism | Same ordered `GameInput` sequence + same initial state → byte-identical framebuffer sequence, both compilers (constitution §4's exact wording, A3); no engine or fixture code reads wall-clock time or draws from an unseeded random source. | AC-4.1, AC-4.2, AC-4.4 + `/peer-review` |
| NFR-6 | **Library lens — public API surface** | Exactly one tick-driving entry point, one `update` contract slot, one `render` contract slot; `GameInput` carries exactly its two named fields (US-3) and no speculative field without a named consumer in this spec. Any other new public symbol is a review finding. | `/peer-review` |
| NFR-7 | **Library lens — contract clarity** | Doc comment states: the fixed update-then-render call order per tick (US-1/US-2), the no-implicit-clear contract (AC-2.3), the no-real-time-pacing contract (AC-1.3, A8), the inherited single-threaded/no-throw contract, and `GameInput`'s exact two fields with no decoding applied (US-3). One usage example showing a minimal consumer. | `/peer-review` |
| NFR-8 | Observability / ops | N/A — pure in-memory call sequencing, no runtime failure mode, no logging surface yet; the only diagnostic output is the test runner naming the failing test (inherited pattern). | — |
| NFR-9 | Security & privacy | N/A — offline device, no input persisted, no personal data (constitution §2). | — |
| NFR-10 | Accessibility | N/A — nothing in this increment reaches a display driver or panel; the test fixture's framebuffer output is verification data, not product UI. Becomes live once a display driver exists. | — |
| NFR-11 | Integrations & dependencies | N/A — no external system, library or new dependency. The only "integrations" are the future Input Abstraction / Collision System / Game State Management / Audio API stories — this feature exposes a contract to them, it calls none of them. | — |

*Lens note: `library` active **scoped** (constitution §2); semver/packaging are no-ops. Public API surface and Contract clarity land as NFR-6/NFR-7.*

## 6. Out of Scope

- **Any game-state machine** (`BOOT/READY/PLAYING/GAME_OVER`, restart wiring). README's own Phase-2 checklist lists "Game Loop" and "Game State Management" as separate items; this story delivers the first, not both (A4).
- **`Entity`/`onCollision(Entity&, Entity&)` and any collision surface.** No `Entity` concept exists; README lists "Collision System" as its own unstarted item (A5).
- **`INPUT`, `COLLISION` and `AUDIO` as call-contract steps.** Narrowed to `UPDATE→RENDER`; `INPUT` is the `GameInput` parameter, `COLLISION`/`AUDIO` have no engine support yet (A6).
- **Real button/joystick decoding, debouncing, edge-detection, GPIO reads.** `GameInput` is a raw two-field pass-through; no hardware is wired (README Phase 1) (A7).
- **Any real-time/wall-clock 60 Hz scheduler on hardware.** Requires ESP-IDF/FreeRTOS, not installed (A8, constitution §4).
- **Implicit framebuffer clearing, double buffering, page flipping.** `render()` owns clearing explicitly if it wants it (A9).
- **A game registry, multiple concurrent consumers, or self-registration.** One test consumer at a time; the registry is its own future constitution §3 item.
- **Score, highscore, persistence, system menu, boot screen, settings.** Unchanged from both prior specs — still future stories.
- **Audio hooks of any kind.** Audio API is its own unstarted Phase-2 item.
- **Display driver, ILI9488, SPI, tile push.** No board wired, no ESP-IDF installed — unchanged from rendering-core/text-rendering.

## 7. Clarifications

| # | Date | Question | Resolution |
|---|---|---|---|
| C1 | 2026-09-02 | Does a `BOOT→READY→PLAYING→GAME_OVER` state machine belong in this first slice? | **No.** README's own Phase-2 checklist already treats "Game Loop" and "Game State Management" as two separate, still-unchecked items — cut, deferred (A4, §6). |
| C2 | 2026-09-02 | Does `onCollision`/`Entity` belong here? | **No.** No `Entity` type exists; README names "Collision System" as its own future item (A5, §6). |
| C3 | 2026-09-02 | Does this story implement README's full 5-step per-frame diagram (`INPUT→UPDATE→COLLISION→RENDER→AUDIO`)? | **No — narrowed to `UPDATE→RENDER`.** `INPUT` becomes the `GameInput` parameter; `COLLISION`/`AUDIO` have no engine support yet and are each their own future item (A6). |
| C4 | 2026-09-02 | What fields does `GameInput` carry? | **Exactly `start` and `fire`** — the two buttons README's Controller section already names, raw pass-through, no decoding (no hardware wired yet) (A7). |
| C5 | 2026-09-02 | Is real 60 Hz wall-clock pacing tested or claimed? | **No.** The tick is a caller-driven discrete step; an actual scheduler needs ESP-IDF/FreeRTOS, absent today (A8). |
| C6 | 2026-09-02 | Does the mechanism clear the framebuffer between ticks? | **No, never implicitly.** Clearing is `render()`'s own explicit choice (A9). |
| C7 | 2026-09-02 | How is "identical output" for determinism defined precisely? | **Byte-identical framebuffer contents after every individual tick**, constitution §4's own wording, verified via a synthetic in-repo test double rather than a real game (A3, A10, US-4). |
| C8 | 2026-09-02 | Half-size version, if the story must shrink? | US-1 + US-2 alone (the mechanism and its data-flow contract) still deliver core value. If forced further, narrow US-3 to `start` only before dropping anything from US-4 — the replay proof is the constitution's own explicit guarantee and is the last thing cut. |

## 8. Design Review

- **N/A, user-confirmed.** This story delivers a pure call-order/timing mechanism and a two-field data struct — no color, layout, visual shape, or interaction surface is decided here (unlike text-rendering, which specified actual glyph shapes). NFR-10 already records that nothing in this increment reaches a display driver or panel. Same posture as rendering-core, which also had no design review. If a future story adds visible on-screen state transitions, that story gets its own design pass — not retrofitted here.

---

## ✅ SPEC GATE

*All boxes checked → `/sprint-plan` may start. Any box open → back to `/story-time` or `/look-and-feel`.*

- [x] Problem, goal and success signal are concrete (no buzzwords, no "everyone")
- [x] Every story has testable Given/When/Then acceptance criteria
- [x] Stories are prioritized (MoSCoW) and at least one is a Must
- [x] Non-functional requirements are stated and measurable (or marked N/A with reason)
- [x] Clarify pass done: functional scope (C1–C3), data (A9/C6), roles/permissions (N/A, no UI), error/edge cases (AC-1.2, AC-2.4), NFRs (NFR-1…11), integrations (NFR-11), UX flows (N/A, no UI), out-of-scope (§6) — all swept
- [x] Open questions are resolved or explicitly accepted as risk — 0 open (see Handoff: scope cuts flagged for user visibility, not blocking)
- [x] Out-of-scope section is filled (something was consciously cut)
- [x] Constitution (`.spark/constitution.md`) respected — §3 Timing and §4 Determinism directly grounded (A3); no conflict found
- [x] Design review done for UI-facing features (or marked N/A with reason) — N/A, user-confirmed (§8): pure mechanism, no visual/UI surface
- [x] Line budget respected: Ist 152 / Soll ~250 (excluding HTML comments)
- [x] Status set to `approved` by the user
