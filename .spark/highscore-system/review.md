# Review Report: highscore-system

| | |
|---|---|
| **Phase** | Review |
| **Owner** | Reviewer (`/peer-review`) |
| **Input** | The working-tree diff of `/increment` vs. `8c4e553`, `.spark/highscore-system/plan.md` |
| **Status** | `passed` |
| **Round** | 2 |
| **Date** | 2026-09-08 |

**Handoff**
- **Status:** mirrors the header table above (authoritative for `Status`).
- **Verdict:** Round 2 independently re-verified all five findings `/increment` fix-mode closed (F2, F3, F4, F9, F10) — each confirmed from source, both new regression tests mutation-checked by me against the pre-fix code, and NFR-4's public surface re-audited by a method that covers Round 1's blind spot (namespace-scope names) plus a compile-level proof. All hold. One new Nit (**F11**) surfaced as a side effect of the F2 fix and was fixed in this round. `passed` — `/demo-day` may start.
- **Open:** `0 open` — F1–F11 all `fixed`; `make test`/`test-asan`/`test-gcc` **318 passed, 0 failed** (clang/asan/gcc, re-run by me after F11); `make lint` OK; `idf.py build` green (`steamcore_system.bin` 0x3f0d0, 75% of the app partition free). AC-1.5 remains honestly `not capturable` (T15 hardware-gated, `blocked`).
- **Binding ruling:** §6 Verdict and the gate checklist below — the only binding location; there is no other round to point to
- **On conflict:** the numbered body below wins for everything except `Status`; log the mismatch as a finding at the next `/peer-review` and proceed — don't stop on it.

## 1. Scope

Reviewed (Round 2): the same working-tree state vs. `8c4e553` (unchanged file set — 6 new engine headers, 4 new `src/*.cpp`, `port/esp32/nvs_highscore_backend.{h,cpp}`, 11 new test files + `bench_highscore.cpp` + `fake_flash_backend.h`, `highscore_harness_game.h`, and the modified `galactic_invasion.h`, `app_main.cpp`, `CMakeLists.txt`, `Makefile`, `tools/check_constraints.sh`, `docs/{host-tests,device-build}.md`), focused on the five fixes closed since Round 1. Re-read in full this round: `highscore.h`, `highscore.cpp`, `highscore_screen.{h,cpp}`, the two new tests, the five test files the `detail` move touched, and `text.h`'s nullptr contract.

Ran myself this round: `make test` / `test-asan` / `test-gcc` (**318 passed, 0 failed** on all three), `make lint` (OK), `idf.py build` from `firmware/system/` (green, `steamcore_system.bin` 0x3f0d0, 75% free) — re-run once more after my own F11 fix. Two mutation checks of my own (F2's centring, F9's clamp), each restored byte-identically afterwards (`diff -q` clean). Two independent NFR-4 audits (a namespace-depth-tracking parse of `highscore.h`, and a compile-level probe asserting each moved name no longer resolves at `steamcore::`). One UBSan probe (nullptr `displayName`) that found **F11**. Nothing was flashed and no hardware behaviour is claimed.

Not reviewed: `assets/Buttons.png`, `assets/fonts/`, `assets/sprites/` — untracked before this increment began and touched by no task. T15 (`blocked`, no board attached) is out of review scope by its own plan-anticipated Definition of Done; AC-1.5 stays explicitly unverified. No blast-radius/scoping tool file was passed, and none was used — scoping was by hand from the plan's per-task file list.

Bounded reading, Round 2: F1/F5/F6/F7/F8 were applied *and* mutation-checked by the Round-1 reviewer itself and are cited from their §3 rows, not re-derived — except F1, whose subject is a Must AC (condition **b**), so `insert()`'s `score <= 0` guard was re-read from source (`highscore.cpp:13`, present). Re-derived from scratch because condition **(a)** applied (this round verifies a fix to that very fact): F2, F3, F4, F9, F10. F3's re-derivation additionally used a *different method* from Round 1's, since Round 1's own audit method was the defect.

## 2. Plan Conformance

