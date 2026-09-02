# Spec: text-rendering

| | |
|---|---|
| **Phase** | Specify |
| **Owner** | Product Owner (`/story-time`), Designer (`/look-and-feel`) |
| **Status** | `approved` |
| **Date** | 2026-09-01 |
| **Ticket** | `none` |

**Handoff**
- **Status:** `approved` — approved by the user on 2026-09-01, after /look-and-feel filled §8 Design Review.
- **Summary:** rendering-core can blit hand-typed sprites; nothing can put a single word or number on screen without hand-authoring a bespoke pixel array per string. Deliver a hand-authored 8×8 monospace bitmap font for BRASS-01's on-device character set — 40 characters demonstrably needed (grounded in README's own committed copy) plus a deliberate 3-character margin (`:`, `!`, `?`) the user chose knowingly — plus a `drawText` composed entirely from existing sprite/blit primitives, proven with a pixel-exact fixture engineered against the wrong-glyph/offset/advance bug classes the last two reviews kept finding as false-green gates.
- **Open:** `none`. Round 1's five flagged judgment calls are resolved: four confirmed as recommended, one overridden (character set expanded 40→43, C1). Round 2 Clarify pass (this revision) found no new product-scope questions — everything found was resolved directly below (C8–C11).
- **Binding ruling:** §4 User Stories US-1…US-3; §7 Clarifications for what was decided and why.
- **On conflict:** the numbered body below wins for everything except `Status`; log the mismatch as a finding at the next `/peer-review` and proceed.

## 1. Problem & Goal

