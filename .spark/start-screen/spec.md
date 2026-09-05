# Spec: start-screen

| | |
|---|---|
| **Phase** | Specify |
| **Owner** | Product Owner (`/story-time`), Designer (`/look-and-feel`) |
| **Status** | `approved` |
| **Date** | 2026-09-04 |
| **Ticket** | `none` |

**Handoff**
- **Status:** `approved` — user approved 2026-09-04, after PO judgment calls on five ambiguities (A1–A5), one explicitly accepted design risk (A6), and both Major design-review findings folded into ACs. A1 (single title screen, no multi-game selection) is **user-confirmed** (2026-09-04). A3 (GameState/GameSession touch point) remains a deliberate architecture-phase deferral to `/sprint-plan`. §8 Design Review (`/look-and-feel`, Mode A) found two Major findings requiring AC-level changes; both are folded in here: AC-1.2 now pins BRIGHT_ORANGE as the prompt's ink colour (contrast), and new AC-1.6 requires the logo's and prompt's documented bounding rectangles to be disjoint (layout collision). NFR-7's contrast claim is corrected to not overstate what holds beyond BRIGHT_ORANGE.
- **Summary:** Every primitive a start/title screen needs — `Framebuffer`, `drawText`, `Sprite`, `GameLoop`, `GameSession`'s READY phase, `GameInput.start` — has shipped, but nothing composes them: the console cannot yet show its own identity or a "press START" prompt. Deliver the minimal static title screen (logo/title + "PRESS START") that renders while a session hasn't started play, and stops rendering once it has.
- **Open:** `0 open` — A1 resolved by explicit user confirmation (2026-09-04, see §3 A1, §7 C1); A3 is a deliberate `/sprint-plan` deferral (see §3 A3, §6); both §8 Design Review Major findings are folded into AC-1.2/AC-1.6; the logo-sprite follow-up design check is recorded as a forward pointer for `/sprint-plan` at C7; the font-legibility risk is explicitly PO-accepted at A6.
- **Binding ruling:** §4 User Stories for the current stories; §7 Clarifications for what changed since the last round and why.
- **On conflict:** the numbered body below wins for everything except `Status`; log the mismatch as a finding at the next `/peer-review` and proceed — don't stop on it.

## 1. Problem & Goal

- **Problem:** `GameSession` (v0.1.0) already models "hasn't started play yet" as `GameState::READY`, and the display driver, framebuffer, sprites and text rendering all shipped and are released — but nothing draws anything while READY. README's own *Boot Experience* mockup ("SYSTEM READY / PRESS START") and constitution Principle 4 ("arcade immediacy... restart always one button away") describe a screen that has zero implementing pixels today. Every consumer of this engine would otherwise invent its own ad hoc idle-screen layout, the same class of duplication `game-state-management` was built to prevent for session bookkeeping.
- **Goal:** One minimal, static title screen — a fixed logo/title element plus a "PRESS START" prompt — drawn with already-shipped primitives only, visible whenever the session hasn't started play, gone the instant it has.
- **Success signal:** A host test renders the framebuffer during the pre-play phase and asserts the logo/title and "PRESS START" text land at their documented positions, with every other pixel BLACK; a second assertion shows both are absent one tick after a START rising edge. Deferred signal: the first real game or system-menu story composes against this without editing a file delivered here (same bar `game-state-management` set for itself).
- **Why now:** This is the cheapest point to build it — text rendering, the display driver and GameSession all just shipped; this feature is pure composition of existing capability, not new capability. It is also the first story that makes the console recognizably itself to a human rather than a rendering test pattern.

## 2. Target Users

- **Engine developer / future game-module author (primary, today):** needs one canonical "not playing yet" screen instead of each future game inventing its own idle-state layout.
- **The eventual console player:** the constitution's actual target user for Principle 4 — sees the console's identity and a clear one-button way to begin. Not directly testable on real hardware by this feature's Musts (no game exists yet to hand off to), same posture every prior engine story has taken.
- *Not a user of this feature:* a hypothetical multi-game chooser. No second game and no game registry exist yet (constitution §3 "Games": self-registration into a registry is itself an unbuilt, `/sprint-plan`-owned mechanism) — see A1.

