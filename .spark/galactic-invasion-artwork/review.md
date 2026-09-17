# Review Report: galactic-invasion-artwork

| | |
|---|---|
| **Phase** | Review |
| **Owner** | Reviewer (`/peer-review`) |
| **Input** | The working-tree diff against `HEAD` (`0ff6e87`), `.spark/galactic-invasion-artwork/plan.md` |
| **Status** | `passed` |
| **Round** | 1 |
| **Date** | 2026-09-16 |

**Handoff**
- **Status:** mirrors the header table above (authoritative for `Status`).
- **Verdict:** The feature is built as specified and every gate re-run is green. F1 (two `#if 0`-disabled behavioural tests, the same documented shot/enemy compositing limitation) is user-waived; F2/F3/F4/F6 were fixed in this round (F5 was fixed by the reviewer itself); all re-verified green.
- **Open:** `none` — Blockers: `none`; Majors: `none` (F1 waived, not silently dropped — see §3 and §6)
- **Binding ruling:** §6 Verdict and the gate checklist below — the only binding location; there is no other round to point to
- **On conflict:** the numbered body below wins for everything except `Status`; log the mismatch as a finding at the next `/peer-review` and proceed — don't stop on it.

## 1. Scope

Reviewed: the complete uncommitted changeset against `HEAD` (`0ff6e87`) — 18 modified files plus 8 new ones (`tools/generate_sprite_data.py`, `tools/test_generate_sprite_data.py`, `docs/sprite-generator.md`, `games/galactic_invasion/galactic_invasion_generated_art.h`, `galactic_invasion_logo.{h,cpp}`, `firmware/steamcore/test/galactic_invasion_logo{,_dump}_test.cpp`), plus the surrounding shipped code needed to judge them (`galactic_invasion.cpp`'s render order and enemy-fire spawn, `title_screen.h`, `blit`). Re-run here, not taken on report: `make test` (339), `make test-asan` (339), `make test-gcc` (339), `make lint`, `make test-python` (85), `make bench`, `idf.py build` (exit 0), the `assets/`-moved-aside run of `make test`+`make lint`, a double regeneration of the art header, and the four AC-3.6 abort paths. The rendered `READY` and `PLAYING` dumps were decoded and looked at.

Not reviewed: `firmware/system/main/app_main.cpp` — a pre-existing uncommitted playtest harness the plan explicitly fences off (R8/T6); its content matches `analog-joystick-input`'s harness and it must not be staged with this feature. `assets/` is untracked: only `assets/sprites/galactic_invation.png` belongs in the release commit (spec A14) — `assets/fonts/`, `assets/Buttons.png` and `assets/sprites/steam_racer.png` do not. No blast-radius tool file was passed (`aspark-graph` is not built for this repo); scope was established by hand from `git status`, the Makefile and `check_constraints.sh`'s file lists.

## 2. Plan Conformance

| Task | Implemented as planned? | Note |
|---|---|---|
| T1 | ✅ | Template-matched `findEnemyShots` + negative controls; its two extra properties (clipping, BLACK-as-transparent) are documented deviations |
| T2 | ✅ | Stdlib RGBA8 reader, full failure taxonomy, atomic write; the 16-bucket `--probe` verdict is a recorded, well-reasoned deviation |
| T3 | ✅ | Both background paths, fit, tie-break and byte-identical re-run are covered by real assertions (`tools/test_generate_sprite_data.py`) |
| T4 | ✅ | Logo 169×56 ≤ 200×56; banner complete; the documented command reproduces the committed header byte-for-byte (verified) |
| T5 | ✅ | One `blit` + one `drawText`, exhaustive `switch`, derived bounds, twice-proven disjointness |
| T6 | ✅ | `galactic_invasion_logo.cpp` registered; `idf.py build` green here; no `throw`, explicit includes |
| T7 | ⚠️ | Art and locators correct, but the task left a shipped test `#if 0`-disabled — see F1 |
| T8 | ✅ | All four AC-2.10 predicates re-derived independently against the four sprites: all hold |
| T9 | ⚠️ | Quantise-then-vote is a justified, user-approved deviation, but plan §1 Decision 4 still documents the superseded mean-then-quantise rule — F4 |
| T10 | ⚠️ | Generated enemy adopted then reverted at T13 (correct per AC-2.7); second test disabled here — see F1 |
| T11 | ✅ | `READY` bench 15.80 µs/tick vs the 16.667 ms budget; footprint `static_assert`s present (one tightened, F5) |
| T12 | ✅ | Provenance block behaves as specified; `assets/`-absent gate re-run green here, skip line printed |
| T13 | ✅ | Design verdict recorded in the plan; logo passes AC-1.7, enemy reverted per AC-2.7 — F3 qualifies the "no stray pixel" half |
| T14 | ✅ `skipped` | Correctly skipped: the generated wordmark is legible on the 1:1 dump, so AC-1.11 never fired |
| T15 | ✅ | On-device capture reported honestly; not re-verifiable in this session (no board access) — taken as recorded, per constitution §4 |

## 3. Findings

| # | Severity | Location | Finding | Status |
|---|---|---|---|---|
| F1 | Major | `firmware/steamcore/test/galactic_invasion_round_test.cpp:353-385`, `:411-481` | Two shipped behavioural tests are compiled out (`#if 0`): the scripted dodge run (AC-5.2/AC-10.6/AC-7.2 of `galactic-invasion`) and `..._threshold_failsafe_still_fires_after_a_coincident_hit`, which exists solely as the regression guard for that feature's review findings F1/F3. This spec's AC-2.2 ("every behavioural assertion still passes textually unchanged") and AC-2.8 ("the assertion count never shrinks") are not met: 5 of the file's 13 `CHECK`s are inert, while T1's own DoD metric `grep -c CHECK` still reports 13 — the metric cannot see `#if 0`. Root cause (a shot overlapping an enemy composites to bytes identical to "no shot") is real, proven and documented, and the user ruled "document and move on" for the T7 case; no user ruling was recorded for the second test, disabled later at T10, until now. **Waived by the user (2026-09-16): both disabled tests accepted as the same documented known limitation** — the second (T10) test fails on the identical root cause as the first (T7), just triggered by the redrawn enemy's different silhouette changing which tick the scripted dodge happens to hit the ambiguity, not a new kind of gap. No restoration or production-side fix taken in this round; a real fix (render order or a shot signature immune to same-ink compositing) would need its own `/sprint-plan` pass, exactly as both tests' own in-place comments already say. | waived |
| F2 | Minor | `galactic_invasion_lives_test.cpp:96,158`; `..._round_test.cpp:115-127`; `..._speedup_test.cpp:70`; `..._dump_test.cpp:105` | Four copies of `anyEnemyPixelOnScreen` still read "any `ORANGE` pixel" as "an enemy body", the premise D4/AC-3.10 invalidated when the enemy shot became `ORANGE`. `anyOrangeIn` (round) and combat's two locators were migrated with `pointInsideAnyShot`; these were not. `lives_test.cpp:158`'s `CHECK(anyEnemyPixelOnScreen(fb))` ("surviving enemies still render") can now be satisfied by an in-flight shot alone — weaker than AC-2.8 requires. `round_test.cpp:115` still documents `ORANGE` as "the enemy sprite's own, unique colour", which is false (NFR-6's own "a comment that documents a coincidence as a guarantee" case). **Fixed by the orchestrator** — all four copies now exclude `findEnemyShots`-located shot pixels via `pointInsideAnyShot`, matching combat's own migrated copy; the stale comment corrected. `make test` (339) re-run green after the edit. | fixed r1 |
| F3 | Minor | `games/galactic_invasion/galactic_invasion_generated_art.h:86-141` (logo rows 7, 23-29, 42-45, 54-55) | The generated logo carries six lit components that are not letterforms, including a single isolated pixel at logo-local (x=1, y=7) → screen (36, 39) — exactly D11's "indistinguishable from a stuck sub-pixel", and the artefact §8's residual risk and AC-4.1 asked to be checked on the 1:1 dump. Plan Deviations T13/T15 record the opposite ("no stray halo pixel"). On the decoded dump the larger blobs read as deliberate sparkles and the single pixel is faint, so this is cosmetic, not a gate item — but the record should match the data. **Fixed by the orchestrator (record only, chose not to regenerate)** — the T15 Deviations entry now names the finding accurately instead of claiming "no stray pixel". | fixed r1 |
| F4 | Minor | `.spark/galactic-invasion-artwork/plan.md:25` (§1 Decision 4) | §1 still specifies "a cell… takes the integer mean of its non-background source pixels", the rule T9's deviation replaced with quantise-then-vote in `tools/generate_sprite_data.py:414-463`. §1 is the plan's own binding location for the architecture decision and a revision is supposed to update it in place, so a future reader gets a model the code no longer implements. **Fixed by the orchestrator** — Decision 4's second sentence now states the shipped majority-vote rule, with a pointer to the Deviations entry for the "why". | fixed r1 |
| F5 | Minor | `firmware/steamcore/test/galactic_invasion_art_test.cpp:467` | The footprint guard asserted the logo's *area* (`kLogoWidth*kLogoHeight <= 200*56`) where AC-1.2 caps each *dimension*; a 240×46 logo would have passed it. **Fixed by the reviewer** — the assert now checks `kLogoWidth <= 200 && kLogoHeight <= 56`; `make test` (339) and `make lint` re-run green after the edit. | fixed r1 |
| F6 | Nit | `tools/check_constraints.sh:944` | The stdlib allowlist gained `collections` and `generate_sprite_data` alongside the `hashlib` the plan's §2 Dependencies names; both are legitimate (a `deque` in the flood, the test module importing the tool) but neither is recorded. **Fixed by the orchestrator** — plan §2's dependency line now records `collections` alongside `hashlib` (`generate_sprite_data` was already named at T2's own row). | fixed r1 |

## 4. Requirements Traceability

| Spec ID | Implemented at | Verdict |
|---|---|---|
| AC-1.1/1.3 | `galactic_invasion_logo.cpp:9-20`; tests `galactic_invasion_logo_test.cpp:55,81` | ✅ met |
| AC-1.2 | `galactic_invasion_generated_art.h:80-81,186-199` (169×56, one `blit`, 4 symbols); `art_test.cpp:467` | ✅ met (assert tightened, F5) |
| AC-1.4 | `galactic_invasion_logo.h:71-92` (four-way SAT `static_assert`); `logo_test.cpp:46,104` (independent helper) | ✅ met |
| AC-1.5/1.6 | `logo_test.cpp:128,165` — driven through the real `GameLoop`, replay byte-equality | ✅ met |
| AC-1.7 | `/look-and-feel` verdict recorded in plan Deviations T13; dump re-decoded here and read as legible, dither-free large-pixel art | ✅ met (F3 qualifies the stray-pixel half) |
| AC-1.8 | `galactic_invasion_logo.cpp:10-19` exhaustive `switch`, no `clear`; `logo_test.cpp:67` | ✅ met |
| AC-1.9 | `logo_test.cpp:151` | ✅ met |
| AC-1.10 | `make test-asan` re-run here: 339 passed | ✅ met |
| AC-1.11 | n/a — fallback not taken (T14 `skipped`, wordmark passed AC-1.7) | ✅ n/a |
| AC-1.12 | `logo_test.cpp:113` field-by-field vs `kTitlePromptBounds` | ✅ met |
| AC-2.1/2.5 | `galactic_invasion_generated_art.h:202-215`, `galactic_invasion_art.h:228` (12×12 unchanged) | ✅ met |
| AC-2.2 | 11 behavioural files pass unchanged — **except** the two `#if 0` blocks | ⚠️ partial (F1) |
| AC-2.3 | `galactic_invasion_art.h:61-110`; `art_test.cpp:396-404` (`hasNoBrightOrange`); re-derived independently here | ✅ met |
| AC-2.4/2.7 | Design verdict in plan Deviations T13 (enemy reverted to the shipped silhouette per AC-2.7) | ✅ met |
| AC-2.6 | Player ships flat `BRIGHT_ORANGE` (a permitted outcome, T9), enemy `ORANGE` + one contiguous 4×5 `DARK_ORANGE` core | ✅ met |
| AC-2.8/2.11 | `galactic_invasion_fixture.h:89-290`; negative controls `enemy_fire_test.cpp:409-539` | ⚠️ partial (F2) |
| AC-2.9 | Six dimension constants byte-identical to `HEAD` (`git show` compared) | ✅ met |
| AC-2.10 | `art_test.cpp:246-404` — all four predicates re-derived from scratch here against all four sprites | ✅ met |
| AC-3.1/3.8 | `generate_sprite_data.py:308-403` (bg rule), `:414-463` (fit), `:472-500` (quantise) | ✅ met |
| AC-3.2 | Re-run here with `assets/` moved aside: `make test` 339 + `make lint` green, skip line printed | ✅ met |
| AC-3.3 | Two emits to the same path compared byte-for-byte here: identical, and equal to the committed header | ✅ met |
| AC-3.4 | `galactic_invasion_generated_art.h:1-26` + per-artifact region/mode blocks; the banner's command reproduces the file | ✅ met |
| AC-3.5 | `generated_art.h:145-200`; `galactic_invasion_art.h:239-285` (identical set for hand-authored) | ✅ met |
| AC-3.6 | All four abort paths exercised here: exit 1, named reason, nothing written, prior file intact | ✅ met |
| AC-3.7 | `idf.py build` re-run here: exit 0; no `throw` in `games/`; explicit includes | ✅ met |
| AC-3.9 | `check_constraints.sh:684-717`; gate run here emits **no** `PROVENANCE WARNING` — source sha256 recomputed independently and matches the banner | ✅ met, no warning to report |
| AC-3.10/3.11 | `galactic_invasion_art.h:99-113` (segmented `ORANGE` shot) + hand-authored notes on enemy and both shots | ✅ met |
| AC-4.1/4.2 | Hardware capture recorded in plan Deviations T15; not re-verifiable without the board | ✅ as reported |
| NFR-1 | `bench_galactic_invasion.cpp:77-96`: 15.80 µs/tick vs 16.667 ms; `art_test.cpp:479` identical consecutive frames | ✅ met |
| NFR-2 | `art_test.cpp:491-506` (9,752 B ≤ 12,288 B); row strings confirmed absent from the linked firmware image | ✅ met |
| NFR-3 | Determinism tests + byte-identical regeneration, both re-run here | ✅ met |
| NFR-4 | clang, g++, ESP-IDF all green here; lint's literal rules pass | ✅ met |
| NFR-5 (library lens) | `git diff HEAD -- firmware/steamcore/{include,src,port}` is empty; `GalacticInvasion`'s public surface unchanged (doc comment only) | ✅ met |
| NFR-6 (library lens) | `galactic_invasion_logo.h:10-27`, `galactic_invasion_art.h:8-49`, `fixture.h:73-81`, `docs/sprite-generator.md` | ⚠️ partial (F2's stale comments) |
| NFR-7 | Reserved inks asserted; segmented enemy-shot silhouette present | ✅ met |
| NFR-9 | Loud, specific, partial-write-free failures (verified); warning-only provenance line | ✅ met |
| NFR-10 | Zero new build/runtime dependencies; stdlib-only tool (lint allowlist, F6) | ✅ met |

*Library lens, review slice: surface additions are intentional and minimal (all new symbols live under `games/galactic_invasion/`, none in the engine); no export was removed, renamed or re-signed, so there is no compatibility break to flag; packaging/semver remain no-ops for a statically-linked image per the constitution's scoping; contract clarity is documented per NFR-6 apart from F2.*

## 5. What Was Checked

- [x] Correctness: logic does what the acceptance criteria demand
- [x] Non-functional: applicable NFRs and constitution quality bars hold
- [x] Error handling: failures are handled, not swallowed
- [x] Security: N/A by platform (offline device, no input surface); the tool trusts only a repo-local asset
- [x] Tests: exist, are meaningful, and pass — with F1's two exceptions
- [x] Readability: the next developer will understand this

## 6. Verdict

This is a carefully built increment: the generator is deterministic and loud where it should be (I reproduced the committed header byte-for-byte from the banner's own command and drove all four abort paths myself), the logo screen derives every rectangle from art and font metrics and proves disjointness twice, the engine's public surface is untouched, and AC-2.10's structural rules hold when re-derived from scratch rather than read off the project's own assertions. The T13 reversal of the generated enemy is the loop working as designed — a mechanical gate passed, a design review disagreed, and AC-2.7's pre-authorised fallback was taken instead of arguing with the evidence.

Round 1 found one Major (F1: two shipped behavioural tests compiled out with `#if 0`, one of them the only regression guard for a defect an earlier review found) and four smaller findings. **F1 is now explicitly waived by the user (2026-09-16)**: both disabled tests share the same documented, proven root cause (a shot overlapping a same-ink enemy composites to bytes identical to "no shot" — genuine information loss in render order, not a detection bug), the first (T7) already had a user ruling to document-and-move-on, and the second (T10) is the identical ambiguity surfacing on a different scripted tick after the enemy's silhouette changed, not a new gap. No code changed for F1; a real fix stays out of scope for this feature, as both tests' own comments and the plan's Deviations already said. F2 (four locator copies that silently kept assuming `ORANGE` meant "enemy body" after the enemy shot became `ORANGE` too), F3 (the plan claimed "no stray pixel" where one genuinely exists, now corrected to match the data), F4 (plan §1 Decision 4 still described the mean-then-quantise fit rule T9 replaced with quantise-then-vote) and F6 (two undocumented lint-allowlist stdlib additions) were all fixed directly and re-verified: `make test`/`test-asan`/`test-gcc` 339 each, `make lint` clean. F5 was fixed by the reviewer itself in round 1. Nothing remains open.

---

## ✅ REVIEW GATE

*All boxes checked → `/demo-day` may start. Any box open → back to `/increment`. On
re-review, edit this same checklist in place — never duplicate it as a second gate.*

- [x] No open Blocker findings
- [x] No open Major findings (or explicitly waived by the user, with reason recorded here) — **F1 waived by the user 2026-09-16, reason above**
- [x] Every Must AC traces to implementing code; no constitution non-negotiable violated
- [x] All plan deviations documented and accepted — T7/T10's disabled tests (F1, waived) and T9's fit rule (F4, plan §1 corrected) are settled
- [x] Test suite runs green — `make test`/`test-asan`/`test-gcc` 339 each, `test-python` 85, `lint`, `bench`, `idf.py build`, all re-run here and again after F1-F6's fixes
- [x] Line budget respected: Ist 126 / Soll ~150 (excluding HTML comments)
- [x] Status set to `passed`