- **Problem:** Every sprite that exists today is a hand-typed `constexpr Color[]` array (rendering-core's own pattern). That doesn't scale to text: "SYSTEM READY" would need its own bespoke 12-character-wide sprite, and a numeric score would need ad hoc per-digit sprites invented on the spot by whichever game needs a number first. README's own module map (*Software-Architektur*) names `text` as a graphics responsibility alongside framebuffer/sprites; the Phase 2 checklist lists "Text Rendering" unchecked directly after the two just-shipped items. Nothing in Phase 2 — boot sequence, main menu, any game's score — can show a letter or digit without this.
- **Goal:** A minimal way to draw fixed-width text into the framebuffer, built entirely on rendering-core's existing `Sprite`/stride/`blit` contract (no new low-level `Framebuffer` primitive), covering the character set BRASS-01's own already-written on-device copy demonstrably needs, plus a small, explicitly bounded margin — verified pixel-exact against a fixture specifically engineered to fail on a wrong-glyph, wrong-atlas-offset or wrong-advance bug, not just "text roughly appeared."
- **Success signal:** one documented host test command draws a fixture exercising **every one of the 43 defined characters** through the real `drawText` API and asserts it byte-identical, pixel by pixel, against an independently pre-computed expected framebuffer — 0 mismatches. The same fixture is also dumped and decoded through the existing `make view` pipeline (framebuffer-viewer, done) so a human confirms it actually reads as the intended string, not only that the assertion passed.
- **Why now:** named as rendering-core's own recommended next story (§6 there); the sprite descriptor's stride field was deliberately built so a font atlas needs no new blitting code. Cheapest moment to build it — before any game, or the not-yet-built boot/menu/highscore stories, invents its own one-off text hack.

## 2. Target Users

- **Engine developer (primary, today):** the solo maintainer — needs to put a string on screen without hand-drawing it pixel by pixel.
- **Game-module author (primary, near future, same person):** will call `drawText` from `render()` for score/UI text once games exist.
- **Boot screen / main menu / highscore stories (consumers, future, not built):** will need this capability, but this story is *not* scoped to their unwritten implementation details — its base character set is grounded only in their already-committed README copy (see A3), never in a requirement those future stories haven't stated yet.
- *Not a user of this feature:* the console player. No display driver exists yet; nothing here reaches a panel.

## 3. Assumptions & Open Questions

| # | Assumption / Question | Resolution |
|---|---|---|
| A1 | The idea arrived partly as a solution ("8×8-Bitmap-Font als C++-Array", "`drawText(x, y, "STRING", color)`"). Treated as a starting constraint, same posture as both prior specs' A1 — literal function name/signature is a `/sprint-plan` decision; only *behaviour* is fixed here. | Accepted |
| A2 | Everything is verifiable with the host toolchain (constitution §4): no board, no ESP-IDF, no cmake required for any AC. | Accepted |
| A3 | **Character set is 43 characters, in two explicit tiers.** **Tier 1 (40, demonstrably needed):** every character that already appears in README's own on-device UI copy (*Boot Experience*, *Main Menu*, *Highscore-System* — the sections constitution §2 calls the product's single-locale English UI) — `A`–`Z`, `0`–`9`, space, `.`, `-`, `>` (e.g. `.` from "INITIALIZING...", `-` from "BRASS-01", `>` from the menu cursor). **Tier 2 (3, a deliberate margin):** `:`, `!`, `?` — added on the user's explicit instruction, overriding the PO's initial recommendation to hold at 40 (round-1 C1). Not scoped against the highscore/boot/menu systems' *implementation* (none exist yet) — Tier 1 against already-committed text, Tier 2 as a knowing, bounded exception (NFR-6). Extending the set further later is a one-file edit, not a format/pipeline change. | **Overridden by user** (C1) |
| A4 | **No image/PNG-to-font-atlas importer tool this story.** The font is a hand-authored `constexpr` pixel array, same pattern as every existing sprite fixture. An importer is a separate asset-pipeline concern (file format, error handling, CLI design) with no dependency in either direction on `drawText` working — it earns its own future story. | Confirmed (C2) |
| A5 | **Glyph cell is a fixed 8×8-pixel monospace cell with one fixed, documented per-character advance** (no proportional spacing, no kerning) — inherited from A1, locked now. Whether 8×8 *reads* as "große Pixel" under constitution §6 at the final display scale is a legibility judgment left to `/look-and-feel` (§8), same posture framebuffer-viewer used for palette hex (its C7). | Confirmed (C3) |
| A6 | **No multi-line, no word-wrap, no alignment.** `drawText` draws left-to-right from one `(x, y)` origin. A literal `\n` or any character outside the 43-set (A3) is treated identically — one deterministic placeholder glyph, never a line break, a skip, or undefined behaviour, whether it appears alone or as the entire string. | Confirmed (C4) |
| A7 | **Caller supplies one ink `Color` per call**, from the existing 4-value palette; "off" pixels of every glyph cell are left untouched (transparent), mirroring `blit`'s existing transparent-by-default convention. No background fill, no per-character colour, no gradient. | Confirmed (C5) |
| A8 | The font asset and `drawText` add **exactly one new public entry point plus the font data itself** — no new low-level `Framebuffer`/`Sprite` method. `drawText` is read via the existing stride mechanism, composed from primitives that already exist. | Accepted |
| A9 | Verification of the visual result reuses the existing dump/PNG viewer (framebuffer-viewer, done, `make view`) — no new tool, no new file format. | Accepted |
| A10 | Threading, allocation and coordinate-overflow contracts are unchanged from rendering-core (A10/A11 there): single-threaded, zero dynamic allocation, signed-32-bit overflow-safe coordinates. | Accepted |
| A11 | Constitution §6's "persisted data carries a format version" rule does **not** apply to the font asset — it is compiled into the firmware image as `constexpr` data, never written to or read from Flash at runtime. That rule governs highscores/settings (data that can be corrupted on-device); a compiled constant can't be. | Accepted (C10) |
| A12 | String length is bounded by realistic usage (on-screen text at most a few hundred characters). Defending `drawText` against a pathologically long, adversarial input whose character-index arithmetic could overflow is not a goal — consistent with the engine's offline, non-adversarial-input threat model (constitution §2). | Accepted (C11) |

## 4. User Stories

### US-1 (Must): A hand-authored 8×8 glyph set for the on-device character set

> As the engine developer, I want pixel data for BRASS-01's character set — the 40 characters its own boot/menu/highscore copy uses, plus a deliberate 3-character margin — so that every later text-drawing feature reads from one hand-authored, statically-linked source instead of each game inventing its own letters.

