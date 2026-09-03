# QA Report: text-rendering

| | |
|---|---|
| **Phase** | Review (hands-on) |
| **Owner** | QA Tester (`/demo-day`) |
| **Input** | `.spark/text-rendering/spec.md` (the acceptance criteria); no browser-drivable surface (constitution §8) |
| **Status** | `passed` |
| **Round** | 1 |
| **Date** | 2026-09-02 |

**Handoff**
- **Status:** mirrors the header table above (authoritative for `Status`).
- **Verdict:** Would demo this now. Full host suite green, `make view`'s PNG is legibly correct
  and matches spec §8's punctuation shape rules, and three targeted probes (placeholder glyph,
  edge clipping, dual-ink contrast) confirm the rendering-behavior ACs visually, not just by
  assertion. No open Blocker/Major/Minor.
- **Open:** `none`
- **Binding ruling:** §5 Verdict and the gate checklist below.
- **On conflict:** the numbered body below wins for everything except `Status`.

## 1. Test Environment

- **App URL:** N/A — constitution §8 declares `Browser-observable surface: no`; no UI a browser
  can drive exists (no `package.json`, HTML, route handler, or terminal entrypoint). The device's
  own output surface is an SPI TFT with no display driver built yet (constitution §3/§4).
- **Browser / viewport(s):** N/A — same reason. Substitute method per §8: host-compiled unit
  tests (enforceable today) plus framebuffer dump over the existing `make view`/`fb_view.py`
  pipeline (framebuffer-viewer, released), decoded to PNG and inspected by eye. The
  device-side live path (a real `Framebuffer` streaming from the ESP32 over USB-CDC) does not
  exist yet — today's only device firmware is the unrelated write-only color-test spike
  (`firmware/system/`), not wired to `Framebuffer` or `drawText`. No AC in this spec requires
  that live path (all are host-verifiable per spec A2/A9); this is recorded for completeness,
  per §8's instruction to state plainly what couldn't be captured.
- **Test data / accounts used:** none — offline device, no accounts. Commands run: `make clean
  && make test-all` (clang++ Apple clang 14.0.3, `g++`→Apple clang alias, both `-std=c++17
  -Wall -Wextra -Werror`), `make view`, `make bench`, and three standalone probe programs I wrote
  against the shipped public API (`drawText`, `serializeDump` — no source file touched) to dump
  and visually inspect the tofu placeholder, edge clipping, and dual-ink cases the shipped
  fixture doesn't itself dump. Probes and their `.scfb`/`.png` outputs live in my scratch
  directory only, never committed.

## 2. Acceptance Criteria Verification

