# Spec: galactic-invasion

| | |
|---|---|
| **Phase** | Specify |
| **Owner** | Product Owner (`/story-time`), Designer (`/look-and-feel`) |
| **Status** | `approved` |
| **Date** | 2026-09-07 |
| **Ticket** | `none` |

**Handoff**
- **Status:** `approved` — user approved 2026-09-07. `/look-and-feel` found no Blockers; AC-11.2 was sharpened (visibly-different rendered width, not just different words) and AC-10.8 was added (player-sprite flicker during invulnerability, no new art, user's explicit choice). **0 open questions remain.**
- **Summary:** SteamCore has eight shipped engine-infrastructure features and zero playable games. Deliver the first — a Galaga/Space-Invaders homage: a horizontally-moving player ship with 3 lives and respawn, a fixed 3×6 enemy grid that side-steps and descends, single-active-projectile firing, collision via the shipped `collision-system`, a distinct win screen on clearing the formation and a distinct loss screen on lives-exhausted, all rendered as hand-authored pixel-art sprites via the shipped-but-never-yet-exercised `Sprite`/`Framebuffer::blit` — composed entirely from what already shipped, wired directly behind the existing title screen with no game-selection menu.
- **Open:** `0 open`. A1–A6 resolved by the user (§3/§7); A12–A15 are new PO-judgment resolutions the bigger scope required, each grounded in an exact existing precedent (never a guess) — see §7 C13–C16.
- **Binding ruling:** §4 User Stories for the current stories; §7 Clarifications for what changed since the last round and why.
- **On conflict:** the numbered body below wins for everything except `Status`; log the mismatch as a finding at the next `/peer-review` and proceed — don't stop on it.

## 1. Problem & Goal

- **Problem:** Nobody can actually play anything on this console today. Pressing START at the title screen leads nowhere — `drawTitleScreen` exists only inside throwaway on-device harnesses, never a real released game loop (`title_screen.h`). All eight shipped features are engine infrastructure; constitution Principle 4 ("arcade immediacy... restart always one button away") describes an experience the codebase has never actually delivered end-to-end. `collision-system` (v0.5.0) has zero consumers, and `Sprite`/`Framebuffer::blit` (shipped inside `framebuffer-viewer`/`rendering-core`) have only ever been exercised by their own synthetic unit tests (`sprite_test.cpp`) — no game has ever composed real sprite art.
- **Goal:** The smallest real, playable arcade shooter that still delivers the user's requested depth: a Galaga/Space-Invaders homage a person can pick up, understand in seconds, lose lives to, win or lose a round of, and restart with one button — built entirely by composing already-shipped primitives (`GameLoop`, `GameSession`, `collision.h`, `Framebuffer`/`drawText`/`Sprite`/`blit`, `drawTitleScreen`), with no new engine-level primitive and no change to any of their shipped public contracts.
- **Success signal:** A host test drives a fixed input sequence through a full round (movement, firing, an enemy kill, a life lost to enemy contact followed by respawn and a verified invulnerability window, a formation-clear producing the win screen in one run and a lives-exhausted loss in another, a restart) and asserts the exact expected framebuffer/score/lives/`GameState` sequence at each tick, byte-identical on replay. Deferred/hardware signal: a human plays a full round on the real device via the framebuffer dump, loses a life and sees it respawn, reaches either end screen, and restarts with a single button press.
- **Why now:** This is the first of README's four planned games, the first proof that the eight prior "library" features actually compose into something a player experiences, and the first feature to put `Sprite`/`Framebuffer::blit` to real use rather than synthetic test coverage — mirroring `analog-joystick-input`'s own precedent as "first real consumer" of a shipped-but-unexercised primitive.

## 2. Target Users

- **The console player (primary — the first feature where this role is fully, directly testable):** picks up the device, presses START, plays a round with real stakes (3 lives, a win or lose outcome), dies or wins, presses START again. Every prior feature deferred this persona; this one serves it end-to-end.
- **Future game-module author (secondary):** Steam Racer / Airship Battle / Mine Runner get their first concrete precedent for how a real `Game` composes `GameSession` + collision + rendering + hand-authored sprite art, beyond the engine's own doc-comment examples. (`assets/sprites/steam_racer.png` already exists as that game's own future concept reference, same role `galactic_invation.png` plays here — see A14.)
- *Not a user of this feature:* a player choosing among multiple games — no second game exists (user's own explicit instruction), matching `start-screen`'s own A1 precedent exactly.

## 3. Assumptions & Open Questions

| # | Assumption / Question | Resolution |
|---|---|---|
| A1 | **Lives / game-over shape.** | **Resolved by the user — larger-scope option chosen.** 3 lives with respawn, not single-hit. Game over means "lives exhausted," not "first hit." Respawn location, invulnerability window and the last-life boundary are specified in US-10 and A12. |
| A2 | **Enemy return fire.** | **Resolved by the user — PO's recommended default confirmed, no change in shape.** Contact/threshold-only is the Must MVP (US-5/US-10); enemy return fire stays a Should (US-8) — first cut if scope must shrink. |
| A3 | **Win / clear-formation behavior.** | **Resolved by the user — larger-scope option chosen.** A dedicated win screen, not a reused loss `GAME_OVER` screen. `GameSession` (`game_state.h`) is **not modified**: it still only offers `READY`/`PLAYING`/`GAME_OVER` and accepts only `sessionEnded` from its caller — no `WIN` value is added to that shipped enum. This game reaches `GAME_OVER` via `sessionEnded=true` for *both* outcomes and separately tracks, as its own private state, which outcome occurred; §4's US-11 states the required **observable** behavior (the win and loss screens must be distinguishable) without dictating how that internal tracking is built — that mechanism is a `/sprint-plan` decision. |
| A4 | **Score-per-kill value and format.** | **Resolved by the user — PO's recommended default confirmed.** +10 per kill, `SCORE: nnnn` top-left, shipped font characters only. |
| A5 | **Rendering: rectangles vs. sprite art.** | **Resolved by the user — larger-scope option chosen.** Pixel-art sprites via the shipped `Sprite`/`Framebuffer::blit`, not `fillRect`. This is the first feature ever to compose `Sprite` for real gameplay rendering — until now it had only `sprite_test.cpp`'s synthetic patterns. See A14 for how the art itself gets authored. |
| A6 | **Formation grid size**, contingent on A5. | **Resolved by the user — the smaller option, explicitly chosen because sprites may be larger than the rect-based default assumed.** 3 rows × 6 columns (18 enemies), down from the rect-default's 4×8 (32) — deliberately leaving screen room for sprite silhouettes whose exact pixel size is not yet fixed. Exact per-sprite dimensions and pitch are a `/look-and-feel`/`/sprint-plan` decision, not fixed here (no UI-layout literal belongs in a spec). |
| A7 | Does a formation reaching the player's row without direct contact also end the round? | **Resolved — yes, PO judgment call, genre convention** (unchanged by A1). The formation's leading edge crossing a fixed y-coordinate near the player's row ends the round outright — see A13 for how this interacts with the new lives system. |
| A8 | Does this feature build the self-registration/game-registry mechanism? | **Resolved — no** (unchanged), matching `start-screen`'s own A1 precedent exactly. |
| A9 | What state does a `START` restart (`GAME_OVER`→`PLAYING`) reset? | **Resolved — everything**, now including the lives system and win/loss additions: a fresh formation, score reset to 0, lives reset to the fixed starting count of 3 (A1), the player back at its starting position with no invulnerability active, this game's internal win/loss outcome flag cleared, and no projectiles in flight. |
| A10 | Screen-edge behavior for the player ship and the formation. | **Resolved** (unchanged). |
| A11 | Determinism for any pseudo-randomness this game introduces. | **Resolved, constitution-bound** (unchanged) — now also covering respawn/invulnerability timing and enemy-projectile spawn choice (US-10/US-8), all tick-counted, never wall-clock. |
| A12 | *(New, required by A1's larger scope.)* Exact respawn location and invulnerability duration/scope? | **Resolved (PO judgment), explicitly decided per the caller's instruction not to leave this implicit.** Respawn is at the ship's fixed starting position (the same position A9 resets to). The window is a fixed **120 ticks (2 seconds at the engine's fixed 60 Hz step, constitution §3)** — long enough to reposition away from whatever caused the hit, short enough not to trivialize a life; industry-standard practice, sized here to also solve the caller-named edge case of an enemy or projectile still overlapping the respawn point at the instant of respawn. See US-10 AC-10.3/AC-10.4. |
| A13 | *(New, required by A1's larger scope.)* Does the formation-threshold failsafe (A7) consume a life, or end the round outright regardless of lives remaining? | **Resolved (PO judgment).** Outright, regardless of lives remaining. A7's entire purpose is closing the "dodge forever" exploit; making it merely cost a life would let a skilled dodger survive several crossings on spare lives, reopening exactly the exploit A7 exists to close. See AC-5.2, AC-10.6. |
| A14 | *(New, required by A5's larger scope.)* How does sprite pixel data actually get authored — a PNG-import pipeline, a conversion tool, or hand-authored code? Is the existing `assets/sprites/galactic_invation.png` usable as source data? | **Resolved (PO judgment), grounded in an exact existing precedent — not invented.** `font.cpp`'s `GlyphArt`/`glyphPixel`/`buildAtlas` already establishes this codebase's one convention for authoring pixel art: rows of a picture written as string literals, converted to `Color` values by a `constexpr` function, validated at compile time. Sprite art for this feature follows that identical convention — hand-authored `Color`-array literals in this game's own source, no PNG decoding, no new tool, no runtime or build-time asset pipeline (matches `start-screen` A2's "no PNG/TTF import pipeline exists" finding exactly, and constitution §6's "large pixels, simple sprites" non-negotiable). `assets/sprites/galactic_invation.png` (untracked, present in the working tree) is a **concept-art reference sheet only** — it depicts far more than this feature builds (three enemy types, a boss, power-ups, multi-frame animation cycles, particle/explosion effects, a heart-icon lives HUD) and is not imported or pixel-sampled; it informs `/look-and-feel`'s silhouette/shape guidance for the one player-ship sprite, one enemy sprite and one projectile sprite this feature actually ships (§6 Out of Scope enumerates what's deliberately not taken from it). |
| A15 | *(New, required by A1's larger scope.)* How is the lives count displayed? | **Resolved (PO judgment).** Text, `LIVES: n`, via the existing `drawText`/font path — mirroring the score display's own precedent (A4) exactly, at a fixed position (top-right) that overlaps neither the score (top-left) nor the player's movement lane. Not the concept sheet's heart-icon sprites (A14) — no new icon asset for this feature. |

## 4. User Stories

### US-1 (Must): Player ship moves horizontally

> As the player, I want to move my ship left and right along the bottom of the screen, so I can dodge enemy fire and line up my shots.

**Acceptance criteria:**

- [ ] AC-1.1: Given `PLAYING` and `input.left`/`input.right` held, when a tick's `update` runs, then the ship's on-screen x-position moves by a fixed per-tick amount in that direction.
- [ ] AC-1.2: Given the ship at either horizontal screen edge, when movement toward that edge continues to be held, then the ship's rect stays fully on-screen and never moves partially or fully off it (A10).
- [ ] AC-1.3: Given `input.up`/`input.down` held, when a tick runs, then the ship's y-position never changes — vertical movement does not exist for the player.

### US-2 (Must): Player fires one projectile at a time

> As the player, I want to fire a shot upward, so I can destroy enemies — but never with continuous full-auto fire.

**Acceptance criteria:**

- [ ] AC-2.1: Given `PLAYING`, no player projectile currently exists, and `input.fire` is true, when the tick runs, then exactly one new player projectile spawns above the ship, traveling upward at a fixed per-tick speed.
- [ ] AC-2.2: Given a player projectile already exists (in flight, not yet destroyed or off-screen), when `input.fire` is true on any subsequent tick, then no second projectile spawns — at most one player projectile exists at any time.
- [ ] AC-2.3: Given a player projectile reaches the top edge of the screen without hitting anything, when that tick runs, then it is removed, freeing the player to fire again.

### US-3 (Must): Enemy formation moves as a fixed grid

> As the player, I want the enemy formation to move side-to-side and step downward over time, so the threat visibly escalates the longer a round runs.

**Acceptance criteria:**

- [ ] AC-3.1: Given `PLAYING` begins, when the formation first renders, then it is a fixed A6-sized grid (3×6, 18 enemies) of enemies at their starting positions — no enemy spawns or despawns outside of being destroyed (US-4) or ending the round.
- [ ] AC-3.2: Given the formation is mid-round, when a fixed number of ticks elapse, then every surviving enemy's x-position shifts by the same fixed amount in the formation's current horizontal direction — movement is a tick count, never a wall-clock interval (A11).
- [ ] AC-3.3: Given any enemy in the formation reaches a horizontal screen edge, when the next movement step occurs, then the entire formation reverses direction and every surviving enemy's y-position steps down by a fixed amount once (A10) — it never exits the screen horizontally.
- [ ] AC-3.4: Given the same fixed input sequence run twice from a freshly constructed round, when the resulting framebuffer/formation-position sequence is compared, then it is byte-identical (constitution §4 determinism).

### US-4 (Must): Player projectile destroys an enemy on collision

> As the player, I want a shot that hits an enemy to destroy it and add to my score, so my shooting has visible consequences.

**Acceptance criteria:**

- [ ] AC-4.1: Given a player projectile and a surviving enemy whose bounds overlap, when the tick's collision check runs (via the shipped `collision.h` `checkCollision`/`sweepCollisions` — no bespoke overlap math written for this game), then that enemy is removed from the formation and the projectile is also removed.
- [ ] AC-4.2: Given an enemy is destroyed, when the same tick finishes, then the displayed score (US-7) increases by the fixed per-kill value (A4).
- [ ] AC-4.3: Given every enemy in the formation has been destroyed, when the next tick runs, then the round ends via the win path (US-11) — `sessionEnded=true` drives `GameSession` to `GAME_OVER` exactly as any other ending, but this game's own internally-tracked outcome flag records a win, not a loss (AC-11.1) — no enemy, no projectile, and no undefined formation state is left rendered.

### US-5 (Must): The player is defeated on enemy contact or formation threshold

> As the player, I want contact with an enemy, or the formation reaching me, to have real stakes, so there's genuine tension to dodging and shooting.

**Acceptance criteria:**

- [ ] AC-5.1: Given the player is not currently invulnerable (AC-10.3) and the player ship's bounds overlap a surviving enemy's bounds, when the tick's collision check runs, then the contact is resolved as a hit: if lives remain after it, US-10's life-loss-and-respawn (AC-10.2) applies and no session-ending transition happens this tick; if this hit exhausts the last life, `sessionEnded=true` is passed to `GameSession::advance` within that same tick — the transition to `GAME_OVER` takes effect on the tick the contact happens, not one tick later (mirrors `game_state.h`'s own "call `advance` after your own simulation" contract note).
- [ ] AC-5.2: Given the formation's leading (lowest) row's y-position crosses a fixed threshold near the player's row without any entity directly overlapping the player, when that tick runs, then the round also ends via `sessionEnded=true` (A7), **regardless of how many lives remain** (A13) — dodging through gaps forever is not a viable strategy, and neither is surviving the threshold on a spare life.
- [ ] AC-5.3: Given `PLAYING` transitions to `GAME_OVER` by the player's last life being lost (AC-5.1) or by the threshold (AC-5.2), when the next tick renders, then the text "GAME OVER" and the final score are drawn via the shipped `drawText`, and no further ship/enemy/projectile movement or spawning occurs — this is the **loss** screen; AC-11.2 states how it stays visually distinct from the win screen.

### US-6 (Must): Title screen leads directly into this one game

> As the player, I want pressing START at the title screen to begin a round immediately, and pressing START again after GAME_OVER to begin a fresh one, so play starts and restarts with exactly one button — no menu, no selection.

**Acceptance criteria:**

- [ ] AC-6.1: Given `GameState::READY`, when rendered, then the existing `drawTitleScreen` is composed unmodified — no change to `title_screen.h`/`.cpp`'s public contract.
- [ ] AC-6.2: Given `READY` and a rising edge of `input.start`, when the next tick runs, then the round begins in `PLAYING` with a freshly-initialized formation, player position, lives count and score of 0 (A6/A9) — no game-selection screen is ever shown.
- [ ] AC-6.3: Given `GAME_OVER` (win or loss) and a rising edge of `input.start`, when the next tick runs, then a completely fresh round begins directly in `PLAYING` (A9) — no intermediate screen, matching `GameSession`'s own documented one-press restart contract, identically regardless of which outcome ended the previous round (AC-11.4).

### US-7 (Must): Running score is visible throughout play

> As the player, I want to see my score while I'm playing, so I know how I'm doing without it being a mystery (constitution Principle 4: "visible score").

**Acceptance criteria:**

- [ ] AC-7.1: Given `PLAYING`, when any tick renders, then the current score is drawn via `drawText` at a fixed, documented on-screen position (top-left, A4), using only characters in the shipped 43-character font set.
- [ ] AC-7.2: Given the score display's documented bounding rectangle and the player ship's/any enemy's/the lives display's (US-10) on-screen positions, when a host test checks them, then none of these ever occupy the same screen region as another.

### US-8 (Should): Enemies fire back

> As the player, I want enemies to occasionally shoot downward at me, so the formation is a real threat while it's still far away, not just a countdown timer.

*Priority note: `Should`, not `Must` — US-1 through US-7 plus US-10/US-11/US-12 fully deliver a playable, losable, winnable, restartable round without this. First cut if scope must shrink (A2).*

**Acceptance criteria:**

- [ ] AC-8.1: Given `PLAYING` and a fixed tick interval elapses, when the interval fires, then at most a fixed small number of enemy projectiles exist in flight at once, each spawned from a surviving enemy chosen by an explicit constant-seeded deterministic generator (A11) — never `rand()`/wall-clock-based.
- [ ] AC-8.2: Given the player is not currently invulnerable (AC-10.3) and an enemy projectile's bounds overlap the player ship's bounds, when the tick's collision check runs, then it is resolved exactly as AC-5.1 describes for direct contact (life lost and respawn, or game-over if it was the last life).
- [ ] AC-8.3: Given an enemy projectile reaches the bottom edge of the screen without hitting the player, when that tick runs, then it is removed.

### US-9 (Should): Formation speeds up as enemies are destroyed

> As the player, I want the remaining enemies to move faster as I clear the formation, so the round has a rising difficulty curve like the genre's own convention.

*Priority note: `Should`, not `Must` — a constant-speed formation (US-3 alone) is still a complete, playable round. Cut second, after US-8, if scope must shrink.*

**Acceptance criteria:**

- [ ] AC-9.1: Given the count of surviving enemies decreases, when the formation's next movement step is due, then the tick interval between steps (AC-3.2) shortens as a fixed, deterministic function of that count — never a wall-clock read (A11).
- [ ] AC-9.2: Given the same fixed input/kill sequence run twice, when the resulting step-timing sequence is compared, then it is byte-identical (constitution §4 determinism).

### US-10 (Must): The player has 3 lives, respawns, and is briefly invulnerable afterward

> As the player, I want to survive a mistake instead of ending the round on my first hit, so a single lapse in dodging isn't the whole game — but I also want the round to genuinely end once I truly run out of chances.

**Acceptance criteria:**

- [ ] AC-10.1: Given `PLAYING` begins (a fresh round or a restart, A9), then the player starts with exactly 3 lives, displayed via `drawText` as `LIVES: n` at a fixed position (top-right, A15) that overlaps neither the score display nor the player's movement lane (mirrors AC-7.2).
- [ ] AC-10.2: Given the player ship is hit by direct enemy contact (AC-5.1) or an enemy projectile (AC-8.2) while more than one life remains, when that tick's collision resolves, then: one life is subtracted and the display updates that same tick; any in-flight player projectile is removed; the ship reappears within that same tick at its fixed starting position (A12); the round remains in `PLAYING` — no `sessionEnded` transition occurs.
- [ ] AC-10.3: Given the player has just respawned (AC-10.2), then it is invulnerable for a fixed window of 120 ticks (A12) — during this window, an overlapping enemy or enemy projectile is not treated as contact for AC-5.1/AC-8.2: no life is lost, no further respawn is triggered, and the overlapping enemy/projectile is itself unaffected (invulnerability protects the player only; it destroys nothing).
- [ ] AC-10.4: Given the invulnerability window elapses (120 ticks since respawn), when the next tick runs, then the player is vulnerable again exactly as before any life was lost — contact on the very next tick is treated as ordinary contact.
- [ ] AC-10.5: Given the hit that would subtract a life leaves the player at 0 lives, when that same tick resolves, then no respawn occurs and `sessionEnded=true` is passed to `GameSession::advance` instead (AC-5.1's last-life branch) — `GAME_OVER` triggers on the life-exhausting hit itself, not one tick later.
- [ ] AC-10.6: Given the formation-threshold failsafe fires (AC-5.2), then it ends the round regardless of lives remaining and regardless of invulnerability (A13) — this story's life system does not soften that failsafe.
- [ ] AC-10.7: Given the same fixed input/hit sequence run twice, when the resulting lives-count, respawn-position and invulnerability-window sequence is compared, then it is byte-identical (constitution §4 determinism).
- [ ] AC-10.8: Given the player is invulnerable (AC-10.3), when each tick renders, then the player ship's sprite flickers — visible on some ticks and skipped (not `blit`'d) on others, in a fixed, deterministic tick pattern — so the grace window is visibly distinguishable from ordinary play rather than silently indistinguishable from a collision that should have happened but didn't (`/look-and-feel` finding 3). No new sprite art or per-frame animation: the single existing player sprite is simply omitted on alternating ticks. The formation/projectiles/HUD render normally throughout; only the player sprite's visibility toggles.

### US-11 (Must): A distinct win screen appears when the formation is cleared

> As the player, I want clearing every enemy to feel like winning, not just another way to see "GAME OVER", so destroying the whole formation is its own satisfying goal.

**Acceptance criteria:**

- [ ] AC-11.1: Given every enemy in the formation is destroyed (AC-4.3), when the next tick runs, then `sessionEnded=true` is passed to `GameSession::advance` and `GameSession::state()` becomes `GAME_OVER` — the same terminal state a loss reaches, since the shipped `GameSession` (`game_state.h`) has no separate `WIN` value and accepts no signal beyond `sessionEnded`. This game records which outcome occurred as its own private state, not as anything added to `GameSession`.
- [ ] AC-11.2: Given `GameState::GAME_OVER` is reached by formation-clear (AC-11.1) versus by lives-exhausted (AC-10.5) or threshold (AC-5.2/AC-10.6), when the next tick renders, then the two screens are visibly and unambiguously different — never the same screen redrawn for both outcomes. Text-only differentiation is sufficient (`/look-and-feel`), but the victory string's rendered width (`(chars) × kGlyphAdvance`) must differ from `"GAME OVER"`'s 72px by a visually obvious margin (e.g. a shorter `"YOU WIN"` at 56px) — the two screens must differ in overall silhouette/position at a glance, not only in words a player must stop and read. Verified by a host test asserting the two rendered framebuffers differ AND their text bounding-box widths differ, mirroring `kTitleWordmarkBounds`'s own precedent.
- [ ] AC-11.3: Given the win screen is displayed, then it also shows the final score (mirrors AC-5.3), using the same `drawText`/font contract.
- [ ] AC-11.4: Given `GAME_OVER` by either outcome, when a rising edge of `input.start` occurs, then AC-6.3's restart behavior applies identically — win vs. loss makes no difference to the restart contract.

### US-12 (Must): Player, enemies and projectiles render as pixel-art sprites

> As the player, I want the ship, enemies and shots to look like a real arcade game, not placeholder rectangles, so it actually feels like something worth playing.

**Acceptance criteria:**

- [ ] AC-12.1: Given the player ship, an enemy, or a projectile renders on any tick, then it is drawn via `Framebuffer::blit` with a `Sprite` — no `fillRect` call is used for any of these three entity types.
- [ ] AC-12.2: Given the shipped `blit` contract (default transparent colour `BLACK`), when a sprite's `BLACK` pixels render, then they leave the existing background undisturbed, exactly as `blit`'s own shipped behavior already guarantees — inherited, not reimplemented.
- [ ] AC-12.3: Given the ship, enemy and projectile sprites' backing `Color` arrays, when their source is read, then each is `static`/`constexpr` compile-time data defined in this game's own source (mirroring `font.cpp`'s `GlyphArt` convention, A14) — no file I/O, no PNG decoding, and no build- or run-time dependency on `assets/sprites/*.png`. Verified by `/peer-review` (source reading), not a host test.
- [ ] AC-12.4: Given the three sprites rendered at their actual on-screen size, when a `/look-and-feel` reviewer compares them, then each is distinguishable from the others by silhouette alone, not merely by which of the 4 palette colours it uses (constitution §6's "clear silhouettes" non-negotiable) — a design-review item, not a host-test assertion.

## 5. Non-Functional Requirements

| # | Category | Requirement (measurable) | How it's verified |
|---|---|---|---|
| NFR-1 | Performance | A full tick (player + formation movement, up to ~25-entity collision sweep via `sweepCollisions` — 18-enemy formation plus player, projectiles and the respawn/invulnerability bookkeeping, A6/A12 — rendering ship/formation/projectiles/score/lives) completes in a small fraction of the 16.6ms 60Hz budget on host, `-O2`, measured by a benchmark (mirrors `bench_collision.cpp`/`bench_game_loop.cpp` precedent). | `/demo-day` (benchmark) |
| NFR-2 | Reliability / memory | Zero dynamic allocation: the enemy grid, both projectile pools (player + enemy, if US-8 ships), the lives counter, the invulnerability tick-counter, the win/loss outcome flag, and all other game state are fixed-size arrays/scalar fields sized by compile-time constants (A6); no `new`/`malloc`/`std::vector`/`std::function` anywhere in the delivered code or its tests. | `/peer-review` (grep, extends `tools/check_constraints.sh`) |
| NFR-3 | Determinism | Same input sequence and starting state → byte-identical framebuffer/score/lives/`GameState` sequence on both compilers; no wall-clock read, no unseeded RNG anywhere in the delivered files (A11). | AC-3.4, AC-9.2, AC-10.7 + `/peer-review` |
| NFR-4 | Portability / toolchain | Compiles clean under `clang++` and `g++`, `-std=c++17 -Wall -Wextra -Werror`; no resolution/tile-size/glyph-metric literal outside the existing constants headers; no ESP-IDF header in delivered logic code. | `/peer-review` (grep + both compilers) |
| NFR-5 | **Library lens — public API surface** | Exactly one new `Game`-conforming type is added as this feature's public surface (satisfying `update(const GameInput&)`/`render(Framebuffer&)`); its internal formation/projectile/score/lives/win-loss state stays private. No change to `GameLoop<Game>`, `GameSession`, `GameInput`, `Framebuffer`, `Sprite`, or `collision.h`'s existing public contract — all five are composed exactly as already shipped. In particular, **`GameState` gains no new enumerator**: the win/loss distinction (US-11) is this game's own private state, never a change to the shipped three-value enum (A3). | `/peer-review` |
| NFR-6 | **Library lens — contract clarity** | The new type's doc comment states: which `GameState` transitions it drives and what sets `sessionEnded=true` and when, for every ending (US-5/US-8/US-11); the fixed grid dimensions and movement rule (A6, US-3); the single-active-projectile firing rule (US-2); the lives/respawn/invulnerability rule and its exact tick duration (US-10, A12); how the win vs. loss outcome is tracked internally despite both routing through `GAME_OVER` (US-11); the inherited single-threaded/no-throw/no-dynamic-allocation contract. One usage example, matching every other steamcore type's doc-comment convention. | `/peer-review` |
| NFR-7 | Accessibility | The score, lives, "GAME OVER" and win-screen text render in `BRIGHT_ORANGE` on `BLACK` — the one palette colour already confirmed (`start-screen` NFR-7) to clear the WCAG 4.5:1 normal-text-contrast floor. Sprite silhouette distinctness among player/enemy/projectile (A5/A14), given only 4 palette colours exist, is a `/look-and-feel` design-review item (AC-12.4), not enumerated further here. | AC-7.1, AC-10.1, AC-5.3, AC-11.2/3 + `/look-and-feel` |
| NFR-8 | Security & privacy | N/A — offline device, no personal data; score/lives exist only in RAM for the current round, never persisted (constitution §2). | — |
| NFR-9 | Observability / ops | N/A — pure in-memory game logic, no runtime failure mode, no logging surface. | — |
| NFR-10 | Integrations & dependencies | N/A beyond what's already stated — composes only already-shipped `GameLoop`, `GameSession`, `collision.h`, `Framebuffer`/`drawText`/`Sprite`/`blit`, `drawTitleScreen`; no new engine-level primitive, no PNG-import dependency (A14). | — |

*Lens note: `library` active **scoped** (constitution §2); semver/packaging are no-ops. Public API surface and contract clarity land as NFR-5/NFR-6.*

## 6. Out of Scope

- **Galaga-style diving/breakout enemies leaving formation, multiple enemy types (scout/fighter/bomber), and a boss.** All shown in `assets/sprites/galactic_invation.png`'s concept sheet (A14) but explicitly excluded — one fixed-grid formation of one enemy type, matching the user's own idea text.
- **Multi-game selection menu / self-registration into a central registry** (constitution §3 "Games"). Deferred until a second game exists (A8), matching `start-screen`'s own A1 precedent exactly — this game is wired directly.
- **A PNG-import pipeline, an asset-conversion tool, or importing `assets/sprites/*.png` at build or run time.** Sprite art is hand-authored `Color`-array literals in code, mirroring `font.cpp`'s convention exactly (A14); the concept sheet informs shape/silhouette design only. A future feature may justify a PNG-to-`Color`-array conversion tool (mirrors `tools/generate_font_anchors.py`'s own precedent), but it isn't built now.
- **Multiple animation frames per entity, power-ups, and explosion/particle effects** — all depicted in the concept sheet but not built. v1 ships exactly one static sprite each for the player ship, the one enemy type, and the one projectile type; no per-frame animation state, no pickups.
- **More than 3 lives, extra-life pickups, or a heart-icon lives HUD.** Lives are fixed at 3 (A1) and shown as text (A15); no power-up system exists or is built here.
- **Infinite wave-looping / multi-board escalating progression after a win.** Clearing the formation shows the win screen (US-11) and the round ends there; restarting begins a fresh single-wave round exactly like a loss does (AC-11.4). Looping to a new, harder wave is explicit future scope.
- **Score/highscore persistence to flash.** Score and lives live only in RAM for the current round and reset on restart; README's own Highscore-System is a distinct, unbuilt future feature (constitution §2 `has-database`).
- **Sound/audio effects.** No audio-synthesis engine exists anywhere in this codebase yet.
- **Pause, settings, or a difficulty-selection screen.** `GameInput.select` stays unwired, same posture `start-screen` already took.
- **Any change to `GameLoop`, `GameSession` (including `GameState`), `GameInput`, `Framebuffer`, `Sprite`, or `collision.h`'s existing public contract.** This game composes all five exactly as already shipped (NFR-5) — the win/loss distinction is solved without touching `GameState` (A3).
- **Networked or online play.** Project-wide out of scope (constitution Principle 5).
- **Analog/fine-grained player movement.** `GameInput`'s discrete `left`/`right` levels are sufficient (US-1); no new input concept is needed.

## 7. Clarifications

| # | Date | Question | Resolution |
|---|---|---|---|
| C1 | 2026-09-07 | Does clearing the formation need a distinct "win" state? | Superseded by C9 below — the user chose the larger-scope option: yes, a dedicated win screen. |
| C2 | 2026-09-07 | Can a player dodge indefinitely by staying between formation columns as it descends past? | No — a fixed y-threshold near the player's row ends the round even without direct contact (A7); resolved as a PO judgment call. |
| C3 | 2026-09-07 | Does this feature need the constitution's game self-registration/registry mechanism? | No — deferred until a second game exists (A8), identical reasoning to `start-screen`'s own A1. |
| C4 | 2026-09-07 | Is a visible score actually required, or is it a nice-to-have? | **Required** — constitution Principle 4 names "visible score" directly; confirmed by the user (A4). |
| C5 | 2026-09-07 | What does a `START` restart reset? | Everything, now including lives and the win/loss outcome flag (A9). |
| C6 | 2026-09-07 | Half-size version, if the story must shrink? | US-1/US-2/US-3/US-4/US-5/US-6/US-7/US-10/US-11/US-12 together are the smallest complete, losable, winnable, restartable, visibly-scored round now that the user chose the larger scope on A1/A3/A5. US-9 (speed-up) is cut first, then US-8 (enemy return fire). |
| C7 | 2026-09-07 | User's answer to A1: single-hit or multiple lives? | **Multiple lives (3) with respawn** — the larger-scope option. Folded into US-10, A1, A12. |
| C8 | 2026-09-07 | User's answer to A2: enemy return fire Must or Should? | **Should, as PO recommended** — no change in shape from the draft. |
| C9 | 2026-09-07 | User's answer to A3: reused `GAME_OVER` screen or a dedicated win screen? | **Dedicated win screen** — the larger-scope option. `GameSession`/`GameState` stay unmodified (A3); the win/loss distinction is this game's own private state, and US-11 states the required observable behavior without dictating the mechanism. |
| C10 | 2026-09-07 | User's answer to A4: score value and format? | **+10/kill, `SCORE: nnnn` top-left** — as PO recommended, confirmed. |
| C11 | 2026-09-07 | User's answer to A5: rectangles or sprite art? | **Pixel-art sprites** — the larger-scope option. This is the first feature to compose `Sprite`/`blit` for real (A5); see C15 for how the art itself gets authored. |
| C12 | 2026-09-07 | User's answer to A6: formation grid size? | **3×6 (18 enemies)** — the smaller grid, explicitly chosen to leave room for sprites that may be larger than the rect-default assumed (A6). |
| C13 | 2026-09-07 | *(New surface from A1.)* Where does the player respawn, and what invulnerability window prevents instant re-death from an entity still overlapping the respawn point? | **Resolved (PO judgment), explicitly decided per the task's own instruction not to leave this implicit.** Fixed starting position; 120 ticks (2s @ 60Hz) of invulnerability, exempting contact-based damage only (A12, AC-10.2–10.4). |
| C14 | 2026-09-07 | *(New surface from A1.)* Does the formation-threshold failsafe (A7) respect lives, or end the round outright? | **Resolved (PO judgment).** Outright, regardless of lives — preserves A7's own anti-exploit purpose (A13, AC-5.2/AC-10.6). |
| C15 | 2026-09-07 | *(New surface from A5.)* Is there a sprite-authoring pipeline, and is `assets/sprites/galactic_invation.png` usable as source data? | **Resolved (PO judgment), grounded in an exact existing precedent.** Hand-authored `Color`-array literals mirroring `font.cpp`'s `GlyphArt` convention; no PNG import. The concept sheet is style/shape reference only for `/look-and-feel`, not source pixel data — it depicts substantially more scope than this feature builds (A14, §6). |
| C16 | 2026-09-07 | *(New surface from A1.)* How is the lives count displayed? | **Resolved (PO judgment).** Text, `LIVES: n`, top-right — mirrors the score display's own precedent exactly, not the concept sheet's heart icons (A15). |
| C17 | 2026-09-07 | `/look-and-feel` finding 3: should invulnerability have a visual indicator (sprite flicker), given AC-10.3's silent grace window could read as a collision bug? | **Resolved by the user — yes.** Added as AC-10.8: the player sprite flickers on a fixed deterministic tick pattern during invulnerability. No new art, no per-frame animation — the single existing sprite's visibility toggles. |

## 8. Design Review

- **Overall impression:** Sound, low-risk design. Everything the spec commits to — HUD placement, end-screen text-only differentiation, sprite rendering via the shipped `Sprite`/`blit` — composes precedents this project has already reviewed once (start-screen's disjoint-bounds convention and `BRIGHT_ORANGE`-on-`BLACK` ink; `font.cpp`'s `GlyphArt` hand-authoring convention). **No Blocker findings.** Nothing here needs to bounce back to `/story-time` — every item below is either a design constraint to hand off to whoever authors sprite art/pixel coordinates at `/increment`, a sharpening of an existing AC's testable rule, or a question for the PO to accept or decline as a small scope addition, not a required fix.

- **Heuristics findings:**
  - *Visibility of status* — Score and lives are drawn every `PLAYING` tick (AC-7.1/AC-10.1) and both end screens still show the final score (AC-5.3/AC-11.3), so the player is never left wondering how they did. One gap: **invulnerability has no visible signal** — see the dedicated finding below.
  - *Consistency & standards* — `BRIGHT_ORANGE` on `BLACK` for all text (NFR-7) correctly reuses `start-screen`'s own confirmed-4.5:1 convention rather than inventing a second one. The shipped 43-character font set covers every character both HUD strings need (`SCORE: nnnn` needs `S,C,O,R,E,:, ,0-9`; `LIVES: n` needs `L,I,V,E,S,:, ,0-9` — all present in `font.cpp`'s `kGlyphArt`). Good consistency, no finding.
  - *Error prevention* — Single-active-projectile (AC-2.2) and the formation-threshold failsafe (AC-5.2/A13) are genre-appropriate constraints on player action, not UI error states; no finding.
  - *Recognition over recall* — HUD is always on-screen during play; nothing the player must remember between screens. No finding.
  - *Minimalism* — Two HUD text elements plus three sprite types is a minimal surface; no clutter, no dead chrome. No finding.
  - *Match to the real world / user control* — Restart is one button from either end screen (AC-6.3/AC-11.4), matching constitution Principle 4 exactly. No finding.

- **Accessibility notes:**
  - *Contrast* — NFR-7's `BRIGHT_ORANGE`-on-`BLACK` text carries forward `start-screen`'s already-confirmed WCAG 4.5:1 pass. Covered, no new finding needed.
  - *Color-independence* — AC-12.4's silhouette-not-color requirement is itself a colorblind-accessible design choice (entity identity never depends on discriminating among the 4 palette colors) — worth naming as a strength, not just a constraint to satisfy.
  - *Keyboard/focus/labels/alt-text/touch-targets* — Not applicable to this platform: SteamCore is a fixed physical-button device with no screen reader, no pointer, and the touch controller is constitutionally off-limits. These standard accessibility checks have no surface to attach to here, same posture `start-screen`'s own design review took.
  - *No-audio, no-alternative-input* — Acknowledged as an accepted project-wide constraint (constitution §3 off-limits list), not a new gap this feature introduces or could unilaterally fix.

- **Design risks & required changes:**

  1. **AC-12.4 sprite silhouette distinctness — design constraint for whoever authors the sprites at `/increment` time (no art exists yet; nothing to fix today, but this must reach that task's description verbatim).** The palette is `BLACK`/`DARK_ORANGE`/`ORANGE`/`BRIGHT_ORANGE` (`color.h`) at 240×160 (`config.h`). `font.cpp`'s `GlyphArt` convention this feature explicitly mirrors (A14) authors glyphs at 8×8 with a two-value on/off marker (`BRIGHT_ORANGE`/`BLACK`); `sprite_test.cpp` confirms the engine itself has no fixed sprite-size floor (it exercises 8×8 and 16×12 alike), so the three game sprites are free to be somewhat larger than a glyph if the formation math (A6, deliberately shrunk to 3×6 to leave room) needs it — but at any size in this range, anti-aliasing does not exist and one on-colour is the likely convention, so **shape, not shading, is the only distinguishing tool.** Concrete guidance to author against:
     - **Player ship:** a single, vertically-symmetric silhouette narrowing to a point at the top (nose-up wedge/arrow), wide at the base — the canonical "this is the thing I steer" read.
     - **Enemy:** a silhouette that reads as the *opposite* shape family — horizontally-symmetric and top-heavy/blocky (classic invader profile with a notch or protrusions breaking the outline), never a scaled or mirrored copy of the ship's wedge. Distinctness must survive at the formation's actual on-screen scale, not just in isolation — check it once rendered 3×6 as a block, not only as a single sprite.
     - **Projectile:** deliberately a *different aspect ratio* from both of the above, not just smaller — a slim vertical bar or dot rather than a miniature ship/enemy — so its motion reads as "a shot," recognizable at a glance even in peripheral vision while tracking the ship.
     - Verify all three side-by-side at actual render scale before this AC is marked satisfied at `/peer-review` — a shape check done only on paper/concept art (`assets/sprites/galactic_invation.png`, correctly out of scope as source data per A14) does not substitute for checking the actual `Color`-array literals once written.

  2. **AC-11.2 win/loss distinctness — recommend sharpening "different text content" into a testable rule, no new scope.** Text-only differentiation is acceptable here — unlike a UI a user stumbles into cold, the player already knows contextually whether they just cleared the last enemy or lost their last life, so the end screen confirms rather than solely conveys the outcome. But "different words" alone under-specifies "reads clearly at a glance": since both screens will presumably center their outcome text the same way `start-screen` centers its wordmark/prompt (width-derived per A14/A9's own convention), two strings of *near-identical rendered width* would produce two screens that look like the same layout with blurry text at a glance, satisfying the AC's letter while missing its intent. Recommended concrete rule for `/sprint-plan` to adopt (no new asset, no animation, purely a text-content choice within scope already granted by A4/AC-11.3): pick the victory string so its rendered width (`(chars) × kGlyphAdvance`) differs from `"GAME OVER"`'s 72px by a visually obvious margin (e.g. a shorter `"YOU WIN"` at 56px, or a longer one) — this makes the two end screens differ in overall silhouette/position, not just in the letters a player would have to stop and read, and is verifiable by the same host test AC-11.2 already calls for (the two framebuffers differing) plus a bounding-box-width assertion mirroring `kTitleWordmarkBounds`'s own precedent. **Resolved: folded into AC-11.2 directly (C17-adjacent, no separate clarification needed — this was a sharpening, not a scope decision).**

  3. **AC-10.3 invulnerability has no visual indicator — question back to the PO, not a mandate.** A12 explicitly anticipates the respawned ship landing directly on top of an enemy or projectile that was already overlapping the respawn point, and AC-10.3 correctly makes that safe (no damage). But with zero visual signal, that exact anticipated moment — the ship visibly "inside" an enemy sprite while nothing happens — will read as a collision bug to a player, not as an intentional grace window; this is a *visibility of system status* gap, not an accessibility-exclusion one, so it's flagged Minor. A common, cheap fix (arcade convention): flicker the player sprite during the invulnerability window by skipping alternate `blit` calls on a fixed tick pattern — this reuses the one already-planned static player sprite (no new art, no new palette use) and is not "multiple animation frames per entity" in the §6 out-of-scope sense (that excludes new per-frame art; this toggles the visibility of the single existing frame). Raising this as a question rather than a required change because it is a small but real scope addition (a new AC under US-10) that the PO should accept or explicitly decline before `/sprint-plan`, not something a design review should silently fold in. **Resolved: the user chose to add it — see AC-10.8 and C17.**

  4. **AC-10.1/AC-7.2 HUD layout — geometrically sound, no change needed.** At 240×160 with `kGlyphWidth`/`kGlyphAdvance` = 8px (`font.h`): `SCORE: nnnn` is 11 glyphs = 88px, fitting fully within `x ∈ [0, 88)` at top-left; `LIVES: n` is 8 glyphs = 64px, fitting fully within `x ∈ [176, 240)` at top-right. That leaves an 88px horizontal gap between them (`x ∈ [88, 176)`) — generous clearance even if the score's digit count ever grows. Both sit in a single 8px-tall row (`y ∈ [0, 8)`). The 3×6 enemy formation (A6) sits below that row; as long as its top row's `y` starts at or below the HUD row (the formation clearly needs the bulk of the remaining 152px of height plus room for the player and projectiles), the two never collide by construction. The player ship's fixed y (bottom of screen, US-1's "bottom" placement, unchanging per AC-1.3) is at the opposite vertical extreme from the HUD row regardless of its x-position, so "the player's movement lane" and the LIVES display are disjoint by construction too — no horizontal-lane collision is geometrically possible given the vertical separation alone. One spacing recommendation, not a requirement: leave at least one full empty glyph row (8px) of clear black between the HUD row and the formation's first row, rather than having the formation start flush at `y = 8`, purely for visual breathing room (spacing/minimalism heuristic) — a `/sprint-plan` pixel-coordinate decision, not a spec change.

---

## ✅ SPEC GATE

*All boxes checked → `/sprint-plan` may start. Any box open → back to `/story-time` or `/look-and-feel`.*

- [x] Problem, goal and success signal are concrete (no buzzwords, no "everyone")
- [x] Every story has testable Given/When/Then acceptance criteria
- [x] Stories are prioritized (MoSCoW) and at least one is a Must
- [x] Non-functional requirements are stated and measurable (or marked N/A with reason)
- [x] Clarify pass done: no ambiguity left unresolved or unparked — every gap the larger-scope answers introduced (A12–A15) is resolved by PO judgment grounded in an exact existing precedent, logged in §7 C13–C16
- [x] Open questions are resolved or explicitly accepted as risk — **0 open**: A1–A15 all resolved, none left to the user's further input
- [x] Out-of-scope section is filled (something was consciously cut) — expanded this round to name everything the concept sheet shows that this feature deliberately does not build
- [x] Constitution (`.spark/constitution.md`) respected, or conflicts recorded as open questions — no conflict found; Principle 4 ("visible score") grounds US-7/A4, constitution §6 ("clear silhouettes") grounds AC-12.4, `GameState`'s shipped contract is respected unmodified (NFR-5)
- [x] Design review done for UI-facing features (or marked N/A with reason) — done by `/look-and-feel` (§8): no Blocker findings; HUD layout (AC-10.1/AC-7.2) confirmed geometrically sound, sprite silhouette guidance (AC-12.4) and a win/loss text-width sharpening (AC-11.2) handed off to `/increment`/`/sprint-plan`, and one Minor invulnerability-visibility item raised as a question back to the PO, not a required change
- [x] Line budget respected: Ist 285 / Soll ~250 (excluding HTML comments) — 35 over; reason: three new Must stories (US-10/11/12), eight new assumption/clarification rows (A12–A15, C13–C16) from the user's three larger-scope choices, plus `/look-and-feel`'s own §8 findings and one more design-driven AC (AC-10.8, C17) — accepted rather than under-specifying a feature whose scope genuinely tripled in Must-story count
- [x] Status set to `approved` by the user — 2026-09-07