## 3. Assumptions & Open Questions

| # | Assumption / Question | Resolution |
|---|---|---|
| A1 | Is "game menu" a selection among multiple games, or a single title/prompt screen? | **Single title screen, no selection.** Zero games and no registry exist to select among (README's five-game menu mockup is Phase-3 aspirational content); building selection UI now has no consumer and is exactly the speculative-surface risk the library lens flags. Multi-game selection is deferred until a registry exists (§6). **User-confirmed 2026-09-04** — presented with "Nur Titel-Screen, keine Auswahl (Empfehlung)" vs. "Echtes Auswahlmenü vorbereiten," the user chose the title-screen-only option, matching this resolution exactly. |
| A2 | What does "logo" mean concretely, given `assets/Buttons.png`, `assets/sprites/*.png` and `assets/fonts/*.ttf` exist untracked in the repo? | **A hand-authored, in-engine visual only** — either styled text via the existing `drawText`/font, or a small `Sprite` built from native `Color` data — never an import of the PNG/TTF files. No PNG/TTF decode or asset-import pipeline exists anywhere in this engine; building one is a separate, unstarted capability and a different story. If the user specifically wants one of those source images, that pipeline decision is out of scope here. |
| A3 | Does this extend `GameState`/`GameSession` with new states, or compose alongside it as a separate mechanism? | **Not decided here — parked for `/sprint-plan`.** `GameSession` staying byte-identical has been an enforced constraint across two prior features with no consumer game yet to justify changing it. This spec states desired behavior only (§4): a title screen is visible exactly while a session hasn't started play, and is gone once it has — not which type owns that decision. This is a deliberate architecture-phase deferral, not an unresolved spec-phase ambiguity; `/sprint-plan` owns the resolution. |
| A4 | Is "selectable" a cycling UI (up/down highlight) or a single button? | **Single button (START), edge-triggered** — there is nothing to cycle among (A1). `GameInput.select`/joystick fields exist and are not consumed by this feature; reserved for a future settings/highscore-menu story. |
| A5 | Is this feature verifiable without hardware? | **Yes for the Musts** — host-compiled framebuffer/text assertions, same method as `text-rendering`/`rendering-core`. It is nonetheless **UI-facing** (new visual composition of a logo and text layout), so `/look-and-feel` Design Review is required, not N/A — "UI-facing" here is about new visual surface, independent of the `ux` lens being inactive project-wide. |
| A6 | Is the shipped 8×8 font's physical legibility on the real ~3.5" panel (≈2.5mm glyph height at the constitution's fixed ×2 scale) acceptable for this feature's highest-stakes use of it — the "PRESS START" call-to-action, the single most important message on the console's first screen? | **Accepted as an inherited platform/font constraint, not a defect of this spec.** The font shipped with `text-rendering`; changing its size is out of scope here (AC-1.2 locks the prompt to `drawText`/the existing font; AC-1.3 forbids introducing a new drawing primitive). The PO explicitly accepts this legibility risk (flagged by `/look-and-feel`, §8) rather than silently inheriting it. US-3's hardware dump (AC-3.1) confirms pixel-position correctness but does not simulate real viewing-distance legibility, so it will not itself catch a legibility problem if one exists — a future feature may need to revisit font size if this proves too small in practice. |

## 4. User Stories

### US-1 (Must): Title screen shows a logo/title and a "PRESS START" prompt before play begins

> As a future game-module author, I want a ready-made "not playing yet" screen — a fixed logo/title element and a "PRESS START" prompt — drawn from already-shipped primitives, so every game gets a consistent idle screen without inventing its own layout.

**Acceptance criteria:**