**Acceptance criteria:**

- [ ] AC-1.1: Given the font asset, when inspected, then it defines pixel data for exactly the 43 characters of A3 — space, `.`, `-`, `>`, `:`, `!`, `?`, `0`–`9`, `A`–`Z` — every one present, no other character defined.
- [ ] AC-1.2: Given any two characters in the defined set, when their authored 8×8 bitmaps are compared, then no two are bit-for-bit identical — a wrong-glyph-selected bug can never hide behind two characters that coincidentally look the same to the test. This explicitly includes the visually similar dot-based marks `.`, `:` and `!` — each must be distinguishable pixel-for-pixel, not merely "different enough by eye."
- [ ] AC-1.3: Given the font asset, when a glyph is retrieved, then it comes back through the existing `Sprite`/stride descriptor mechanism (rendering-core A9) with no new low-level `Framebuffer` or `Sprite` primitive added.
- [ ] AC-1.4: Given the compiled asset, when checked, then it is a compile-time-fixed, statically allocated array (grep gate: no `new`/`malloc`) — 43 × 8×8 × 1 byte ≈ 2.75 KB (NFR-2).
- [ ] AC-1.5: Given the space glyph, when inspected, then all 64 of its pixels are the background/transparent value — space is a true blank cell, never an accidental copy of another glyph.

### US-2 (Must): `drawText` renders a fixed-width string into the framebuffer

> As a game-module author, I want to draw a null-terminated ASCII string at a position with one ink colour, so that I can show a label, a status line or a score without writing per-character blit calls myself.

*Advance, clipping and transparency behave like the existing sprite/rect primitives (A5–A7); no new failure mode is invented.*

**Acceptance criteria:**

- [ ] AC-2.1: Given a cleared framebuffer, when a single defined character is drawn at `(x, y)` with a chosen ink colour, then exactly that character's authored bitmap appears at `(x, y)` in that colour, every "off" pixel of the cell and every pixel outside it is unchanged.
- [ ] AC-2.2: Given a string of N defined characters drawn from one `(x, y)` origin, when read back, then character *i*'s cell occupies exactly `[x + i·advance, x + i·advance + 8)` horizontally, using the one fixed, documented advance constant — no overlap, no unintended gap.
- [ ] AC-2.3: Given a string containing a character outside the 43-set — including a lowercase letter and a control character such as `\n` — when drawn, then that cell renders the one deterministic placeholder glyph (distinct from every defined glyph per AC-1.2) rather than crashing, silently skipping the cell, or reading past the glyph table.
- [ ] AC-2.4: Given a string that runs off the left, right, top or bottom edge, or is entirely off-screen, when drawn, then only the on-screen portion of each affected cell is written (nothing, in the fully-off-screen case), no off-screen pixel is touched, and the suite reports no ASan out-of-bounds finding — mirrors rendering-core AC-3.3.
- [ ] AC-2.5: Given an empty string, when drawn, then the framebuffer is unchanged.
- [ ] AC-2.6: Given the same string drawn twice from the same starting state, when compared, then the two results are byte-identical.
- [ ] AC-2.7: Given the same string drawn twice with two different ink colours over the same background, when compared, then only the "on" pixels differ (each matching its requested colour) and every "off"/background pixel is identical in both.
- [ ] AC-2.8: Given a string composed **entirely** of characters outside the defined set, when drawn, then every cell renders the placeholder glyph at its correct fixed-advance position — the whole-string case is exercised, not only a single embedded unsupported character (AC-2.3).

### US-3 (Must): A pixel-exact, non-uniform fixture that would actually catch a wrong-glyph bug

> As the engine developer, I want the text fixture engineered so that a wrong-glyph-selected bug, a font-atlas-offset bug, or a wrong-advance bug each produce a failing test — not a coincidentally-similar-looking pass — and I want the result visible, not just asserted.