| Task | Implemented as planned? | Note |
|---|---|---|
| T1 | ✅ | `RoundResult`, the wrapper's tick shape and the `reported_` latch are exactly Decision 2; `GalacticInvasion` gains exactly one member and the NFR-5 amendment is written in both places the DoD named. |
| T2 | ⚠️ | Table math correct, but the DoD's claim that score 0 was covered "via `insert()` itself remaining a no-op" is not in the test file, and `insert()` was in fact not a no-op → **F1**. |
| T3 | ✅ | Byte-at-a-time LE encoding, six rejection paths each its own case, byte offsets asserted independently. |
| T4 | ✅ | The extra `qualifies(slot, score)` method beyond the plan's literal list is recorded in the DoD, not silent. |
| T5 | ✅ | Rising-edge-only, latch-at-`begin()`, `.start` absent (lint-enforced). |
| T6 | ✅ (r2) | Round 1's Decision 8 deviation (**F2**) is closed: `nameGlyphCount()` + `kNameX + (kNameWidth - n*kGlyphAdvance)/2` mirrors the score line's own treatment exactly; re-verified from source and mutation-checked. |
| T7 | ✅ | Stub removed outright; the unplanned `highscore_game.h` touch is recorded in the DoD. |
| T8 | ⚠️ | End-to-end wiring is real and correct; the AC-5.1 "exactly once" sub-claim describes a scenario the test does not actually reach → **F7** (fixed). |
| T9 | ✅ | Deviation (2000 vs. 1200 ticks) recorded with reasoning; raw persisted bytes compared, not just decoded tables. |
| T10 | ✅ | Two dumps, `make view` wiring mirrors `GAME_DUMP`, doc row updated. |
| T11 | ✅ | Four measurements, N/2N ratio guard, full-tick-budget bar stated honestly as host-only. |
| T12 | ✅ | Two uncaught mutations turned into two isolating tests before closing — the row's own rule, honoured. |
| T13 | ⚠️ | Eight sub-rules, each demonstrated firing. The recorded conclusion that AC-1.4 "needed no new rule" is incomplete: the ban lists `nvs_flash.h` but not `nvs.h` → **F8** (fixed). |
| T14 | ✅ | Device build green, honestly scoped ("not flashed, not run"); `port/` is already covered by the pre-existing global no-alloc scan, so the block's "T14 extends if needed" note is legitimately a no-op. |
| T15 | ✅ (`blocked`) | Hardware absence verified two ways and recorded; AC-1.5 stays `not capturable`, never "passed". Correct per CLAUDE.md's split. |
| T16 | ✅ (r2) | Doc comments are real and good. **F3** closed: the byte layout now lives in `steamcore::detail`, and both artifacts that carried the over-broad audit claim (`plan.md` T16, `docs/host-tests.md:72–89`) now record the blind spot and the correction honestly rather than silently restating the old conclusion. |

## 3. Findings

