# Release: game-loop

| | |
|---|---|
| **Phase** | Keep |
| **Owner** | Release Manager (`/go-live`) |
| **Input** | `review.md` (`passed`, round 2), `qa.md` (`passed`, round 1) |
| **Status** | `released` |
| **Version** | v0.0.4 |
| **Date** | 2026-09-03 |

**Handoff**
- **Status:** `released` — both gates green, pre-flight re-verified fresh on current HEAD,
  tag created and confirmed pointing at the historical source commit, post-release smoke
  check green.
- **Summary:** Retroactive catch-up release for `GameLoop<Game>`, the fixed-timestep
  `update`-then-`render` tick mechanism with a tested determinism guarantee. Source shipped
  weeks ago as commit `91e2641`; only `.spark/game-loop/qa.md` and this release report are
  new at commit time. No source changes. **Fourth and last** of the queued retroactive
  catch-ups (`rendering-core` = v0.0.1, `framebuffer-viewer` = v0.0.2, `text-rendering` =
  v0.0.3, all done; this closes the queue at v0.0.4).
- **Open:** `none` — tag created and verified, smoke check green. The docs-commit-vs-tag
  split (docs at `a493019`, tag at `91e2641`) is intentional per the retroactive-catch-up
  pattern (CLAUDE.md), not an outstanding item.
- **Binding ruling:** §3 Release Actions and the KEEP GATE below carry the final ruling.
- **On conflict:** the numbered body below wins for everything except `Status`/`Version`;
  log the mismatch at the next `/go-live` and proceed.

## 0. Gates & Delivery Mode

- `review.md`: Status `passed`, round 2. REVIEW GATE fully checked — 0 open Blocker/Major;
  the only Major (F1) confirmed fixed r2; 3 Minor/Nit findings closed, none blocking.
