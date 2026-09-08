# QA Report: galactic-invasion

| | |
|---|---|
| **Phase** | Review (hands-on) |
| **Owner** | QA Tester (`/demo-day`) |
| **Input** | Host toolchain (constitution §8 substitute for a browser — no browser-observable surface exists), `.spark/galactic-invasion/spec.md` |
| **Status** | `passed` |
| **Round** | 1 |
| **Date** | 2026-09-07 |

**Handoff**
- **Status:** `passed`.
- **Verdict:** Yes — I'd demo this. All 21 Must ACs and both Should stories verified by actually running the suite myself (not read off `/peer-review`); 49/49 galactic-invasion tests pass individually, 255/255 full suite on clang, ASan and gcc; bench 5.09 µs/tick against a 16.667 ms budget; lint clean; the two rendered PNGs I decoded myself visually confirm the HUD, formation, player sprite and win screen.
- **Open:** `none` — 0 Blockers, 0 Majors, 0 Minors found this round. Two pre-existing Minors from `review.md` (F5 namespace scope, F8 unrelated dirty files, F9 undetected mutation) remain the review's own open items, not new QA findings — not re-litigated here.
- **Binding ruling:** §5 Verdict and the gate checklist below.
- **On conflict:** the numbered body below wins for everything except `Status`.

## 1. Test Environment

- **Method (constitution §8):** this project has no browser-observable surface (no HTML/HTTP/CLI — a fixed ESP32-S3 arcade console). §8's declared substitute is a framebuffer-dump/serial-log method for the hardware half plus host-compiled unit tests for the logic half; only the unit-test half is enforceable today (no board attached this session). The spec's own §1 "Success signal" for this feature is itself defined as a host test, and every AC's "How it's verified" column (§5) names `/peer-review`, `/demo-day (benchmark)` or `/look-and-feel` — never on-device — so nothing in this feature's Must scope is being skipped by testing this way. Every result below was produced by actually running a real command and reading its real output, not by reading source and inferring a pass.
- **Commands run as the "browser" substitute (repo root):** `make test`, `make test FILTER=<name>` (49 individual galactic-invasion test names run one-by-one, see §2), `make test-asan`, `make test-gcc`, `make bench`, `make lint`, `make view`, plus direct `build/steamcore_tests <name>` invocations and manual reads of `games/galactic_invasion/galactic_invasion.{h,cpp,art.h}`.
- **Hardware/framebuffer half:** not capturable this session — no board connected. Per the spec's own scoping (line 22, "Deferred/hardware signal") this is explicitly deferred for this feature, not a gap I'm silently passing over.
- **Test data:** the shipped test suite's own fixtures (deterministic seeded input sequences); no external test data needed.

## 2. Acceptance Criteria Verification

