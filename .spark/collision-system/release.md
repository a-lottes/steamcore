# Release: collision-system

| | |
|---|---|
| **Phase** | Keep |
| **Owner** | Release Manager (`/go-live`) |
| **Input** | `review.md` (`passed`, round 1), `qa.md` (`passed`, round 1) |
| **Status** | `released` |
| **Version** | v0.5.0 |
| **Date** | 2026-09-05 |

**Handoff**
- **Status:** `released` — both gates green, pre-flight re-run fresh at
  prepare time and reproduced again post-tag (this section). Commit
  `b203a14652e0cd655ca80b95705c33b574f157ad` tagged `v0.5.0`. No push/PR/
  deploy exist for this project (direct mode, no remote) — local commit +
  tag is the entire publish action, both executed on explicit caller go.
- **Summary:** Adds `Entity`, `overlaps()`, `checkCollision()` and the
  optional `sweepCollisions()` — the AABB collision primitive every roadmap
  game needs. Pure host-only addition; nothing player-visible changes yet
  (no game exists that uses it).
- **Open:** none. The one item open at prepare time (commit/tag/smoke-check
  execution) is done; both gates were already 0 Blockers/Majors.
- **Binding ruling:** §3 Release Actions and the KEEP GATE below.
- **On conflict:** the numbered body below wins for everything except
  `Status`/`Version`; log the mismatch at the next `/go-live` and proceed.

## 0. Gates & Delivery Mode

- `review.md`: `passed`, round 1. REVIEW GATE fully checked: 0 open
  Blockers/Majors (4 Minors/Nits open, none gate-blocking, F3/F4 accepted by
  the user as non-blocking), all 15 Must + 3 Should ACs traced, suite green,
  status `passed`.
- `qa.md`: `passed`, round 1. QA GATE fully checked: every Must-story AC
  (US-1/US-2, 12 ACs) and every applicable NFR (NFR-1–7, NFR-12) verified and
  passed, 0 open Blocker/Major, status `passed`.
- Constitution §7: **`direct`** mode, explicitly declared (solo project, no
  remote, approver n/a, target branch `main`, ticket `none`, terminal status
  `released`). No `pr`-mode language applies here.
- Constitution §8 QA Method: standing project fact (not a per-feature
  override) — `Browser-observable surface: no`; declared substitute is
  framebuffer dump + serial transcript, with hardware-independent logic
  covered by host-compiled unit tests. For this feature the framebuffer-dump
  half is not applicable at all — pure geometry, zero rendering surface
  (spec A7/NFR-12) — so QA ran the unit-test half of the declared method in
  full; `qa.md` records every AC/NFR ID under it, status `passed`.

## 1. Pre-Flight Checks

*Run fresh, right now, on the uncommitted working tree — not copied from
`review.md`/`qa.md`.*

- [x] `review.md` status is `passed`
- [x] `qa.md` status is `passed`
- [x] Full test suite green — `make test`/`test-asan`/`test-gcc`:
      **197/197 all three.** `test-negative`: harness self-check OK. `lint`:
      28 rules clean (incl. the 4 collision-system rules). `bench`: all 5
      binaries OK, `overlaps()` 5.43 ns/call (8M pairs, doubling ratio
      1.98x), `sweepCollisions()` 1.00 µs/sweep (32 entities, 400K passes,
      ratio 2.09x) — both ≪ the 1.6667 ms sweep budget. `test-python`:
      31/31. `test-roundtrip`: 2/2. `test-png-external`: OK.
- [x] Build succeeds from a clean state — `rm -rf build && make test-all`:
      exit 0, identical results to above. This feature has **no device-side
      component at all** (plan §2/§4, deliberate): `git diff --exit-code
      HEAD -- firmware/system/` is empty, so no `idf.py build` step applies.
- [x] No uncommitted changes belonging to this feature — **true as of the
      release commit** `b203a1465`: `git diff --stat HEAD` shows only the
      three pre-existing, unrelated items already named below, nothing from
      this feature's diff.
- Five existing engine types confirmed byte-identical to `HEAD` (A8/C4):
  `git diff --exit-code HEAD -- game_loop.h game_state.{h,cpp} input.h
  framebuffer.{h,cpp} sprite.h` → empty.

## 1b. Post-Release Smoke Check (re-run on the tagged commit)