*Why this needs its own story, not just an AC of US-2: both prior reviews' worst findings (rendering-core F3, F1/F11; framebuffer-viewer F12) were a uniform test pattern that a real bug could hide behind, or a gate that reported success without actually checking. Text rendering adds a genuinely new failure surface — a lookup-index bug, a glyph-atlas-offset bug, a per-character-advance bug — that a string like `"AAAA"` or a single-glyph test would not catch: a swapped index still "looks like text was drawn somewhere." Adding the 3 visually-similar margin punctuation marks (`:`, `!`, `?`, Tier 2 of A3) sharpens this risk further — a `.`/`:` mix-up is exactly the kind of "coincidentally similar-looking" bug this fixture must not let slide.*

**Acceptance criteria:**

- [ ] AC-3.1: Given a fixture string that is a **permutation containing every one of the 43 defined characters exactly once** — so US-1's entire glyph set is exercised, not a sample — arranged so the two dot-based punctuation marks (`.`, `:`) are adjacent at least once, and whose first and last characters are whichever characters the font asset's **own internal storage** places first and last (not an order this spec dictates, so the test genuinely probes the real data structure's extremes, not just a prose list's), when it is drawn through the real `drawText` API into a cleared framebuffer, then the **entire** resulting framebuffer is compared, pixel by pixel, against an **independently pre-computed** expected framebuffer (each glyph's authored bitmap placed at its expected fixed-advance offset, computed by the test, not copied from `drawText`'s own output) — 0 mismatches, and a single wrong pixel fails the test and names its coordinate. Because no two characters share a bitmap (AC-1.2) and every character is exercised, this structurally cannot pass under a wrong-glyph-selected bug, a neighbour-glyph-bleed bug, or a wrong-advance bug — each shifts or substitutes at least one pixel, and the comparison covers the whole buffer.
- [ ] AC-3.2: Given the fixture's framebuffer from AC-3.1, when it is serialised with the existing dump writer and decoded to PNG via `tools/fb_view.py` / `make view` (framebuffer-viewer, done), then a PNG is produced showing the fixture string legibly, with no new tool, file format, or public symbol added by this story — reusing the established viewer end-to-end, so "does this actually look like the string" is checked with eyes as well as asserted numerically.

## 5. Non-Functional Requirements

| # | Category | Requirement (measurable) | How it's verified |
|---|---|---|---|
| NFR-1 | Performance | Drawing a full 240×160 screen of text (30×20 = 600 characters, the maximum this virtual resolution can hold) completes in < 5 ms with `-O2` on the reference host (Apple clang 14). | host benchmark test + `/peer-review` |
| NFR-2 | Reliability / memory | Zero dynamic allocation (grep gate, unchanged rule from rendering-core NFR-2). Font asset is a static array, budgeted at ≈2.75 KB / 2,752 bytes (43 glyphs, AC-1.4). | `/peer-review` (grep) |
| NFR-3 | Reliability / bounds | Full suite runs under `-fsanitize=address,undefined` with zero findings, including every off-screen, out-of-set-character (single and whole-string, AC-2.3/AC-2.8) and extreme-coordinate case. | rendering-core AC-1.3 pattern |
| NFR-4 | Portability / toolchain | Compiles clean under `clang++` and `g++`, `-std=c++17 -Wall -Wextra -Werror`; no ESP-IDF header; no literal `8` (glyph cell), `43` (character count) or the advance constant outside one constants header. | `/peer-review` (grep + both compilers) |
| NFR-5 | Determinism | Same string + same initial state → byte-identical framebuffer, both compilers; no wall-clock/unseeded-random read. | AC-2.6 + `/peer-review` |
| NFR-6 | **Library lens — public API surface** | Exactly one new public entry point (`drawText`) plus the font asset; no font-selection parameter (one font exists), no measure-text/alignment API (no caller, A6). **3 of the 43 defined glyphs (`:`, `!`, `?`) have no current textual consumer** — a deliberate, user-confirmed, explicitly bounded exception to this rule (A3 Tier 2), not a precedent for further speculative additions (§6). Any other new public symbol, or any character added beyond this named exception, is a review finding. | `/peer-review` |
| NFR-7 | **Library lens — contract clarity** | Doc comment states: the 43-character set, its two tiers and where each is defined (A3), the fixed 8×8 cell/advance (A5), placeholder-glyph behaviour for any other input including `\n` and the whole-string case (A6, AC-2.8), clipping behaviour (inherits `blit`), the ink-colour/transparent-background contract (A7), null-`char*` precondition, and the inherited single-threaded/no-throw contract (A10). One usage example. | `/peer-review` |
| NFR-8 | Observability | N/A — pure in-memory drawing, no runtime failure mode, no logging surface (same posture as rendering-core NFR-8). | — |
| NFR-9 | Security & privacy | N/A — offline device, no input, no persistence (constitution §2); font data is compile-time only (A11). | — |
| NFR-10 | Accessibility | Glyph legibility at the 8×8 cell size / eventual ×2 display scale is a Design Review judgment (§8), not a WCAG check — no interactive surface exists yet. Deferred to `/look-and-feel`, same posture as A5/C3. | `/look-and-feel` |
| NFR-11 | Integrations & dependencies | N/A — no external system, library, or new dependency; the only "integration" is the already-built dump/PNG viewer, reused unchanged (A9). | — |