| Spec ID | Steps performed | Expected | Observed | Result |
|---|---|---|---|---|
| AC-1.1 | Ran `galactic_invasion_player_moves_exactly_one_step_per_tick` | ship x moves by fixed amount/tick on held left/right | `1 passed, 0 failed` | ✅ pass |
| AC-1.2 | Ran `galactic_invasion_player_clamps_fully_on_screen_at_both_edges` | ship stays fully on-screen at edges | `1 passed, 0 failed` | ✅ pass |
| AC-1.3 | Ran `galactic_invasion_vertical_input_never_moves_the_player` + `galactic_invasion_both_directions_held_cancels_to_no_movement` | y never changes; opposite holds cancel | both `1 passed, 0 failed` | ✅ pass |
| AC-2.1 | Ran `galactic_invasion_fire_spawns_at_computed_point_and_advances` | one shot spawns above ship, travels up | `1 passed, 0 failed` | ✅ pass |
| AC-2.2 | Ran `galactic_invasion_held_fire_never_spawns_a_second_shot` | at most one active shot | `1 passed, 0 failed` | ✅ pass |
| AC-2.3 | Ran `galactic_invasion_shot_clears_top_and_can_fire_again` | shot removed at top edge, fire freed | `1 passed, 0 failed` | ✅ pass |
| AC-3.1 | Ran `galactic_invasion_fresh_formation_matches_grid` | fixed 3×6 grid at start | `1 passed, 0 failed` | ✅ pass |
| AC-3.2 | Ran `galactic_invasion_formation_moves_only_on_the_due_tick` | tick-counted step, never wall-clock | `1 passed, 0 failed` | ✅ pass |
| AC-3.3 | Ran `galactic_invasion_formation_reverses_and_descends_at_the_edge` | reverse + one descend at edge | `1 passed, 0 failed` | ✅ pass |
| AC-3.4 | Ran `galactic_invasion_replay_is_byte_identical_every_tick`; cross-checked with `make test-gcc` | byte-identical replay | `1 passed, 0 failed`; gcc build also `255 passed, 0 failed` | ✅ pass |
| AC-4.1/4.2 | Ran `galactic_invasion_shot_destroys_a_known_enemy_and_scores`, `galactic_invasion_shot_passes_through_a_destroyed_slot` | hit destroys enemy+shot, score +10 | both `1 passed, 0 failed` | ✅ pass |
| AC-4.3 | Ran `galactic_invasion_clearing_every_enemy_wins` | last kill ends round via win path | `1 passed, 0 failed` | ✅ pass |
| AC-5.1 | Ran `galactic_invasion_last_life_lost_ends_the_round` (lives_test) | last-life hit ends round same tick | `1 passed, 0 failed` | ✅ pass |
| AC-5.2 | Ran `galactic_invasion_dodge_run_reaches_threshold_with_lives_intact` and `galactic_invasion_threshold_failsafe_still_fires_after_a_coincident_hit` (the F1/F3 regression test) | threshold ends round regardless of lives, including the coincident-hit edge case | both `1 passed, 0 failed` | ✅ pass |
| AC-5.3 | Ran `galactic_invasion_both_end_screens_show_the_final_score`; decoded `build/galactic_invasion_win_pattern.png` myself | loss/win screen shows final score via `drawText` | test passed; PNG visually shows `SCORE: 0180` in orange under `YOU WIN` | ✅ pass |
| AC-6.1 | Ran `galactic_invasion_ready_frame_composes_title_screen`; read source for `drawTitleScreen` call | READY renders unmodified title screen | `1 passed, 0 failed`; `title_screen.h`/`.cpp` untouched (`git status --porcelain` on those paths empty) | ✅ pass |
| AC-6.2 | Ran `galactic_invasion_start_rising_edge_enters_playing` | START edge begins fresh PLAYING round | `1 passed, 0 failed` | ✅ pass |
| AC-6.3/AC-11.4 | Ran `galactic_invasion_restart_from_either_outcome_matches_a_fresh_round` | restart from either outcome is a fresh round | `1 passed, 0 failed` | ✅ pass |
| AC-7.1 | Ran `galactic_invasion_hud_matches_independent_reference`; decoded `build/galactic_invasion_pattern.png` myself | score drawn top-left via `drawText` | test passed; PNG shows `SCORE: 0000` top-left | ✅ pass |
| AC-7.2 | Ran `galactic_invasion_hud_bounds_are_pairwise_disjoint_at_runtime`; read the four `static_assert`s in `galactic_invasion.h:271-278` myself | score/lives/player/formation bounds never overlap | test passed; static_asserts present and compile (build succeeded) | ✅ pass |
| AC-8.1 (Should) | Ran `galactic_invasion_never_more_than_max_enemy_shots_in_flight`, `galactic_invasion_every_enemy_shot_spawns_at_a_column_centre`, `galactic_invasion_enemy_fire_sequence_is_deterministic` | bounded pool, deterministic shooter choice | all `1 passed, 0 failed` | ✅ pass |
| AC-8.2 (Should) | Ran `galactic_invasion_enemy_shot_hit_costs_one_life_and_starts_invulnerability`, `galactic_invasion_enemy_shot_through_invulnerable_player_is_unaffected` | enemy-shot hit resolves like AC-5.1; invuln blocks it | both `1 passed, 0 failed` | ✅ pass |
| AC-8.3 (Should) | Ran `galactic_invasion_enemy_shot_reaching_bottom_is_removed` | shot removed at bottom edge | `1 passed, 0 failed` | ✅ pass |
| AC-9.1 (Should) | Ran `galactic_invasion_step_interval_is_monotone_and_bounded`, `galactic_invasion_full_health_cadence_matches_pre_us9_behavior`, `galactic_invasion_cadence_matches_formula_after_nine_kills` | step interval shortens deterministically with survivor count | all `1 passed, 0 failed` | ✅ pass |
| AC-9.2 (Should) | Ran `galactic_invasion_speedup_sequence_is_deterministic` | byte-identical step-timing replay | `1 passed, 0 failed` | ✅ pass |
| AC-10.1 | Ran `galactic_invasion_fresh_round_shows_three_lives`; PNG shows `LIVES: 3` top-right | starts with 3 lives, displayed | `1 passed, 0 failed`; PNG confirms | ✅ pass |
| AC-10.2 | Ran `galactic_invasion_contact_hit_drops_a_life_and_respawns`, `galactic_invasion_contact_hit_clears_an_in_flight_shot` | life -1, shot cleared, instant respawn, stays PLAYING | both `1 passed, 0 failed` | ✅ pass |
| AC-10.3 | Ran `galactic_invasion_invulnerability_blocks_damage_for_120_ticks` | overlap during window causes no damage/re-respawn | `1 passed, 0 failed` | ✅ pass |
| AC-10.4 | Ran `galactic_invasion_vulnerability_resumes_after_the_120_tick_window` | vulnerable again after exactly 120 ticks | `1 passed, 0 failed` | ✅ pass |
| AC-10.5 | Ran `galactic_invasion_last_life_lost_ends_the_round` | last-life hit ends round, no respawn | `1 passed, 0 failed` | ✅ pass |
| AC-10.6 | Ran `galactic_invasion_threshold_failsafe_still_fires_after_a_coincident_hit` | failsafe fires regardless of lives/invulnerability | `1 passed, 0 failed` | ✅ pass |
| AC-10.7 | Ran `galactic_invasion_replay_is_byte_identical_every_tick`; cross-checked `make test-gcc` | lives/respawn/invuln sequence byte-identical | `1 passed, 0 failed`; gcc agrees | ✅ pass |
| AC-10.8 | Ran `galactic_invasion_flicker_follows_the_exact_tick_pattern` | player sprite flickers on fixed deterministic pattern during invuln | `1 passed, 0 failed` | ✅ pass |
| AC-11.1 | Ran `galactic_invasion_clearing_every_enemy_wins` | `sessionEnded=true` on formation clear, outcome recorded WIN internally | `1 passed, 0 failed` | ✅ pass |
| AC-11.2 | Ran `galactic_invasion_win_and_loss_screens_are_visually_different`; read the test body myself (`round_test.cpp:436-458`) — asserts `!framebuffersEqual(...)` AND a rendered-width margin `>= 2*kGlyphAdvance` (16px) | screens differ visibly, not just in words | `1 passed, 0 failed`; confirmed the assertion is exactly the framebuffer-diff + width-margin rule the spec required | ✅ pass |
| AC-11.3 | Ran `galactic_invasion_both_end_screens_show_the_final_score` | win screen shows final score too | `1 passed, 0 failed` | ✅ pass |
| AC-11.4 | see AC-6.3 above | restart identical regardless of outcome | `1 passed, 0 failed` | ✅ pass |
| AC-12.1 | Ran `make lint`'s "no fillRect( in games/" rule | no `fillRect` used for these 3 entity types | rule reported clean, `make lint OK` | ✅ pass |
| AC-12.2 | Ran the three `*_blits_with_inherited_transparency` tests | BLACK pixels leave background undisturbed via shipped `blit` | all 3 `1 passed, 0 failed` | ✅ pass |
| AC-12.3 | Ran `make lint`'s "no asset/PNG/TTF reference in games/" rule; read `galactic_invasion_art.h` myself — `Color` arrays built by `constexpr buildPlayerSprite()`/etc. from string-literal rows, no file I/O | compile-time `Color` data only, no PNG decode | lint clean; header confirms `constexpr`, no `<fstream>`/`assets/` reference anywhere in the file | ✅ pass |
| AC-12.4 | Cited `/look-and-feel` design-review pass (already run this session, no findings) plus my own visual check of the decoded PNG (player = wedge, enemies = blocky notched invaders — clearly different silhouettes at actual formation scale) | 3 sprites distinguishable by silhouette alone | design review passed; PNG visually confirms | ✅ pass (design judgment cited, not re-derived; mechanical half self-verified) |
| NFR-1 | Ran `make bench` | tick time << 16.667 ms budget | `galactic-invasion tick: 5.09 us/tick ... ratio 1.72x; budget: < 16.6667 ms/tick` — `BENCH OK` | ✅ pass |
| NFR-2 | Ran `make lint`'s "no dynamic allocation in the galactic-invasion file set" rule | zero dynamic allocation | rule clean, `make lint OK` | ✅ pass |
| NFR-3 | Ran `galactic_invasion_determinism_test`'s replay test plus `make test-gcc` | byte-identical across compilers | `1 passed, 0 failed`; gcc `255 passed, 0 failed` matches clang | ✅ pass |
| NFR-4 | Ran `make test-gcc` and `make lint`'s ESP-IDF-header/resolution-literal/glyph-metric-literal rules for `games/` | compiles clean on both compilers, `-Wall -Wextra -Werror`, no literal violations | gcc build clean, all lint rules clean, `make lint OK` | ✅ pass |
| NFR-5 | Read `galactic_invasion.h:291-299` myself: `class GalacticInvasion { public: GalacticInvasion() = default; void update(...); void render(...); private: ... }` | exactly one `Game`-conforming type, ctor/update/render public, rest private | confirmed by direct read — class surface is exactly that | ✅ pass (class-level); namespace-scope leakage of `Enemy`/`isAlive`/generic constants is `review.md` F5, an accepted forward-looking Minor, not re-litigated here |
| NFR-6 | Read the file-level doc comment `galactic_invasion.h:12-99` myself | states GameState transitions, all 3 `sessionEnded` conditions, grid/timing rule, single-shot rule, lives/respawn/invuln rule + exact 120-tick duration, win/loss tracking mechanism, inherited contract, one usage example | all present, read in full — confirmed | ✅ pass |
| NFR-7 | Read `galactic_invasion.cpp:141` (`fb.clear(Color::BLACK)`) and all 4 `drawText(...)` call sites (`:171-191`) myself | score/lives/GAME OVER/win text render `BRIGHT_ORANGE` on `BLACK` | all 4 `drawText` calls pass `Color::BRIGHT_ORANGE` explicitly; background cleared to `BLACK` | ✅ pass; silhouette-distinctness half cited from `/look-and-feel` per instructions |
| NFR-8/9/10 | N/A per spec (offline, no persistence, no logging surface) | N/A | N/A | ✅ N/A |

