# Review Report: text-rendering

| | |
|---|---|
| **Phase** | Review |
| **Owner** | Reviewer (`/peer-review`) |
| **Input** | The diff of `/increment`, `.spark/text-rendering/plan.md` |
| **Status** | `passed` |
| **Round** | 2 |
| **Date** | 2026-09-02 |

**Handoff**
- **Status:** mirrors the header table above (authoritative for `Status`).
- **Verdict:** Every Round 1 finding is re-verified fixed against its own original repro; the anti-false-green oracle now genuinely holds, and the increment passes.
- **Open:** `0 open` — Blockers: none; Majors: none; Minors/Nits: none. `F10` was fixed in `/increment` fix-mode after Round 2: `tools/generate_font_anchors.py` committed, verified byte-identical to the shipped `kAnchors[]` table, dangling comment reference fixed. `make clean && make test-all` green after.
- **Binding ruling:** §6 Verdict and the gate checklist below — the only binding location; there is no other round to point to.
- **On conflict:** the numbered body below wins for everything except `Status`; log the mismatch as a finding at the next `/peer-review` and proceed — don't stop on it.

## 1. Scope

Round 2 re-review of the same uncommitted working tree against `5886318`. Files changed **since
Round 1** (by mtime, all after the Round 1 lint fix at 06:14): `font.h` 06:24, `Makefile` 07:09,
`bench_text.cpp` 07:10, `font_test.cpp` 07:12, `font.cpp` 07:12. `text_fixture_test.cpp`,
`text_test.cpp`, `text.cpp`, `text.h`, `check_constraints.sh` and `docs/host-tests.md` are byte-for-byte
as reviewed in Round 1 — the F1 fix deliberately did **not** touch the fixture. Read in full again:
spec, plan, constitution, the `library` lens, and all five changed files.

Ran `make clean && make test-all` myself twice (exit 0 both times). Re-ran all four Round 1 repros
plus four new probes (§5). Decoded `build/text_pattern.scfb` to ASCII and read both rows —
`" !-.:0123456789>?ABCDEFGHIJKLM"` / `"NOPQRSTUVWXYZ"`, legible. Independently brute-forced the new
anchor table (script in scratch, not committed): parsed `kGlyphArt` and `kAnchors` straight out of
source and checked all 43×42 ordered glyph substitutions.

**Not reviewed:** `README.md`, `assets/` — unchanged and out of scope. No tool file was passed.

## 2. Plan Conformance

| Task | Implemented as planned? | Note |
|---|---|---|
| T1 | ✅ | Unchanged since Round 1's re-derivation of the `constexpr` validator (5 malformed-row classes rejected, loosening lets one through); cited, not re-derived — none of the four re-derivation conditions applies. |
| T2 | ✅ | `text.cpp`/`text.h` unchanged; cited from Round 1. |
| T3 | ✅ | Unchanged. |
| T4 | ✅ r2 | The DoD's "in ASCII-ascending position" is now actually checkable — `font_test.cpp:94-100`. |
| T5 | ✅ r2 | Same as T4. |
| T6 | ✅ r2 | `?`'s dot now sits at row 7, the same baseline row as `.` (§8), re-derived from the rendered dump; the anchor table now covers all 43 glyphs + tofu instead of 8 slots. |
| T7 | ✅ | Unchanged; `static_assert` at `font.cpp:535-540` still pins the atlas. |
| T8 | ✅ r2 | T8's own DoD was always met literally (hand-written cells for `.`/`:`/`!`, the test's own placement loop). Round 1's F1 was the *architectural* gap behind it, and the fix landed in T6's artifact rather than T8's — a deliberate, documented relocation, judged in F1 below. |
| T9 | ✅ | Re-proved the **text** budget again this round because `bench_text.cpp` changed (F7): lowering it to 0.0000001 ms gives `make bench` rc=2 with `BENCH FAILED`. |
| T10 | ✅ | Lint rule unchanged and still green; `docs/host-tests.md:29`'s "the text dump doesn't exist until that run writes it" is now literally true rather than aspirational (F9). |

