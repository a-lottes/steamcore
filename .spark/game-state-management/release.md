# Release: game-state-management

| | |
|---|---|
| **Phase** | Keep |
| **Owner** | Release Manager (`/go-live`) |
| **Input** | `review.md` (`passed`, round 2), `qa.md` (`passed`, round 1) |
| **Status** | `preparing` |
| **Version** | v0.1.0 (proposed only — not yet tagged) |
| **Date** | 2026-09-02 |

**Handoff**
- **Status:** `preparing` — prepare-only pass, no outward-facing or local-write action taken (no commit, no tag). Gates green, pre-flight green, awaiting the user's go to execute §3.
- **Summary:** READY/PLAYING/GAME_OVER session-phase component, both gates green, fresh suite green on the current worktree; commit + tag are prepared but not executed.
- **Open:** `1 outstanding` — explicit user authorization to run the three commands in §3 (commit, tag; no push/deploy exists in direct mode). Owner: the user (caller relays the go).
- **Binding ruling:** §3 Release Actions and the KEEP GATE below carry the final ruling.
- **On conflict:** the numbered body below wins for everything except `Status`/`Version`; log the mismatch as a finding at the next `/go-live` and proceed.

## 0. Gates & Delivery Mode

- `review.md`: Status `passed`, round 2. REVIEW GATE fully checked — no open Blocker/Major; F2 confirmed fixed by re-injected mutant; F5/F6/F8 user-accepted/excluded.
- `qa.md`: Status `passed`, round 1. QA GATE fully checked — 15/15 Must ACs verified via the constitution's declared substitute method (§8: unit-test half, host toolchain; framebuffer-dump half N/A, not yet enforceable — no ESP-IDF/board), as a standing project fact, not a per-feature waiver. 0 bugs found.
- `.spark/constitution.md` §7 Delivery & Handoff: **`direct`** mode, explicitly declared (release mode `direct`, approver `n/a`, target branch `main`, ticket format `none`, terminal status `released`). Confirmed independently: `git remote -v` returns nothing — no remote configured. No PR/handoff step; terminal status is `released`, not `handed-off`.

## 1. Pre-Flight Checks

*Re-run fresh, this pass, not copied from `plan.md`/`review.md`/`qa.md`.*

- [x] `review.md` status is `passed`
- [x] `qa.md` status is `passed`
- [x] Full test suite green on the current worktree — `make clean && make test-all`: **109 passed / 0 failed** in each of clang `-O2`, ASan+UBSan, and the g++-alias build; `test-negative` OK (both self-check assertions); 3× `BENCH OK` (dirty scan 0.0036 ms, text 0.1453 ms, game-loop replay 0.0008 ms — all under the 5 ms budget); 15 Python tests + 2 round-trip tests OK; `test-png-external` OK (`sips` cross-check 240×160); `make lint` OK (11 rule blocks, including the 3 new game-state ones)
- [x] Build succeeds from a clean state — `make clean` then full rebuild, no cached objects, all targets above compiled from scratch
- [ ] No uncommitted changes in the working tree — **expected, not a failure**: this is a prepare-only pass before the release commit exists. `git status` shows exactly: untracked `game_state.h/.cpp` + 3 test/fixture files + `.spark/game-state-management/` (this feature's own files, slated for §3's commit), plus `README.md` modified (deliberately **excluded** — belongs to the already-shipped `rendering-core` increment, per review F5/user ruling) and untracked `assets/Buttons.png`, `assets/fonts/`, `assets/sprites/` (unrelated in-progress work, deliberately **excluded**). No stray or unexplained diff.

## 2. Changelog

### Added
- A shared session state (Ready / Playing / Game Over) any future game built on this engine can use instead of each one inventing its own "has play started / has the run ended" bookkeeping from scratch.
- Pressing and holding the start button now reliably begins a session exactly once — holding the button down can never re-trigger the start, unlike a naive check that would flicker between states every tick it stays held.
- After a run ends, one fresh press of start jumps straight back into play — no extra screen or second press required.

### Changed
- (none — no existing behavior changes; this is new, additive engine surface only)

### Fixed
- (none — new capability, not a bug fix)

## 3. Release Actions

*Prepared, not executed. Local commit + local annotated tag are the entire "publish" step in `direct` mode (no remote, no PR, no deploy) — both still require the user's explicit go.*

