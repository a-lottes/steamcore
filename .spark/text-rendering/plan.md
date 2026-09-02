# Plan: text-rendering

| | |
|---|---|
| **Phase** | Plan |
| **Owner** | Engineering Manager (`/sprint-plan`) |
| **Input** | `.spark/text-rendering/spec.md` (`approved`) |
| **Status** | `approved` |
| **Date** | 2026-09-01 |

**Handoff**
- **Status:** `approved` — approved by the user on 2026-09-01 in the `/sprint-plan` session.
- **Summary:** Two new modules — `font` (a compile-time-validated ASCII-art glyph table compiled into one `constexpr Color` atlas, retrieved as a `Sprite`) and `text` (a free `drawText` that recolours each glyph cell into a 64-byte stack buffer and hands it to the existing `Framebuffer::blit`). No new `Framebuffer`/`Sprite` primitive, no new Makefile target.
- **Open:** none — all 10 tasks `done`. `make test-all` green from a clean checkout: 71 C++ tests + 15 Python tests + 2 standalone round-trip tests, sips OK, both NFR-1 benchmarks OK, lint OK (incl. new glyph-metric-literal rule). One deviation from plan found and fixed during T8: incremental glyph authoring across T4-T6 (each task appending its batch at the end) produced insertion-order storage, not the ASCII-ascending order plan §1 Decision 4 specifies -- glyphCharAt(42) was '?' instead of 'Z'. Reordered kGlyphArt to true ASCII-ascending order to match the approved architecture, rather than silently deviating or changing the test to fit the accident. Ready for /peer-review.
- **Binding ruling:** §3 Task Breakdown for current task status; a plan revision after review/QA findings updates §1/§3 in place, never a new section
- **On conflict:** the numbered body below wins for everything except `Status`; log the mismatch as a finding at the next `/peer-review` and proceed — don't stop on it.

## 1. Architecture Decision

- **Context:** rendering-core shipped `Sprite{pixels, width, height, stride}` and a clipped
  `Framebuffer::blit` explicitly so a font atlas would need no new blitting code (spec A8, AC-1.3).
  What is missing is (a) 43 hand-authored 8×8 bitmaps that must be pairwise bit-distinct and visually
  unambiguous for `.`/`:`/`!` (§8), (b) a per-call *ink colour* — which `blit` cannot express, because
  `blit` copies whatever colour the source holds, and (c) a fixture whose failure mode is a red test,
  not a plausible-looking picture. The two prior increments' review rounds were dominated by one bug
  class: a gate that reports success without having verified its claim. Every gate this plan adds is
  therefore required to be demonstrated failing once, on purpose, before it counts as done.