- [ ] AC-1.1: Given a framebuffer cleared to BLACK and a session that has not yet started play, when the screen's render step runs, then a title/logo element is drawn at a fixed, documented on-screen position, and every pixel not covered by it or the prompt (AC-1.2) remains BLACK.
- [ ] AC-1.2: Given the same pre-play state, when rendered, then the text "PRESS START" is drawn at a fixed, documented position in **BRIGHT_ORANGE** ink — the only one of the engine's three non-BLACK palette colours that clears the WCAG 4.5:1 normal-text contrast floor against BLACK (§8 Design Review) — using the engine's existing `drawText`/font (its uppercase/digit/punctuation charset already covers this string).
- [ ] AC-1.3: Given the delivered code, when inspected, then only `Framebuffer`, `Sprite`/`blit` and `drawText` — all already-shipped — are used; no new low-level drawing primitive is added.
- [ ] AC-1.4: Given the logo element is implemented as a `Sprite`, when its pixel data is inspected, then it uses only the engine's existing four-colour palette and is hand-authored `Color` data (A2) — no decoded PNG/TTF byte appears anywhere in it.
- [ ] AC-1.5: Given the suite runs under `-fsanitize=address,undefined`, when the title screen renders at the framebuffer's actual edges, then no out-of-bounds access occurs (inherited `Framebuffer`/`Sprite` clipping contract, not re-implemented here).
- [ ] AC-1.6: Given the logo/title element's documented bounding rectangle (AC-1.1) and the "PRESS START" prompt's documented bounding rectangle (AC-1.2), when a host test computes their intersection, then the intersection is empty — the two elements' bounding boxes are disjoint and never visually overlap or crowd each other.

### US-2 (Must): The title screen disappears the instant play begins

> As a future game-module author, I want this feature's own drawing to stop the tick after START is pressed, so a playing game's screen is never fought over or obscured by leftover title pixels.

**Acceptance criteria:**

- [ ] AC-2.1: Given the pre-play state followed by a `start` rising edge, when the next tick renders, then neither the logo/title nor "PRESS START" is drawn by this feature's own rendering step.
- [ ] AC-2.2: Given a `start` level that stays `true` for several consecutive ticks after the rising edge, when each of those ticks renders, then the title screen stays absent throughout — it never flickers back while `start` is merely held.
- [ ] AC-2.3: Given the same fixed input sequence run twice from a freshly constructed starting state, when the resulting framebuffers at every step are compared, then they are byte-identical (constitution §4 determinism).

### US-3 (Should): Title screen is confirmable on real hardware, not only by host assertion

> As the engine developer, I want the title screen dumped from the real device the same way prior features proved themselves, so the first screen a human actually sees is verified, not just asserted in a host test.

*Priority note: `Should`, not `Must` — the Musts (US-1, US-2) are fully proven host-side and gate the feature by themselves. This is the first thing cut if scope must shrink; it adds confidence, not correctness.*

**Acceptance criteria:**

