# Spec: collision-system

| | |
|---|---|
| **Phase** | Specify |
| **Owner** | Product Owner (`/story-time`), Designer (`/look-and-feel`) |
| **Status** | `approved` |
| **Date** | 2026-09-05 |
| **Ticket** | `none` |

**Handoff**
- **Status:** `approved` — user approved 2026-09-05, after explicitly confirming A1, A2 and A4 (Entity shape, callback mechanism, touching-edge semantics), with A3 and A5–A10 accepted-by-silence.
- **Summary:** No `Entity` type and no collision primitive exist anywhere in the engine (constitution §2); every roadmap game needs one. Deliver the smallest possible position+bounds type, a pure AABB overlap test over it, and a stateless callback-dispatch mechanism invoked exactly when two such bounds overlap — no physics, no game rules, no hierarchy beyond position+bounds. All PO judgment calls in §3 (A1–A10) are now resolved — see Open line.
- **Open:** `0 open` — all of A1–A10 in §3 are resolved: **A1, A2 and A4 were explicitly confirmed by the user on 2026-09-05** (Entity is a four-`int32_t`-field POD struct with no ID/tag/velocity/user-data; the callback is a compile-time template-parameter callable, never `std::function`/a function pointer; touching-but-not-overlapping edges do not count as a collision), each after the stated alternative was offered and explicitly declined. **A3 and A5–A10 are accepted-by-silence** — the user reviewed the full draft and raised no objection to any of them. Nothing here is genuinely open pending further input; the only remaining step is the user's own `approved` decision.
- **Binding ruling:** §4 User Stories for the current stories; §7 Clarifications for what was decided and why.
- **On conflict:** the numbered body below wins for everything except `Status`; log the mismatch as a finding at the next `/peer-review` and proceed — don't stop on it.

## 1. Problem & Goal

- **Problem:** Every roadmap game (Galactic Invasion, Steam Racer, Airship Battle, Mine Runner) fundamentally depends on knowing when two on-screen objects touch — a bullet vs. an enemy, a ship vs. an obstacle, a player vs. a hazard — but no such primitive exists anywhere in the engine, and neither does any game-object concept: reading every header under `firmware/steamcore/include/steamcore/` confirms zero `Entity` type, matching the constitution's own §2 note that `onCollision(Entity&, Entity&)` "is not yet implemented — no `Entity` type exists anywhere in the codebase." A game-module author today has nothing to build against; each would invent its own position+bounds representation and its own overlap math, one game at a time — exactly the duplication `game-state-management` was built to prevent for session bookkeeping.
- **Goal:** One minimal, host-testable primitive: a position+bounds representation small enough that "no entity hierarchy beyond position+bounds" is true by construction, a pure AABB overlap test over it, and a stateless callback-dispatch mechanism fired exactly when two such bounds overlap — nothing else. A future game composes this into its own `update()`, the same way it will compose `GameSession`.
- **Success signal:** a host test suite proves the overlap test correct at ordinary, boundary (touching-but-not-overlapping), degenerate (zero/negative size), and extreme (`int32_t` min/max) inputs, and proves the callback fires exactly when — and only when — two entities overlap, both for a single pair and for a fixed-size sweep over several — 0 failures, 0 sanitizer findings, on both supported compilers. Deferred signal: the first roadmap game (or a synthetic test game) composes against this without editing a file delivered here.
- **Why now:** this is the one primitive the constitution itself already flags as unbuilt (§2), and — per the `/next-steps` analysis that produced this idea — it is the single item genuinely blocking every roadmap game rather than a nice-to-have; all 8 shipped features to date are engine infrastructure, and zero games exist yet.

## 2. Target Users

- **Future game-module author (primary):** needs one proven position+bounds type and overlap test instead of inventing one per game.
- **Engine developer (today, same person):** builds and proves this primitive now, while it is cheap and isolated, rather than retrofitting it once a game already has its own ad hoc collision code.
- *Not a user of this feature:* the console player. No visual, no sound, no game exists yet that a player interacts with; this is a pure logic primitive, same posture as `game-state-management`.

## 3. Assumptions & Open Questions