*Lens note: `library` active **scoped** (constitution §2); semver/packaging are no-ops. Public API surface and Contract clarity land as NFR-6/NFR-7.*

## 6. Out of Scope

- **PNG/image-to-font-atlas importer tool.** Its own future story — file format, error handling and CLI design that don't block `drawText` from working today (A4).
- **Lowercase letters and any punctuation beyond the 43-character set** (A3). The 3-character margin (`:`, `!`, `?`) is a named, bounded, user-confirmed exception — not an invitation for further speculative additions (NFR-6). Extending the set is cheap (one array edit) when a real consumer names a real need.
- **Multi-line rendering, word-wrap, `\n` as a line break, text alignment/centering, a measure-text/width-query API.** No current caller for any of them (A6, NFR-6).
- **Proportional spacing / kerning.** Fixed monospace advance only (A5).
- **Multiple font support or a font-selection parameter.** One font exists; a selector with one option is the NFR-6 violation this story must not create.
- **Per-character colour, colour gradients, opaque/background-filled text** (e.g. a highlighted selection box behind menu text). One ink colour, transparent background, per call (A7).
- **Any actual boot screen, main menu, or highscore screen.** Future consumers of this capability, not part of it — deliberately not built here (§2).
- **ILI9488 driver, display of text on real hardware.** Unchanged from both prior stories — no board wired, no ESP-IDF installed.
- **Any change to the existing `Framebuffer`/`Sprite`/`DirtyTracker` public surface.** This story only adds; it touches nothing rendering-core shipped (e5d4be3).
- **Defending against pathologically long / adversarial input strings** (A12) — the device is offline with no adversarial input path (constitution §2).

## 7. Clarifications

