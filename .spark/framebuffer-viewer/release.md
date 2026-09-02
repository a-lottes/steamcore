# Release: framebuffer-viewer

| | |
|---|---|
| **Phase** | Keep |
| **Owner** | Release Manager (`/go-live`) |
| **Input** | `review.md` (`passed`, round 3), `qa.md` (`passed`, round 1) |
| **Status** | `preparing` |
| **Version** | v0.0.2 (proposed) |
| **Date** | 2026-09-02 |

**Handoff**
- **Status:** `preparing` — both gates green, pre-flight re-verified fresh on the current
  worktree, commit/tag prepared but **not executed** — no outward-facing action taken,
  no authorization to publish relayed by the caller for this run (`/go-live` invoked
  prepare-only, step 2).
- **Summary:** Retroactive catch-up release for the `.scfb` framebuffer-dump format and
  `tools/fb_view.py` PNG decoder. The source shipped weeks ago as commit `5886318`; only
  `.spark/framebuffer-viewer/qa.md` and this release report are new today. No source
  changes. Second of four queued retroactive catch-ups (`rendering-core` = `v0.0.1`,
  done; this = `v0.0.2`; `text-rendering`, `game-loop` queued behind).
- **Open:** `1 outstanding` — the commit + tag below are prepared, not published; go/no-go
  for the actual `git commit`/`git tag` is a caller/user decision.
- **Binding ruling:** §3 Release Actions and the KEEP GATE below carry the final ruling.
- **On conflict:** the numbered body below wins for everything except `Status`/`Version`;
  log the mismatch at the next `/go-live` and proceed.

## 0. Gates & Delivery Mode

- `review.md`: Status `passed`, round 3. REVIEW GATE fully checked — 0 open Blocker/Major;
  4 open Nits (F9, F10, F11, F17), none blocking, all explicitly left open by the user's
  own scope decision.