| # | Assumption / Question | Resolution |
|---|---|---|
| A1 | **What is `Entity`, minimally?** The idea's own wording ("no entity hierarchy beyond position+bounds") is the steer. **Judgment call: a single POD struct carrying exactly position (x, y) and size (w, h) as four `int32_t` fields — no ID/type-tag, no velocity, no arbitrary user data, no rendering reference.** The constitution's "Entity" wording is treated as literal (this is genuinely the smallest thing that name can mean), not as license to invent a class hierarchy. **Confirmed by user, 2026-09-05** — the alternative (a generic, un-named rect-vs-rect free function with no `Entity` concept at all) was explicitly offered and declined. |
| A2 | **What shape is the callback mechanism?** **Judgment call: a template parameter (a compile-time callable — lambda, free function or functor), never `std::function`/a function pointer.** Matches this codebase's existing zero-vtable, template-seam convention (`GameLoop<Game>`, `InputReader<Source>`, `TilePusher<Transmitter>`), and `std::function` is already on `tools/check_constraints.sh`'s forbidden-container list — using it would fail the project's own lint gate. The caller decides what "collision happened" *means* (mirrors `GameSession` never deriving what ends a session); this type only detects and reports the fact. **Confirmed by user, 2026-09-05** — the function-pointer alternative was explicitly offered and declined. |
| A3 | How many entities does one detection pass handle? **Judgment call: the Musts (US-1, US-2) cover exactly one pair.** A fixed-size, no-allocation N-entity pairwise sweep is added as a Should (US-3), not required — matches the idea's own wording ("between two Entity instances"). | Accepted-by-silence, 2026-09-05 (C1) — user reviewed the full draft and raised no objection; distinct from A1/A2/A4's explicit confirmation. |
| A4 | Do touching-but-not-overlapping bounds (edges exactly adjacent, zero shared area) count as a collision? **Judgment call: no — strict overlap only.** Two entities whose edges merely meet produce `false`. This is the more common convention in 2D game AABB tests and avoids a "collision" firing for objects that are visually just adjacent, not touching. **Confirmed by user, 2026-09-05** — the inclusive alternative (touching counts as a collision) was explicitly offered and declined; either reading is defensible, but only one could ship. | Confirmed by user, 2026-09-05 |
| A5 | What happens with a zero-width/zero-height or negative-size entity? **Judgment call: treated as an empty region that can never overlap anything, including another empty region** — mirrors `Framebuffer::fillRect`'s own "non-positive width/height is a no-op" precedent, so this feature's edge-case posture matches an already-established engine convention rather than inventing a new one. | Accepted-by-silence, 2026-09-05 (C2) — user reviewed the full draft and raised no objection. |
| A6 | What about negative coordinates and values at the extremes of `int32_t` (potential overflow in an `x + w` style sum)? **Judgment call: negative coordinates are valid and meaningful** (an entity — e.g. a freshly spawned bullet — may legitimately sit above/left of the visible 240×160 screen; this primitive is not bounded by or clipped to screen dimensions, unlike `Framebuffer`'s drawing calls). **The overlap test must never overflow or invoke UB at any `int32_t` input**, computing pairwise sums in a wider type before comparing — mirrors `Framebuffer::fillRect`/`setPixel`'s own documented int64_t-before-narrowing precedent. | Accepted-by-silence, 2026-09-05 (C3) — user reviewed the full draft and raised no objection. |
| A7 | Is this feature purely host-testable? **Yes — 100% host-CI-verifiable, zero ESP-IDF dependency, zero rendering.** This is pure geometry/math with no I/O and no visual surface at all — a stronger claim than `game-state-management`'s (which was also fully host-testable, the closest sibling precedent), since that feature at least anticipated a future rendered "GAME OVER" text; this one anticipates no rendering ever. | Accepted-by-silence, 2026-09-05 — user reviewed the full draft and raised no objection. |
| A8 | Does this feature change `GameLoop`, `GameSession`, `GameInput`, `Framebuffer` or `Sprite`? **No.** This is a new, separate, composable primitive a future `Game` calls from inside its own `update()` — none of the five existing shipped types' public surfaces are touched. | Accepted-by-silence, 2026-09-05 (C4) — user reviewed the full draft and raised no objection. |
| A9 | Any persisted-data/format-version concern (constitution §6)? **No.** Entities and collision results are transient per-tick values, never written to flash. | Accepted-by-silence, 2026-09-05 — user reviewed the full draft and raised no objection. |
| A10 | Threading/allocation/no-throw contracts? **Unchanged from every prior primitive:** single-threaded, zero dynamic allocation, no exception, no error code — same inherited contract as `Framebuffer`/`GameLoop`/`GameSession`. | Accepted-by-silence, 2026-09-05 — user reviewed the full draft and raised no objection. |

## 4. User Stories

### US-1 (Must): Detect whether two entities' bounds overlap

> As a future game-module author, I want a pure test of whether two axis-aligned position+bounds values overlap, so I can tell when two game objects (a bullet and an enemy, a ship and an obstacle) touch, without writing my own AABB math.

**Acceptance criteria:**

- [ ] AC-1.1: Given two entities whose bounds do not overlap at all, when the overlap test runs, then it returns false.
- [ ] AC-1.2: Given two entities whose bounds share at least a 1×1-pixel region, when the overlap test runs, then it returns true.
- [ ] AC-1.3: Given two entities whose edges are exactly adjacent (e.g. one's right edge equals the other's left edge, no interior pixel shared), when the overlap test runs, then it returns false — touching is not colliding (A4).
- [ ] AC-1.4: Given an entity with width ≤ 0 or height ≤ 0, when tested against any other entity (including another zero-size one), then the overlap test returns false — an entity with no area never overlaps anything (A5).
- [ ] AC-1.5: Given two entities with negative coordinates, or coordinates and sizes at the extremes of `int32_t` chosen so that a naive 32-bit sum would overflow, when the overlap test runs, then it returns the mathematically correct result with no integer overflow or undefined behavior (A6), verified under `-fsanitize=undefined`.
- [ ] AC-1.6: Given any two entities, when the overlap test runs, then neither entity's fields are modified — the test is a pure, read-only comparison.
- [ ] AC-1.7: Given the overlap test's source, when inspected, then it performs no dynamic allocation, includes no ESP-IDF header, and produces identical results on both `clang++` and `g++`.

### US-2 (Must): Dispatch a callback exactly when two entities collide

> As a future game-module author, I want a callback invoked automatically when two entities' bounds overlap, so I don't have to duplicate `if (overlaps(a, b))` boilerplate in every game that needs it.

**Acceptance criteria:**

- [ ] AC-2.1: Given two entities whose bounds overlap and a caller-supplied callable, when the dispatch mechanism runs, then the callable is invoked exactly once, receiving references to both entities.
- [ ] AC-2.2: Given two entities whose bounds do not overlap, when the dispatch mechanism runs, then the callable is never invoked.
- [ ] AC-2.3: Given the dispatch call's two entity arguments in a fixed order, when the callable is invoked, then it always receives them in that same order — never swapped.
- [ ] AC-2.4: Given the dispatch mechanism's source, when inspected, then it accepts any compile-time callable (lambda, free function or function object) via a template parameter — no `std::function`, no function-pointer indirection, no dynamic allocation, matching this engine's existing template-seam convention (A2).
- [ ] AC-2.5: Given the same two entities and the same callable, when the dispatch mechanism is run twice in a row with no change to either entity between calls, then the *detection* fires identically both times — the mechanism itself holds no state and is not affected by, and does not affect, anything but the two entities it's given (what the caller's own callback body does is entirely its own business).

