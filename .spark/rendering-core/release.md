# Release: rendering-core

| | |
|---|---|
| **Phase** | Keep |
| **Owner** | Release Manager (`/go-live`) |
| **Input** | `review.md` (`passed`, round 3), `qa.md` (`passed`, round 1) |
| **Status** | `preparing` |
| **Version** | v0.0.1 (proposed) |
| **Date** | 2026-09-02 |

**Handoff**
- **Status:** `preparing` — both gates green, fresh pre-flight green on the current worktree, nothing committed or tagged yet. No commit/tag has been authorized; this run is prepare-only.
- **Summary:** Retroactive catch-up release for the rendering core (framebuffer, 4-colour palette, clipped drawing, sprite blitting, dirty-tile detection). The source shipped weeks ago as commit `e5d4be3`, the first commit in the repo; only `qa.md` and this release report are new today. No source changed.
- **Open:** `1 outstanding` — the proposed version scheme (v0.0.1, tagging the historical source commit rather than today's commit) needs user confirmation before it is applied here and to the three remaining backlog features (`framebuffer-viewer`, `text-rendering`, `game-loop`); see §3 Version Ordering below.
- **Binding ruling:** §3 Release Actions and the KEEP GATE below carry the final ruling.
- **On conflict:** the numbered body below wins for everything except `Status`/`Version`; log the mismatch at the next `/go-live` and proceed.

## 0. Gates & Delivery Mode

- `review.md`: Status `passed`, round 3. REVIEW GATE fully checked — 16 findings across 3 rounds, all `fixed` and independently re-verified this round (not taken on the `fixed` cell's word), 0 open Blocker/Major.
- `qa.md`: Status `passed`, round 1. QA GATE fully checked — 28/28 Must ACs verified via the constitution's declared substitute method (§8 QA Method: `Browser-observable surface: no`, host-compiled unit tests as primary evidence, USB-CDC framebuffer-dump half "not capturable yet" per spec A3, not a gap) as a standing project fact, not a per-feature waiver. 0 bugs found.
- `.spark/constitution.md` §7 Delivery & Handoff: **`direct`** mode, explicitly declared (release mode `direct`, approver `n/a`, target branch `main`, terminal status `released`). Confirmed independently: `git remote -v` returns nothing. No PR/handoff step.

## 1. Pre-Flight Checks

*Re-run fresh, this pass, on the current worktree (`HEAD` = `8a0931b`, which includes today's `game-state-management` release plus the hardware-spike/README/constitution commits on top of rendering-core) — not copied from `plan.md`/`review.md`/`qa.md`.*

- [x] `review.md` status is `passed`
- [x] `qa.md` status is `passed` — QA ran via the constitution's declared substitute method (§8), cited as a standing project fact; 28/28 Must ACs recorded individually, 0 bugs
- [x] Full test suite green on the current tree — `make clean && make test-all`: **109 passed / 0 failed** across clang `-O2`, ASan+UBSan and the g++-alias build; `test-negative` OK (both self-check assertions, incl. zero-match filter); benches OK (dirty scan 0.0032 ms, text 0.1405 ms, game-loop replay 0.0015 ms — all under the 5 ms budget); 15+2 Python tests OK; `test-png-external` OK (`sips` cross-check 240×160); `make lint` OK (11 rule blocks). Rendering-core's own ACs (framebuffer, clipping, sprite, dirty-tile tests) all present and passing inside this run, not just historically at `e5d4be3`.
- [x] Build succeeds from a clean state — `make clean` then full rebuild, no cached objects.
- [x] No uncommitted changes belonging to this feature — working tree is clean except `.spark/rendering-core/qa.md` (new, untracked, this release's own artifact) and unrelated untracked assets (`assets/Buttons.png`, `assets/fonts/`, `assets/sprites/`) belonging to later, not-yet-started work. Neither `README.md`, `docs/host-tests.md` nor `tools/check_constraints.sh` carry any pending diff on this tree.

## 2. Changelog

### Added
- A shared 240×160 pixel framebuffer over a fixed 4-colour palette that any future game draws into directly — no game needs to invent its own pixel buffer or drawing code.
- Single-pixel and filled-rectangle drawing that clips safely at every screen edge — a shape drawn partly or fully off-screen never crashes or corrupts the picture.
- Sprite blitting with a choosable transparent colour, so a ship, gear or obstacle can be drawn over an existing background without erasing what's underneath, and a black silhouette can still be drawn over a lit area.
- Support for pulling a sub-image out of a larger sprite sheet, so future art can share one sheet instead of needing one file per sprite.
- Dirty-tile change tracking that reports exactly which 16×16 regions of the screen changed since the last update — the foundation the future display driver will use to redraw only what moved, instead of repainting the whole screen every frame.

### Changed
- (none — first rendering capability added to the engine)

### Fixed
- (none — new capability, not a bug fix)

## 3. Version Ordering (read before Release Actions)

- **Situation:** `v0.1.0` already exists and tags `212e2e5` (`game-state-management`, 2026-09-02). `rendering-core`'s source is `e5d4be3` (2026-09-01), the repo's very first commit. Confirmed: `git merge-base --is-ancestor e5d4be3 212e2e5` → true; `git log --oneline e5d4be3..212e2e5` lists `5886318`, `6b88467`, `91e2641`, `212e2e5` in between — `e5d4be3` is a genuine ancestor, not a parallel branch.
- **Proposed resolution:** tag this release **`v0.0.1`**, an annotated tag pointing at the historical source commit `e5d4be3` itself (not at today's qa.md/release.md commit) — marking retroactively where the rendering core became QA-verified, without continuing sequentially past the already-existing `v0.1.0`. Placing the tag on `e5d4be3` keeps the tag graph's chronology consistent with the version numbers (`v0.0.1`'s commit genuinely precedes `v0.1.0`'s commit), rather than a `v0.0.1` tag sitting on a commit descended from `v0.1.0`.
- **Batch note, explicit:** this is the **first of four** retroactive catch-up releases. `framebuffer-viewer` (`5886318`), `text-rendering` (`6b88467`) and `game-loop` (`91e2641`) are queued behind it in the same commit-chronological order and are about to go through the identical QA+release catch-up one at a time. The scheme this release establishes — `v0.0.1`…`v0.0.4`, each an annotated tag on its own historical source commit, each release commit carrying only that feature's `qa.md` + `release.md` — is proposed as the pattern for all four, not a one-off.
- **Flagged as the one open item:** this is a versioning-convention decision with no prior example in this project (game-state-management's `release.md` notes "no tagging/versioning convention existed yet" when it picked `v0.1.0`). Reasonably confident in the proposal above, but it commits the next three releases to the same pattern, so it is surfaced for explicit confirmation rather than silently applied.

## 4. Release Actions

*Nothing below has been executed. `direct` mode means commit + local tag are the entire "publish" step — both are still outward, irreversible-in-spirit actions this report defers pending the user's go, per prepare-only scope.*

| Action | Result |
|---|---|
| Version bump & tag | **Pending.** Proposed: `git add .spark/rendering-core/qa.md .spark/rendering-core/release.md` (explicit paths, no `-A`); commit with message below; then `git tag -a v0.0.1 e5d4be3 -m "v0.0.1: rendering core (framebuffer, palette, sprite blitting, dirty tiles) — retroactive QA/release catch-up"`. |
| PR / merge | N/A — `direct` mode, no remote configured |
| Deploy | N/A — no build/flash/deploy pipeline exists for this host-tested logic increment |
| Post-release smoke check | **Pending** — to run after commit+tag: `git log -1 --stat` on the new commit shows exactly the two `.spark/rendering-core/` files; re-run `make clean && make test-all` once more, unchanged from §1 |

**Exact commands pending authorization:**
```
git add .spark/rendering-core/qa.md .spark/rendering-core/release.md
git commit -m "$(cat <<'EOF'
Add QA verification and release notes for rendering-core

Retroactive SPARK catch-up: spec/plan/review were run against
already-shipped code (commit e5d4be3, the repo's first commit);
this commit adds the missing QA gate and release report. No
source changed.
EOF
)"
git tag -a v0.0.1 e5d4be3 -m "v0.0.1: rendering core (framebuffer, palette, sprite blitting, dirty tiles) — retroactive QA/release catch-up"
```

**Rollback path** (local-only, nothing pushed, nothing to unwind remotely):
- Tag wrong: `git tag -d v0.0.1` — removes the local tag only; `e5d4be3` is untouched (it is old, shipped, load-bearing history — never reset or rewritten).
- Today's commit (qa.md + release.md) needs undoing: `git reset --soft HEAD~1` — restores both files to staged/modified in the working tree; nothing lost, no force-push, no coordination needed.
- The commit touches only the two named `.spark/rendering-core/` files, so undoing it cannot affect `e5d4be3`'s source or any of the three other backlog features.

## 5. Learnings (Keep!)

- **What went well:** review round 3's mutation-based re-verification (16 findings, all re-attacked from scratch rather than trusted from `fixed` cells) gave this retroactive release genuine confidence in code that shipped a day before its own QA — the gate did its job even applied after the fact.
- **What we'd do differently:** running QA and release this far behind the source commit means the release commit can no longer contain the source diff — worth deciding, before the next three catch-up releases, whether that's acceptable practice going forward or whether future features should get `qa.md`/`release.md` same-day.
- **Patterns worth reusing:** retroactive-release versioning (`v0.0.1`…`v0.0.4` before the already-tagged `v0.1.0`, each tag pinned to its historical source commit rather than the catch-up commit) is a reusable pattern worth a `CLAUDE.md` note if this project ever needs to backfill ceremony again.

---

## ✅ KEEP GATE

*All boxes checked → the loop is closed. The feature is done-done.*

- [x] All pre-flight checks passed at release time
- [x] Changelog written in user-facing language
- [ ] Release actions executed and verified (or `aborted` with reason) — **prepared, awaiting go**: commands listed in §4 are pending explicit user authorization; nothing has been committed or tagged
- [x] Learnings recorded
- [x] Line budget respected: Ist 98 / Soll ~100 (excluding HTML comments)
- [ ] Status set to `released` — held at `preparing`; version-ordering question in §3 is the one open item, plus the pending commit/tag authorization
