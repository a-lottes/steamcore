# Release: text-rendering

| | |
|---|---|
| **Phase** | Keep |
| **Owner** | Release Manager (`/go-live`) |
| **Input** | `review.md` (`passed`, round 2), `qa.md` (`passed`, round 1) |
| **Status** | `preparing` |
| **Version** | v0.0.3 (proposed) |
| **Date** | 2026-09-03 |

**Handoff**
- **Status:** `preparing` — both gates green, pre-flight re-verified fresh on the current
  worktree, release commit and annotated tag drafted below but **not executed** — this run
  is prepare-only, no outward-facing (or local-commit) action taken, per explicit
  instruction. Awaiting the caller's go before any command in §3 runs.
- **Summary:** Retroactive catch-up release for the 8×8 bitmap font and `drawText`. The
  source shipped weeks ago as commit `6b88467`; only `.spark/text-rendering/qa.md` and this
  release report are new at commit time. No source changes. Third of four queued
  retroactive catch-ups (`rendering-core` = `v0.0.1`, done; `framebuffer-viewer` = `v0.0.2`,
  done; this = `v0.0.3`, prepared; `game-loop` queued behind as `v0.0.4`).
- **Open:** `1 outstanding` — publish (release commit + annotated tag) awaiting explicit
  user go-ahead; owner is whoever relays that authorization back to `/go-live`.
- **Binding ruling:** §3 Release Actions and the KEEP GATE below carry the final ruling.
- **On conflict:** the numbered body below wins for everything except `Status`/`Version`;
  log the mismatch at the next `/go-live` and proceed.

## 0. Gates & Delivery Mode

- `review.md`: Status `passed`, round 2. REVIEW GATE fully checked — 0 open Blocker/Major/
  Minor/Nit (all 12 findings fixed and re-verified against their own repros).
- `qa.md`: Status `passed`, round 1. QA GATE fully checked — 15/15 Must ACs verified via
  the constitution's declared substitute method (§8 QA Method: `Browser-observable
  surface: no`; framebuffer dump decoded by `tools/fb_view.py` plus host-compiled unit
  tests) as a **standing project fact**, decided once at `/charter` — not a per-feature
  waiver. 0 bugs found.
- `.spark/constitution.md` §7 Delivery & Handoff: **`direct`** mode, explicitly declared
  (release mode `direct`, approver `n/a`, target branch `main`, ticket format `none`,
  terminal status `released`). No PR/handoff step; terminal status will be `released` once
  §3 executes.

## 1. Pre-Flight Checks

*Run fresh, this pass, on current HEAD (`43b500e`) — not copied from `plan.md`/`review.md`/`qa.md`.*

- [x] `review.md` status is `passed`
- [x] `qa.md` status is `passed` — QA ran via the constitution's declared substitute method
      (§8), cited as a standing project fact; 15/15 Must ACs recorded individually, 0 bugs
- [x] Full test suite green on current HEAD — `make clean && make test-all`: **109
      passed / 0 failed** across clang `-O2`, ASan+UBSan and the g++-alias build;
      `test-negative` OK (both self-check assertions); benches OK (dirty scan 0.0080 ms,
      text 0.1280 ms for 600 chars, game-loop replay 0.0008 ms — all under the 5 ms
      budget); 15 Python tests via `discover` + 2 standalone round-trip OK;
      `test-png-external` OK (`sips` cross-check 240×160); `make lint` OK (11 rule blocks,
      incl. the text-rendering glyph-metric-literal rule). This feature's own tests
      (`font_test.cpp`, `text_test.cpp`, `text_fixture_test.cpp`) run and pass inside this
      suite, not just historically at `6b88467`.
- [x] Build succeeds from a clean state — `make clean` then full rebuild, no cached
      objects; this project has no separate `build` target, `make test-all` is the
      build+test gate.
- [x] No uncommitted changes belonging to this feature — working tree clean except
      `.spark/text-rendering/qa.md` (untracked, new for this release) and unrelated
      untracked assets (`assets/Buttons.png`, `assets/fonts/`, `assets/sprites/`) belonging
      to later, not-yet-started work — confirmed via `git status`.

## 2. Changelog

### Added
- A hand-authored 8×8 pixel font covering every letter, digit and the punctuation the
  device's boot, menu and highscore screens already use.
- A way to draw a line of text — labels, status messages, a score — directly into the
  picture the console builds, in a single chosen colour.
- From here on, any on-screen message ("SYSTEM READY", a score, a menu label) can be shown
  as real text instead of being hand-drawn pixel by pixel for every new string.

### Changed
- (none — no existing behavior changes; new capability only)

### Fixed
- (none — new capability, not a bug fix)

## 3. Release Actions

*`direct` mode: local commit + local annotated tag are the entire "publish" step. **Not
executed** this run — prepared only, pending explicit user go-ahead.*

| Action | Result |
|---|---|
| Version bump & tag | **Prepared, not executed.** `v0.0.3` — next in the retroactive-catch-up sequence established by `rendering-core`/`framebuffer-viewer` (CLAUDE.md). Confirmed via `git log --oneline` that `6b88467` sits immediately after `5886318` (`v0.0.2`) and immediately before `91e2641` (queued as `v0.0.4`, game-loop) in the original chronology. Annotated tag would be created on `6b88467` itself (the historical source commit), never on the catch-up commit. |
| Release commit | **Prepared, not executed.** Would add only `.spark/text-rendering/qa.md` and `.spark/text-rendering/release.md`, staged individually via two explicit `git add <path>` calls — never `-A`/`.`. |
| PR / merge | N/A — `direct` mode, no remote configured |
| Deploy | N/A — no build/flash/deploy pipeline exists for this host-tested logic increment |
| Post-release smoke check | Pending — to be run on the resulting HEAD after the commit above, before status moves to `released` |

**Commands prepared, pending explicit go:**
```
git add .spark/text-rendering/qa.md
git add .spark/text-rendering/release.md
git commit -m "Add QA verification and release notes for text-rendering