- `qa.md`: Status `passed`, round 1. QA GATE fully checked — 15/15 Must ACs (US-1..US-4)
  verified via the constitution's declared substitute method (§8 QA Method: `Browser-
  observable surface: no`; host-compiled unit tests, since this feature reaches no display
  driver) as a **standing project fact**, decided once at `/charter` — not a per-feature
  waiver. 0 bugs found; 4 independent mutation tests (call-order swap, wall-clock injection,
  allocation injection, dropped-branch determinism fixture) all caught by name.
- `.spark/constitution.md` §7 Delivery & Handoff: **`direct`** mode, explicitly declared
  (approver `n/a`, target branch `main`, ticket format `none`, terminal status `released`).
  No PR/handoff step.

## 1. Pre-Flight Checks

*Run fresh, this pass, on current HEAD (`a493019`) — not copied from `plan.md`/`review.md`/`qa.md`.*

- [x] `review.md` status is `passed`
- [x] `qa.md` status is `passed` — QA ran via the constitution's declared substitute method
      (§8), cited as a standing project fact; 15/15 Must ACs recorded individually, 0 bugs
- [x] Full test suite green on current HEAD — `make clean && make test-all`, run twice
      (pre-tag pre-flight and post-tag smoke check), identical result both times: **109
      passed / 0 failed** across clang `-O2`, ASan+UBSan and the g++-alias build;
      `test-negative` OK (both self-check assertions, incl. zero-match-filter guard); benches
      OK (dirty scan 0.0031 ms, text 0.1451 ms/600 chars, game-loop replay 0.0010 ms for 53
      ticks x 2 runs — all under the 5 ms budget; comparison-cost 0.3729 ms correctly labeled
      non-gating); 15 Python tests via `discover` + 2 standalone round-trip OK;
      `test-png-external` OK (`sips` cross-check 240x160); `make lint` OK (11 rule blocks,
      incl. both game-loop-specific rules: no wall-clock/RNG, no allocation in the test set).
      This feature's own tests (`game_loop_test.cpp`, `game_loop_determinism_test.cpp`,
      `fb_compare_test.cpp`) run and pass inside this suite, not just historically at `91e2641`.
- [x] Build succeeds from a clean state — `make clean` then full rebuild, no cached objects,
      run twice this pass; this project has no separate `build` target, `make test-all` is
      the build+test gate.
- [x] No uncommitted changes belonging to this feature — working tree clean except unrelated
      untracked assets (`assets/Buttons.png`, `assets/fonts/`, `assets/sprites/`) belonging to
      later, not-yet-started work — confirmed via `git status` before and after the tag.

## 2. Changelog

### Added
- A single, fixed way for a game to run: each tick, the game's logic updates first, then
  the picture is drawn — always in that order, every time.
- Every tick's input is delivered as exactly two signals, START and FIRE, with no delay,
  no dropped presses and no guessing which tick a button press belongs to.
- The console's determinism guarantee is now a tested fact, not a claim: replaying the same
  sequence of button presses from a fresh start produces the exact same picture, tick for
  tick, every single time.
- From here on, any future game plugs into this one proven tick mechanism instead of
  inventing its own timing and call-order logic.

### Changed
- (none — no existing behavior changes; new capability only)

### Fixed
- (none — new capability, not a bug fix)

## 3. Release Actions

*`direct` mode. This pass executes the outward-facing steps: explicit go-ahead ("go", in
direct response to the pending tag command) was relayed by the caller this run.*

| Action | Result |
|---|---|
| Release commit (docs) | **Already executed**, prior run: commit `a493019` "Add QA verification and release notes for game-loop" — adds only `.spark/game-loop/qa.md` and `.spark/game-loop/release.md` (that version), no source diff, matching the retroactive-catch-up rule (CLAUDE.md: source already shipped in `91e2641`). |
| Version bump & tag | **Executed this run.** `git tag -a v0.0.4 91e2641 -m "v0.0.4: game loop (fixed-timestep tick mechanism, determinism proof) — retroactive QA/release catch-up"`. Verified: `git rev-list -n1 v0.0.4` = `91e2641af4e4e46945ab4bbea8be58791bed5093`, exactly equal to `git rev-parse 91e2641` — the tag points at the historical source commit, immediately after `6b88467` (`v0.0.3`) and immediately before `212e2e5` (`v0.1.0`), not at HEAD (`a493019`) and not at the docs commit. |
| PR / merge | N/A — `direct` mode, no remote configured |
| Deploy | N/A — no build/flash/deploy pipeline exists for this host-tested logic increment |
| Post-release smoke check | **Executed this run.** `make clean && make test-all` re-run on current HEAD (`a493019`) after tagging: identical green result to pre-flight — 109/109 passed (clang, ASan/UBSan, g++-alias), both `test-negative` self-checks correct, 3 benches `BENCH OK`, Python/roundtrip OK, `test-png-external` OK, `make lint` OK. No deploy pipeline exists, so "alive" means the exact tagged history's test suite passes on demand — confirmed. |

**Rollback path** (local-only, nothing pushed, nothing to unwind remotely):
- Docs commit needs undoing → `git reset --soft HEAD~1` — restores both files to
  staged/modified in the working tree; nothing lost, no force-push, no coordination needed
  (nothing pushed; `main` has no remote configured).
- Tag wrong → `git tag -d v0.0.4` — removes the local tag only; `91e2641` is untouched (old,
  shipped, load-bearing history — never reset or rewritten).
- The docs commit touches only the two named `.spark/game-loop/` files, so undoing it cannot
  affect `91e2641`'s source or any other feature's history.

## 4. Learnings (Keep!)

- **What went well:** the retroactive-versioning pattern (CLAUDE.md) held on its fourth and
  final application without adjustment — confirming `91e2641`'s exact position between
  `6b88467` (v0.0.3) and `212e2e5` (v0.1.0) before tagging `v0.0.4` caught the same drift
  class the pattern exists to prevent, this time by closing the sequence rather than
  extending it. The prepare/publish split (docs committed one run, tag created the next on
  explicit go) kept the irreversible step (the tag) deliberate and separately authorized from
  the reversible one (the docs commit), exactly as the role's Hard Rules require.
- **What we'd do differently:** same note as the three prior retroactive releases — running
  QA/release this far behind the source commit means the release commit can never carry a
  source diff; the backlog is now empty, so this is a closed question rather than a standing
  recommendation for future increments run same-day through the full loop.
- **Patterns worth reusing:** this closes the retroactive-catch-up arc — every increment in
  the repo's history (`rendering-core`, `framebuffer-viewer`, `text-rendering`, `game-loop`)
  now has a completed review -> QA -> release chain, and every increment from
  `game-state-management` onward is already running the full loop same-day. Worth recording
  in `CLAUDE.md` that the backlog is closed, so a future session doesn't re-scan history for
  more catch-up candidates.

---

## ✅ KEEP GATE

*All boxes checked → the loop is closed. The feature is done-done.*

- [x] All pre-flight checks passed at release time
- [x] Changelog written in user-facing language
- [x] Release actions executed and verified — docs commit (prior run), tag (this run,
      confirmed pointing at `91e2641`), post-release smoke check (this run, green)
- [x] Learnings recorded
- [x] Line budget respected: Ist 113 / Soll ~100 (excluding HTML comments) — modest overage
      carried over from the prepare/publish split's explicit tag-verification detail; not
      waived, just recorded
- [x] Status set to `released`