## 3. Exploratory Findings

| # | Severity | Steps to reproduce | Expected vs. observed | Status |
|---|---|---|---|---|
| — | — | `build/steamcore_tests galactic_invasion_this_test_does_not_exist_xyz123` (nonsense filter) | Expected: harness treats zero-match as a failure, not a silent no-op. Observed: `0 passed, 0 failed` + `ERROR: no test matched filter "..."`, exit code 1 — confirmed correct; also re-confirmed via the harness's own documented `make test-negative` self-check. No finding — behaves as designed. | n/a |
| — | — | `grep -rn "TODO\|FIXME\|XXX" games/galactic_invasion/` and the 13 `galactic_invasion_*_test.cpp` files | Expected: none left in shipped code. Observed: zero matches. No finding. | n/a |
| — | — | `git status --porcelain` on `collision.h`, `framebuffer.h`, `game_state.h`, `game_loop.h`, `game_input.h`, `sprite.h`, `title_screen.h` | Expected: zero diff (NFR-5/NFR-4's "unmodified primitives" claim). Observed: empty — confirmed untouched myself, matching `review.md`'s NFR-4 row. No finding. | n/a |
| — | — | Ran every one of the 49 galactic-invasion-specific `STEAMCORE_TEST` cases individually by exact name (not just by filter substring — an early filter run for "enemy_fire"/"speedup" accidentally matched only 1 of 6/4 tests each due to substring overlap; caught by cross-counting `grep -c "STEAMCORE_TEST("` per file against tests actually executed, then re-ran every missed one by full name) | Expected: 49/49 pass. Observed: 49/49 pass, matching the file-count total and `review.md`'s "255 passed" full-suite figure. No finding — but recording this because a shallower filter-based pass would have silently under-tested US-8/US-9 while still reporting green. | n/a |
| — | — | Decoded `build/galactic_invasion_pattern.png` and `build/galactic_invasion_win_pattern.png` via `make view` and viewed them directly | Expected: HUD/formation/player render correctly, win screen shows `YOU WIN`/score distinctly. Observed: exactly that — `SCORE: 0000`/`LIVES: 3` HUD, 3×6 blocky-invader formation, wedge-shaped player ship, and a centered `YOU WIN` / `SCORE: 0180` win screen, all in orange on black. No finding. | n/a |

No Blocker, Major or Minor bugs found this round.

## 4. Console & Network

N/A — no browser console or network surface exists for this project (constitution §8). The host-CI equivalent — compiler warnings and lint output — was clean throughout: `-Wall -Wextra -Werror` on both clang and gcc produced zero warnings (a warning would have failed the build outright), and `make lint OK` with no rule violations.

## 5. Verdict

Yes, I would demo this right now. Every Must AC (US-1 through US-7, US-10, US-11, US-12) and every Should AC (US-8, US-9) was verified by running the actual named test and reading its actual pass/fail output — not inferred from `/peer-review`'s prior claims. I re-ran `make test`, `make test-asan` and `make test-gcc` myself (255/255 all three), re-ran `make bench` myself (5.09 µs/tick, comfortably under the 16.667 ms budget — the 4.63 µs figure in `review.md` is the same order of magnitude, run-to-run host-timing variance, not a discrepancy), re-ran `make lint` myself (clean), and decoded and visually inspected two framebuffer PNGs myself rather than trusting the review's description of them. I also caught and corrected a real risk in my own process: an early filter-based pass under-tested US-8/US-9 by accident of substring matching, until I cross-checked file-level `STEAMCORE_TEST` counts and re-ran every test by its exact name. The traceability I built independently matches `review.md` §4 with no discrepancy found. AC-12.4's design judgment and NFR-7's silhouette half are cited from the already-completed `/look-and-feel` pass, per instructions, rather than re-derived — everything else in this report rests on a command I ran and output I read myself. The framebuffer/serial hardware half of §8's substitute method remains explicitly deferred, per the spec's own §1 scoping, not silently skipped.

---

## ✅ QA GATE

*All boxes checked → `/go-live` may start. Any box open → back to `/increment`, then re-run
`/demo-day`. On re-test, edit this same checklist in place — never duplicate it as a second gate.*

- [x] Every Must-story acceptance criterion verified via the host-CI substitute (constitution §8) and passed
- [x] Every host-observable NFR verified and passed (NFR-1–NFR-7; NFR-8/9/10 N/A per spec)
- [x] No open Blocker or Major bugs (none found this round)
- [x] Host toolchain output (compiler warnings, lint) free of errors on every tested path
- [x] Tested on the one agreed "surface" this project has — the host toolchain (no viewports apply; no browser exists)
- [x] Line budget respected: Ist 113 / Soll ~130 (excluding HTML comments)
- [x] Status set to `passed`