- `qa.md`: Status `passed`, round 1. QA GATE fully checked — 22/22 Must ACs verified via
  the constitution's declared substitute method (§8 QA Method: `Browser-observable
  surface: no`; framebuffer dump decoded by `tools/fb_view.py` plus host-compiled unit
  tests) as a **standing project fact**, decided once at `/charter` — not a per-feature
  waiver. This feature *is* that substitute method's own tooling, so every AC was
  captured; 0 bugs found.
- `.spark/constitution.md` §7 Delivery & Handoff: **`direct`** mode, explicitly declared
  (release mode `direct`, approver `n/a`, target branch `main`, ticket format `none`,
  terminal status `released`). Confirmed independently: `git remote -v` returns nothing.
  No PR/handoff step; terminal status will be `released` once published.

## 1. Pre-Flight Checks

*Re-run fresh, this pass, on current `HEAD` (`ebd5d21`) — not copied from `plan.md`/
`review.md`/`qa.md`.*

- [x] `review.md` status is `passed`
- [x] `qa.md` status is `passed` — QA ran via the constitution's declared substitute
      method (§8), cited as a standing project fact; 22/22 Must ACs recorded individually,
      0 bugs
- [x] Full test suite green on current HEAD — `make clean && make test-all`: **109
      passed / 0 failed** across clang `-O2`, ASan+UBSan and the g++-alias build;
      `test-negative` OK (both self-check assertions, incl. zero-match-filter case);
      benches OK (dirty scan 0.0036 ms, text 0.1387 ms, game-loop replay 0.0008 ms — all
      under the 5 ms budget); 15 Python tests via `discover` + 2 standalone round-trip OK;
      `test-png-external` OK (`sips` cross-check 240×160); `make lint` OK (11 rule blocks,
      incl. the stdlib-only import check over `tools/*.py`). This feature's own tests
      (`dump_format_test.cpp`, `tools/test_fb_view.py`, `tools/test_roundtrip.py`) run and
      pass inside this suite, not just historically at `5886318`.
- [x] Build succeeds from a clean state — `make clean` then full rebuild, no cached
      objects; this project has no separate `build` target, `make test-all` is the
      build+test gate.
- [x] No uncommitted changes belonging to this feature — working tree clean except
      `.spark/framebuffer-viewer/qa.md` (untracked, new for this release) and unrelated
      untracked assets (`assets/Buttons.png`, `assets/fonts/`, `assets/sprites/`)
      belonging to later, not-yet-started work — confirmed via `git status`, nothing else
      present.

## 2. Changelog

### Added
- A documented dump-file format (`.scfb`) that captures exactly one full frame the
  rendering engine drew — every pixel, byte for byte — so a rendering change can be
  inspected without any screen or panel attached.
- A command-line viewer (`tools/fb_view.py`) that turns a dump into a PNG image anyone
  can open in a normal image viewer — no ESP32 board, no wired display, no ESP-IDF
  install required.
- From here on, any rendering-touching change (sprites, animations, the boot screen) can
  be visually confirmed correct by actually looking at the pixels it produced, instead of
  trusted on unit-test assertions alone.

### Changed
- (none — no existing behavior changes; new capability only)

### Fixed
- (none — new capability, not a bug fix)

## 3. Release Actions

*`direct` mode: local commit + local annotated tag are the entire "publish" step. **Not
yet executed** — this run is prepare-only; awaiting the caller's explicit go.*

| Action | Result |
|---|---|
| Version bump & tag | **Prepared, not executed.** `v0.0.2` — next in the retroactive-catch-up sequence established by `rendering-core` (`v0.0.1` on `e5d4be3`); confirmed via `git log --oneline e5d4be3..91e2641` that `5886318` is the next commit after `e5d4be3`, before `6b88467`/`91e2641` — matches `CLAUDE.md`'s recorded pattern exactly, not re-derived from scratch. Tag target: annotated `v0.0.2` on `5886318` itself (the historical source commit), never on the catch-up commit. |
| PR / merge | N/A — `direct` mode, no remote configured |
| Deploy | N/A — no build/flash/deploy pipeline exists for this host-tested logic increment |
| Post-release smoke check | **Pending** — runs only after commit + tag are authorized and executed |

**Exact commands pending authorization:**
```
git add .spark/framebuffer-viewer/qa.md .spark/framebuffer-viewer/release.md
git commit -m "Add QA verification and release notes for framebuffer-viewer

Retroactive catch-up: source already shipped as 5886318. This commit adds
only qa.md and release.md, per the pattern established by rendering-core."
git tag -a v0.0.2 5886318 -m "v0.0.2: framebuffer viewer (.scfb dump format, tools/fb_view.py PNG decoder) — retroactive QA/release catch-up"
```

**Rollback path** (local-only, nothing pushed, nothing to unwind remotely):
- Tag wrong: `git tag -d v0.0.2` — removes the local tag only; `5886318` is untouched
  (old, shipped, load-bearing history — never reset or rewritten).
- Release commit (qa.md + release.md) needs undoing: `git reset --soft HEAD~1` —
  restores both files to staged/modified in the working tree; nothing lost, no
  force-push, no coordination needed.
- The commit touches only the two named `.spark/framebuffer-viewer/` files, so undoing
  it cannot affect `5886318`'s source or any of the two remaining backlog features
  (`text-rendering`, `game-loop`).

## 4. Learnings (Keep!)

- **What went well:** the retroactive-versioning pattern proved itself reusable on the
  second application, not just the first — confirming `5886318`'s position in the
  original commit chronology (`git log e5d4be3..91e2641`) before assigning `v0.0.2` caught
  the exact drift class the pattern exists to prevent (a tag landing out of chronological
  order).
- **What we'd do differently:** same note as `rendering-core`'s release — running
  QA/release this far behind the source commit means the release commit can never carry
  a source diff; worth a project-level decision on whether future features get same-day
  ceremony instead of backlog catch-up.
- **Patterns worth reusing:** already captured in `CLAUDE.md` after `rendering-core`;
  this release is the confirming second data point, no new note needed.

---

## ✅ KEEP GATE

*All boxes checked → the loop is closed. The feature is done-done.*

- [x] All pre-flight checks passed at release time
- [x] Changelog written in user-facing language
- [ ] Release actions executed and verified — **prepared, not executed**: commit and tag
      commands are drafted above and awaiting explicit go; nothing published
- [x] Learnings recorded
- [x] Line budget respected: Ist 108 / Soll ~100 (excluding HTML comments) — 8 over, the
      prepare-only state adds an explicit "pending commands" block the template doesn't
      carry for a completed release
- [ ] Status set to `released` — **not yet**; status is `preparing`, terminal `released`
      pending the caller's authorization to run the commands in §3