- **Decision:**
  1. **`include/steamcore/font.h` + `src/font.cpp`** (a normal `.h`/`.cpp` pair, not a POD header like
     `sprite.h`, because there is real out-of-line data and a lookup). The header exposes only the
     metrics `kGlyphWidth`/`kGlyphHeight`/`kGlyphAdvance`/`kGlyphCount`, `Sprite glyphFor(char)` and
     `char glyphCharAt(int32_t)`. The pixel array itself stays private in `font.cpp` — a caller can
     retrieve a glyph, never index the atlas.
  2. **Glyphs are authored as ASCII art, not as `Color` literals.** One table in `font.cpp`,
     `struct GlyphArt { char ch; const char* rows[kGlyphHeight]; }`, each entry 8 rows of `' '`/`'#'`
     written out as the picture it draws. A `constexpr` builder converts the table into the atlas at
     compile time and **fails the build** on a row that is not exactly `kGlyphWidth` characters or
     contains any other character (a `throw` on an unreachable-if-valid path is not a constant
     expression, so the error surfaces as a compile error, not at runtime). This is the single
     mitigation for the plan's biggest risk: 44 hand-typed grids that a reviewer must verify by eye.
  3. **One table is the only source of truth.** The character order, the char→slot lookup table and
     `kGlyphCount`'s `static_assert` are all derived from `GlyphArt` at compile time — there is no
     second hand-maintained list that could disagree with the first.
  4. **Atlas layout: one flat `constexpr Color` array, glyph *i* at offset `i * kGlyphWidth *
     kGlyphHeight`, returned as `Sprite{&atlas[…], 8, 8, 8}`** — i.e. a vertical strip atlas of width
     8. Slot order is **ASCII ascending** (`' ' ! - . 0…9 : > ? A…Z`), so slot 0 is `' '` and slot 42
     is `'Z'`; the tofu placeholder is slot 43, outside the defined set and unreachable by
     `glyphCharAt`. `stride == width` here, which is the legal tightly-packed case `sprite.h`
     documents; the sub-rectangle-of-a-larger-array mechanism is still exactly what selects a glyph.
  5. **Lookup is a 256-entry `int8_t` table** indexed by `static_cast<unsigned char>(c)`, holding the
     slot index or the placeholder slot. Built at compile time from `GlyphArt` (decision 3), so it
     costs 256 bytes of `.rodata`, no branch and no duplication.
  6. **`drawText` is a free function** in `include/steamcore/text.h` + `src/text.cpp`, following
     `dump_format.h`'s "free function over the public `Framebuffer` API" pattern:
     `void drawText(Framebuffer& fb, int32_t x, int32_t y, const char* text, Color ink)`. For each
     character it copies the glyph, through its `Sprite`, into a 64-entry `Color` stack cell, writing
     `ink` for every `'#'` pixel and a **transparent key** for every blank one, then calls
     `fb.blit(cell, gx, y, key)`. The key is `Color::BLACK` normally and `Color::BRIGHT_ORANGE` when
     `ink` is `BLACK` — so black text draws real black pixels instead of vanishing (AC-2.7), using
     `blit`'s existing `transparent` parameter exactly as its doc comment describes. Glyph origins are
     computed in `int64_t` and a glyph whose cell lies entirely outside the screen is skipped, so
     extreme `x` values never overflow the `int32_t` handed to `blit` (AC-2.4, A12). `nullptr` text is
     a no-op, matching the framebuffer's "invalid input is ignored, never UB" contract. The
     `BLACK`-ink rule is confirmed as consistent with `blit`'s overridable `transparent` parameter,
     which rendering-core designed for exactly this black-over-lit case.
  7. **No new Makefile target.** `ENGINE_SRCS`/`TEST_SRCS` are wildcards, so the new sources join
     `test`, `test-asan`, `test-gcc` automatically. Three surgical edits are still needed: a second
     bench binary (NFR-1) run by the existing `bench` target, a `TEXT_DUMP` variable, and `view`
     decoding that dump as a second PNG (AC-3.2). The text dump lives in `build/`, is **not** committed
     — `reference_pattern.scfb` is committed because `tools/test_roundtrip.py` reads it with no C++
     build; nothing reads this one, it exists for `make view`'s human check, and an uncommitted
     artifact cannot go stale.

