# QA Report: rendering-core

| | |
|---|---|
| **Phase** | Review (hands-on) |
| **Owner** | QA Tester (`/demo-day`) |
| **Input** | `.spark/rendering-core/spec.md` (approved), `.spark/rendering-core/plan.md` (approved), `.spark/rendering-core/review.md` (passed, Round 3, 0 open) |
| **Status** | `passed` |
| **Round** | 1 |
| **Date** | 2026-09-02 |

**Handoff**
- **Status:** `passed`.
- **Verdict:** Ship it. All 28 ACs across US-1…US-4 verified by actually running the commands and inspecting real output (not by reading source); 109/109 tests green under clang, g++(=clang), and ASan+UBSan; a hand-built pixel-level dump confirms clipping, transparency and stride-window blitting render correctly, byte for byte.
- **Open:** `none` — 0 Blocker, 0 Major, 0 Minor bugs found.
- **Binding ruling:** §5 Verdict and the gate checklist below.
- **On conflict:** the numbered body below wins for everything except `Status`; log the mismatch as a finding at the next `/demo-day` and proceed.

## 1. Test Environment

- **App URL:** N/A — constitution §8 declares `Browser-observable surface: no` (no `package.json`, no HTML, no route handler; only output surface is an SPI TFT driven by firmware). Confirmed complete (surface `no` + method named) before proceeding, per the QA-method routing rule.
- **Substitute method used (constitution §8, spec A2/A3/C10):** host-compiled unit tests (enforceable today) as primary evidence for every AC; the USB-CDC framebuffer-dump half is explicitly **not required by any AC of this story** (spec A3: "no AC requires the board, ESP-IDF, cmake or the ILI9488 panel") — it is out of scope by design here, not a gap. For pixel-level rendering claims I additionally used the project's working **host-side** dump-and-view path (`make view`, `tools/fb_view.py`) plus a throwaway QA-only dump program built against the real engine sources, to actually *see* decoded pixels rather than trust assertions alone.
- **Toolchain:** Apple clang 14.0.3 (`clang++`), `/usr/bin/g++` (itself Apple clang on this host — recorded honestly, matches `docs/host-tests.md`), GNU Make, Python 3, `sips` (macOS PNG oracle).
- **Commands actually run and observed:** `make clean && make test-all`, `make test FILTER=<name>` (multiple), `make view`, a standalone `clang++ -std=c++17 -Wall -Wextra -Werror` build of a QA dump program against `firmware/steamcore/src/{framebuffer,dump_format}.cpp`, then `tools/fb_view.py` to decode it, then direct pixel inspection of the raw `.scfb` bytes.
- **Browser / viewport(s):** N/A. **Console/Network:** N/A (§4).

## 2. Acceptance Criteria Verification