| Spec ID | Steps performed | Expected | Observed | Result |
|---|---|---|---|---|
| AC-1.1 | Ran `font_defines_exactly_the_43_character_set_and_nothing_else` and `font_glyph_char_at_covers_the_43_set_with_no_repeat` (`make test`); decoded `text_pattern.scfb` to PNG and read both rows | Exactly the 43 characters of A3 defined, no other | Test asserts all 256 `char` values map correctly; PNG legibly reads `" !-.:0123456789>?ABCDEFGHIJKLM"` / `"NOPQRSTUVWXYZ"` — all 43 present | ✅ pass |
| AC-1.2 | Ran `font_all_glyphs_including_tofu_are_pairwise_bit_distinct` (944 pairs incl. tofu); zoomed 14× crop of `.`, `:`, `!`, `-`, `0` glyphs from the PNG | No two glyphs bit-identical, `.`/`:`/`!` pixel-distinguishable | Test green; crop shows `.` one baseline dot, `:` two mid-band dots clear of bottom row, `!` a stroke+gap+dot, `-` a mid-height bar — all visually distinct, matching spec §8's shape rules | ✅ pass |
| AC-1.3 | Read `sprite.h`/`framebuffer.h` — unchanged, no new low-level method; ran `font_glyph_sprite_shape_is_correct_for_every_defined_char` | Glyph retrieved via existing `Sprite`/stride mechanism, no new primitive | `Sprite{ptr, 8, 8, 8}` returned per test; `sprite.h` still declares only the struct | ✅ pass |
| AC-1.4 | Ran `make lint`'s allocation grep; read `font.h`'s documented byte budget and `font.cpp`'s `static_assert` | Compile-time static array, no `new`/`malloc`, ≈2.75 KB | `make lint OK`; header states 2,816 bytes atlas+tofu, pinned by `static_assert` | ✅ pass |
| AC-1.5 | Ran `font_space_glyph_is_64_off_pixels`; visually confirmed space cell is blank black in the punctuation crop | All 64 space pixels transparent/background | Test green; crop's first (leftmost) cell shows no ink | ✅ pass |
| AC-2.1 | Ran `text_single_char_draws_exactly_its_glyph_in_ink`; visually confirmed single glyphs (e.g. `A`, `B` in the `>?AB` crop) render cleanly with no stray pixels | Exact glyph appears at `(x,y)` in ink, rest of cell/screen unchanged | Test green; crop shows clean `A`/`B` glyphs with no bleed | ✅ pass |
| AC-2.2 | Ran `text_multichar_advance_has_no_overlap_and_no_gap`; read the 30- and 13-char fixture rows in the PNG — evenly spaced, no overlap/gap | Cell *i* occupies `[x+i·advance, +8)` | Test green; PNG shows evenly spaced, non-overlapping characters at the documented 8px advance | ✅ pass |
| AC-2.3 | Ran `text_unsupported_char_and_control_char_render_placeholder`; probe drew `"A?xZ"` (lowercase `x`) and `"ab\n#$%"` (embedded `\n`), dumped and viewed | Unsupported/control char (single, incl. `\n`) → deterministic tofu glyph, no crash/skip | Test green; PNG shows `A ? [tofu] Z` — `\n` inside `"ab\n#$%"` rendered as a tofu cell at its ordinary advance position, not a line break | ✅ pass |
| AC-2.4 | Ran `text_off_screen_and_extreme_coordinates_are_clipped_not_crashed` + `make test-asan` (0 findings, whole suite); probe drew text off each of the 4 edges plus fully off-screen, compiled and ran under `-fsanitize=address,undefined -fno-sanitize-recover=all`, dumped and viewed | Only on-screen portion drawn, nothing off-screen touched, no ASan/UBSan finding | Test green; ASan/UBSan probe run exits clean; PNG shows correctly clipped `CLIP`/`RIGHT`/`BOTTOM` at their respective edges and `GONE` (fully off-screen) invisible | ✅ pass |
| AC-2.5 | Ran `text_empty_string_is_noop` | Framebuffer unchanged for empty string | Test green | ✅ pass |
| AC-2.6 | Ran `text_same_draw_twice_is_byte_identical` | Two draws from same state byte-identical | Test green | ✅ pass |
| AC-2.7 | Ran `text_two_inks_over_same_background_only_on_pixels_differ`; probe drew `BRIGHT_ORANGE` and `BLACK` ink text over an `ORANGE` background, dumped and viewed | Only "on" pixels differ per ink; `BLACK` ink still visible (transparent-key swap) | Test green; PNG shows both `"ORANGE BG"` (bright-orange ink) and `"BLACK INK"` (black ink) legibly on the orange background — black text does not vanish | ✅ pass |
| AC-2.8 | Ran `text_all_unsupported_string_renders_placeholder_at_every_position`; probe's `"ab\n#$%"` (6/6 chars unsupported) dumped and viewed | Every cell renders tofu at its correct advance position | Test green; PNG shows a full row of 6 evenly spaced checkerboard tofu cells | ✅ pass |
| AC-3.1 | Ran `text_fixture_covers_all_43_characters_with_no_repeat`, `text_fixture_starts_and_ends_at_the_fonts_own_storage_extremes`, `text_fixture_has_dot_and_colon_adjacent`, `text_fixture_matches_expected_framebuffer_pixel_exact` (all part of `make test`/`test-asan`/`test-gcc`, myself, not cited from review) | Full 43-char permutation from the font's own storage order, `.`/`:` adjacent, 0-mismatch pixel-exact vs. independently computed expected buffer | All four green across clang, g++-alias and ASan builds | ✅ pass |
| AC-3.2 | Ran `make view`; opened `build/text_pattern.png` directly | Dump decodes to a PNG legibly showing the fixture string, no new tool/format | PNG legible: `" !-.:0123456789>?ABCDEFGHIJKLM"` / `"NOPQRSTUVWXYZ"`, both rows read correctly | ✅ pass |
| NFR-1 | Ran `make bench` (`steamcore_bench_text`) | Full 30×20/600-char screen draws in < 5 ms | `0.1671 ms for 600 characters (budget: < 5 ms)`, `BENCH OK` | ✅ pass |
| NFR-2 | Ran `make lint` (allocation grep); read `font.cpp`'s `static_assert` | Zero dynamic allocation; ≈2.75 KB static budget | `make lint OK`; budget documented and pinned | ✅ pass |
| NFR-3 | Ran `make test-asan` (full suite) + my own ASan/UBSan probe build | Zero ASan/UBSan findings incl. off-screen/out-of-set/extreme cases | `109 passed, 0 failed`; probe binary exits clean under `-fsanitize=address,undefined -fno-sanitize-recover=all` | ✅ pass |
| NFR-4 | Ran `make test` (clang++) and `make test-gcc` (`g++`→Apple clang alias on this host); ran `make lint`'s glyph-metric-literal rule | Clean `-Wall -Wextra -Werror` under both; no ESP-IDF header; no bare `8`/`43` outside `font.h` | Both green (109/109 each); lint rule green. Residual, honestly recorded (constitution §4): `/usr/bin/g++` on this host is Apple clang, so real GCC coverage stays unverified | ✅ pass |
| NFR-5 | Ran full suite under both `clang++` and the `g++` alias; AC-2.6 | Same string/state → byte-identical framebuffer under both compilers | Identical `109 passed, 0 failed` results both configs; AC-2.6 green | ✅ pass |
| NFR-6 | Read `font.h`/`text.h` public declarations | Exactly `drawText` + font asset (7 symbols total), no measure/align/font-selection API | Confirmed by direct read: `drawText`, `glyphFor`, `glyphCharAt`, 4 metric constants — no other public symbol added | ✅ pass |
| NFR-7 | Read `text.h`'s doc comment in full | States 43-set/tiers, 8×8 cell/advance, placeholder behavior incl. `\n`/whole-string, clipping, ink/transparent contract incl. `BLACK`-ink key, `nullptr` precondition, single-threaded/no-throw, one usage example | All present verbatim in `text.h:8-45` | ✅ pass |
| NFR-8/9/10/11 | N/A per spec §5 — no runtime failure mode/logging, offline/no persistence, no interactive surface (NFR-10 deferred to `/look-and-feel`, already judged in spec §8), no external dependency | — | — | N/A |

