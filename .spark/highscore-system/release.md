# Release: highscore-system

| | |
|---|---|
| **Phase** | Keep |
| **Owner** | Release Manager (`/go-live`) |
| **Input** | `review.md` (`passed`), `qa.md` (`passed`) |
| **Status** | `released` |
| **Version** | v0.8.0 |
| **Date** | 2026-09-08 |

**Handoff**
- **Status:** `released`. Direct mode (constitution §7) has no outward-facing step at all — no remote, no PR, no deploy, no publish — so the release commit + local annotated tag *are* the complete release; nothing was held back awaiting authorization because nothing outward-facing exists to authorize.
- **Summary:** Persistent, format-versioned top-5 highscore table per game with arcade-style initials entry, galactic-invasion wired as its first real consumer — release commit `da8f3f4` and local annotated tag `v0.8.0` on `main`.
- **Open:** `none` — both gates green, pre-flight re-verified fresh on the release commit, commit and tag exist locally, rollback path recorded below.
- **Binding ruling:** §3 Release Actions and the KEEP GATE below carry the final ruling.
- **On conflict:** the numbered body below wins for everything except `Status`/`Version`; log the mismatch as a finding at the next `/go-live` and proceed — don't stop on it.

## 1. Pre-Flight Checks

- [x] `review.md` status is `passed` (Round 2; F1–F11 all `fixed`/`fixed r2`, gate checklist fully checked)
- [x] `qa.md` status is `passed` (Round 1; 0 open Blockers/Majors/Minors, gate checklist fully checked)
- [x] Full test suite green on the release commit — `make test`/`test-asan`/`test-gcc`: **318 passed, 0 failed** on all three, re-run by me just now
- [x] Build succeeds from a clean state — `idf.py build` from `firmware/system/` (ESP-IDF v5.4.4) green: `steamcore_system.bin` 0x3f0d0 bytes, 75% of app partition free (matches review/QA figures exactly)
- [x] No uncommitted changes in the working tree — `make lint` re-run clean (all sub-rules incl. the new highscore-system block); `git status` reconciled against plan.md §2's own file list before staging (see §3); `assets/Buttons.png`/`assets/fonts/`/`assets/sprites/` left untracked, exactly as instructed — not part of this feature's task list

## 2. Changelog

### Added
- A persistent top-5 highscore table for each game, saved to the console's internal flash — a good round is still there the next time the console powers on.
- Arcade-style initials entry: after a qualifying round, type 3 letters with the existing controls (up/down to cycle, fire to confirm) — no keyboard needed.
- A top-5 list screen shown right after entering initials, so players see exactly where they landed.
- Galactic Invasion now reports its final score into this system — a good run is no longer lost the moment the round ends.

### Changed
- A non-qualifying Galactic Invasion round still ends exactly as before — this feature never gets in the way of a quick restart.

### Fixed
- (none — new feature, no prior user-facing behavior corrected)

## 3. Release Actions

| Action | Result |
|---|---|
| Version bump & tag | `v0.8.0` — minor bump from `v0.7.0` (galactic-invasion). Justification: this project's own established pattern bumps minor for every feature that completes the full spec-to-QA loop (v0.1.0 through v0.7.0 so far); `highscore-system` is a new Must-priority feature (US-1 through US-6, multiple new user-facing stories) that went through the complete loop live, so it continues that same series rather than a patch or a sub-0.1.0 retroactive slot (those are reserved for pre-loop catch-up releases per CLAUDE.md, which doesn't apply here). Release commit `da8f3f4`; local annotated tag `v0.8.0` created pointing at it. |
| PR / merge | N/A — direct mode (constitution §7): solo project, no remote configured (`git remote -v` empty), target branch `main`. No PR ever exists in this project. |
| Deploy | N/A — no deploy target exists for this project; the committed/tagged source itself is the deliverable, ready to be flashed on request. |
| Post-release smoke check | Not run — nothing was pushed/deployed to check (no remote, no deploy target). T15/AC-1.5's hardware-gated real power-cycle proof remains the actual smoke check for this feature once a board is attached; still `blocked`/`not capturable` today (no board attached this session, per plan.md T15 and qa.md). |

**Rollback path:** this project's established pattern (prior releases) — `git revert da8f3f4` (or `git reset --hard 8c4e553` if the commit must disappear entirely) restores the pre-release tree; `git tag -d v0.8.0` removes the local tag. No deployed service or remote exists to unwind; nothing was ever pushed.

## 4. Learnings (Keep!)

- **What went well:** The `qualifies()`/`insert()` clamp-agreement and NFR-4 namespace-scope findings (F9, F3) show the value of re-deriving audits by an independent method each review round rather than citing the prior round's conclusion — Round 1's class-body-only `public:` scan structurally couldn't see the namespace-scope leak that Round 2's parse/compile-probe caught.
- **What we'd do differently:** F2 (TABLE screen name not centred in its worst-case field) and F11 (nullptr regression introduced by F2's own fix) show a fix-then-regress pattern — a fix to a layout/centring bug is exactly the kind of change worth re-running the full boundary-case sweep (empty/nullptr/worst-case) against, not just the originally reported case.
- **Patterns worth reusing:** The `steamcore::detail` namespace convention for hiding byte-layout/version constants (already used by `input.h`/`analog_axis.h`/`game_loop.h`/`title_screen.h`) is now also `highscore.h`'s answer to a library-lens NFR — worth calling out explicitly in CLAUDE.md as the default home for any future persisted-format internals, so a future feature's own audit doesn't repeat Round 1's blind spot from scratch.

---

## ✅ KEEP GATE

*All boxes checked → the loop is closed. The feature is done-done.*

- [x] All pre-flight checks passed at release time
- [x] Changelog written in user-facing language
- [x] Release actions executed and verified (or `aborted` with reason) — release commit + local tag executed and verified (`git log --oneline --decorate` confirms `da8f3f4`/`v0.8.0`); no PR/deploy/publish step exists in direct mode
- [x] Learnings recorded
- [x] Line budget respected: Ist 80 / Soll ~100 (excluding HTML comments)
- [x] Status set to `released`, or `handed-off` in declared `pr` mode — direct mode (constitution §7); set to `released`