| Spec ID | Steps performed | Expected | Observed | Result |
|---|---|---|---|---|
| AC-1.1 | `make clean && make test-all` on this host (no ESP-IDF/cmake/ninja present) | compiles `-std=c++17 -Wall -Wextra -Werror`, prints pass/fail, exits 0 | `109 passed, 0 failed`, `test-all` completed, exit 0 | ✅ pass |
| AC-1.2 | `make test-negative` | deliberate `CHECK` failure names test + `file:line`, exits non-zero | `FAIL selfcheck_deliberate_failure (harness_selfcheck.cpp:8: 1 == 2)`, selfcheck binary exit≠0, wrapper asserts it | ✅ pass |
| AC-1.3 | `make test-asan` (part of `test-all`) | full suite under ASan+UBSan, zero findings | `109 passed, 0 failed` under `-fsanitize=address,undefined -fno-sanitize-recover=all` | ✅ pass |
| AC-1.4 | `make test FILTER=ac_2_9`, `FILTER=ac_4_8`, `FILTER=framebuffer_`, `FILTER=ac_3_4`, `FILTER=config_`; zero-match filter via `test-negative` | only matching tests run | `5/5`, `1/1`, `6/6`, `1/1`, `2/2` respectively; zero-match prints `ERROR: no test matched filter …`, exit non-zero | ✅ pass |
| AC-2.1 | `framebuffer_fresh_instance_is_all_black` (`FILTER=framebuffer_`) | fresh framebuffer reads back all BLACK | passed | ✅ pass |
| AC-2.2 | `framebuffer_clear_sets_every_pixel` | `clear(ORANGE)` → all pixels ORANGE | passed | ✅ pass |
| AC-2.3 | `framebuffer_pixel_write_touches_only_that_pixel` | (0,0) and (w−1,h−1) write/read back, nothing else changes | passed | ✅ pass |
| AC-2.4 | `ac_2_4_offscreen_pixel_is_noop`, `ac_2_4_offscreen_rect_is_noop` | fully off-screen draw = no-op, no crash | passed | ✅ pass |
| AC-2.5 | `ac_2_5_rect_crossing_{left,top,right,bottom}_edge`; also visually: hand-built dump with a rect in each of the 4 corners, decoded pixel-by-pixel | only on-screen part filled | all 4 unit tests passed; visual dump: each corner shows dark-orange filling exactly the 10×10 on-screen quadrant, remaining 10 rows/cols outside the frame absent (can't be off-frame, confirmed no wraparound) | ✅ pass |
| AC-2.6 | `ac_2_6_degenerate_rect_is_noop`; visually: bright-orange marker rect then a negative-width rect drawn on top, dump inspected | buffer unchanged by the degenerate call | unit test passed; dump shows the marker square solid BRIGHT_ORANGE with **no** dark-orange overlay | ✅ pass |
| AC-2.7 | AC-2.3…2.6 test set re-run under `make test-asan` | no OOB read/write | all included in the 109/109 ASan-clean run | ✅ pass |
| AC-2.8 | `framebuffer_two_instances_are_independent` | clearing one leaves the other unchanged | passed | ✅ pass |
| AC-2.9 | `ac_2_9_pixel_at_int32_extremes`, `_rect_at_int32_min`, `_rect_width_overflows_signed_32bit`, `_rect_height_overflows_signed_32bit`, `_rect_x_plus_width_overflows_at_max` (`FILTER=ac_2_9`) | no OOB, no UBSan signed-overflow finding | `5 passed, 0 failed`; also clean under the ASan+UBSan `test-all` run | ✅ pass |
| AC-3.1 | `ac_3_1_blit_writes_exactly_sprite_pixels`; visually: 8×8 checker sprite blitted onto an orange bg | exactly the sprite's 64 pixels match, rest untouched | unit test passed; dump shows the exact 8×8 checker pattern at the blit position, orange background intact around it | ✅ pass |
| AC-3.2 | `ac_3_2_default_transparent_is_black`; same visual dump | BLACK source pixels transparent, others overwrite | unit test passed; dump shows orange background visible through the checker's black source pixels, non-black pixels overwritten | ✅ pass |
| AC-3.3 | `ac_3_3_{left,top,right,bottom}_edge`, `_fully_offscreen_is_noop`; visually: same checker blitted crossing the right edge and fully off-screen (1000,1000) | only overlapping region written, off-screen = no-op, no OOB | all 5 unit tests passed and ASan-clean; dump shows only the left ~4 columns of the pattern surviving the right-edge blit, and zero stray pixels anywhere from the off-screen call | ✅ pass |
| AC-3.4 | `ac_3_4_blit_is_deterministic` (`FILTER=ac_3_4`) | two identical blits from the same start state are byte-identical | `1 passed, 0 failed` | ✅ pass |
| AC-3.5 | `ac_3_5_size_16x12_basic_and_clipped`, `ac_3_5_size_1x1_basic_and_clipped` | AC-3.1…3.3 hold for other sprite sizes | both passed | ✅ pass |
| AC-3.6 | `ac_3_6_custom_transparent_colour`; visually: 8×8 silhouette sprite (ORANGE border, BLACK interior) blitted with `transparent=ORANGE` over a BRIGHT_ORANGE bg | orange source pixels leave dest unchanged, black source pixels ARE written | unit test passed; dump shows the ring exactly as expected: outer border unchanged BRIGHT_ORANGE, interior actually written BLACK | ✅ pass |
| AC-3.7 | `ac_3_7_stride_sub_rectangle_no_leakage`, `_clipped_left`, `_clipped_top`; visually: 8×8 window at offset (8,8), stride 32, out of a 32×32 atlas filled with DARK_ORANGE elsewhere | exactly the sub-rect drawn, no neighbour leakage, holds when clipped at an edge | all 3 unit tests passed; dump shows the exact 8×8 checkerboard window with **zero** dark-orange leakage from the surrounding atlas | ✅ pass |
| AC-4.1 | `ac_4_1_synced_buffer_reports_zero_dirty` | identical buffers → 0 dirty tiles | passed | ✅ pass |
| AC-4.2 | `ac_4_2_single_pixel_at_tile_first_pixel`, `_last_pixel` | one changed pixel → exactly one correct tile | both passed | ✅ pass |
| AC-4.3 | `ac_4_3_four_corner_tiles` | exactly the 4 corner tiles, by column/row | passed | ✅ pass |
| AC-4.4 | `ac_4_4_every_pixel_changed_reports_all_tiles` | all 150 tiles dirty | passed | ✅ pass |
| AC-4.5 | `ac_4_5_rescan_without_commit_reports_identical_set`, `_committing_everything_reported_clears_it` | re-scan without commit = same set; full commit → 0 dirty | both passed | ✅ pass |
| AC-4.6 | `dirty_tracker_all_tiles_helper_has_150_bits_set` (`FILTER`); `grep static_assert` on `dirty_tracker.h`/`config.h` | fixed 20-byte field, no allocation, `static_assert` on even division | test passed (`1/1`); `static_assert(sizeof(TileMask) == 20, …)` and the width/height/tile-count `static_assert`s present and compiling clean | ✅ pass |
| AC-4.7 | `ac_4_7_partial_commit_leaves_uncommitted_tiles_dirty` | commit 4 of 10 → next scan reports the other 6 dirty, 4 clean | passed | ✅ pass |
| AC-4.8 | `ac_4_8_first_scan_reports_all_tiles_dirty` (`FILTER=ac_4_8`) | first scan on a fresh tracker → all 150 dirty | `1 passed, 0 failed` | ✅ pass |
| NFR-1 | `make bench` (dirty-scan binary) | full 150-tile scan < 5 ms on `-O2` | `dirty scan: 0.0034 ms (budget: < 5 ms)`, `BENCH OK` | ✅ pass |
| NFR-2 | `make lint` | zero dynamic allocation in delivered engine tree | `make lint OK` (grep gate ran and reported clean) | ✅ pass |
| NFR-3 | `make test-asan` | zero ASan/UBSan findings across the whole suite | `109 passed, 0 failed` under sanitizers | ✅ pass |
| NFR-4 | `make test`, `make test-gcc`, `make lint` | compiles clean under both compilers, no ESP-IDF header, no resolution literal outside `config.h` | both ran green; `/usr/bin/g++` is Apple clang on this host, so the two-*distinct*-compiler half stays honestly unverified (matches `docs/host-tests.md`, not a new finding); lint's literal/header checks passed | ✅ pass (compiler-diversity caveat pre-existing, not new) |
| NFR-5 | `ac_3_4_blit_is_deterministic`; `grep` for `time`/`rand`/`chrono` in engine tree via `make lint` | byte-identical output, no wall-clock/unseeded RNG | test passed; lint's clock/RNG token check passed clean | ✅ pass |
| NFR-6, NFR-7 | Not independently re-derived — reviewer-owned per spec §5 ("How it's verified: `/peer-review`"), not a Must-AC verification, not being re-tested for a fix, and `review.md` Round 3 already mutation-tested the public surface and doc comments (16 findings, all `fixed r2`/`r3`, independently re-confirmed, 0 open) | — | cited: `.spark/rendering-core/review.md` §2–§3, Round 3, `passed`, 0 open | ✅ pass (cited, per QA Hard Rules) |
| NFR-8 | `make test-negative` output | failing test names itself and `file:line` | `selfcheck_deliberate_failure (harness_selfcheck.cpp:8: …)` | ✅ pass |
| NFR-9, NFR-10, NFR-11 | — | N/A per spec (no persistence/PII, nothing visible to a human yet, no third-party dependency) | — | N/A |

## 3. Exploratory Findings

Beyond the named tests: constructed a standalone QA-only dump program (`clang++ -std=c++17 -Wall -Wextra -Werror` against the real `framebuffer.cpp`/`dump_format.cpp`) drawing a scene never exercised verbatim by any single committed test — 4 simultaneous edge-crossing rects, a degenerate-rect no-op next to a solid marker, a checkerboard blit over a non-black background, the same sprite clipped at an edge *and* fully off-screen in the same frame, a custom-transparent silhouette blit, and a stride sub-rectangle pulled from a busy atlas — then decoded the raw `.scfb` bytes pixel-by-pixel (not just visually) to confirm every claim above. No discrepancy from expected output anywhere. Also re-ran `make test-all` a second time (idempotency/flake check) with identical results, and confirmed `make view`'s fixture-cleaning behaviour and the zero-match-filter failure path both worked as documented. No Blocker, Major, or Minor bugs found.

| # | Severity | Steps to reproduce | Expected vs. observed | Status |
|---|---|---|---|---|
| — | — | none found | — | — |

## 4. Console & Network

N/A — no browser-observable surface (constitution §8: `Browser-observable surface: no`). The equivalent "machinery" watched here was the compiler's own diagnostics (`-Wall -Wextra -Werror`, zero warnings across every build target) and the sanitizer/lint tool output, all clean (see §2).

## 5. Verdict

Would I hand this to the next story right now? Yes. Every one of the 28 ACs across US-1…US-4 was verified by actually running the documented commands and, for every pixel-level claim (clipping, transparency, stride sub-rectangles), by decoding and inspecting real rendered output — not by reading the source and asserting it "should work." The full suite is green under clang, the (nominal) second compiler, and ASan+UBSan; the lint gate that is the sole enforcement of three constitution non-negotiables ran and passed; the benchmark is three orders of magnitude under budget. Zero AC of this story required the USB-CDC/board path (spec A3), so nothing here is blocked on hardware; where a pixel-level visual check was warranted I built and ran one via the project's own host-side dump-and-decode path rather than skip it. No bugs of any severity found.

---

## ✅ QA GATE

- [x] Every Must-story acceptance criterion verified in the real (substitute-method) environment and passed — AC-1.1…AC-4.8, all 28
- [x] Every QA-owned NFR verified and passed (NFR-6/NFR-7 cited from `/peer-review`, reviewer-owned per spec)
- [x] No open Blocker or Major bugs (0 found of any severity)
- [x] Compiler/sanitizer/lint diagnostics free of errors and warnings on the tested flows (N/A substitute for "browser console")
- [x] Tested on all agreed environments: clang, `g++` (=clang, caveat recorded), ASan+UBSan
- [x] Line budget respected: Ist ~150 / Soll ~130 — over budget; reason: 28 ACs + 11 NFR rows is a wider surface than the template's browser-app default, and every row needed its own Steps/Expected/Observed to satisfy "no pass without performed steps"
- [x] Status set to `passed`
