# Spec: input-driver

| | |
|---|---|
| **Phase** | Specify |
| **Owner** | Product Owner (`/story-time`), Designer (`/look-and-feel`) |
| **Status** | `approved` |
| **Date** | 2026-09-04 |
| **Ticket** | `none` |

**Handoff**
- **Status:** `approved` — user approved as proposed, including the expanded `GameInput` field set (start/fire/select/up/down/left/right), the US-4 Must→Should hardware gate, and Design Review N/A (no new visual surface).
- **Summary:** `GameInput` grows from two fields to seven — `start`, `fire`, `select`, `up`, `down`, `left`, `right`, all raw independent booleans, no encoding — populated by one debounced, host-testable core (a `TilePusher<Transmitter>`-style pure/impure seam) proven against a simulated GPIO source. No physical button, joystick or SELECT switch exists yet, so this cycle also delivers a wiring guide/pin table (US-3) so the user has exactly what's needed to go acquire and wire the right parts. On-device confirmation (US-4) is a **Should**, explicitly gated on that hardware existing by `/increment` time — the Musts (US-1–US-3) are all provable without it.
- **Open:** `0` forced-choice forks remain. `3` PO judgment calls made resolving them this round (§7 C6–C8) — not blocking, but worth the user's sanity-check before approving.
- **Binding ruling:** §4 User Stories for the current stories; §7 Clarifications for what changed since the last round and why.
- **On conflict:** the numbered body below wins for everything except `Status`; log the mismatch as a finding at the next `/peer-review` and proceed.

## 1. Problem & Goal

- **Problem:** the console has a proven render path (`display-driver`, v0.2.0) and a proven session state machine (`GameSession`, v0.1.0, READY/PLAYING/GAME_OVER, edge-triggered restart) — but every input either of them has ever seen is a literal `GameInput{}` written by test/harness code, and `GameInput` itself only ever knew about two of README's five planned controller signals (`start`/`fire` — never the joystick's four directions or `SELECT`). No human has ever started, fired, steered or selected anything on the physical cabinet, and no physical button, joystick or SELECT switch is wired to the board yet.
- **Goal:** extend `GameInput` to all seven signals README's Controller section commits to, and deliver a debounced, host-testable core that populates them from raw GPIO-shaped reads — proven first against a simulated source (no hardware required), then, once the board is actually wired per this feature's own wiring guide, confirmed live on the physical cabinet.
- **Success signal:** host-side, a simulated source proves every one of the seven signals debounces correctly in isolation and in combination (including a simultaneous two-direction "diagonal") with zero ESP-IDF header touched. Once wired: a human presses START once from READY and the serial log shows exactly one READY→PLAYING transition — never re-firing while held, never missing a press — the same for GAME_OVER's one-press restart; FIRE, SELECT and each joystick direction (including a deliberate diagonal) each show their correct debounced level in the log when physically actuated, with zero spurious extras across a sustained multi-minute session of varied real presses.
- **Why now:** the explicit mirror of `display-driver` — that closed the engine→pixel gap, this closes the human→engine gap, and does it once for the whole planned controller surface rather than twice (start/fire now, joystick/SELECT later as a second, riskier public-API change to an already-shipped type). `GameSession` has sat fully proven but untouched by real input since v0.1.0; every future game/menu increment needs trustworthy real input — including SELECT and steering — to be playable at all.

## 2. Target Users

- **Engine developer (primary, today):** needs to physically verify `GameSession`'s state transitions from real button presses, and needs the full seven-signal `GameInput` contract settled once, not extended piecemeal across multiple future public-API changes to an already-released type.
- **Future game-module author (indirect beneficiary):** this cycle's on-device harness proves the exact contract they'll rely on — a fully populated `GameInput` reaching their `update()` with no debounce logic of their own to write, and no surprise mid-project extension of the input type once they've already built against it.
- *Not a user of this feature:* the console player. No game, menu or registry exists yet — this delivers the input pipe (plus one throwaway verification harness and a wiring guide), not gameplay, matching `display-driver`'s own posture.