### US-3 (Should): Check a fixed-size collection of entities pairwise in one call

> As a future game-module author with several on-screen entities, I want every overlapping pair in a fixed-size collection checked in one call, so I don't write my own O(n²) double loop for every game that needs more than two entities.

*Priority note: `Should`, not `Must` — the idea's own wording scopes the primitive to "two Entity instances"; US-1/US-2 alone deliver the core value and are fully provable on their own. This is the first thing cut if scope must shrink.*

**Acceptance criteria:**

- [ ] AC-3.1: Given a fixed-size, caller-owned collection of N entities (no dynamic allocation at any N) and a callable, when the sweep runs, then the callable fires exactly once for every unordered pair whose bounds overlap, and never for an entity against itself.
- [ ] AC-3.2: Given a collection of 0 or 1 entities, when the sweep runs, then the callable never fires and no error occurs.
- [ ] AC-3.3: Given a collection where every possible pair overlaps, when the sweep runs, then the callable fires exactly N×(N−1)/2 times — no more, no fewer.

## 5. Non-Functional Requirements

| # | Category | Requirement (measurable) | How it's verified |
|---|---|---|---|
| NFR-1 | Performance | The pairwise overlap test and a 32-entity sweep (US-3) both complete in a small fraction of the 16.6 ms 60 Hz tick budget on host, `-O2` — measured by a benchmark, not asserted (mirrors `bench_game_loop.cpp`/`bench_title_screen.cpp` precedent). | `/demo-day` (benchmark) |
| NFR-2 | Reliability / memory | Zero dynamic allocation anywhere in the delivered code and its tests: no `new`/`malloc`/`std::vector`/`std::function`. | `/peer-review` (grep; extends `tools/check_constraints.sh` with a collision-system file-set block, mirroring the game-loop/game-state precedent) |
| NFR-3 | Reliability / bounds | Full suite, including the extreme-value cases (AC-1.5), runs under `-fsanitize=address,undefined` with zero findings. | AC-1.5, AC-1.7 |
| NFR-4 | Portability / toolchain | Compiles clean under `clang++` and `g++`, `-std=c++17 -Wall -Wextra -Werror`; no resolution/tile-size literal outside the existing constants header; no ESP-IDF header anywhere. | `/peer-review` (grep + both compilers) |
| NFR-5 | Determinism | The overlap test and dispatch mechanism are pure functions of their entity inputs only — no wall-clock read, no RNG call, anywhere in the delivered files. | `/peer-review` (grep, extends the existing `CLOCK_RNG_PATTERN` lint block) |
| NFR-6 | **Library lens — public API surface** | Exactly one new POD type (four `int32_t` fields) plus the pairwise overlap test and callback-dispatch function (Musts), plus the optional sweep (Should) — no class hierarchy, no virtual dispatch, no accessor beyond the type's own public fields; no change to `GameLoop`, `GameSession`, `GameInput`, `Framebuffer` or `Sprite`'s existing public surface (A8). Any further public symbol is a review finding. | `/peer-review` |
| NFR-7 | **Library lens — contract clarity** | Doc comment states: the type's exact field set and units (screen-space `int32_t` pixels, may be negative or off-screen — A6); the touching-edge and zero/negative-size semantics (A4/A5); the overflow-safety guarantee at `int32_t` extremes (A6); the callback's fixed argument order and the mechanism's own statelessness (A2, AC-2.5); the inherited single-threaded/no-throw/no-alloc contract. One usage example. | `/peer-review` |
| NFR-8 | Observability / ops | N/A — pure in-memory geometry test, no runtime failure mode, no logging surface. | — |
| NFR-9 | Security & privacy | N/A — offline device, no personal data (constitution §2). | — |
| NFR-10 | Accessibility | N/A — no visual or UI surface; any rendering of a collision (a flash, a sound, an effect) is a future game's own concern, not built here. | — |
| NFR-11 | Integrations & dependencies | N/A — no external dependency; composes only with a future `Game`'s own `update()`, the same way `GameSession` does; touches no existing type's contract (A8). | — |
| NFR-12 | Verifiability | 100% host-CI-verifiable: every AC above is checkable with `clang++`/`make` unit tests; zero framebuffer dump, zero hardware dependency — the strongest verifiability posture of any feature to date, since this primitive has no rendering or I/O surface at all (A7). | `/demo-day`, `/peer-review` |