| # | Severity | Location | Finding | Status |
|---|---|---|---|---|
| F1 | Major | `firmware/steamcore/src/highscore.cpp:5` | `insert()` was not a no-op for `score <= 0`, contradicting its own doc comment and A7: it wrote initials into the first empty rank, and a **negative** score persisted a block `decodeBlock()` then rejects as malformed — wiping *every* slot's table on the next `load()` (reproduced: slot 1's legitimate 500-point entry silently vanished). Unreachable via `HighscoreGame` today, but `insert()`/`record()` are the public API US-6 invites a second game to call. Fix applied: `if (score <= 0) return;` + doc aligned; regression tests added at table and store level (`highscore_table_test.cpp:51`, `highscore_store_test.cpp:136`), both verified to fail against the unfixed code. | fixed |
| F2 | Major | `firmware/steamcore/src/highscore_screen.cpp:89` | The TABLE screen's `displayName` is drawn at the fixed `kNameX` (= 52, centred for the worst-case 17-glyph name) instead of being centred *within* that proven field the way the score line is. Plan §1 Decision 8 requires the worst-case-field + centre-within-field treatment for "the score … **and the display name**"; the deviation is undocumented. Every test and the `make view` dump uses the 17-glyph `"GALACTIC INVASION"`, where the bug is invisible. An 11-glyph name renders 24 px left of centre, a 4-glyph one 52 px; a >17-glyph name silently overruns the compile-time-proven field (clipped by `drawText`, no UB). Fix applied (fix-mode, post-Round-1): `nameGlyphCount()` computes the actual glyph count (clamped to `kMaxNameGlyphs`) and offsets by `(kNameWidth - actual*kGlyphAdvance)/2`, exactly as `drawInitialsEntryScreen` already does for the score; `highscore_screen_table_header_stays_inside_and_centred_in_its_field` added (both a short and the worst-case name), verified to fail against the unfixed code. **r2 confirmation:** `kNameX`/`kNameWidth` are genuinely the *field's* left edge and worst-case width (`highscore_screen.h:143–144`), so the offset is a true centring, not a double-offset; the test's short case is a 2-glyph `"AB"`, not the 17-glyph name; I re-ran the mutation myself (`nameX = kNameX`) and the test fails at `highscore_screen_test.cpp:153`, then restored byte-identically. The `make view` TABLE dump is unaffected (17 glyphs ⇒ offset 0). | fixed r2 |
| F3 | Major | `firmware/steamcore/include/steamcore/highscore.h:140`–`172` | NFR-4 promises "internal byte layout, the format-version constant's value, and rank-shifting logic stay private". All of `kMagic`, `kFormatVersion = 1`, `kBitsPerByte`, `kBytesPerU32`, `kEntryBytes`, `kTableBytes`, `kTablesBytes`, `kHeaderBytes`, `kChecksumBytes`, `HighscoreBlock`, `encodeBlock()`, `decodeBlock()` and `insert()` are public `steamcore::` names — and this codebase already has the idiomatic home for exactly that (`steamcore::detail` in `input.h`, `analog_axis.h`, `game_loop.h`, `title_screen.h`). T16's audit could not have found this: scoping `public:`→`private:` inside class bodies structurally never sees namespace-scope names, yet `plan.md` T16 and `docs/host-tests.md` both record the audit as showing no exposed byte offset or version value. Fix applied (fix-mode, post-Round-1): every listed symbol moved into `steamcore::detail` in both `highscore.h` and `highscore.cpp`; `kBlockSize` re-exported at the top level as `inline constexpr int32_t kBlockSize = detail::kBlockSize;` (a `Backend` implementer needs it). Five test files that referenced `insert()`/`HighscoreBlock`/`encodeBlock`/`decodeBlock` directly updated to `using steamcore::detail::...`. Re-ran the `awk` audit: `highscore.h`'s top-level surface is now exactly `HighscoreEntry`, `HighscoreTable`, `isOccupied`, `count`, `qualifies`, `kGameSlotCount`, `kBlockSize`, `HighscoreStore` — nothing else. `make test`/`-asan`/`-gcc`/`lint` and `idf.py build` all re-confirmed green after the restructuring. **r2 confirmation, re-derived by two methods that do not share Round 1's blind spot:** (i) a namespace-depth-tracking parse of `highscore.h` lists the top-level `steamcore::` surface as exactly `kLetterCount`, `kFirstLetter`, `kInitialsCount`, `kTableSize`, `kMaxStoredScore`, `HighscoreEntry`, `HighscoreTable`, `isOccupied`, `count`, `qualifies`, `kGameSlotCount`, `kBlockSize`, `HighscoreStore` — nothing byte-layout-related; (ii) a compile-level probe (`using steamcore::X;`) fails for all 13 moved names and succeeds for all of them under `steamcore::detail::`, with `steamcore::kBlockSize == 191` still reachable as intended. `grep` for `steamcore::<oldname>` across `*.cpp`/`*.h`/`*.md`/`*.sh` is empty; no `detail`-namespace name collides with `input.h`/`game_loop.h`/`analog_axis.h`/`title_screen.h`. `kBlockSize`'s re-export is a total size, not an offset or a version value, and is documented as such — NFR-4 is met, not narrowly evaded. | fixed r2 |
| F4 | Minor | `.spark/highscore-system/plan.md:20` | Handoff still reads `Open: 16 tasks not done` while §3 shows 15 `done` + 1 `blocked`. Logged here per the plan's own on-conflict rule; §3 taken as authoritative throughout this review. Fix applied (fix-mode, post-Round-1): Handoff line refreshed. **r2 confirmation:** `plan.md:20` now reads `Open: 15 done, 1 blocked (T15, hardware-gated Should — no board attached)`, agreeing with §3. | fixed r2 |
| F5 | Minor | `firmware/steamcore/include/steamcore/highscore.h:182` | `HighscoreStore`'s contract did not say that `record()` re-encodes and writes the **whole** block, so recording into a store that was never `load()`ed silently overwrites every other slot's persisted entries with an empty block — a real data-loss footgun for the reusable API (NFR-5 / library-lens contract clarity). The one shipped consumer (`highscore_harness_game.h:47`) does it right. Fix applied: the "call `load()` exactly once, before the first `record()`" paragraph added to the type's contract. | fixed |
| F6 | Minor | `firmware/steamcore/include/steamcore/highscore_screen.h:71` | AC-2.1's label text (`HIGH SCORE`, C10) was asserted by no test: the literal appears exactly once in the whole repo — in production code — and both header-detection helpers build their expected frame from `kEntryHeaderText` itself, so a silent text change would keep the suite green (`title_screen.h` avoids this by keeping its strings in `detail` and restating them in the test). Fix applied: `highscore_screen_entry_header_label_reads_high_score` restates the literal independently. | fixed |
| F7 | Minor | `firmware/steamcore/include/steamcore/highscore_game.h:77` | AC-5.1's "reported exactly once" latch (`reported_`) had no test that would fail if it were deleted — once the flow opens it owns every tick, so the qualifying path can never show the difference, and the e2e test's 200 "GAME_OVER" ticks actually run while the flow sits on its TABLE screen (its comment claimed the flow had finished). Fix applied: counting-stub test `highscore_game_asks_the_store_once_per_ending_not_once_per_tick` (asserts exactly 1 `qualifies()` call across an ended, un-latched round + 200 further ticks), and the misleading comment corrected. | fixed |
| F8 | Minor | `tools/check_constraints.sh:58`, `:698` | The AC-1.4 "no ESP-IDF header in logic code" ban lists `nvs_flash.h` but not `nvs.h` — and `nvs.h` is the header this feature actually introduced (`port/esp32/nvs_highscore_backend.h:5`, for `nvs_handle_t`). T13's DoD recorded "needed no new rule" on the strength of the `nvs_flash.h` entry alone. No violation exists today (`grep -rn nvs include/ src/` is empty). Fix applied: `nvs\.h` added to both ban sites; demonstrated firing against a planted `#include "nvs.h"` in `src/highscore.cpp`, then reverted byte-identically (`diff` clean) and `make lint` re-run. | fixed |
| F9 | Nit | `firmware/steamcore/include/steamcore/highscore.h:90` | `qualifies()` compares the raw score while `insert()` clamps to `kMaxStoredScore`: on a table whose lowest entry is already 99999, a score of 100000 qualifies but inserts nothing — the player would type three initials and see no entry appear. Unreachable for galactic-invasion (its scores are far below the clamp). Fix applied (fix-mode, post-Round-1): `qualifies()` now clamps identically to `insert()`; `highscore_table_qualifies_agrees_with_insert_at_the_clamp_ceiling` added, verified to fail against the unfixed code. **r2 confirmation:** `highscore.h:100–103` clamps identically to `highscore.cpp:14`; I traced both branches (`n < kTableSize` needs no clamp — any positive score fills an empty rank and `insert()` agrees) and re-ran the mutation myself (`score >` instead of `clamped >`), which fails the test at `highscore_table_test.cpp:220`; restored byte-identically. | fixed r2 |
| F10 | Nit | `firmware/steamcore/include/steamcore/highscore.h:201` | `load()`'s `uint8_t bytes[kBlockSize]` is uninitialised; a `Backend` returning `true` without filling every byte would feed indeterminate values into `decodeBlock`. Both shipped backends are correct, so this is defence in depth only. Fix applied (fix-mode, post-Round-1): `uint8_t bytes[kBlockSize] = {};`. **r2 confirmation:** present at `highscore.h:238`. `record()`'s own `uint8_t bytes[kBlockSize];` (`:277`) is deliberately left uninitialised and is correct — `encodeBlock()` writes all 191 bytes before any read (`highscore.cpp:95–118`, offset advances `12 + 175 + 4`). | fixed r2 |