- **Alternatives considered:**
  | Alternative | Why rejected |
  |---|---|
  | `Framebuffer::drawText` as a method | Grows the low-level surface the spec (A8/AC-1.3) and rendering-core's own plan explicitly protect; text is composition, not a primitive |
  | Glyph literals as `Color kA[64] = {…}` arrays (dump_format_test.cpp's pattern) | 44 × 64 enum literals no reviewer can verify by eye, and a 63-entry typo silently zero-fills. The ASCII-art builder catches it at compile time |
  | Horizontal-strip atlas (352 px wide, `stride=352`) | Flexes `stride != width` prettily, but each source line then interleaves all 44 glyphs — it destroys exactly the by-eye reviewability decision 2 exists for |
  | 43 separate small arrays | 43 more symbols, no single order to derive the lookup and the fixture from, and AC-3.1's "the font's own storage order" would become undefined |
  | Four pre-recoloured atlases (one per palette colour) | 4× the data, and `BLACK` ink would still be invisible under `blit`'s default transparency |
  | New `blit` overload taking an ink override | A new low-level primitive — the one thing AC-1.3 and NFR-6 forbid |
  | `drawText` writing pixels via `setPixel` instead of `blit` | Legal under A8, but re-implements clipping that `blit` already gets right for the whole `int32_t` range; the 64-byte stack copy is cheaper than a second clipping implementation to review |
  | Linear scan or binary search over the sorted char table | Correct but slower and more code than a 256-byte table that is generated, not maintained |
  | Committing a second `.scfb` fixture next to `reference_pattern.scfb` | A second binary blob only `make view` reads, plus a stale-blob false-green risk the review rounds already punished once |
  | Hand-written fixture string trusted as-is | AC-3.1's permutation/extremes/adjacency properties would be prose. They are asserted at runtime against `glyphCharAt` instead (T8) |

- **Consequences:** Easier — adding a character is one `GlyphArt` entry plus bumping `kGlyphCount`;
  everything else (lookup, order, fixture coverage) follows automatically. Editing a glyph means
  editing a picture. Harder — the `constexpr` builder must compile under both Apple clang 14 and
  `g++`, which is why T1 proves it end-to-end before 40 more glyphs exist. `drawText` costs one
  64-byte copy per character (NFR-1 has ~4 orders of magnitude of headroom). Public surface grows by
  seven symbols rather than one; see §2 for the NFR-6 justification of each.

## 2. Affected Components

*No tool file was passed with this task, so no blast-radius query was run; the scope below was
determined by hand from the repository.*

- **New:** `firmware/steamcore/include/steamcore/font.h`, `firmware/steamcore/src/font.cpp`,
  `firmware/steamcore/include/steamcore/text.h`, `firmware/steamcore/src/text.cpp`,
  `firmware/steamcore/test/font_test.cpp`, `firmware/steamcore/test/text_test.cpp`,
  `firmware/steamcore/test/text_fixture_test.cpp`, `firmware/steamcore/test/bench_text.cpp`.
- **Modified:** `Makefile` (bench binary #2, `TEXT_DUMP`, `view`), `tools/check_constraints.sh`
  (NFR-4 glyph-metric literal check), `docs/host-tests.md` (what `bench` and `view` now do).
- **Untouched:** `framebuffer.{h,cpp}`, `sprite.h`, `color.h`, `config.h`, `dirty_tracker.*`,
  `dump_format.*`, `tools/fb_view.py`, `firmware/steamcore/test/fixtures/` (spec §6).
- **New dependencies:** none — no library, no tool, no Python package.
- **Public API surface added (NFR-6), each with its named consumer:** `drawText` (the one entry
  point); `kGlyphWidth`/`kGlyphHeight`/`kGlyphAdvance`/`kGlyphCount` (NFR-4 requires one constants
  home, and AC-2.2 is stated in terms of the advance constant); `glyphFor` (the font asset itself —
  the retrieval AC-1.3 names); `glyphCharAt` (AC-3.1 requires the fixture's extremes to come from the
  font's *own storage order*, which is unimplementable unless that order is queryable). No
  font-selection, measure-text or alignment symbol is added. This is the same NFR-6 reading
  framebuffer-viewer applied to `kDumpHeaderSize`/`kDumpFormatVersion` — the test suite is a named
  consumer, not a speculative extension point.

## 3. Task Breakdown

| # | Task | Story | Covers (AC / NFR) | Depends on | Status | Definition of Done |
|---|---|---|---|---|---|---|
| T1 | Font module skeleton: ASCII-art table, `constexpr` atlas builder, char→slot table, `glyphFor`/`glyphCharAt` | US-1 | AC-1.2, AC-1.3, AC-1.5, NFR-2, NFR-5 | – | `done` | `font.h` declares the four metrics and the two accessors; `font.cpp` holds one `GlyphArt` table, provisionally the 4 ASCII-ascending entries `' '`, `'.'`, `':'`, `'A'` plus the tofu slot, with `kGlyphCount == 4` and a `static_assert` tying it to the table's real length. The builder rejects a row that is not `kGlyphWidth` characters or contains anything but `' '`/`'#'`, and that rejection is demonstrated once with a deliberately broken row that fails to compile under both compilers, with the observed error quoted in the task note. `glyphFor` returns a valid `Sprite` for each defined char and the tofu slot for every other `char` value including negative ones. New `font_test.cpp` proves pairwise bit-distinctness across all slots, that `' '` is 64 transparent pixels, and the returned Sprite's width/height/stride/non-null pointer. `make test`, `make test-gcc`, `make test-asan`, `make lint` all green — files: firmware/steamcore/include/steamcore/font.h, firmware/steamcore/src/font.cpp, firmware/steamcore/test/font_test.cpp |
| T2 | `drawText` over `blit` with per-call ink, advance, clipping and placeholder | US-2 | AC-2.1…AC-2.8, NFR-3, NFR-5 | T1 | `done` | `text.h`/`text.cpp` add exactly one public function; `text_test.cpp` covers, against T1's 4-glyph set, each of AC-2.1 to AC-2.8 as its own named test case: single glyph placement, N-glyph advance boundaries read back at `x + i*kGlyphAdvance` and `+kGlyphWidth-1`, an unsupported char rendering the tofu slot, an all-unsupported string, all four edges plus fully-off-screen plus `x` near `INT32_MAX`, empty string, `nullptr` text, byte-identical redraw, and two inks over the same background where only "on" pixels differ. `make test-asan` reports zero findings for the whole file — files: firmware/steamcore/include/steamcore/text.h, firmware/steamcore/src/text.cpp, firmware/steamcore/test/text_test.cpp |
| T3 | Walking skeleton closed: dump a drawn string and view it as PNG | US-3 | AC-3.2 | T2 | `done` | `text_fixture_test.cpp` draws a short string with the 4 available glyphs, serialises it with the existing `serializeDump` and writes it to the path the new `TEXT_DUMP` Makefile variable names, passed to C++ as a `-D` macro and never spelled as a literal in source. `make view` decodes both dumps and produces `build/text_pattern.png` in one command from a clean checkout, and the PNG visibly shows the drawn string. No new Makefile target — files: firmware/steamcore/test/text_fixture_test.cpp, Makefile |
| T4 | Author `B`–`Z` | US-1 | AC-1.1, AC-1.2 | T1 | `done` | 25 further `GlyphArt` entries in ASCII-ascending position, `kGlyphCount` raised to 29; every glyph's art block is legible as its letter in the source; the T1 distinctness test now runs over 29 slots and passes; `make test`, `make test-gcc`, `make test-asan`, `make lint` green — files: firmware/steamcore/src/font.cpp, firmware/steamcore/include/steamcore/font.h |
| T5 | Author `0`–`9` | US-1 | AC-1.1, AC-1.2 | T4 | `done` | 10 further entries in ASCII-ascending position, `kGlyphCount` raised to 39, distinctness green over 39 slots, same four gates green — files: firmware/steamcore/src/font.cpp, firmware/steamcore/include/steamcore/font.h |
| T6 | Author `!`, `-`, `>`, `?` and the final tofu glyph, to the §8 shape rules, with an anchor table | US-1 | AC-1.1, AC-1.2, NFR-7 | T5 | `done` | `kGlyphCount` reaches 43. `.` is one baseline block, `:` two mid-band blocks clear of the bottom row, `!` a continuous vertical stroke plus a baseline dot, `-` a single mid-cell horizontal bar, `?` a curve over a baseline dot, tofu a checkerboard, each matching §8. `font_test.cpp` gains an anchor table in the dump-format.md sense: hand-written `(char, row, col) -> ink/blank` truths for `.`, `:`, `!`, `-`, `?`, tofu, `A` and `Z`, authored from the intended picture and not read back out of `glyphFor` while writing them — this is the independent oracle that a slot-offset bug cannot satisfy. All four gates green — files: firmware/steamcore/src/font.cpp, firmware/steamcore/include/steamcore/font.h, firmware/steamcore/test/font_test.cpp |
| T7 | Exact-set and budget assertions | US-1 | AC-1.1, AC-1.4, NFR-2 | T6 | `done` | A test walks all 256 `char` values and asserts that exactly the 43 characters of A3 map to a defined slot and every other value maps to the placeholder, and that `glyphCharAt` over `[0, kGlyphCount)` yields those same 43 characters with no repeat; a `static_assert` in `font.cpp` pins the atlas to `(kGlyphCount + 1) * kGlyphWidth * kGlyphHeight` bytes and the header documents the 2,752 + 64 + 256 byte budget; `make lint`'s allocation grep stays green — files: firmware/steamcore/test/font_test.cpp, firmware/steamcore/src/font.cpp, firmware/steamcore/include/steamcore/font.h |
| T8 | The 43-character pixel-exact fixture | US-3 | AC-3.1, AC-3.2, AC-2.2, NFR-5 | T3, T7 | `done` | The fixture is two hand-written row strings, 30 and 13 characters, drawn by two real `drawText` calls. Before comparing anything the test asserts the fixture's own properties against the font: total length equals `kGlyphCount`, every slot index occurs exactly once, the first character equals `glyphCharAt(0)`, the last equals `glyphCharAt(kGlyphCount - 1)`, and `.` and `:` are adjacent within one row — so a change to the storage order fails loudly instead of silently weakening the fixture. An expected framebuffer is then built by the test's own placement loop using `setPixel` and its own advance arithmetic, and all 240×160 pixels are compared, with the first mismatching coordinate and both colours named on failure and 0 mismatches required. For `.`, `:` and `!` the expected cells are additionally taken from the test's own hand-written art, not from `glyphFor`. The fixture dump replaces T3's placeholder string and `make view` renders it legibly — files: firmware/steamcore/test/text_fixture_test.cpp |
| T9 | NFR-1 benchmark | US-2 | NFR-1 | T7 | `done` | `bench_text.cpp` draws a full 30×20 screen of text and prints the measured milliseconds against the 5 ms budget, exiting non-zero when exceeded, in the style of `bench_dirty_scan.cpp`; the existing `bench` target builds and runs both bench binaries as separate recipe lines, both listed in the binary `.PHONY` block; a deliberately lowered budget is shown once to make `make bench` fail, proving the target is not merely printing — files: firmware/steamcore/test/bench_text.cpp, Makefile |
| T10 | NFR-4 lint rule, doc comments, docs | US-1, US-2 | NFR-4, NFR-6, NFR-7 | T8, T9 | `done` | `check_constraints.sh` gains a check that no glyph-metric literal `8` or `43` appears outside `font.h` in the text-rendering module set, defined as `font.cpp`, `text.h`, `text.cpp` and any file under `include/` or `src/` that includes either header, with `//` comment lines and single-quoted character literals stripped first so `'8'` in the glyph table is not a false positive; the scoping and its reason are commented in the script, and the check is demonstrated failing once against a temporarily inserted literal. `text.h`'s doc comment states the 43-character set and its two tiers, the 8×8 cell and advance, placeholder behaviour for any other input including `\n` and the whole-string case, clipping inherited from `blit`, the ink/transparent contract including the `BLACK`-ink key, the `nullptr` precondition, the single-threaded no-throw contract, and one usage example; `font.cpp` carries §8's five shape rules above the glyph table. `docs/host-tests.md` describes what `bench` and `view` now do. `make test-all` green from a clean checkout — files: tools/check_constraints.sh, firmware/steamcore/include/steamcore/text.h, firmware/steamcore/src/font.cpp, docs/host-tests.md |

## 4. Test Strategy

- **US-1 (font data) — unit, `font_test.cpp`.** Three layers, deliberately not one: the `constexpr`
  builder rejects malformed art at *compile* time (T1); a pairwise bit-distinctness loop over all
  slots covers AC-1.2 mechanically as the set grows (T1, re-run at T4/T5/T6); a hand-written **anchor
  table** covers AC-1.1's shapes independently (T6). The anchor table is load-bearing: every other
  test in this plan reaches glyph pixels through `glyphFor`, so only truths written from the intended
  picture can catch a slot-offset bug inside `glyphFor` itself. Distinctness is asserted at runtime
  rather than as a `static_assert` on purpose — a failing `static_assert` cannot name the offending
  pair, and naming it is the whole point.
- **US-2 (`drawText`) — unit, `text_test.cpp`.** One named case per AC-2.x, each reading pixels back
  through `Framebuffer::pixel()`. Off-screen and extreme-coordinate cases exist to be run under
  `make test-asan`, which is where AC-2.4/NFR-3 are actually proven.
- **US-3 (fixture) — integration, `text_fixture_test.cpp`.** Full-buffer comparison against an
  expected framebuffer the test computes itself. The chain of proof is: no two glyphs share a bitmap
  (AC-1.2) *and* every one of the 43 is drawn once *and* all 38,400 pixels are compared, so a wrong
  slot, a neighbouring-glyph bleed or a wrong advance must shift or substitute at least one compared
  pixel. The fixture's own AC-3.1 properties are asserted, not assumed. AC-3.2 is the same buffer
  through the shipped `serializeDump` → `fb_view.py` chain into `build/text_pattern.png`.
- **Deliberately not automated:** whether the rendered string is *legible* — that is the human check
  at `/demo-day` on the PNG `make view` produces, and §8's shape rules are the criteria. No display
  hardware exists (constitution §4), so nothing here is or may be reported as hardware-verified.
- **Gate integration:** zero new Makefile targets. `test`, `test-asan`, `test-gcc` pick the new
  sources up through the existing wildcards; `bench` gains a second binary as a second recipe line
  (separate lines, so a failure stops the recipe — the `;`-joined-recipe finding from the last
  review); `view` decodes a second dump; `lint` gains the NFR-4 rule. Every gate this plan adds —
  the compile-time art validator, the new lint rule, the new bench budget — is only accepted once it
  has been observed *failing* on purpose.

## 5. Risks & Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| 44 hand-authored 8×8 grids: typos, mirrored or shifted glyphs, a letter that reads as another | High — the defect lands in every string the console will ever draw | Glyphs are authored as pictures (D2), validated at compile time, split across three small tasks (T4/T5/T6), pinned by an anchor table, and finally looked at as a PNG |
| The `constexpr` art builder does not compile identically under Apple clang 14 and the ESP-IDF GCC | High — the font would need rewriting after all glyphs exist | T1 proves it under `make test` and `make test-gcc` with 4 glyphs, before T4 starts. Residual: `/usr/bin/g++` on this host is clang; real GCC coverage stays unverified and must be stated as such, not claimed |
| The expected framebuffer and `drawText` both read glyphs through `glyphFor`, so a bug *inside* `glyphFor` could satisfy both sides | High — precisely the false-green class the last two reviews kept finding | The T6 anchor table and the T8 hand-written cells for `.`/`:`/`!` are authored from the intended picture, independent of `glyphFor`; this is why T6 is not optional |
| `.` and `:` pass the bit-distinctness check yet still read alike at 8×8 | Medium — a real confusion the fixture would not fail on | §8's shape rules are binding on T6 (position, not just dot count) and repeated in the source above the table; final judgment is the `/demo-day` PNG |
| The new NFR-4 lint rule false-positives (`'8'` in the glyph table) or is scoped so narrowly it never fires | Medium — a tuned-to-pass gate is worse than no gate | Scope and stripping rules are specified in T10 and the rule must be demonstrated failing once against an inserted literal |
| `BLACK` ink over a lit background is a behaviour no existing primitive has | Low — but AC-2.7 would fail late | Settled in D6 by the transparent-key rule and covered by T2's AC-2.7 case with both a `BLACK` and a non-`BLACK` ink |
| Inherited assumption A12: no defence against pathologically long strings | Low — accepted by the spec | Glyph origins are computed in `int64_t` and off-screen cells are skipped, so ordinary and extreme *coordinates* are safe even though adversarial lengths are out of scope |
| Inherited assumption A5/§8: 8×8 legibility at the eventual display scale | Low today, unverifiable | No panel exists; the judgment stays with `/look-and-feel`, and nothing here is reported as hardware-verified |

---

## ✅ PLAN GATE

*All boxes checked → `/increment` may start. Any box open → back to `/sprint-plan`.*

- [x] Spec status is `approved` (never plan against a draft)
- [x] Architecture decision includes rejected alternatives (a decision without alternatives is a guess)
- [x] Architecture respects the constitution's technical constraints (no dynamic allocation — the atlas, the lookup table and the per-glyph cell are `constexpr`/stack; C++17; snake_case `.h`/`.cpp` pairs; `steamcore` namespace; no ESP-IDF header; one `-I` path unchanged; English throughout)
- [x] Every task maps to a user story — no orphan tasks, no story without tasks
- [x] Every Must AC and every applicable NFR is covered by at least one task (AC-1.1…1.5, AC-2.1…2.8, AC-3.1…3.2, NFR-1…NFR-7; NFR-8/9/10/11 are N/A per the spec, NFR-10 stays with `/look-and-feel`)
- [x] Every task has a checkable definition of done
- [x] Task order respects dependencies
- [x] Test strategy covers every Must story
- [x] Line budget respected: Ist 189 / Soll ~300 (excluding HTML comments)
- [x] Status set to `approved` by the user — approved 2026-09-01