| # | Date | Question | Resolution |
|---|---|---|---|
| C1 | 2026-09-01 | What character set is "enough" for a first cut, without binding to the not-yet-built highscore/boot/menu systems? | PO recommended 40, grounded strictly in README's committed copy. **User overrode: add a deliberate 3-character margin (`:`, `!`, `?`) → 43 total.** The 40 stay demonstrably needed; the 3 are recorded as a knowing, bounded exception to "no consumer-less symbol" (A3, NFR-6), not scope creep. |
| C2 | 2026-09-01 | Does an image/PNG-to-font-atlas importer belong in this story? | **No — confirmed.** Hand-authored `constexpr` array, same pattern as every sprite fixture so far; the importer is a separate asset-pipeline story with its own scope. → A4. |
| C3 | 2026-09-01 | Is 8×8 fixed now, and who judges whether it reads as "große Pixel"? | **Confirmed: cell size fixed at 8×8 now** (inherited from the idea, A1); the *legibility* judgment is `/look-and-feel`'s, not re-litigated here. → A5, NFR-10, §8. |
| C4 | 2026-09-01 | Multi-line, word-wrap, alignment in v1? | **Confirmed: none of them** — no named caller for any (A6); an alignment feature with no consumer is exactly the NFR-6 concern raised against this idea. |
| C5 | 2026-09-01 | Does `drawText` take a per-call ink colour, or is it baked into the font asset? | **Confirmed: per-call `Color` argument**, mirroring every other drawing primitive's existing signature (`clear`, `fillRect`, `blit`'s transparent param). → A7. |
| C6 | 2026-09-01 | Half-size version, if the story must shrink? | **US-1 + US-2 alone** still deliver core value. If even that's too much, narrow A3's Tier 2 first (drop `:`/`!`/`?`, back to the demonstrably-needed 40) — **US-3's fixture rigor is the last thing cut**, not the first. |
| C7 | 2026-09-01 | Out-of-set-character behaviour (lowercase, `\n`, other control chars)? | **Confirmed: one deterministic placeholder glyph, uniformly** — never a crash, a silent skip, or a line-break side effect, whether it's one character or the whole string. → A6, AC-2.3, AC-2.8. |
| C8 | 2026-09-01 (round 2) | US-3's original wording picked fixture "boundary" characters from A3's *prose list* order — but if the font's actual storage order (array/atlas layout, a `/sprint-plan`/implementation decision) differs, testing the prose-list extremes wouldn't exercise the real data structure's edges, weakening exactly the atlas-offset-bug coverage this story exists for. | **Fixed directly:** AC-3.1 now targets whichever characters the font asset's own internal storage places first/last, not A3's documentation order — and covers all 43 characters via full permutation rather than sampled "boundary" picks, so the ambiguity is moot either way. |
| C9 | 2026-09-01 (round 2) | With 43 characters, is "at least one letter/digit/punctuation" still a strong enough fixture, given 3 of the 6 punctuation marks are now visually similar dot-based glyphs (`.`, `:`, `!`)? | **Strengthened:** AC-3.1 now requires a full 43-character permutation (every glyph exercised, not a sample) with `.` and `:` placed adjacent at least once — directly targeting the "coincidentally similar-looking" bug class named in the original ask. |
| C10 | 2026-09-01 (round 2) | Does the font asset need a persisted-data format version per constitution §6? | **No** — it's compiled into the firmware image, never written to or read from Flash at runtime; §6's rule targets on-device-corruptible data (highscores/settings), not compile-time constants. → A11. |
| C11 | 2026-09-01 (round 2) | Must `drawText` defend against a pathologically long input whose per-character index arithmetic could overflow? | **No** — out of scope, consistent with the offline, non-adversarial-input threat model; ordinary on-screen-length strings are the only case this story defends (constitution §2). → A12, §6. |

## 8. Design Review

- **Overall impression:** Good. This is the first story with a genuine
  glyph-design judgment call (A5/C3 explicitly left cell size to this
  review), and the underlying decisions hold up under the maths, not just by
  eye. 8×8 is confirmed as the right cell size for this virtual resolution
  and this aesthetic (below). The punctuation-confusability risk US-3/AC-1.2
  already names is real but adequately fenced by the fixture's *bit-level*
  distinctness check — it is not, on its own, a *perceptual* distinctness
  guarantee, so this review adds concrete dot-position guidance for whoever
  authors the 43 bitmaps (informs implementation, does not reopen the spec).
  All-caps-only is confirmed as coherent with the platform's own committed
  voice, not a hidden limitation. No blocker found; one Minor guidance item
  recorded for the doc comment (NFR-7), one heads-up flagged to the PO as a
  question for a future story, not a required change here.