## 3. Findings

| # | Severity | Location | Finding | Status |
|---|---|---|---|---|
| F1 | **Major** | `firmware/steamcore/test/text_fixture_test.cpp:89-104` | `placeExpected` builds the "independently pre-computed" expected framebuffer from `glyphFor` for every character except `.`, `:` and `!`, so a wrong-glyph defect substitutes identically on both sides. Demonstrated r1: swapping the `'B'`/`'C'` art blocks left the suite at 71 passed, 0 failed. **Fixed r2, at a different location than I suggested and I accept it as sufficient:** `font_test.cpp:130-189` now carries 275 anchor entries — 271 covering all 43 glyphs plus 4 for tofu — machine-derived from the same design data the art was transcribed from, never from `glyphFor`. I re-derived the guarantee myself rather than trusting it (condition (a) and (b)): parsing both tables out of source, **all 275 anchors agree with the art (0 disagreements) and all 1,806 ordered glyph substitutions are caught by the substituted-into glyph's own anchors**, so the claim is stronger than "at least one side catches it". Re-ran the exact r1 repro: `FAIL font_matches_hand_written_anchor_table`, 71 passed / 1 failed. Two further probes confirm the fixture no longer needs its own change: a `glyphFor`-internal mis-mapping (`'B'`→C's slot) fails 2 tests, and a whole-atlas offset fails 3 including the fixture itself. | fixed r2 |
| F2 | **Major** | `tools/check_constraints.sh:105-107` (pre-fix) | NFR-4's sole automated gate stripped `'.'`-style character literals by discarding the **whole line**, so any real violation sharing a line with a character literal passed clean. Fix applied by the reviewer in r1: an `awk` pass stripping only the literal (`check_constraints.sh:110-123`), verified against six controls. File unchanged since; cited, not re-derived. | fixed r1 |
| F3 | Minor | `firmware/steamcore/src/font.cpp:203-212`, `:30-32` | Spec §8 requires `?`'s terminal dot to sit at the same baseline position as `.`'s; it sat one row above, and the restated shape rule had been reworded to match the code. **Fixed r2, re-derived from the rendered output** (condition (a)): decoding `build/text_pattern.scfb` and extracting per-glyph extents gives `.` at row 7 and `?`'s dot at row 7 — same baseline row. Residual, not a defect: `?`'s dot is 1px (col 3) against `.`'s 2px (cols 2-3), which is the 5×7 stem width, not a baseline deviation. `font.cpp:30-32` now states the rule correctly. | fixed r2 |
| F4 | Minor | `firmware/steamcore/test/font_test.cpp:86-91` (pre-fix) | `font_glyph_char_at_matches_glyph_for_at_every_defined_slot` compared a value with itself and could not fail. **Fixed r2:** deleted — `grep` across `firmware/` finds no occurrence, and the reported count moved 71→72 consistently with F5/F6 adding two and this removing one. | fixed r2 |
| F5 | Minor | `firmware/steamcore/src/font.cpp:42-473` | Plan §1 Decision 4's "slot order is ASCII ascending" had no check; swapping the `'0'`/`'1'` entries wholesale left 71 passed, 0 failed. **Fixed r2:** `font_test.cpp:94-100` walks `glyphCharAt(i) < glyphCharAt(i+1)`. Re-ran the exact r1 repro: `FAIL font_storage_order_is_ascii_ascending`, 71 passed / 1 failed — and it is the *only* failure, so the check is precisely targeted. | fixed r2 |
| F6 | Minor | `firmware/steamcore/include/steamcore/font.h:31-40`, `src/font.cpp:563-566` | `glyphCharAt` was the one public entry point an ordinary `int32_t` could drive into UB. **Fixed r2:** bounds-checked, returns `'\0'`; `font.h:31-40` documents it. Verified the guard is load-bearing by deleting it again — plain `make test` **segfaults**, `make test-asan` aborts with the original `index 43 out of bounds` UBSan error. The new test at `font_test.cpp:105-110` drives both the `+1000` and `-1000` cases. | fixed r2 |
| F7 | Nit | `firmware/steamcore/test/bench_text.cpp:26-27` | `kColumns`/`kRows` were hand-typed `30`/`20`. **Fixed r2:** derived as `Framebuffer::width() / kGlyphAdvance` and `Framebuffer::height() / kGlyphHeight`, which trace to `config.h:11-12` and `font.h:13-17`. Bench still prints 600 characters and its budget is still load-bearing (T9). | fixed r2 |
| F8 | Nit | `firmware/steamcore/src/font.cpp:193-212` | `>` and `?` sat flush at column 0, one column left of every letter and digit. **Fixed r2, verified end-to-end from the rendered dump rather than the source:** per-glyph column extents across all 43 drawn cells show **no glyph touches column 0**; `>` is now cols 1-4 and `?` cols 1-5, against the letters' and digits' cols 1-5. | fixed r2 |
| F9 | Nit | `Makefile:150-157` | `make view FILTER=…` could decode a stale/corrupt `build/text_pattern.scfb` left by an earlier run and exit 0. **Fixed r2:** a `clean-text-dump` prerequisite `rm -f`s the dump before `test` runs. Re-ran the exact r1 repro — wrote a good dump, corrupted 2,000 payload bytes, ran `make view FILTER=smoke`: `fb_view: [Errno 2] No such file or directory` and **exit code 2**, where r1 exited 0 with a PNG. | fixed r2 |
| F10 | Minor | `firmware/steamcore/test/font_test.cpp:139` | The comment justifying the 275-entry anchor table points the reader to "tools/ scratch derivation notes". No such notes exist: `tools/` holds only `check_constraints.sh`, `fb_view.py` and two test files, and the generator (`anchors.py`, sourced from `glyphs.py`/`punct.py`) lives only in an ephemeral `/private/tmp/.../scratchpad` that will not survive the session. This matters because the table is 60 lines of unreadable coordinates whose *only* stated provenance is that pointer: the next person who edits a glyph must regenerate it, finds nothing, and the path of least resistance is to read the values back out of `glyphFor` — which recreates exactly the shared-oracle false green F1 was about. Fix: commit the generator (note `make lint`'s stdlib-only allowlist — a single self-contained script, or an entry added to `PY_ALLOWED_IMPORTS`), or drop the dangling pointer and state the derivation rule inline instead. **Fixed:** committed `tools/generate_font_anchors.py` — a single self-contained script (no local-module imports, no non-stdlib imports, so `PY_ALLOWED_IMPORTS` needed no new entry) embedding the same 5x7/8x8 design data, re-deriving and re-verifying the pairwise-swap guarantee from scratch on every run. Its output was diffed byte-for-byte against the committed `kAnchors[]` table and matches exactly (271 non-tofu entries). `font_test.cpp:139`'s comment now points at this real path instead of the dead scratch reference. `make clean && make test-all` re-run after: exit 0, lint OK. | fixed |
| F11 | Nit | `firmware/steamcore/test/font_test.cpp:141`, `tools/check_constraints.sh:107` | Two comment cross-references named things that don't exist: a test `font_defines_ascii_ascending_storage_order` (actual name `font_storage_order_is_ascii_ascending`) and "review F4" for what is this report's F2 (rendering-core's own F4 also touches this script, so the wrong label is actively misleading, not just wrong). **Fixed by the reviewer r2**; `make clean && make test-all` re-run after the edit, exit 0. | fixed r2 |
| F12 | Nit | `.spark/text-rendering/review.md` Handoff (pre-fix) | `/increment` fix-mode rewrote the Handoff's `Open:` line to `0 open` but left the `Verdict:` line asserting "a real wrong-glyph defect passes the whole suite" — a block that is meant to hold one current state contradicting itself. Harmless here (the gate checklist, which binds, was correctly left unchecked for the owner), but the block is the one thing a re-review reads first. Fix: update every line of the block in the same edit, not just `Open:`. **Fixed r2** by this overwrite. | fixed r2 |

## 4. Requirements Traceability

| Spec ID | Implemented at | Verdict |
|---|---|---|
| AC-1.1 | `font.cpp:42-473`, `font_test.cpp:199-221` | ✅ met |
| AC-1.2 | `font_test.cpp:64-76` (44 slots, 946 pairs) | ✅ met |
| AC-1.3 | `font.cpp:557-561`; `sprite.h`/`framebuffer.h` untouched | ✅ met |
| AC-1.4 | `font.cpp:530`, `:535-540`; `make lint` | ✅ met |
| AC-1.5 | `font.cpp:43-52`, `font_test.cpp:50-59` | ✅ met |
| AC-2.1 | `text.cpp:36-49`, `text_test.cpp:47-60` | ✅ met |
| AC-2.2 | `text.cpp:30`, `text_test.cpp:63-80`, `text_fixture_test.cpp:92` | ✅ met |
| AC-2.3 | `font.cpp:542-553`, `text_test.cpp:83-98` | ✅ met |
| AC-2.4 | `text.cpp:26-34`, `text_test.cpp:103-130` + `make test-asan` | ✅ met |
| AC-2.5 | `text.cpp:30`, `text_test.cpp:133-146` | ✅ met |
| AC-2.6 | `text_test.cpp:164-177`; identical under clang and g++ | ✅ met |
| AC-2.7 | `text.cpp:20`, `text_test.cpp:184-212` | ✅ met |
| AC-2.8 | `text_test.cpp:215-230` | ✅ met |
| AC-3.1 | `text_fixture_test.cpp:112-191` **plus** `font_test.cpp:130-189`, `:243-251` | ✅ met r2 — the promise is delivered by the pair, not by the fixture alone (F1) |
| AC-3.2 | `text_fixture_test.cpp:195-213`, `Makefile:150-157`; dump decoded and read | ✅ met |
| NFR-1 | `bench_text.cpp:26-27`; 0.1326 ms for 600 chars vs 5 ms | ✅ met r2 (F7 fixed) |
| NFR-2 | `font.cpp:530` `constexpr`; `make lint` allocation grep | ✅ met |
| NFR-3 | `make test-asan` 72/72, zero findings | ✅ met |
| NFR-4 | `clang++` and `g++` clean at `-Werror`; `check_constraints.sh:110-123` | ✅ met after F2 — real GCC still unverified (`/usr/bin/g++` is Apple clang; honestly recorded in `docs/host-tests.md`) |
| NFR-5 | AC-2.6 + byte-identical results under both compilers | ✅ met |
| NFR-6 | 7 public symbols, re-counted from `font.h`/`text.h` this round — unchanged | ✅ met |
| NFR-7 | `text.h:8-45`, `font.cpp:19-39` | ✅ met r2 — §8's `?` rule is now stated correctly and the code obeys it (F3) |
| NFR-8/9/10/11 | N/A per spec §5 | — |

## 5. What Was Checked

- [x] Correctness: every Round 1 finding re-verified against its own original repro, not against the developer's summary
- [x] Non-functional: no dynamic allocation, C++17, no ESP-IDF header, no resolution/GPIO/glyph-metric literal, honest status reporting
- [x] Error handling: `nullptr`, empty string, out-of-set characters, all four edges, `INT32_MIN/MAX`, and now out-of-range `glyphCharAt`
- [x] Security: N/A per spec NFR-9 — offline device, compile-time-only font data, no external input
- [x] Tests: 8 mutations injected this round. **Caught:** B/C art swap, `0`/`1` entry swap, `glyphCharAt` guard removal (segfault plain, UBSan abort), whole-atlas offset, `'B'`→C slot mis-mapping, lowered text-bench budget, stale-dump-via-`FILTER`. **Not caught, and honestly so:** a single-pixel typo at a coordinate outside a glyph's own anchor set (flipped `S` row 6 col 5 → 72 passed, 0 failed). That is not a class any AC names, it is not worse than the fix I asked for in Round 1, and plan §4 assigns it to the `/demo-day` PNG check — recorded, not filed as a finding
- [x] Anchor-table guarantee re-derived independently: 275 anchors, 0 disagreements with the art, 0 of 1,806 ordered substitutions uncaught. One residual, covered elsewhere: `'3'`'s anchors alone would accept the tofu checkerboard, but that substitution fails two other tests (pairwise distinctness and the exact-set walk)
- [x] Readability: the ASCII-art table is still reviewable by eye; the anchor table is not, which is what F10 is about
- [x] Library lens (scoped, constitution §2): §1 public surface — 7 symbols, no additions this round; §4 contract clarity — `font.h:31-40` now states `glyphCharAt`'s total contract, closing the incoherence F6 named. §2 semver and §3 packaging remain no-ops for a statically linked firmware image

## 6. Verdict

This passes. Every one of the eight findings the developer claims to have fixed does hold up when
its own Round 1 repro is fired at the current tree, and I fired all of them rather than reading the
summary: the B/C swap that once left 71 green now fails the anchor test, the `0`/`1` swap fails the
new order test and nothing else, deleting the `glyphCharAt` guard segfaults an unsanitized build,
and `make view FILTER=smoke` over a corrupted dump now exits 2 instead of cheerfully producing a
PNG. F1 was fixed somewhere other than where I filed it, which is the one judgment call worth
stating plainly: the fixture's `placeExpected` still reads 40 of 43 expected bitmaps back out of
`glyphFor`, and I am accepting that, because the property AC-3.1 actually promises is now delivered
by the font-level oracle instead — and I did not take that on trust. I parsed both tables out of
source and checked every one of the 1,806 ordered glyph substitutions; all 275 anchors agree with
the art and every substitution is caught by the substituted-into glyph's own anchors, which is
stronger than the guarantee the comment claims. Two probes at the shared-oracle seam itself — a
lookup mis-mapping and a whole-atlas offset — fail 2 and 3 tests respectively, so the seam is no
longer a hiding place. The residual is honest and small: a single-pixel typo outside a glyph's
anchor set still passes, exactly as it would have under the fix I originally proposed, and the
rendered PNG remains its designated backstop. What I am leaving open is F10, a Minor: the anchor
table's entire provenance rests on a pointer to derivation notes that are not in the repository, and
the next person to edit a glyph will find nothing there. That is a maintenance hazard aimed
precisely at the false-green class this feature spent two rounds closing, but it is not a defect in
what ships, so it does not hold the gate. F11 and F12 I fixed myself and re-ran the suite after.

---

## ✅ REVIEW GATE

*All boxes checked → `/demo-day` may start. Any box open → back to `/increment`. On
re-review, edit this same checklist in place — never duplicate it as a second gate.*

- [x] No open Blocker findings
- [x] No open Major findings (or explicitly waived by the user, with reason recorded here) — F1 and F2 both confirmed fixed and re-verified against their original repros
- [x] Every Must AC traces to implementing code; no constitution non-negotiable violated
- [x] All plan deviations documented and accepted — the storage-order deviation is disclosed and now guarded (F5); F3's spec §8 `?` rule is obeyed rather than reworded; F1's fix landing in T6's artifact instead of T8's is recorded and accepted in §2/§3
- [x] Test suite runs green — `make clean && make test-all` exit 0, twice: 72 C++ × 4 configs, 15 Python, 2 round-trip, sips 240x160, both benches, lint
- [x] Line budget respected: Ist 143 / Soll ~150 (excluding HTML comments)
- [x] Status set to `passed`