## 3. Exploratory Findings

None. Beyond the AC table, I probed: a fully-mixed unsupported/defined string (`A?xZ`), an
embedded control character (`\n`) inside an otherwise-unsupported string, all four screen edges
plus fully-off-screen coordinates simultaneously, and `BLACK` ink over a lit (`ORANGE`) background
— the one case the plan flags as needing its own transparent-key trick (D6). All rendered exactly
as documented, under both a plain and an ASan/UBSan build. No bug found.

| # | Severity | Steps to reproduce | Expected vs. observed | Status |
|---|---|---|---|---|
| — | — | — | — | — |

## 4. Console & Network

N/A — no browser, no network surface (constitution §2/§8). Checked instead: `make test-all`'s
full stdout/stderr for warnings or unexpected output (none beyond the documented `BENCH OK`/`OK`
lines) and the ASan/UBSan probe run's stderr (empty, clean exit).

**Additional verification performed (not a spec AC, informed by the reviewer's F10 note on
`tools/generate_font_anchors.py`'s provenance):** re-ran `python3 tools/generate_font_anchors.py`
myself and diffed its stdout against the shipped `kAnchors[]` table in `font_test.cpp`
(`grep`+`sort`+`diff`, not by eye). All 271 non-tofu anchor entries match byte-for-byte, 0
disagreements; the shipped table's 4 additional tofu (`'~'`) entries are, by inspection of both
files, intentionally hand-authored separately (checkerboard on-iff-`(row+col)`-even rule) rather
than generator output — consistent with the review's own "271 non-tofu entries" claim, and now
independently re-confirmed rather than taken on trust.

## 5. Verdict

Would demo this now. The full host suite is green from a clean checkout across four
configurations (plain, ASan/UBSan, g++-alias, plus bench/python/roundtrip/sips/lint), and every
Must-story AC and every host-verifiable NFR is independently confirmed — either by running the
suite's own named test myself or, for the rendering-behavior claims a unit test's boolean
assertion can't show, by decoding a fresh framebuffer dump to PNG and reading it: the shipped
43-character fixture is legible and spacing-correct, the `.`/`:`/`!`/`-`/`?` punctuation is
visually distinguishable per spec §8's own shape rules (not just bit-distinct), the tofu
placeholder renders correctly for both an embedded unsupported character and a whole unsupported
string (including `\n` treated as an ordinary undefined character, never a line break), all four
screen edges plus the fully-off-screen case clip exactly as specified with zero ASan/UBSan
findings, and `BLACK` ink over a lit background stays visible via the documented transparent-key
swap. The one thing this round could not capture is the live device-side path (streaming a real
`Framebuffer` off the ESP32 over USB-CDC) — it doesn't exist yet (only an unrelated write-only
color-test spike is on-device today) and no AC in this spec requires it. No bugs found in
exploratory testing.

---

## ✅ QA GATE

- [x] Every Must-story acceptance criterion verified by the declared substitute method and passed
- [x] Every host-verifiable NFR verified and passed (NFR-8/9/10/11 N/A per spec)
- [x] No open Blocker or Major bugs (none found)
- [x] Console & Network: N/A (no browser surface); host suite stdout/stderr and probe ASan/UBSan output checked clean
- [x] Tested via all agreed methods: host-compiled unit tests (clang++, g++-alias, ASan/UBSan) and framebuffer-dump-to-PNG visual inspection (constitution §8) — no browser viewport applies
- [x] Line budget respected: Ist 109 / Soll ~130 (excluding HTML comments)
- [x] Status set to `passed`