- **Glyph cell size — confirmed 8×8 (resolves A5/C3, binds NFR-10):**

  At 240×160 this gives a 30-column × 20-row text grid. Checked directly
  against the README's own committed boot/menu copy this story is scoped
  against (A3):

  | Line (README, verbatim) | Chars | Width @ 8px | % of 240px |
  |---|---|---|---|
  | `AETHER COMPUTING SYSTEM` | 24 | 192px | 80% |
  | `MEMORY ........ OK` | 19 | 152px | 63% |
  | `> GALACTIC INVASION` (menu cursor + longest game title) | 19 | 152px | 63% |
  | `1. AND   12500` (highscore row) | 15 | 120px | 50% |
  | `SYSTEM READY` | 12 | 96px | 40% |
  | `BRASS-01` | 8 | 64px | 27% |

  Every real string this story is grounded in fits comfortably inside 30
  columns, most with 40–50% of the line to spare for margins. 8×8 is also
  not an arbitrary pick against the platform's own genre reference: it is
  the classic tile-font cell size used across the actual early-80s arcade
  hardware this platform explicitly emulates the *feel* of (Pac-Man,
  Galaga-class high-score/status text), at a comparable order-of-magnitude
  virtual resolution. Against constitution §6's "große Pixel, klare
  Silhouetten, hohe Lesbarkeit" this reads as period-correct chunkiness, not
  as fine-grained modern type crammed into a retro shell. **Verdict: 8×8
  confirmed, no change requested.**

  Two things worth flagging as forward-looking observations, **not required
  changes to this spec** (multi-line layout, alignment and line spacing are
  explicitly out of scope here, A6/§6 — they belong to the boot/menu
  consumer story):
  - `AETHER COMPUTING SYSTEM` at 80% of line width leaves only ~3 characters
    of margin on a 240px-wide screen. Fine for a single centered line; a
    future boot-screen story should be aware there is very little slack if
    that string is ever lengthened.
  - 20 rows at a flush 8px pitch fill all 160px height with **zero**
    interline gap if a future caller places `drawText` calls back-to-back
    every 8px. The README's own boot mockup uses blank lines between groups
    (status block, `SYSTEM READY`, `PRESS START`) — a future consumer will
    need to budget extra vertical spacing manually (e.g. 10–12px pitch),
    since `drawText` itself has no leading/line-height concept (correctly,
    per A6). Flagging this as a question back to the PO for whichever story
    builds the boot screen — not something to add here.

- **Punctuation disambiguation — guidance for the T-tasks authoring the 43
  bitmaps (binds AC-1.2/AC-1.4, informs NFR-7's doc comment):**

  AC-1.2 only guarantees the 43 glyphs are pairwise **bit-for-bit**
  different — a font where `.` and `:` differ by a single stray pixel would
  legally pass that AC while still being genuinely hard to tell apart by
  eye at 8×8 with no anti-aliasing. That is exactly the "coincidentally
  similar-looking" failure class US-3/C9 was written to catch, so it is
  worth pinning the *shape* rule, not just the *test* rule, before a glyph
  gets hand-authored:

  - **`.` (period):** exactly one small ink block, placed at the very
    **bottom** of the cell (last row or two), horizontally centered.
    Baseline-anchored, nothing above it.
  - **`:` (colon):** exactly two small ink blocks of the same shape as the
    period, stacked vertically but placed in the cell's **middle band**
    (roughly rows 2–3 and rows 5–6), with a visible gap row between them and
    — critically — **not** touching the bottom row the way `.` does. The
    distinguishing cue is vertical position (mid-height, two marks) versus
    baseline (one mark), not just "two dots vs one."
  - **`!` (exclamation):** a **continuous vertical stroke** (not a dot)
    running most of the cell's height (e.g. rows 0–5), then a gap, then one
    small dot at the bottom row. The stroke — a tall single shape, not a
    cluster of small blocks — is what must read differently from `:`'s two
    discrete dots at a glance, not merely "has more ink."
  - **`-` (hyphen):** a single **horizontal** bar, one row thick, in the
    cell's vertical middle, spanning most of the cell's width. Being the
    only glyph in the set with a wide horizontal stroke and no vertical or
    dot component, it is structurally distinct from all three dot-based
    marks above and low-risk — flagged only for completeness.
  - **`?` (question mark):** ensure its terminal dot sits at the same
    baseline position as `.`'s, with the hook/curve occupying the upper
    rows — the dot is fine to share a position with `.`'s because the
    curve above it is what disambiguates the two; nothing else in the set
    has a curved stroke to confuse it with.

  **Recommended action (Minor, non-blocking):** fold these five bullet
  points into the font's doc comment (NFR-7 already requires one usage
  example and a description of the character set — this is a natural
  addition, not new scope) so a future glyph edit doesn't accidentally
  reintroduce an ambiguous shape that would still pass AC-1.2's bit-check.