| F11 | Nit | `firmware/steamcore/src/highscore_screen.cpp:77` | Regression introduced *by* the F2 fix: `nameGlyphCount()` reads `name[0]` before `drawText` ever sees the pointer, so `drawHighscoreTableScreen(fb, nullptr, table)` — a documented no-op before the fix, since `text.h:39` makes a nullptr `text` a no-op and the old code forwarded straight to it — became UB. Reproduced under UBSan: `runtime error: load of null pointer of type 'const char'` at `highscore_screen.cpp:77`. Unreachable today (both call sites pass a literal: `highscore_flow.cpp:51` ← `highscore_game.h:89/:99` ← the harness's own string) and the header never promised nullptr tolerance, so this is defence in depth of the same class as F10 — but it is a silent narrowing of behaviour the shipped `drawText` contract otherwise guarantees throughout this codebase. Fix applied by me (r2), one line: `if (name == nullptr) return 0;` with the reasoning in the helper's comment; UBSan probe re-run clean, and all three host suites (318/318), `make lint` and `idf.py build` re-run green after it. | fixed r2 |

## 4. Requirements Traceability

| Spec ID | Implemented at | Verdict |
|---|---|---|
| AC-1.1 | `highscore.h:90` (`qualifies`), `highscore.cpp:5` (`insert`) | ✅ met (after F1) |
| AC-1.2 | `highscore.h:200` + `highscore_store_test.cpp:25`, `galactic_invasion_highscore_test.cpp:144` | ✅ met |
| AC-1.3 | `highscore.cpp:118` (`decodeBlock`), `highscore_block_test.cpp` (12 rejection cases incl. all-`0xFF`/all-`0x00`) | ✅ met |
| AC-1.4 | `highscore.h`/`.cpp` (no ESP-IDF, no alloc), lint block `check_constraints.sh:796+` | ✅ met (guard widened, F8) |
| AC-1.5 (Should) | — | ⚠️ not capturable — T15 `blocked`, no board attached; never claimed as passed |
| AC-2.1 | `highscore_screen.cpp:38`, `highscore_screen.h:105–119` (static_asserts) | ✅ met |
| AC-2.2 | `initials_entry.cpp:15` | ✅ met |
| AC-2.3 | `initials_entry.cpp:32`, `highscore_flow.cpp:17` | ✅ met |
| AC-2.4 | `.start` absent from `initials_entry.{h,cpp}` + lint sub-rule (d) | ✅ met |
| AC-2.5 | `initials_entry_test.cpp:124`, `highscore_determinism_test.cpp:65` | ✅ met |
| AC-3.1 | `highscore_screen.cpp:103`, `highscore_flow.cpp:50` | ✅ met |
| AC-3.2 | `highscore_screen.cpp:114` (`count(table)` rows only) | ✅ met |
| AC-3.3 | `highscore_flow.cpp:29`, `highscore_game.h:71` (real input on the finishing tick) | ✅ met |
| AC-4.1 (Should) | `highscore_game.h:86`, `galactic_invasion_highscore_test.cpp:246` (byte-identical unwrapped frames) | ✅ met |
| AC-5.1 | `highscore_game.h:74–82` | ✅ met (latch now tested, F7) |
| AC-5.2 | `galactic_invasion.h:321` — exactly one additive member; amendment recorded in both doc locations | ✅ met |
| AC-5.3 | Full pre-existing suite green (318/318, clang/asan/gcc) | ✅ met |
| AC-6.1 (Should) | `highscore.h:213`/`:236` — `slot` is the only identifier; no game name inside the store | ✅ met |
| AC-6.2 (Should) | `highscore_store_test.cpp:85`/`:104` (both orders) | ✅ met |
| NFR-1 | `bench_highscore.cpp` (4 measurements, N/2N ratio guard) | ✅ (figures re-measured at `/demo-day`) |
| NFR-2 | lint sub-rule (b) over engine+test+bench | ✅ |
| NFR-3 | lint sub-rule (c) + `highscore_determinism_test.cpp` | ✅ |
| NFR-4 | `highscore.h` namespace scope (byte layout, `kFormatVersion`, `insert()` all in `steamcore::detail`) | ✅ met r2 — F3 closed, re-audited two ways |
| NFR-5 | Contract blocks in all six new headers | ✅ (after F5) |
| NFR-6 | `decodeBlock` version+slot-count+checksum+well-formedness; lint (f)/(g) | ✅ |
| NFR-7 | `highscore_screen.cpp:60` (`-` underline), `BRIGHT_ORANGE` on `BLACK` | ✅ |
| NFR-8 / NFR-9 | N/A per spec; NFR-9's accepted silent-write-failure recorded in `highscore.h` and R11 | ✅ |
| NFR-10 | `port/esp32/nvs_highscore_backend.*` only; `nvs_flash` added to `REQUIRES` only | ✅ |

## 5. What Was Checked

- [x] Correctness: logic does what the acceptance criteria demand — every Must AC traced above; Round 1's three probes plus, this round, two mutation checks (F2's centring, F9's clamp — both restored byte-identically) and one UBSan probe that found F11
- [x] Non-functional: applicable NFRs and constitution quality bars hold — §6 format-version non-negotiable is genuinely enforced (version **and** an independent slot-count guard **and** a checksum **and** semantic well-formedness); no dynamic allocation; no clock/RNG; no ESP-IDF header in logic code; no resolution/GPIO literal. NFR-4 (library lens, public API surface) re-audited independently this round by two methods and now genuinely met
- [x] Error handling: failures are handled, not swallowed — a failed read/write, a first boot, a wrong version and a corrupt block are all ordinary, documented outcomes; nothing is caught-and-ignored
- [x] Security: no injected input trusted, no secrets in code — the only external bytes are the flash block, fully validated before use; initials are structurally A–Z; offline device, no PII (constitution §2)
- [x] Tests: exist, are meaningful, and pass — 318/318 on clang/asan/gcc; the suite's independent-oracle discipline (`placeExpected`, an independent FNV-1a, an intersection helper sharing no code with `overlaps()`) is genuinely strong, and every production-affecting fix across both rounds has now been mutation-checked against the unfixed code — the two Round-2 ones by me, not on the fixer's word
- [x] Readability: the next developer will understand this — doc comments are the best in the repo; naming and file layout follow established convention

## 6. Verdict

This passes. Both Round-1 Majors are genuinely closed, not papered over, and I confirmed each from source rather than from the fix report. The TABLE screen now centres its display name inside the same worst-case field the score line already used — I checked that `kNameX`/`kNameWidth` really are the field's origin and width (so the offset is a centring, not a double-offset), that the new test exercises a 2-glyph name and not only the 17-glyph one where the bug was invisible, and that the test actually fails when I re-introduce the bug. The `steamcore::detail` move genuinely closes NFR-4: audited two ways that both cover Round 1's exact blind spot — a namespace-depth parse listing the top-level surface, and a compile-level probe proving all thirteen moved names no longer resolve at `steamcore::` while every one of them resolves under `detail::` — with `kBlockSize` the single, justified, documented re-export (a total size, not an offset or a version value). `qualifies()` and `insert()` now agree at the clamp ceiling in both branches, and that test too fails under my own mutation. The plan's Handoff line and the two artifacts that had recorded the over-broad audit claim are all corrected honestly, which matters more than the code fix: a future reader now learns the blind spot instead of inheriting it. The restructuring caused no collateral damage — no stale references to the old top-level names anywhere, no `detail`-namespace collisions with the four other headers using that namespace, the `make view` TABLE dump unaffected, and 318/318 green on clang, ASan and gcc plus a green `idf.py build`. The one thing the fixes did introduce is F11: centring made `drawHighscoreTableScreen` dereference a nullptr `displayName` that the old code, by forwarding straight to `drawText`, had handled safely — unreachable today, a Nit, and fixed here in one line with the suites re-run after it. AC-1.5 remains honestly unverified and must stay that way until a board is attached; `/demo-day` may start.

---

## ✅ REVIEW GATE

*All boxes checked → `/demo-day` may start. Any box open → back to `/increment`. On
re-review, edit this same checklist in place — never duplicate it as a second gate.*

- [x] No open Blocker findings
- [x] No open Major findings — **F2** and **F3** both confirmed `fixed r2` from source, each re-derived independently; no waiver needed or used
- [x] Every Must AC traces to implementing code; no constitution non-negotiable violated
- [x] All plan deviations documented and accepted — T6's Decision 8 deviation (F2) is now implemented as planned; T16's audit-method gap (F3) is recorded in `plan.md` T16 and `docs/host-tests.md` rather than silently corrected; T2's/T8's/T13's DoD overclaims (F1/F7/F8) are all fixed and recorded
- [x] Test suite runs green — `make test` / `test-asan` / `test-gcc` **318 passed, 0 failed**; `make lint` OK; `idf.py build` green (0x3f0d0, 75% free) — all re-run by me, last after the F11 fix
- [x] Line budget respected: Ist 126 / Soll ~150 (excluding HTML comments)
- [x] Status set to `passed`