| Action | Result |
|---|---|
| Version bump & tag | **Pending — awaiting go.** Exact commands: <br>`git add firmware/steamcore/include/steamcore/game_state.h firmware/steamcore/src/game_state.cpp firmware/steamcore/test/game_state_test.cpp firmware/steamcore/test/game_state_determinism_test.cpp firmware/steamcore/test/session_replay_fixture.h docs/host-tests.md tools/check_constraints.sh .spark/game-state-management/spec.md .spark/game-state-management/plan.md .spark/game-state-management/review.md .spark/game-state-management/qa.md .spark/game-state-management/release.md` <br>`git commit -m "Add game state management: READY/PLAYING/GAME_OVER session-phase component" -m "One entry point (advance) drives edge-triggered START handling and a one-press restart from GAME_OVER to PLAYING, proven deterministic the same way game-loop proved its replay guarantee. No change to GameInput or GameLoop<Game>."` <br>`git tag -a v0.1.0 -m "v0.1.0: game state management (READY/PLAYING/GAME_OVER session-phase component)"` |
| PR / merge | N/A — `direct` mode, no remote configured, no PR workflow |
| Deploy | N/A — no build/flash/deploy pipeline exists (no ESP-IDF toolchain, no board); this is a host-tested logic increment, same posture as the four prior increments |
| Post-release smoke check | N/A this pass — nothing published yet. Once tagged: `git log -1 --stat` and `make clean && make test-all` on the tagged commit stand in for "alive" (no running device/service to ping in this project) |

**Version justification:** `git tag -l` returns nothing and no `.spark/*/release.md` exists for any of the 4 prior increments — **no tagging/versioning convention exists yet** in this project; nothing to follow, so none is invented from a false precedent. Proposing the simplest sensible default for a solo/local repo: semver, starting at **v0.1.0** — pre-1.0 because the project is still assembling its Phase-2 engine checklist (README) with no hardware-verified build yet (constitution §4), not because this increment is unstable. This is an initial tag, not a "bump" — there is nothing before it to bump from.

**Rollback path** (local-only, no remote to unwind — nothing was ever pushed):
- Tag created but wrong: `git tag -d v0.1.0` — removes the local tag only, commit is untouched.
- Commit itself needs undoing (before or after removing the tag): `git reset --soft HEAD~1` — restores every file from the commit back to staged/modified in the working tree; nothing is lost, no force-push, no remote coordination needed since none exists.
- `README.md`'s own pending diff is never staged or touched by any of the above — it was excluded from `git add` by design (F5) and stays exactly as it is in the working tree.

## 4. Learnings (Keep!)

- **What went well:** two-round `/peer-review` caught a real false-green gap (AC-3.2's "pressing" case, F2) with targeted mutation testing — injecting the exact defect class the AC exists to catch — before it ever reached QA; the gate did the job it exists for.
- **What we'd do differently:** new-test documentation (`docs/host-tests.md`) lagged behind new-lint-rule documentation in the same task (F8, accepted); folding "list the new test names" into the same DoD line as "list the new lint rules" would have caught it at `/increment` time instead of at review.
- **Patterns worth reusing:** the "mutate the transition to the specific wrong behavior an AC exists to forbid, confirm exactly the AC's own test fails and nothing else does" technique (T5(d), F2, F4) is a strong, repeatable template for any future edge-triggered or state-machine feature — candidate for `CLAUDE.md` / project memory as a standard `/peer-review` technique on this codebase.

---

## ✅ KEEP GATE

*All boxes checked → the loop is closed. The feature is done-done.*

- [ ] All pre-flight checks passed at release time — 4/5 true; the 5th ("no uncommitted changes") is the expected pre-commit state of a prepare-only pass, not a failure, and is explained in §1
- [x] Changelog written in user-facing language
- [ ] Release actions executed and verified (or `aborted` with reason) — **not executed this pass by design**; prepared and awaiting the user's go (§3); this is a normal, reportable state, not a failure
- [x] Learnings recorded
- [x] Line budget respected: Ist 98 / Soll ~100 (excluding HTML comments)
- [ ] Status set to `released` — intentionally left as `preparing`; this pass is prepare-only, no commit/tag/publish action was authorized or taken