- [ ] AC-3.1: Given the real device flashed with a harness driving this title screen (mirroring `input-driver`'s `InputHarnessGame` pattern), when the framebuffer is dumped over USB-CDC through the Python viewer, then the decoded image shows the logo/title and "PRESS START" at their documented positions.
- [ ] AC-3.2: If hardware access is unavailable when this story is built, then QA records this AC as "not capturable yet" with that reason, per constitution §8 — it does not block US-1/US-2, and source reading is not substituted for it.

## 5. Non-Functional Requirements

| # | Category | Requirement (measurable) | How it's verified |
|---|---|---|---|
| NFR-1 | Performance | Rendering the full title screen (one sprite blit or a handful of glyphs, plus the prompt) completes in well under the 60Hz tick budget on host with `-O2` — both drawing calls are already independently benchmarked by `rendering-core`/`text-rendering`; no new cost model is introduced. | host test + `/peer-review` |
| NFR-2 | Reliability / memory | Zero dynamic allocation; any logo `Sprite` pixel data is `static const`, no runtime allocation; no ESP-IDF header in delivered logic code. | `/peer-review` (grep) |
| NFR-3 | Determinism | Same input sequence and starting state → byte-identical framebuffer sequence, both compilers; no wall-clock or unseeded-random read. | AC-2.3 + `/peer-review` |
| NFR-4 | Portability / toolchain | Compiles clean under `clang++` and `g++`, `-std=c++17 -Wall -Wextra -Werror`; no resolution/tile-size literal outside the existing constants header. | `/peer-review` (grep + both compilers) |
| NFR-5 | **Library lens — public API surface** | Exactly the minimal new public surface needed to render and query the title screen (one type or function a game/harness calls); no speculative accessor, no new `GameInput` field, and no change to `GameLoop<Game>`'s call contract. Whatever `GameState`/`GameSession` touch point `/sprint-plan` chooses, it is the *only* new surface this feature adds beyond that. | `/peer-review` |
| NFR-6 | **Library lens — contract clarity** | Doc comment states: exactly which session phase triggers the title screen (A3's resolution, once made), that it draws nothing once play has started (US-2), the primitives it composes (US-1), and the inherited single-threaded/no-throw/no-alloc contract. One usage example. | `/peer-review` |
| NFR-7 | Accessibility / legibility | "PRESS START" is rendered in BRIGHT_ORANGE ink on BLACK (AC-1.2), measured ≈9.9:1 against the shipped RGB values in `panel_format.cpp` — clears the WCAG 4.5:1 normal-text-contrast floor. This holds for BRIGHT_ORANGE specifically, **not** for the palette as a whole: ORANGE (≈4.3:1) and DARK_ORANGE (≈1.6:1) both fail that floor and are not used for the prompt's ink. Exact logo shape/placement and the font's physical legibility on the real panel (A6) get a dedicated pass at `/look-and-feel` (§8) rather than being enumerated further here, since no `ux` lens is active project-wide. | `/look-and-feel` + AC-1.2 |
| NFR-8 | Security & privacy | N/A — offline device, no input persisted, no personal data (constitution §2). | — |
| NFR-9 | Observability / ops | N/A — pure rendering logic, no runtime failure mode, no logging surface. | — |
| NFR-10 | Integrations & dependencies | N/A — no external dependency; composes only already-shipped engine types; adds no new `GameInput` field (A4). | — |

*Lens note: `library` active **scoped** (constitution §2); semver/packaging are no-ops. Public API surface and Contract clarity land as NFR-5/NFR-6.*

## 6. Out of Scope

- **Multi-game selection menu, up/down cycling, highlighted entries.** No second game or registry exists to select among (A1); this is a single-cabinet title screen, not a chooser. **User-confirmed 2026-09-04** — the user was explicitly offered the alternative (preparing real multi-game selection infrastructure) and declined it in favor of this option.
- **Importing/decoding `assets/Buttons.png`, `assets/sprites/*.png`, `assets/fonts/*.ttf`.** No asset-import pipeline exists in this engine; the logo is hand-authored native `Color`/text data instead (A2). Building a PNG/TTF pipeline is its own story if ever wanted.
- **Any change to `GameState`'s enum values, `GameSession`'s transition rules, `GameInput`'s fields, or `GameLoop<Game>`'s call contract.** Whether this composes as new states or a separate mechanism is explicitly a `/sprint-plan` decision (A3), not resolved here.
- **A distinct GAME_OVER / restart screen.** `game_state.h`'s own doc-comment example already treats GAME_OVER as a separate draw case from READY's "PRESS START"; this feature covers only the pre-play moment, not a run's end screen.
- **Animation, fades, transition effects, sound on the title screen.** Constitution Non-Negotiables forbid modern UI animation; audio synthesis is a separate, unbuilt engine area.
- **SELECT-driven settings, highscores, or any other system-menu entry** from README's Boot Experience mockup. `GameInput.select` physically exists but is not wired to anything by this feature.
- **Physical button wiring.** `input-driver`'s own hardware wiring remains blocked (`docs/wiring-input.md` T10); this feature needs only the logical `GameInput.start` level, real or harness-supplied.
- **Composite/CRT output, the final 800×480 panel's scaling.** Unrelated, already out of scope project-wide until Phase 4/5.
- **A dedicated Mode-B `/look-and-feel` pass on the logo sprite's actual pixel art, right now.** A2 leaves the logo's concrete shape/size open because no artwork exists yet (Mode A can't critique a sprite that isn't authored). This is not skipped, only deferred — see §7 C7 for the forward-pointer `/sprint-plan` must schedule once the sprite is built.

## 7. Clarifications

| # | Date | Question | Resolution |
|---|---|---|---|
| C1 | 2026-09-04 | Does "game menu" mean selecting among multiple games? | **No — single title screen, no selection**, since no second game or registry exists (A1). **User-confirmed 2026-09-04**: offered "Nur Titel-Screen, keine Auswahl (Empfehlung)" vs. "Echtes Auswahlmenü vorbereiten," the user explicitly chose the title-screen-only option. |
| C2 | 2026-09-04 | Does "logo" require importing the untracked PNG/TTF art assets? | **No.** Hand-authored in-engine `Color`/text data only; no import pipeline exists or is built here (A2). |
| C3 | 2026-09-04 | Does this feature extend `GameState`/`GameSession`? | **Not decided here.** Behavior only is specified; the type-level decision is a deliberate `/sprint-plan` deferral (A3), not an open spec-phase question. |
| C4 | 2026-09-04 | Is "selectable" a cycling menu or a single button? | **Single button (START)** — nothing exists to cycle among yet (A4). |
| C5 | 2026-09-04 | Is hardware required to verify this feature? | **No for the Musts** (host framebuffer assertions); hardware confirmation is a `Should` (US-3), and Design Review is required as UI-facing regardless (A5). |
| C6 | 2026-09-04 | Half-size version, if the story must shrink? | US-1 alone (a static title screen that renders while not playing) already delivers the core value — a human sees the console's identity. US-2 (disappearing on START) is cut second, despite being needed for a real game to ever compose against this cleanly. US-3 (hardware confirmation) is cut first, per its own priority note. |
| C7 | 2026-09-04 | Should the logo sprite's shape/size get a dedicated design check once it's actually authored? | **Yes — deferred to `/sprint-plan`, not silently dropped.** A2 deliberately leaves the logo's concrete shape/size open since no artwork exists yet; `/look-and-feel`'s Mode A pass (§8) could not critique pixel art that doesn't exist. When `/sprint-plan` schedules the sprite's authoring, it must also schedule a follow-up Mode-B `/look-and-feel` pass (or an explicit `/peer-review` checklist item) against §6's "simple sprites, clear silhouettes" — nothing in this spec triggers that check automatically. |

## 8. Design Review

- **Overall impression:** The spec is well-aligned with this project's own graphics philosophy (§6 non-negotiables) and correctly scopes itself *down* from README's full Boot Experience mockup rather than transcribing it: no boot log, no blinking "attract mode," no fades — all of which the constitution's Non-Negotiables and Principle 1 ("atmosphere beats capability") would have forbidden or discouraged anyway. AC-1.1's "every pixel not covered by the logo or the prompt remains BLACK" is a genuinely strong, structurally-testable minimalism guarantee — most specs assert minimalism as intent; this one makes it a Given/When/Then. AC-1.3/AC-1.4 correctly fence the implementation to already-shipped primitives and the 4-colour palette, closing off the obvious ways a developer could accidentally reintroduce a "large texture"/gradient/photorealistic look. Two concrete gaps below (ink colour, layout collision) should be closed before `/sprint-plan`; neither requires new capability, both are inexpensive to add as ACs.

- **Heuristics findings:**
  - **Visibility of status — met.** A static logo + "PRESS START" against an otherwise all-BLACK screen (AC-1.1/1.2), with instant, non-flickering removal on the START rising edge (AC-2.1/2.2), communicates "machine is on and idle, one button away from play" exactly as a real cabinet does, without inventing a status the constitution doesn't ask for (no animation, no counter, no attract-mode cycling — correctly out of scope per §6).
  - **Recognition over recall — met.** "PRESS START" is the real-world arcade convention (README's own Boot Experience mockup uses the identical phrase); a first-time player needs no instructions, matching Principle 4 ("no text the player must read to begin").
  - **Error prevention / layout collision — gap.** No AC constrains the *relationship* between the logo's documented position and the prompt's documented position. AC-1.5 only proves memory safety at the framebuffer's physical edges (ASan), not that the two elements' bounding boxes stay disjoint from each other. Nothing stops an implementer from documenting positions that visually overlap or crowd each other on the 240×160 canvas — that would satisfy every current AC (including AC-1.1's "every other pixel is BLACK," which is about pixels *outside* both elements, not about the two elements colliding with each other) while still shipping a garbled screen. **Severity: Major.** Concrete fix: add an AC requiring the logo's and prompt's documented bounding rectangles to be non-overlapping, verified by the same host test that checks their pixel positions (e.g. assert the two rectangles' intersection is empty before asserting their individual contents). Flagged to the PO below rather than added directly (out of this ceremony's scope).
  - **Consistency with the graphics philosophy — met, with one open edge.** AC-1.3/1.4 correctly close off new-primitive and off-palette risks. What they don't close off: A2 leaves the logo's *size and pixel complexity* fully open ("hand-authored... Sprite" of unspecified dimensions). A sprite that is technically 4-colour and uses only shipped primitives could still be large, busy, or dithered to *simulate* more tones — violating "large pixels, simple sprites, clear silhouettes" in spirit without tripping any grep-able AC. This isn't a spec defect to fix now (the actual artwork doesn't exist yet, so Mode A can't critique it) — it's a process gap: recommend a second, Mode-B `/look-and-feel` pass (or an explicit `/peer-review` checklist item) once the logo sprite is actually authored, before `/go-live`, since nothing currently schedules that check.

- **Accessibility notes:** (this platform has no keyboard, no pointer and no touchscreen — the XPT2046 touch controller is off-limits per constitution §3 — so keyboard-navigation/focus-order/touch-target checks are **N/A by platform**, not skipped; the one check that *does* apply is contrast, and it surfaces a real, fixable gap)
  - **Contrast — gap, computed against the actual shipped palette** (`panel_format.cpp`'s real RGB values, not the enum names): BRIGHT_ORANGE (0xFF9933) on BLACK ≈ **9.9:1** — comfortably clears the 4.5:1 floor. ORANGE (0xB35900) on BLACK ≈ **4.3:1** — just *under* 4.5:1 for normal-size text (8×8 glyphs are not "large text" by any reasonable reading, even accounting for the ×2 panel scale), though it clears the 3:1 large-text floor. DARK_ORANGE (0x4D2600) on BLACK ≈ **1.6:1** — fails badly, far below even the large-text floor; at that ratio "PRESS START" would be barely distinguishable from the background on the physical panel. **Neither AC-1.2 (the prompt) nor AC-1.1 (the logo) pins down which of the three non-black colours must be used.** NFR-7's claim that the palette is "inherently high-contrast" is true only for BRIGHT_ORANGE against BLACK — it does not hold for all three non-BLACK colours, so it cannot substitute for an explicit choice in the AC. **Severity: Major** (accessibility findings are never Minor by default here, and this gates whether the single most important on-screen message — the call to action — is legible at all). Concrete fix: AC-1.2 should state the prompt's ink colour explicitly; BRIGHT_ORANGE is the only one of the three that clears 4.5:1 outright and is the recommended value. Flagged to the PO below since it changes an AC.
  - **Physical legibility at cabinet viewing distance — risk, not a blocker.** The engine's only font is the already-shipped 8×8 monospace glyph (font.h); at the constitution's fixed ×2 panel scale that's 16 physical pixels tall on the ~3.5" KMRTM35018 panel — roughly 2.5 mm of glyph height. That ratio is inherited from `text-rendering` and isn't something this spec can change without new scope (AC-1.3 forbids a new drawing primitive, and a larger custom hand-authored sprite is only available to the *logo*, not to "PRESS START," which AC-1.2 locks to `drawText`). This is the first feature where that font size carries the single most important message on the whole console, so the stakes of it being too small to read from normal play distance are higher here than in any prior use. Not rated as a finding against this spec (nothing here is fixable within this spec's scope), but recorded as a design risk for the PO to accept explicitly rather than discover after `/go-live`.

- **Design risks & required changes:**
  - **Required before `/sprint-plan` (both are AC-level changes — out of this ceremony's scope to edit directly, routed to the PO):**
    1. Pin the ink colour for "PRESS START" (and ideally the logo, if it's rendered as text/`drawText` rather than a `Sprite` with baked-in colour) to **BRIGHT_ORANGE** explicitly in AC-1.2, rather than leaving colour choice implicit — closes the 1.6:1/4.3:1 contrast risk above.
    2. Add an AC requiring the logo's and prompt's documented bounding boxes to be non-overlapping — closes the layout-collision gap above.
  - **Accepted as-is, flagged for awareness, not requiring a spec change:**
    - A2's deliberately open logo shape/size is acceptable to leave to `/sprint-plan`/implementation, *provided* the actual authored sprite gets a Mode-B `/look-and-feel` or `/peer-review` pass against §6's "simple sprites, clear silhouettes" once it exists — nothing currently schedules that pass, so name it explicitly when writing the plan.
    - The 8×8 font's physical legibility on the real panel is an inherited platform constraint, not a defect of this spec; the PO should explicitly accept it (rather than silently discover it) given this is the font's highest-stakes use to date. US-3's hardware dump (AC-3.1) confirms pixel-position correctness but does not simulate real viewing distance, so it will not itself catch a legibility problem if one exists.
    - The spec's exclusion of a blinking/attract-mode prompt, a boot log and any transition animation is correct and well-grounded (§6 Non-Negotiables, Principle 1) — no change needed.

---

## ✅ SPEC GATE

*All boxes checked → `/sprint-plan` may start. Any box open → back to `/story-time` or `/look-and-feel`.*

- [x] Problem, goal and success signal are concrete (no buzzwords, no "everyone")
- [x] Every story has testable Given/When/Then acceptance criteria
- [x] Stories are prioritized (MoSCoW) and at least one is a Must
- [x] Non-functional requirements are stated and measurable (or marked N/A with reason)
- [x] Clarify pass done: functional scope (A1/A3, C1/C3), data/assets (A2, C2), roles (§2, single audience), error/edge cases (AC-2.2, AC-1.5), NFRs (NFR-1…10), integrations (NFR-10, A4), UX flows (§8 completed, contrast + layout-collision findings folded into AC-1.2/AC-1.6), out-of-scope (§6) — all swept
- [x] Open questions are resolved or explicitly accepted as risk — A1 is **user-confirmed** (2026-09-04, §3 A1 / §7 C1); A3 is a deliberate architecture deferral to `/sprint-plan` (§3/§6); A6 (font legibility) is explicitly PO-accepted as risk; §8's two Major findings are resolved via AC-1.2/AC-1.6; the logo-sprite follow-up check is recorded as a forward pointer (§7 C7 / §6).
- [x] Out-of-scope section is filled (something was consciously cut)
- [x] Constitution (`.spark/constitution.md`) respected — Principle 4 grounded (US-1/US-2); no conflict found
- [x] Design review done for UI-facing features (or marked N/A with reason) — completed 2026-09-04 (§8, Mode A); two Major findings (contrast, layout collision) folded into AC-1.2 and new AC-1.6 by the PO
- [x] Line budget respected: Ist 149 / Soll ~250 (excluding HTML comments)
- [x] Status set to `approved` by the user — 2026-09-04