- **All-caps-only — confirmed defensible, not a hidden limitation:** every
  piece of on-device copy this story is scoped against (A3) — `BRASS-01`,
  `AETHER COMPUTING SYSTEM`, `SYSTEM READY`, `PRESS START`, all five game
  titles, `HIGH SCORES`, `SETTINGS`, and the highscore initials (`AND`,
  `MAX`, `EVA`...) — is *already* all-caps in the README's own committed
  copy. This isn't the font imposing a constraint the product doesn't
  already have; it's the font matching a voice the product already
  committed to. It's also period-authentic: real early-80s arcade cabinets
  (the genre this platform names explicitly) were commonly uppercase-only
  for the same reason (character-ROM cost), so this reads as intentional
  retro character, not corner-cutting. §6's own text ("extending the set is
  cheap... when a real consumer names a real need") already frames this as
  revisitable, not a permanent wall — so no additional hedge is needed here.
  **Verdict: confirmed, no change requested.**

- **Heuristics findings:** No new findings beyond the two guidance items
  above. One craft note on the placeholder glyph (AC-2.3/AC-2.8, C7): its
  exact bitmap is an implementation detail this spec correctly leaves open,
  but for recognizability it should read unambiguously as "undefined
  character" — a solid block or checkerboard fill (the common bitmap-font
  "tofu" convention) is a safer choice than any shape that could be mistaken
  for a real glyph, since AC-1.2 only guarantees it differs from the 43 real
  glyphs bit-for-bit, not that it reads as "this wasn't a real letter" at a
  glance. Same posture as the punctuation guidance above: recorded here for
  whoever authors it, not a spec change.

- **Accessibility notes:** N/A, confirmed — same posture as NFR-10 already
  states and framebuffer-viewer's §8 established for this project: no
  interactive surface exists yet (no keyboard path, no focus order, no
  input to label), so WCAG contrast/keyboard checks don't apply to a
  compiled glyph table. The one accessibility-*adjacent* property that
  genuinely applies here — can the punctuation marks be told apart at a
  glance — is a visual-craft question, and is covered above as the
  punctuation disambiguation guidance, not restated as a WCAG finding.

- **Design risks & required changes:** None blocking; this spec may proceed
  to `/sprint-plan`. Two non-blocking items carried forward from above:
  1. **Minor, actionable now:** add the five-glyph disambiguation rules
     above to the font's doc comment when it's authored (NFR-7).
  2. **Informational, for the PO, not a scope addition:** whichever future
     story builds the boot/menu screen will need to hand-manage vertical
     line spacing itself (8×8 cells have no built-in leading, correctly per
     A6) — flagged here so it isn't rediscovered as a surprise later.

---

## ✅ SPEC GATE

*All boxes checked → `/sprint-plan` may start. Any box open → back to `/story-time` or `/look-and-feel`.*

- [x] Problem, goal and success signal are concrete (no buzzwords, no "everyone")
- [x] Every story has testable Given/When/Then acceptance criteria
- [x] Stories are prioritized (MoSCoW) and at least one is a Must
- [x] Non-functional requirements are stated and measurable (or marked N/A with reason)
- [x] Clarify pass done: full taxonomy swept across two rounds; C1–C7 then C8–C11 resolved (character-set override folded in, fixture strengthened to a full 43-glyph permutation, storage-order ambiguity fixed, persisted-data and long-input non-goals recorded)
- [x] Open questions are resolved or explicitly accepted as risk — 0 open
- [x] Out-of-scope section is filled (something was consciously cut)
- [x] Constitution (`.spark/constitution.md`) respected — no conflict found; the 3-character margin is a named, bounded, user-confirmed exception to the library lens's "no consumer-less symbol" concern, not a silent gap
- [x] Design review done for UI-facing features (or marked N/A with reason) — completed via `/look-and-feel`: 8×8 cell confirmed (§8), punctuation disambiguation guidance recorded for NFR-7's doc comment, all-caps-only confirmed coherent with README's committed copy; no blocker, no spec change required
- [x] Line budget respected: Ist 199 / Soll ~250 (excluding HTML comments)
- [x] Status set to `approved` by the user — approved 2026-09-01
