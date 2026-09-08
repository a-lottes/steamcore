# QA Report: highscore-system

| | |
|---|---|
| **Phase** | Review (hands-on) |
| **Owner** | QA Tester (`/demo-day`) |
| **Input** | Host toolchain (constitution §8 substitute — no browser-observable surface exists), `.spark/highscore-system/spec.md` |
| **Status** | `passed` |
| **Round** | 1 |
| **Date** | 2026-09-08 |

**Handoff**
- **Status:** `passed`.
- **Verdict:** Yes — I'd demo this. Every Must AC except the hardware-gated AC-1.5 is verified by actually running the named test myself and reading its real output: 318/318 on clang, ASan and gcc; `make lint`/`make bench`/`make view` all run by me; both new PNGs decoded and visually inspected. AC-1.5 is `not capturable` — no board attached this session, verified myself (`ls /dev/cu.usbmodem*` empty, `system_profiler SPUSBDataType` shows no serial/ESP device) — never marked passed, exactly per plan T15's own anticipated `blocked` state.
- **Open:** `none` — 0 Blockers, 0 Majors, 0 Minors found this round.
- **Binding ruling:** §5 Verdict and the gate checklist below.
- **On conflict:** the numbered body below wins for everything except `Status`.

## 1. Test Environment

- **Method (constitution §8):** no browser-observable surface (fixed ESP32-S3 arcade console). Declared substitute: framebuffer dump over USB-CDC + serial transcript for the hardware half, host-compiled unit tests for the logic half. Verified myself this session which half is enforceable: `source ~/esp/esp-idf/export.sh && idf.py build` from `firmware/system/` built green (`steamcore_system.bin` 0x3f0d0, 75% of app partition free) — toolchain works; `ls /dev/cu.usbmodem*` found no match and `system_profiler SPUSBDataType` shows no serial/ESP32 device — no board attached, so the live framebuffer-dump/serial-transcript half is not capturable this session. Every host-verifiable AC in this spec was confirmed by a command I ran and output I read myself.
- **Commands run (repo root):** `make test`, `make test-asan`, `make test-gcc`, `make lint`, `make bench`, `make view`, ~40 individual test names run one-by-one via `build/steamcore_tests <name>`, plus `idf.py build` from `firmware/system/`, direct reads of `games/galactic_invasion/galactic_invasion.h` and `firmware/steamcore/include/steamcore/highscore.h`.
- **Hardware/framebuffer half:** not capturable — no board connected (checked myself, above). Affects AC-1.5 only, exactly as plan.md T15 anticipated (hardware-gated Should).
- **Test data:** the shipped suite's own deterministic fixtures (`FakeFlashBackend`, scripted input sequences); no external test data needed.

## 2. Acceptance Criteria Verification