## 3. Assumptions & Open Questions

| # | Assumption / Question | Resolution |
|---|---|---|
| A1 | **`GameInput`'s shape — user override, PO field-set/order judgment.** The user chose to extend `GameInput` now with the joystick and SELECT, not just `start`/`fire`. PO decision: exactly **seven raw, independent booleans — `start`, `fire`, `select`, `up`, `down`, `left`, `right`** — no encoding, no bitmask, no direction enum, matching `GameInput`'s own existing "raw pass-through, no decoding" philosophy (`game_loop.h` header comment). **Field order deliberately keeps `start`/`fire` declared first**, exactly as they are today: every existing positional `GameInput{a, b}`-style literal in the codebase (verified — every call site in `game_state_test.cpp`, `session_replay_fixture.h`, `replay_fixture.h`, `game_loop_test.cpp` uses exactly 0 or 2 positional arguments, never more) keeps its original `start=a, fire=b` meaning unchanged, not merely "still compiles" (NFR-8). | **Resolved (user override + PO judgment)** |
| A2 | **Joystick topology resolved at the `GameInput` level, not deferred to the enclosure.** README's Controller section ("4/8-Wege-Joystick... die finale Anzahl und Anordnung wird zusammen mit dem Gehäuse festgelegt") leaves the *enclosure-level* joystick hardware genuinely undecided. Four independent direction bits make 4-way support direct and 8-way/diagonal support a natural consequence of two adjacent bits held simultaneously (AC-1.4) — no separate "8-way mode" or diagonal encoding is needed at this layer at all. Same "resolve what can be resolved now, defer what genuinely depends on unbuilt hardware" reasoning `game-loop`'s own spec used for `GameInput` originally (its A7). Still genuinely deferred: the physical joystick component's exact panel position, mounting and final button count/layout (README Phase 4) — not its data shape. | **Resolved (PO judgment)** |
| A3 | **Physical hardware readiness — user override, PO Must/Should-split judgment.** The user confirmed no buttons, joystick or SELECT switch are physically acquired or wired yet ("noch nichts vorhanden"), and asked for wiring/acquisition to become explicit scope. PO decision, chosen over the alternative of blocking spec approval on an undated physical event: this spec's Musts (US-1–US-3) are all achievable and verifiable — host-side for the debounce logic, document-only for the wiring guide — with zero physical part in hand. On-device confirmation (US-4) is scoped as a **Should**, explicitly conditioned on that hardware existing by `/increment` time — mirrors `display-driver`'s own "first thing dropped if this story must shrink" posture for its US-5, and the same real-world-event-gated pattern the constitution used for the display panel's arrival (§3, "confirming against the real hardware is the first thing done with it"). | **Resolved (user override + PO judgment)** |
| A4 | **The wiring guide/pin table (US-3) is an expected deliverable of this feature** — comparable to `display-driver`'s own pin-assignment work, except here the parts don't exist yet, so the guide's job is to tell the user exactly what to acquire and how to wire it, not to confirm wiring already done. | Accepted |
| A5 | Seven new GPIO pins land in `board_config.h`, one per signal, avoiding GPIO26–37 (PSRAM/flash), boot-strapping pins 0/3/45/46, USB D-/D+ (19/20), and the display's already-claimed 9–14 (constitution §3). Exact pin numbers are a `/sprint-plan` decision; this spec only requires they land in `board_config.h`, never as a bare literal elsewhere. | Accepted |
| A6 | **Debounce correctness bar.** For every one of the seven signals independently: a debounced signal must produce exactly one logical transition per deliberate physical press/release, with no felt input lag — "arcade immediacy" (constitution Principle 4) bounds responsiveness qualitatively; the exact debounce window (ms) is a `/sprint-plan` engineering decision, not fixed here. | Accepted |
| A7 | **On-device verification method** reuses `display-driver`'s now-proven precedent: serial log transcript + a human physically actuating real controls, cross-checked against the log and (where the same harness also drives the already-shipped display) the physical panel showing `GameSession`'s state. No oscilloscope/electrical bounce measurement required. | Accepted |
| A8 | **Held-vs-edge split applies uniformly to all seven signals.** The debounce layer reports a clean **level** for every signal (true while physically held, false while released) — it does not itself edge-detect, for any signal. `GameSession::advance` (already shipped) keeps sole ownership of edge detection, and only for `start`; the other six signals (`fire`, `select`, four directions) have no consumer-side interpretation yet — `GameSession` is unmodified and still reads only `.start` (game_state.h's own documented contract: "`input.fire` never affects the state, in any state"). | Accepted |
| A9 | No dynamic allocation, single-threaded, no wall-clock read — same inherited contract as every prior increment; debounce timing is tick-driven (the existing deterministic 60Hz step), never `<chrono>`/wall-clock, so `GameLoop`'s replay-determinism guarantee is not broken by the input layer. | Accepted |
| A10 | **One shared debounce mechanism, not seven bespoke ones.** A single generic debounce component, parameterized on a raw-level source the same way `TilePusher<Transmitter>` is parameterized on a `Transmitter` (host tests supply a fake/simulated source; the real device-side GPIO reader satisfies the same concept on-device), is instantiated seven independent times — one per signal, each carrying its own bounce-timing state. No signal gets special-cased logic (AC-2.2). | **Resolved (PO judgment)** |
| A11 | Wiring a real running consumer, menu or registry, and building any actual playable game, are out of scope — mirrors `display-driver`'s own boundary drawn around `GameSession` (its A3/§6). | Accepted |

## 4. User Stories

### US-1 (Must): Debounced signals populate all seven GameInput fields, proven against a simulated source

> As the engine developer, I want a debounced core that populates all seven `GameInput` fields from raw, GPIO-shaped reads, proven correct against a simulated/fake source, so the full input contract is trustworthy before any physical hardware exists.

**Acceptance criteria:**

- [ ] AC-1.1: Given a fake/simulated source (host-test double, mirroring `TilePusher<Transmitter>`'s pure/impure seam) reporting each of the seven raw signals individually pressed then released, when sampled tick-by-tick through the debounce core, then the corresponding `GameInput` field shows exactly one clean false→true→false level cycle for that one simulated event — no extra glitches — independently of the other six fields' state.
- [ ] AC-1.2: Given any one signal's simulated source held continuously true across many consecutive ticks, when read, then that field reads `true` on every one of those ticks — a level, not a single pulse (A8).
- [ ] AC-1.3: Given no signal asserted, when read at the first tick and at any time after, then all seven `GameInput` fields read `false` — no floating or undefined state.
- [ ] AC-1.4: Given two or more direction signals simulated `true` simultaneously (e.g. `up` + `right` held together), when read, then both corresponding fields read `true` independently — `GameInput` makes no attempt to resolve, forbid or collapse simultaneous direction bits into a single "diagonal" value; the four independent bits are the entire contract (A2).

### US-2 (Must): One shared debounce mechanism filters bounce without felt lag, applied identically to all seven signals

> As the engine developer / future player, I want one shared, generic debounce mechanism — never seven bespoke ones — filtering a noisy raw signal into one clean level change per press, without making any control feel sluggish, so a human pressing a real control gets exactly one clean level change, never spurious multiples.

**Acceptance criteria:**

- [ ] AC-2.1: Given a raw simulated source exhibiting mechanical bounce (multiple rapid highs/lows within a few milliseconds around one simulated press) on any one of the seven signals, when passed through the debounce mechanism under a host-run fixture, then exactly one clean level transition is reported for that field for that one simulated event.
- [ ] AC-2.2: Given the same bounce fixture applied to all seven signals independently within one test run, when read, then each field's debounced outcome is correct independently of what the other six are doing at the same tick — proving A10's "one shared mechanism, seven independent instances" design actually isolates per-signal state, not seven different algorithms.
- [ ] AC-2.3: Given a debounced `GameInput.start` sequence fed into `GameSession::advance` (already-shipped, unmodified), when a single simulated press occurs while in READY, then `GameSession` transitions to PLAYING exactly once for that press — never a second entry from the same underlying press, never a missed edge.
- [ ] AC-2.4: Given the debounce core's source, when compiled and run under the host `make test` gate, then it builds and passes with zero ESP-IDF header included.

### US-3 (Must): Pin assignment and a physical wiring guide are ready before any hardware exists

> As the user who will go acquire and wire the parts, I want a named GPIO pin table and a written wiring reference for all seven signals, so I know exactly what to buy and how to wire it — before I own a single button.

**Acceptance criteria:**

- [ ] AC-3.1: Given `board_config.h`, when read, then it names seven distinct GPIO constants, one per `GameInput` signal, each avoiding GPIO26–37, boot-strapping pins 0/3/45/46, USB D-/D+ (19/20), and the display's already-claimed 9–14 (A5) — no bare GPIO literal for these signals appears anywhere else.
- [ ] AC-3.2: Given a written wiring reference (`docs/`), when read, then for each of the seven named signals it states its GPIO pin and its expected switch topology, and, for the four direction signals specifically, that they are four independent single-pole switches sharing a common ground — not a single multi-position or analog input — consistent with US-1/AC-1.4's data model.
- [ ] AC-3.3: Given the wiring reference, when read, then it states plainly that no physical button, joystick or SELECT switch is wired yet, and names this as the explicit next physical-world action needed before US-4 can be attempted.

### US-4 (Should, gated on hardware existing): Real presses reach GameSession and GameInput, confirmed on the physical board

> As the engine developer, I want a minimal on-device harness (mirrors `display-driver`'s throwaway synthetic-consumer pattern) driving a real `GameSession` from real physical presses, so the full human-press → `GameInput` → `GameSession`/game-logic pipeline is proven end to end — once the board is actually wired per US-3's guide. If hardware does not exist by `/increment` time, this story's ACs are parked as explicitly unverified, not silently dropped, per constitution §4's honest-status-reporting rule.

**Acceptance criteria:**

- [ ] AC-4.1: Given the on-device harness running with a physical START button wired, when a human presses START once while the harness sits in READY, then the serial log records exactly one READY→PLAYING transition.
- [ ] AC-4.2: Given the harness in GAME_OVER (reached via the harness's own synthetic `sessionEnded` trigger, not a real game), when a human presses START once, then the serial log records exactly one GAME_OVER→PLAYING transition.
- [ ] AC-4.3: Given the harness running for a sustained multi-minute session with a human pressing START and FIRE in varied patterns (rapid, held, released), when the serial log is reviewed, then the number of logged `GameSession` transitions matches the number of deliberate START presses the human counted — zero spurious extras — and FIRE's raw debounced level is separately logged matching each press/release the human performed (`GameSession` itself never transitions on FIRE, A8).
- [ ] AC-4.4: Given the joystick's four directions and SELECT physically wired per US-3's guide, when a human actuates each in turn — including at least one deliberate two-direction diagonal (e.g. `up`+`right` held together) — then the serial log records each corresponding `GameInput` field toggling to match, independently and correctly for the diagonal case (confirming AC-1.4's simultaneous-bits model on real hardware, not just simulated); no `GameSession` behavior is asserted for these five signals, since no consumer reads them yet.
- [ ] AC-4.5: Given the on-device harness's source, when read, then `GameSession`'s and `GameLoop`'s already-shipped public APIs are used exactly as documented, unmodified — no new parameter, no new method.

## 5. Non-Functional Requirements

| # | Category | Requirement (measurable) | How it's verified |
|---|---|---|---|
| NFR-1 | Performance | Debounce adds no perceptible input lag on any of the seven signals: bounded by a documented, small millisecond window (exact value a `/sprint-plan` decision). For START/FIRE (the only signals with a visible consumer today), a human on-device tester judges press-to-visible-transition as immediate; for the other five, "no perceptible lag" is judged from the serial log's timestamp gap between physical actuation and logged level change. Chosen window recorded in `qa.md`. | qa.md (on-device, human-judged + recorded value) |
| NFR-2 | Reliability / memory | Zero dynamic allocation anywhere in the driver: no `new`/`malloc`/`std::vector`/`std::string`, including the seven independent debounce-state instances (A10), which are fixed-size. | `/peer-review` (grep) |
| NFR-3 | Determinism | Debounce logic is tick-driven only — never reads wall-clock time (`<chrono>`, `time()`, etc.) or unseeded RNG — uniformly for all seven signals (A10), so `GameLoop`'s replay-determinism guarantee is not broken by the input layer. | `/peer-review` (grep) |
| NFR-4 | Constitution literals | No new GPIO literal outside `board_config.h` — extends to all seven new named constants (A5). | `/peer-review` (grep) |
| NFR-5 | Observability / ops | Every real, human-driven `GameSession` transition, and every debounced level change on the five signals `GameSession` doesn't consume, is logged by the on-device harness with at minimum which signal and its resulting level/transition (constitution §4 "honest status reporting"). | AC-4.1–4.4 |
| NFR-6 | **Library lens — public API surface** | The changed/new public surface, named exactly: (1) `GameInput` itself gains five fields (seven total) — a public-API change to an already-released type (`game-loop` v0.0.4), not a new type; (2) one debounce-core entry point/type, parameterized on a raw-level source concept the same way `TilePusher<Transmitter>` is parameterized on a `Transmitter` — host tests supply a fake/simulated source, the real device-side reader satisfies the same concept on-device. No other new public symbol; internal GPIO/timing plumbing stays private. The on-device harness (US-4) is explicitly throwaway/test-only, not counted against this list — mirrors `display-driver`'s NFR-7/A13. | `/peer-review` |
| NFR-7 | **Library lens — contract clarity** | Doc comment on `GameInput` states its seven fields are raw, undecoded, independent booleans — no diagonal encoding, no edge-detection (extends `game_loop.h`'s existing header comment). Doc comment on the debounce-core entry point states: returns a level, not an edge (`GameSession` alone owns edge-detection, for `start` only, A8); tick-driven, never wall-clock (NFR-3); single-threaded, no-throw, no dynamic allocation; one shared mechanism, seven independent instances (A10); one usage example, mirroring `DirtyTracker`'s/the display driver's own header-comment style. | `/peer-review` |
| NFR-8 | **Library lens — contract stability under extension** | `GameInput`'s field order deliberately keeps `start`/`fire` declared first (A1) so every already-shipped positional `GameInput{a, b}`-style literal keeps its original `start=a, fire=b` meaning unchanged after this extension — not merely "still compiles." The `static_assert` enforcing `GameInput`'s exact size moves from `2 * sizeof(bool)` to `7 * sizeof(bool)` — a deliberate, visible compile-time signal of the change. Migrating existing call sites to name all seven fields explicitly (or accept the trailing defaults) is `/sprint-plan`'s implementation task list, not a spec-level compatibility break. | `/peer-review` (grep for `sizeof(GameInput)`, the `static_assert`, and every `GameInput{` call site) |
| NFR-9 | Security & privacy | N/A — offline device, no personal data, no network path touched by an input read. | — |
| NFR-10 | Accessibility | N/A — no new visual/UI surface; physical control ergonomics/final layout is explicitly README's enclosure-era (Phase 4) decision, not this feature's (A2). | — |

*Lens note: `library` active **scoped** (constitution §2) — semver/packaging are no-ops for this statically-linked firmware image; Public API surface, Contract clarity and Contract stability under extension land as NFR-6/NFR-7/NFR-8.*

## 6. Out of Scope

- **Any interpretation of SELECT or the four joystick directions beyond a raw level** — e.g. menu-navigation binding, an 8-way-vs-4-way mode flag, or any diagonal-encoding abstraction layered on top of the four raw bits (A2). They are raw levels like `start`/`fire`, with no consumer semantics defined this cycle.
- **Wiring, soldering or acquiring the actual physical buttons, joystick or SELECT switch** — that is the user's own physical-world action, guided by US-3's deliverable, not something any SPARK ceremony performs (A3).
- **On-device confirmation before hardware exists** — US-4 is a Should, explicitly parked (not silently dropped) until wiring is done (A3, §4).
- **Wiring a real running consumer, menu, or game registry; any actual playable game.**
- **Any change to `GameLoop`'s or `GameSession`'s already-shipped *behavior*** — `GameSession` still reads only `.start`; the five new fields are not wired into any session/state logic this cycle. Only `GameInput`'s data shape changes (NFR-6, NFR-8).
- **External USB/Bluetooth controllers** — README's own later-phase aspiration.
- **Final enclosure control count/layout** — resolved *for this increment* at the data-shape level (A2: four independent direction bits cover both 4-way and 8-way with no further decision needed here); the physical component's panel position, mounting and final layout genuinely stay README's Phase-4 "wird zusammen mit dem Gehäuse festgelegt" call, not something this spec still silently defers without saying so.
- **Debounce timing tuned beyond "no felt lag, no spurious edge"** — the exact millisecond window is `/sprint-plan`'s engineering call.
- **Oscilloscope or other electrical bounce-timing capture** — visual/log verification only (A7).
- **Edge-detection logic duplicated outside `GameSession`, for any of the seven signals** — this feature reports a level; `GameSession` already owns rising-edge detection, and only for `start` (A8).
- **A detailed electrical schematic (resistor values, exact switch part numbers)** — US-3's wiring guide names pins, signal-to-pin mapping and switch topology (independent NO switches, common ground); component-level circuit design is a `/sprint-plan` engineering decision.

## 7. Clarifications

| # | Date | Question | Resolution |
|---|---|---|---|
| C1 | 2026-09-04 | Does `GameInput` need extending for a joystick/SELECT, or do the two existing fields suffice? | **Extended — user override.** `GameInput` gains the joystick's four directions plus `SELECT` this cycle, alongside `start`/`fire` — seven fields total. Exact field set/order: C6. |
| C2 | 2026-09-04 | Is physical wiring (acquiring/connecting real controls) part of this cycle's scope? | **Partially — user override.** Producing the wiring guide/pin table (US-3) is in scope now; physically acquiring and connecting the parts, and the resulting on-device confirmation (US-4), stay conditioned on that hardware existing. See A3, C7. |
| C3 | 2026-09-04 | Does the debounce layer edge-detect, duplicating `GameSession`'s own rising-edge logic — for any of the seven signals? | **No, for any signal.** The debounce layer reports a clean level only, uniformly; `GameSession::advance` (already shipped) keeps sole ownership of edge detection, and only for `start` (A8, §6). |
| C4 | 2026-09-04 | Can debounce timing read wall-clock time? | **No.** Tick-driven only, never `<chrono>`/wall-clock, uniformly across all seven signals (A9, NFR-3). |
| C5 | 2026-09-04 | What counts as "verified" for this feature's Must stories, given no hardware exists? | Host-side, against a simulated/fake source, for US-1–US-3 (A6, A10). On-device verification (US-4) reuses `display-driver`'s serial-log + human-observation method once hardware exists (A7). |
| C6 | 2026-09-04 | Given `GameInput` now carries seven fields, what exact set and declaration order? | **PO judgment call.** `start, fire, select, up, down, left, right` — raw, independent, no encoding (A1). Order deliberately keeps `start`/`fire` first so every existing positional `GameInput{a, b}` literal keeps its original meaning (NFR-8). Flagged for the user's sanity-check before approval, not silently assumed. |
| C7 | 2026-09-04 | Given no hardware exists yet, is on-device confirmation a Must or a Should, and does that block spec approval? | **PO judgment call.** Should, gated on hardware existing by `/increment` time (A3) — not a block on spec approval itself, since the Musts are fully verifiable without hardware. Rejected alternative: pausing the whole `/story-time` gate on an undated physical event. |
| C8 | 2026-09-04 | Does each of the seven signals get its own bespoke debounce algorithm, or one shared mechanism? | **One shared, generic mechanism, instantiated seven independent times** (A10) — the same `TilePusher<Transmitter>`-style pure/impure seam already proven in this codebase, not seven different algorithms. |
| C9 | 2026-09-04 | Does README's "joystick topology decided with the enclosure" language still block this feature from committing to a `GameInput` shape? | **No — resolved for this increment (A2).** Four independent direction bits support both 4-way and 8-way with no further decision needed at this layer; only the physical joystick component's panel position/count/layout stays genuinely deferred to Phase 4. |

## 8. Design Review

- **Overall impression:** **N/A — no new visual/UI surface.** Unlike `display-driver`
  (the first story to put pixels on the physical panel, per `rendering-core`'s own
  spec §8 foreshadowing), this feature adds no new visual output of any kind — it
  reads physical buttons and populates `GameInput`, reusing the already-approved
  `GameSession`/display pipeline purely as US-4's confirmation channel, not as
  something this feature designs or changes. Constitution §6's graphics philosophy
  has nothing new to review here.
- **Heuristics findings:** N/A — no user-facing visual interaction surface exists
  in this increment.
- **Accessibility notes:** N/A — see NFR-11. Physical button ergonomics (size,
  spacing, labeling) are an enclosure/Phase-4 decision, not this increment's.
- **Design risks & required changes:** none for the visual surface. The equivalent
  risk here is the *API* contract (`GameInput`'s field-set extension), which the
  constitution's active `library` lens covers instead — captured as NFR-6/NFR-7/
  NFR-8, verified at `/peer-review`.

---

## ✅ SPEC GATE

*All boxes checked → `/sprint-plan` may start. Any box open → back to `/story-time` or `/look-and-feel`.*

- [x] Problem, goal and success signal are concrete (no buzzwords, no "everyone")
- [x] Every story has testable Given/When/Then acceptance criteria
- [x] Stories are prioritized (MoSCoW) and at least one is a Must
- [x] Non-functional requirements are stated and measurable (or marked N/A with reason)
- [x] Clarify pass done: no ambiguity left unresolved or unparked — functional scope (C1/C6/C9), data (A1/A2, field set/order), roles/permissions (N/A, no UI), error/edge cases (C3/C8, per-signal debounce), NFRs (NFR-6/7/8 extended for the library lens), integrations (A5, board_config.h), UX flows (N/A, no UI), out-of-scope (§6, joystick topology and SELECT interpretation explicitly resolved-for-this-increment, not left open) — all swept
- [x] Open questions are resolved or explicitly accepted as risk — both prior forks (A1, A3) are resolved via the user's explicit override plus named PO judgment calls (A1, A2, A3, A10), logged in §7 C6–C9, not silently assumed
- [x] Out-of-scope section is filled (something was consciously cut)
- [x] Constitution (`.spark/constitution.md`) respected, or conflicts recorded as open questions — no conflict found; extending `GameInput` is a scoped public-API decision, not a constitutional conflict
- [x] Design review done for UI-facing features (or marked N/A with reason) — marked N/A (§8): no new visual/UI surface, confirmed by the user
- [x] Line budget respected: Ist 162 / Soll ~250 (excluding HTML comments)
- [x] Status set to `approved` by the user
