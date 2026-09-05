# Release: collision-system

| | |
|---|---|
| **Phase** | Keep |
| **Owner** | Release Manager (`/go-live`) |
| **Input** | `review.md` (`passed`, round 1), `qa.md` (`passed`, round 1) |
| **Status** | `preparing` |
| **Version** | v0.5.0 (proposed, not yet tagged) |
| **Date** | 2026-09-05 |

**Handoff**
- **Status:** `preparing` — both gates green, fresh pre-flight green (host
  only, re-run just now on the working tree that will become the release
  commit). Explicitly **prepare-only**: no `git add`/`commit`/`tag` executed,
  no push. Everything below is drafted and ready to run on the caller's go.
- **Summary:** Adds `Entity`, `overlaps()`, `checkCollision()` and the
  optional `sweepCollisions()` — the AABB collision primitive every roadmap
  game needs. Pure host-only addition; nothing player-visible changes yet
  (no game exists that uses it).
- **Open:** `1 outstanding` — the commit/tag/staging commands are drafted in
  §3 but not run; awaiting the caller's explicit go to execute them. Nothing
  from review/QA is open (both 0 Blockers/Majors).
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
- [ ] No uncommitted changes in the working tree — **not yet true, by
      design**: this is prepare-only, nothing has been committed. `git
      status` re-checked immediately before writing this report and
      reconciled against §3's file list below: every modified/untracked path
      belongs to one of two buckets — this feature's diff (§3), or three
      pre-existing, unrelated items (`.spark/start-screen/release.md`,
      `CLAUDE.md`, `assets/{Buttons.png,fonts/,sprites/}`) that predate this
      feature and stay untouched by it. No drift, nothing unaccounted for.
- Five existing engine types confirmed byte-identical to `HEAD` (A8/C4):
  `git diff --exit-code HEAD -- game_loop.h game_state.{h,cpp} input.h
  framebuffer.{h,cpp} sprite.h` → empty.

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

*Prepared, not executed. Direct mode, no remote — the two pending local
commands below (`git add` + `git commit`, then `git tag -a`) are the entire
publish action for this project; no push, no PR, no deploy exist for it.*

| Action | Result |
|---|---|
| Version bump & tag | **Pending — drafted, not run.** Proposed **v0.5.0**. Tag command drafted: `git tag -a v0.5.0 -m "collision-system: AABB overlap detection and callback dispatch for future games; host-only, no device component" <commit>` (commit hash filled in once §3's commit is made). |
| Commit | **Pending — drafted, not run.** Staging: explicit `git add <path>` per file in the list below, never `-A`/`.`. Message: `feat: add collision system — AABB overlap detection and callback dispatch for future games` + Co-Authored-By trailer (matching this repo's convention, `git log`). |
| PR / merge | N/A — `direct` mode, no remote configured. |
| Deploy | N/A for this feature specifically — no device-side component exists (plan §2/§4, deliberate); nothing is flashed or pushed anywhere by this release. |
| Post-release smoke check | **Pending.** Once tagged: re-run `make test-all` against the tagged commit and confirm every §1 number reproduces exactly — same pattern as every prior release. |

**Version justification.** Continues this project's normal `v0.x.0`
sequence from `v0.4.0` (start-screen). **Minor** bump: purely additive — one
new public header (`collision.h`) exposing exactly four new symbols
(`Entity`, `overlaps`, `checkCollision`, `sweepCollisions`), zero change to
any existing shipped symbol (confirmed byte-identical, §1). Not a PATCH (new
functionality, not a bug fix); not a MAJOR (nothing removed or resignatured).
Every prior same-day full-loop feature in this project's history landed as a
minor bump too — no reason found to deviate.

**Exact file list to stage (never `-A`/`.`), reconciled against a fresh
`git status` run immediately before this report:**
- Modified: `Makefile`, `docs/host-tests.md`, `tools/check_constraints.sh`
- New: `.spark/collision-system/spec.md`, `plan.md`, `review.md`, `qa.md`,
  `release.md` (this file), `firmware/steamcore/include/steamcore/collision.h`,
  `firmware/steamcore/test/collision_test.cpp`,
  `firmware/steamcore/test/collision_overflow_test.cpp`,
  `firmware/steamcore/test/collision_dispatch_test.cpp`,
  `firmware/steamcore/test/collision_sweep_test.cpp`,
  `firmware/steamcore/test/bench_collision.cpp`

**Explicitly excluded — pre-existing, unrelated, confirmed by `git status`
reconciliation:** `.spark/start-screen/release.md` (unrelated prior-session
edit), `CLAUDE.md` (two learnings sections added after start-screen's
release, unrelated to this feature), `assets/Buttons.png`, `assets/fonts/`,
`assets/sprites/` (untracked, from a different in-progress effort, not
referenced anywhere by collision-system).

**Rollback path** (local-only; `git remote -v` is empty, nothing to unwind
remotely):
- Not yet committed: nothing to roll back — declining the go simply leaves
  the working tree as-is; no destructive command has run.
- Once committed/tagged and found wrong: `git tag -d v0.5.0` (tag exists →
  delete first), then `git reset --soft HEAD~1` — restores every file to the
  working tree, nothing lost.
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
  instead of shipping a best-case-only number.
- **Patterns worth reusing:** proving a numeric safety claim twice —
  `constexpr`/`static_assert` at compile time plus a `volatile`-laundered
  UBSan case at run time — makes an overflow guarantee fail in two
  independent ways if either is ever weakened, worth reusing for any future
  fixed-width arithmetic.

---

## ✅ KEEP GATE

*All boxes checked → the loop is closed. The feature is done-done.*

- [x] All pre-flight checks passed at release time — full suite green fresh
      (197/197 x3, lint clean, bench OK, python/roundtrip/png-external OK,
      clean-state rebuild green); one box intentionally unchecked (no
      uncommitted-changes state yet, since nothing has been committed —
      expected for prepare-only)
- [x] Changelog written in user-facing language
- [ ] Release actions executed and verified — **not executed**: commit, tag
      and post-release smoke check are all drafted and pending the caller's
      explicit go; nothing outward-facing or irreversible has run
- [x] Learnings recorded
- [x] Line budget respected: Ist 132 / Soll ~100 — 32 over; reason: the
      explicit never-`-A` file list, the version justification paragraph and
      the dual gate-recap in §0 (same overage class as `input-driver`'s and
      `start-screen`'s releases)
- [ ] Status set to `released` — **not set**; status is `preparing`,
      awaiting the caller's go to run the pending commands in §3. Nothing
      here reads as shipped.