Retroactive catch-up: source already shipped as 6b88467. This commit adds
only qa.md and release.md, per the pattern established by rendering-core
and framebuffer-viewer."
git tag -a v0.0.3 6b88467 -m "v0.0.3: text rendering (8x8 bitmap font, drawText) — retroactive QA/release catch-up"
```

**Rollback path** (local-only, nothing pushed, nothing to unwind remotely):
- Nothing has been committed yet — if the go is withdrawn, no action is needed; the working
  tree already reflects the pre-release state except the untracked `qa.md`/`release.md`.
- Once executed: tag wrong → `git tag -d v0.0.3` — removes the local tag only; `6b88467` is
  untouched (old, shipped, load-bearing history — never reset or rewritten).
- Once executed: release commit needs undoing → `git reset --soft HEAD~1` — restores both
  files to staged/modified in the working tree; nothing lost, no force-push, no
  coordination needed (nothing would ever have been pushed).
- The prepared commit touches only the two named `.spark/text-rendering/` files, so undoing
  it cannot affect `6b88467`'s source or the one remaining backlog feature (`game-loop`).

## 4. Learnings (Keep!)

- **What went well:** the retroactive-versioning pattern held on its third application
  without adjustment — confirming `6b88467`'s position in the original commit chronology
  before assigning `v0.0.3` again caught the exact drift class the pattern exists to
  prevent. Round 2's anchor-table fix (review F1) is a genuinely reusable idea: an
  independently-derived, machine-checkable oracle beats "eyeball the ASCII art."
- **What we'd do differently:** same note as the two prior retroactive releases — running
  QA/release this far behind the source commit means the release commit can never carry a
  source diff; still worth a project-level decision on same-day ceremony vs. backlog
  catch-up for future features.
- **Patterns worth reusing:** the review's own `tools/generate_font_anchors.py` pattern
  (F10) — an independently-derived oracle, committed and re-runnable, not just asserted in
  a report — is worth naming explicitly in `CLAUDE.md` for any future asset-table feature
  (sprites, tilesets) that needs the same anti-false-green guarantee.

---

## ✅ KEEP GATE

*All boxes checked → the loop is closed. The feature is done-done.*

- [x] All pre-flight checks passed at release time
- [x] Changelog written in user-facing language
- [ ] Release actions executed and verified — **prepared, not executed**; awaiting
      explicit user go-ahead for the commit + tag commands in §3
- [x] Learnings recorded
- [x] Line budget respected: Ist 100 / Soll ~100 (excluding HTML comments)
- [ ] Status set to `released` — currently `preparing`, pending the go