*Executed on the explicit caller go, on commit `b203a1465` / tag `v0.5.0`,
right after tagging — not copied from §1.*

- [x] `make test-all`: exit 0.
- [x] `make test` (clang): **197 passed, 0 failed** — matches §1.
- [x] `make test-asan`: **197 passed, 0 failed** — matches §1.
- [x] `make test-gcc`: **197 passed, 0 failed** — matches §1.
- [x] `make test-negative`: both harness self-check cases reported OK —
      matches §1.
- [x] `make bench`: all 5 binaries **BENCH OK**. `overlaps()` 5.42 ns/call
      (8M pairs, doubling ratio 1.84x); `sweepCollisions()` 0.96 µs/sweep (32
      entities, 400K passes, ratio 1.99x) — both still ≪ the 1.6667 ms sweep
      budget. Small deltas from §1's 5.43 ns / 1.00 µs / 1.98x / 2.09x are
      ordinary run-to-run timing noise on the same hardware, not a
      regression — same conclusion (comfortably under budget, ~doubling
      scaling) both times.
- [x] `make test-python`: **31/31**, `make test-roundtrip`: **2/2**,
      `test-png-external`: OK — all match §1.
- [x] `make lint`: **OK**, 0 violations. Discrepancy noted honestly rather
      than silently reconciled: the script (`tools/check_constraints.sh`)
      currently defines **29** rule blocks, not the 28 §1 cites — a
      pre-existing wording miscount in this report's own §1 (all 29 ran and
      passed both times; no rule was added or removed between prepare-time
      and now, `git diff --stat HEAD -- tools/check_constraints.sh` is
      empty). Functionally identical outcome (lint clean); only the prose
      count in §1 was off by one.

**Conclusion:** the tagged commit reproduces the same green result recorded
at pre-flight — same pass counts everywhere, same lint outcome, bench numbers
within ordinary measurement noise and still far under budget. Nothing
regressed between prepare and publish.

## 2. Changelog

### Added
- Games can now detect when two on-screen objects touch (a bullet and an
  enemy, a ship and an obstacle, a player and a hazard) and run their own
  code exactly when that happens — the shared building block every future
  game will use instead of inventing its own overlap check. Objects that are
  merely touching edge-to-edge, with no overlapping area, do not count as
  colliding. A game can also check a whole group of objects against each
  other in one call.

### Changed
- (none — purely additive; nothing previously shipped changes behavior)

### Fixed
- (none — new capability, not a bug fix)

**Not player-visible yet.** No game exists that composes this primitive; it
lands invisibly until the first roadmap game uses it (spec §2).

## 3. Release Actions

*Executed on the caller's explicit go. Direct mode, no remote — the two
local commands below are the entire publish action for this project; no
push, no PR, no deploy exist for it.*