*Lens note: `library` active **scoped** (constitution §2); semver/packaging are no-ops. Public API surface and Contract clarity land as NFR-6/NFR-7.*

## 6. Out of Scope

- **Actual physics** — velocity, collision resolution/pushback, momentum, restitution. Explicitly excluded by the idea's own wording ("no physics").
- **Any specific game's collision *rules*** (e.g. "a bullet destroys an enemy," "a hazard costs a life"). This feature detects and reports overlap only; what a game does about it is entirely that game's own future logic, the same way `GameSession` never decides what ends a session.
- **Broad-phase spatial partitioning, quadtrees, spatial hashing, or any optimization beyond a naive pairwise sweep.** This is a 240×160 screen with a handful of on-screen objects, not a physics engine (US-3's own AC bound is N×(N−1)/2, not sub-quadratic).
- **Circular, polygon or pixel-perfect collision shapes.** AABB only, per the idea's own wording.
- **Anything on `Entity` beyond position and bounds** — no type-tag/ID, no velocity, no arbitrary user data, no `Sprite`/rendering reference (A1).
- **Any actual game logic, score, lives, or the roadmap games themselves** (Galactic Invasion, Steam Racer, Airship Battle, Mine Runner). This is purely the engine primitive they will each depend on.
- **Any change to `GameLoop`, `GameSession`, `GameInput`, `Framebuffer` or `Sprite`'s existing public contract** (A8).
- **Rendering or otherwise visualizing a collision** (a flash, a hit effect, a sound). No display driver is touched here.
- **Multi-threaded or concurrent collision checks.** Single-threaded, same as every existing primitive.
- **Continuous collision detection** (tunneling prevention for fast-moving objects crossing a gap between ticks). This is discrete, per-tick overlap only — a future concern if a roadmap game's bullets prove too fast for it, not built pre-emptively here.

## 7. Clarifications

| # | Date | Question | Resolution |
|---|---|---|---|
| C1 | 2026-09-05 | Does this feature need to handle more than a single pair of entities? | **Musts cover exactly one pair (US-1/US-2).** A fixed-size N-entity sweep is a Should (US-3), matching the idea's own "between two Entity instances" wording (A3, accepted-by-silence — the user reviewed the full draft and raised no objection). |
| C2 | 2026-09-05 | What happens with a zero-width/zero-height or negative-size entity? | **Never overlaps anything** — mirrors `Framebuffer::fillRect`'s existing non-positive-size no-op precedent, rather than inventing a new edge-case convention (A5). |
| C3 | 2026-09-05 | What happens with negative coordinates or values at the `int32_t` extremes? | **Negative coordinates are valid** (an entity may legitimately sit off-screen); **the test must never overflow or invoke UB at any input**, mirroring `Framebuffer`'s own wider-type-before-narrowing precedent (A6). |
| C4 | 2026-09-05 | Does this feature change any existing shipped type's public contract? | **No.** `GameLoop`, `GameSession`, `GameInput`, `Framebuffer`, `Sprite` are all untouched; this is a new, separate, composable primitive (A8). |
| C5 | 2026-09-05 | Half-size version, if the story must shrink? | US-1 alone (the overlap test, with no callback) still delivers real value — a game can write its own `if (overlaps(a, b))`. US-2 (callback dispatch) is the convenience layer built next. US-3 (the N-entity sweep) is cut first, per its own priority note — it's a Should precisely because it's the first thing a shrinking scope loses. |

## 8. Design Review

- **N/A, PO-recorded.** This feature delivers a pure, headless geometry/math primitive — no color, layout, rendering call, or interaction surface is introduced anywhere in it; it has no display-driver touch point and produces no pixel this feature is responsible for. Same posture as `game-state-management`'s Design Review (also N/A) — if a future game visualizes a collision (a flash, a hit effect), that story gets its own design pass, not retrofitted here.

---

## ✅ SPEC GATE

*All boxes checked → `/sprint-plan` may start. Any box open → back to `/story-time` or `/look-and-feel`.*

- [x] Problem, goal and success signal are concrete (no buzzwords, no "everyone")
- [x] Every story has testable Given/When/Then acceptance criteria
- [x] Stories are prioritized (MoSCoW) and at least one is a Must
- [x] Non-functional requirements are stated and measurable (or marked N/A with reason)
- [x] Clarify pass done: functional scope (A1–A3, C1), data (A5/A6, C2/C3), roles/permissions (N/A, no UI — §2), error/edge cases (AC-1.3/1.4/1.5, AC-3.2), NFRs (NFR-1…12), integrations (NFR-11, A8, C4), UX flows (N/A, no UI — §8), out-of-scope (§6) — all swept
- [x] Open questions are resolved or explicitly accepted as risk — **0 open.** 3 load-bearing judgment calls (A1 Entity shape, A2 callback mechanism, A4 touching-edge semantics) were **explicitly confirmed by the user on 2026-09-05**, each stated alternative offered and declined; the remaining 7 (A3, A5–A10) are **accepted-by-silence** — the user reviewed the full draft and raised no objection.
- [x] Out-of-scope section is filled (something was consciously cut)
- [x] Constitution (`.spark/constitution.md`) respected — directly closes the §2 "not yet implemented" gap; no conflict found
- [x] Design review done for UI-facing features (or marked N/A with reason) — N/A, PO-recorded (§8): pure geometry primitive, no visual/UI surface
- [x] Line budget respected: Ist 195 / Soll ~250 (excluding HTML comments)
- [x] Status set to `approved` by the user — 2026-09-05