| Spec ID | Steps performed | Expected | Observed | Result |
|---|---|---|---|---|
| AC-1.1 | Ran `highscore_table_zero_score_never_qualifies_even_when_empty`, `highscore_table_a_non_positive_score_never_occupies_a_rank`, `highscore_table_insertion_at_each_rank_shifts_and_discards_correctly`, `highscore_table_insertion_into_a_partial_table_fills_the_next_slot`, `highscore_table_a_tying_score_does_not_bump_the_lowest`, `highscore_table_stays_sorted_and_hole_free_after_fifty_inserts` | score>0 qualifies+inserts at correct rank with shift/discard; 0/negative never qualify | all `1 passed, 0 failed` | ✅ pass |
| AC-1.2 | Ran `highscore_store_survives_reconstruction_over_the_same_backend` | fresh `HighscoreStore` over same backend bytes reads back identical table | `1 passed, 0 failed` | ✅ pass |
| AC-1.3 | Ran `highscore_block_rejects_wrong_format_version_even_with_a_valid_checksum`, `highscore_block_rejects_wrong_slot_count_even_with_a_valid_checksum`, `highscore_block_rejects_a_checksum_mismatch`, `highscore_block_rejects_an_all_0xff_block`, `highscore_block_rejects_an_all_zero_block` | wrong-version/corrupt bytes yield a clean empty table, never a crash | all `1 passed, 0 failed` | ✅ pass |
| AC-1.4 | Ran `make lint` (highscore-system block, 4 sub-rules incl. no-ESP-IDF-header, no-dynamic-alloc); direct `grep` myself for `new `/`malloc`/`std::vector`/`std::string`/ESP-IDF headers across all 6 new engine headers + 4 new `.cpp` files | zero dynamic allocation, no ESP-IDF header, no filesystem dependency outside `port/` | `make lint OK`; my own grep: zero matches | ✅ pass |
| AC-1.5 (Should, hardware-gated) | Checked myself: `ls /dev/cu.usbmodem*` (no match), `system_profiler SPUSBDataType` (no serial/ESP device) | real power-cycle survival on physical board | no board attached this session | ⚠️ not capturable — never claimed as passed, matches plan.md T15's anticipated `blocked` state |
| AC-2.1 | Ran `highscore_screen_entry_header_label_reads_high_score`, `highscore_screen_entry_elements_never_overlap_at_any_score_width`, `highscore_screen_entry_screen_draws_nothing_outside_its_own_rows`; decoded `build/highscore_entry_pattern.png` myself | `HIGH SCORE` header, score, 3 letters, underline; no WIN/LOSS text; replaces game's end screen | all `1 passed, 0 failed`; PNG shows exactly `HIGH SCORE` / `99999` / `GOA`, no outcome text | ✅ pass |
| AC-2.2 | Ran `initials_entry_holding_up_advances_exactly_one_letter`, `initials_entry_26_press_sweep_wraps_back_to_a_and_stays_in_range` | one step per rising edge, A–Z wrap, no auto-repeat | both `1 passed, 0 failed` | ✅ pass |
| AC-2.3 | Ran `initials_entry_three_fire_edges_submit_on_the_third` | third fire-edge locks + submits all 3 letters same tick | `1 passed, 0 failed` | ✅ pass |
| AC-2.4 | Ran `initials_entry_start_held_throughout_changes_nothing`; `make lint` sub-rule (d) "no `.start` read in `initials_entry.{h,cpp}`" | `start` has no effect, never reused | test `1 passed, 0 failed`; lint clean (structural, not just behavioral) | ✅ pass |
| AC-2.5 | Ran `initials_entry_replay_is_byte_identical_every_tick` | same input sequence twice → byte-identical letter/cursor sequence | `1 passed, 0 failed` | ✅ pass |
| AC-3.1 | Ran `highscore_screen_table_draws_exactly_count_rows`, `highscore_table_screen_dumps_to_disk_for_visual_check`; decoded `build/highscore_table_pattern.png` myself | header + one line per occupied rank, `drawText` only | test `1 passed, 0 failed`; PNG shows `GALACTIC INVASION` + 5 correctly-ranked, legible rows (1. ZZZ 99999 ... 5. AAA 10) | ✅ pass |
| AC-3.2 | Ran `highscore_screen_table_elements_never_overlap_at_any_row_count` (1/2/5-entry cases) | only occupied ranks drawn, no placeholder rows | `1 passed, 0 failed` | ✅ pass |
| AC-3.3 | Ran `galactic_invasion_highscore_start_held_through_flow_restarts_on_one_press`, `highscore_flow_start_during_entry_changes_nothing` | `start` on TABLE screen proceeds straight into the existing restart path, no extra screen | both `1 passed, 0 failed` | ✅ pass |
| AC-4.1 (Should) | Ran `highscore_game_non_qualifying_round_matches_unwrapped_frame`, `galactic_invasion_highscore_non_qualifying_round_is_untouched` | non-qualifying round's end screen renders exactly as before, byte-identical to unwrapped game | both `1 passed, 0 failed` | ✅ pass |
| AC-5.1 | Ran `galactic_invasion_highscore_reports_the_rendered_score_exactly_once`, `highscore_game_asks_the_store_once_per_ending_not_once_per_tick` | reported score equals the round's actual final score, exactly once | both `1 passed, 0 failed` | ✅ pass |
| AC-5.2 | Read `games/galactic_invasion/galactic_invasion.h` myself (public section + doc comment) | exactly one additive public member, no lives/formation/other accessor, NFR-5 amendment recorded | confirmed by direct read: public surface is `GalacticInvasion()`, `update()`, `render()`, `roundResult()` only; doc comment at line ~96/109 records the amendment | ✅ pass |
| AC-5.3 | Ran full suite: `make test`/`test-asan`/`test-gcc` | galactic-invasion's own pre-existing suite still passes unchanged | 318/318 on all three (no regression vs. the 255-case baseline plus this feature's own new cases) | ✅ pass |
| AC-6.1 (Should) | Read `HighscoreStore` class body in `highscore.h` myself | `slot` int is the only per-call identifier; no game name/logic hardcoded inside | confirmed by direct read — `table()`/`qualifies()`/`record()` take `slot` only, comment states "holds no game-specific name or branch anywhere in it" | ✅ pass |
| AC-6.2 (Should) | Ran `highscore_store_two_slots_never_cross_contaminate_forward_order`, `highscore_store_two_slots_never_cross_contaminate_reverse_order` | two synthetic slots never cross-contaminate, either order | both `1 passed, 0 failed` | ✅ pass |
| NFR-1 | Ran `make bench` myself | record/load/render each a small fraction of the 16.667 ms tick budget | `record()` 0.334 µs, `load()` 0.398 µs, `render()[ENTRY]` 6.421 µs, `render()[TABLE]` 6.450 µs — all ratios 1.96–2.02x (no `-O2` dead-code elision); `BENCH OK` | ✅ pass |
| NFR-2 | Ran `make lint` sub-rule (b) over engine+test+bench file set | zero dynamic allocation anywhere in the delivered code/tests | rule clean, `make lint OK` | ✅ pass |
| NFR-3 | Ran `make lint` sub-rule (c); ran `highscore_composed_feature_replay_is_byte_identical_every_tick` (2000-tick full-feature replay) | no wall-clock/RNG; byte-identical replay across two independent instances | lint clean; test `1 passed, 0 failed` | ✅ pass |
| NFR-4 | Read `highscore.h` myself: confirmed `kMagic`, `kFormatVersion`, `HighscoreBlock`, `encodeBlock`/`decodeBlock` all live inside `namespace detail`, only `kBlockSize` re-exported at top level; confirmed `GameLoop`/`GameSession`/`GameInput`/`Framebuffer`/`drawText` headers untouched (`git status` on those paths) | internal byte layout/version/rank logic stay private; no change to existing public contracts | confirmed by direct read — matches review F3's fixed state exactly | ✅ pass |
| NFR-5 | Read `highscore.h`'s top-of-file Contract block myself | states qualification rule, format-version meaning/mismatch behavior, initials-entry input contract, one usage example | all four present, verbatim, read in full | ✅ pass |
| NFR-6 | Same tests as AC-1.3, plus `make lint` sub-rules (f)/(g) (format-version + slot-count guard text presence) | every persisted block carries a format-version field; mismatch/corrupt read always yields clean empty table | all pass; lint clean | ✅ pass |
| NFR-7 | Ran `highscore_screen_underline_sits_beneath_the_active_letter_only`; decoded `build/highscore_entry_pattern.png` myself | `BRIGHT_ORANGE` on `BLACK`; static `-` underline one row beneath the active letter only | test `1 passed, 0 failed`; PNG visually confirms the underline sits under exactly the 3rd letter position (orange text, black background) | ✅ pass |
| NFR-8 / NFR-9 | N/A per spec (offline device, no PII; no logging surface beyond AC-1.3's already-required safe handling) | N/A | N/A | ✅ N/A |
| NFR-10 | Read `firmware/system/main/CMakeLists.txt`; ran `idf.py build` myself | only already-shipped engine types composed; the one new dependency (`nvs_flash`) stays behind `port/esp32/` | `idf.py build` green (`steamcore_system.bin` 0x3f0d0, 75% free); `nvs_flash`/`nvs.h` confined to `port/esp32/nvs_highscore_backend.{h,cpp}` per lint | ✅ pass |

## 3. Exploratory Findings

| # | Severity | Steps to reproduce | Expected vs. observed | Status |
|---|---|---|---|---|
| — | — | Ran `highscore_table_a_score_above_the_max_is_clamped_on_insert` and `highscore_table_qualifies_agrees_with_insert_at_the_clamp_ceiling` (the F9 regression pair) individually | Expected: `qualifies()` and `insert()` agree at the 99999 clamp ceiling (review F9's fixed state). Observed: both `1 passed, 0 failed`. No finding — confirms the fix myself rather than trusting review's word. | n/a |
| — | — | Ran `highscore_screen_table_header_stays_inside_and_centred_in_its_field` (the F2 regression test, 2-glyph + 17-glyph cases) | Expected: display name centred within its worst-case field at any length (review F2's fixed state). Observed: `1 passed, 0 failed`. No finding. | n/a |
| — | — | `grep -rn "TODO\|FIXME\|XXX" firmware/steamcore/include/steamcore/highscore*.h firmware/steamcore/src/highscore*.cpp firmware/steamcore/src/initials_entry.cpp` and all 11 new test files | Expected: none left in shipped code. Observed: zero matches. No finding. | n/a |
| — | — | `git status --porcelain` on `game_loop.h`, `game_state.{h,cpp}`, `framebuffer.{h,cpp}`, `text.{h,cpp}`, `font.{h,cpp}`, `title_screen.{h,cpp}`, `collision.h`, `sprite.h`, `config.h` | Expected: zero diff (NFR-4's "unmodified primitives" claim). Observed: empty — confirmed myself, matching review's NFR-4 row. No finding. | n/a |
| — | — | Decoded `build/highscore_entry_pattern.png` and `build/highscore_table_pattern.png` via `make view` and viewed both directly | Expected: entry screen shows header/score/letters/underline; table screen shows 5 legible, non-overlapping, correctly-ranked rows. Observed: exactly that. No finding. | n/a |
| — | — | Ran `idf.py build` myself from `firmware/system/` (fresh, this session) | Expected: green build, matching plan T14/T16's claims. Observed: `steamcore_system.bin` 0x3f0d0 bytes, 75% of app partition free — matches review.md exactly. No finding. | n/a |

No Blocker, Major or Minor bugs found this round.

## 4. Console & Network

N/A — no browser console or network surface exists (constitution §8). Host-CI equivalent (compiler warnings, lint) checked clean: `-Wall -Wextra -Werror` on clang/ASan/gcc all produced zero warnings; `make lint OK` with all highscore-system sub-rules reporting clean.

## 5. Verdict

Yes, I would demo this right now. Every Must AC (US-1 through US-3, US-5) and every Should AC (US-4, US-6, and AC-1.5's honest exception) was verified by running the actual named test myself and reading its real pass/fail output, or by reading the actual source myself for the two ACs that are source-level claims (AC-5.2, AC-6.1) — never inferred from `/peer-review`'s prior description. I independently re-ran `make test`/`test-asan`/`test-gcc` (318/318 all three), `make lint` (clean), `make bench` (four measurements, all comfortably under the 16.667 ms budget, N/2N ratios all in the expected 2x band so `-O2` isn't silently eliding the work), `make view`, and decoded and visually inspected both new PNGs myself rather than trusting the dump test's own description of them — the entry screen's underline sits under exactly the third letter and the table screen's five rows are legible and correctly ranked. I also independently confirmed the two Round-2 review fixes (F2's centring, F9's clamp-agreement) by running their regression tests myself, and independently re-derived NFR-4's `steamcore::detail` namespace-scope claim by reading `highscore.h` myself rather than citing the review's audit. I verified the toolchain/hardware state myself rather than trusting the prompt: `idf.py build` from `firmware/system/` built green this session (`steamcore_system.bin` 0x3f0d0 bytes, 75% partition free), and no board is attached (`ls /dev/cu.usbmodem*` empty, `system_profiler SPUSBDataType` shows no serial/ESP device) — so AC-1.5 is recorded here as `not capturable`, never as passed, exactly matching plan.md T15's own anticipated `blocked` state. Nothing in this report rests on reading source and asserting it "should work" without a command backing it.

---

## ✅ QA GATE

*All boxes checked → `/go-live` may start. Any box open → back to `/increment`, then re-run
`/demo-day`. On re-test, edit this same checklist in place — never duplicate it as a second gate.*

- [x] Every Must-story acceptance criterion verified via the host-CI substitute (constitution §8) and passed
- [x] Every host-observable NFR verified and passed (NFR-1–NFR-7, NFR-10; NFR-8/9 N/A per spec)
- [x] No open Blocker or Major bugs (none found this round)
- [x] Host toolchain output (compiler warnings, lint) free of errors on every tested path
- [x] Tested on the one agreed "surface" this project has — the host toolchain (no viewports apply; no browser exists); AC-1.5's real-hardware half is honestly `not capturable`, not silently skipped
- [x] Line budget respected: Ist 133 / Soll ~130 (excluding HTML comments) — a 3-line overage from the AC table's Should-scope columns (AC-1.5/AC-4.1/AC-6.1/AC-6.2), not padding
- [x] Status set to `passed`