| Action | Result |
|---|---|
| Version bump & tag | **Done.** `v0.5.0`. `git tag -a v0.5.0 -m "collision-system: AABB overlap detection and callback dispatch for future games; host-only, no device component" b203a14652e0cd655ca80b95705c33b574f157ad` |
| Commit | **Done.** Commit `b203a14652e0cd655ca80b95705c33b574f157ad`, message `feat: add collision system — AABB overlap detection and callback dispatch for future games` + `Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>` trailer (matches this repo's convention). Staged via explicit `git add <path>` per file, never `-A`/`.`. |
| PR / merge | N/A — `direct` mode, no remote configured. |
| Deploy | N/A for this feature specifically — no device-side component exists (plan §2/§4, deliberate); nothing is flashed or pushed anywhere by this release. |
| Post-release smoke check | **Done** — see §1b. Same green result as pre-flight, reproduced on the tagged commit. |

**Version justification.** Continues this project's normal `v0.x.0`
sequence from `v0.4.0` (start-screen). **Minor** bump: purely additive — one
new public header (`collision.h`) exposing exactly four new symbols
(`Entity`, `overlaps`, `checkCollision`, `sweepCollisions`), zero change to
any existing shipped symbol (confirmed byte-identical, §1). Not a PATCH (new
functionality, not a bug fix); not a MAJOR (nothing removed or resignatured).
Every prior same-day full-loop feature in this project's history landed as a
minor bump too — no reason found to deviate.

**Exact file list staged (never `-A`/`.`), matched against a fresh
`git status` immediately before staging — one drift item found and excluded,
see below:**
- Modified: `Makefile`, `docs/host-tests.md`, `tools/check_constraints.sh`
- New: `.spark/collision-system/spec.md`, `plan.md`, `review.md`, `qa.md`,
  `release.md` (this file), `firmware/steamcore/include/steamcore/collision.h`,
  `firmware/steamcore/test/collision_test.cpp`,
  `firmware/steamcore/test/collision_overflow_test.cpp`,
  `firmware/steamcore/test/collision_dispatch_test.cpp`,
  `firmware/steamcore/test/collision_sweep_test.cpp`,
  `firmware/steamcore/test/bench_collision.cpp`

**Explicitly excluded — pre-existing or unrelated, confirmed by a fresh
`git status --short` immediately before staging:** `.spark/start-screen/
release.md` (unrelated prior-session edit), `CLAUDE.md` (learnings sections
added after start-screen's release, unrelated to this feature),
`assets/Buttons.png`, `assets/fonts/`, `assets/sprites/` (untracked, from a
different in-progress effort). **New since the prepare-time snapshot:**
`.spark/analog-joystick-input/` (untracked directory containing only
`spec.md`) had appeared by execution time — a different, later feature's
spec draft with no plan/review/qa yet, unrelated to collision-system and not
referenced anywhere by it. Named here per this project's own standing
practice (CLAUDE.md, "re-check `git status` before staging a release") and
excluded from staging rather than assumed away.

**Rollback path** (local-only; `git remote -v` is empty, nothing to unwind
remotely):
- Committed and tagged as `b203a14652e0cd655ca80b95705c33b574f157ad` /
  `v0.5.0`. If found wrong: `git tag -d v0.5.0` (delete the tag first), then
  `git reset --soft HEAD~1` — restores every file to the working tree,
  nothing lost, and re-exposes the untracked/modified state exactly as it
  was pre-commit.
- Tag wrong but commit fine: `git tag -d v0.5.0` only, then re-tag once
  corrected.

## 4. Learnings (Keep!)

- **What went well:** the T9-before-T7 dependency-graph reordering (plan
  §Deviations) avoided the exact "lint block references a not-yet-created
  file" trap `input-driver` hit — same class of problem, solved this time by
  checking the dependency graph before reordering rather than after.
- **What we'd do differently:** `bench_collision.cpp`'s LCG workload
  produced zero actual collisions across 8M measured pairs (review F3, open
  Minor) — a representative benchmark should assert a nonzero hit rate on
  itself, not just a doubling ratio, so a degenerate workload fails loudly
  instead of shipping a best-case-only number. Also: this report's own §1
  miscounted the lint rule total by one (28 vs the actual 29) — a prose
  count copied by hand rather than derived from the script is exactly the
  kind of thing that silently drifts; worth generating that number from
  `tools/check_constraints.sh` itself next time instead of typing it.
- **Patterns worth reusing:** proving a numeric safety claim twice —
  `constexpr`/`static_assert` at compile time plus a `volatile`-laundered
  UBSan case at run time — makes an overflow guarantee fail in two
  independent ways if either is ever weakened, worth reusing for any future
  fixed-width arithmetic.

---

## ✅ KEEP GATE

*All boxes checked → the loop is closed. The feature is done-done.*

- [x] All pre-flight checks passed at release time — full suite green fresh
      at prepare time (197/197 x3, lint clean, bench OK, python/roundtrip/
      png-external OK, clean-state rebuild green), and reproduced again
      post-tag on the actual released commit (§1b): same pass counts, same
      lint outcome, bench within ordinary noise, no uncommitted changes
      belonging to this feature.
- [x] Changelog written in user-facing language
- [x] Release actions executed and verified — commit `b203a1465`, tag
      `v0.5.0`, and the post-release smoke check are all done (§3, §1b); no
      push/PR/deploy exist for this project (direct mode, no remote).
- [x] Learnings recorded
- [x] Line budget respected: Ist 152 / Soll ~100 — 52 over; reason: added
      §1b (post-release smoke check detail) plus the drift-item and
      lint-miscount write-ups on top of the existing overage class (explicit
      never-`-A` file list, version justification, dual gate-recap in §0)
      already flagged in `input-driver`'s and `start-screen`'s releases
- [x] Status set to `released`
