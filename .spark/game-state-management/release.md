# Release: game-state-management

| | |
|---|---|
| **Phase** | Keep |
| **Owner** | Release Manager (`/go-live`) |
| **Input** | `review.md` (`passed`, round 2), `qa.md` (`passed`, round 1) |
| **Status** | `released` |
| **Version** | v0.1.0 |
| **Date** | 2026-09-02 |

**Handoff**
- **Status:** `released` — commit and local annotated tag executed on explicit user go ("go, commit und tag für game-state-management"); post-release smoke check green.
- **Summary:** READY/PLAYING/GAME_OVER session-phase component, both gates green, fresh suite green on the release commit and again on the tagged commit; `direct` mode, no remote — commit + tag are the entire publish step, both now done.
- **Open:** none. The release is complete for this increment.
- **Binding ruling:** §3 Release Actions and the KEEP GATE below carry the final ruling.
- **On conflict:** the numbered body below wins for everything except `Status`/`Version`; log the mismatch as a finding at the next `/go-live` and proceed.

## 0. Gates & Delivery Mode

- `review.md`: Status `passed`, round 2. REVIEW GATE fully checked — no open Blocker/Major; F2 confirmed fixed by re-injected mutant; F5/F6/F8 user-accepted/excluded.
- `qa.md`: Status `passed`, round 1. QA GATE fully checked — 15/15 Must ACs verified via the constitution's declared substitute method (§8: unit-test half, host toolchain; framebuffer-dump half N/A, not yet enforceable — no ESP-IDF/board), as a standing project fact, not a per-feature waiver. 0 bugs found.
- `.spark/constitution.md` §7 Delivery & Handoff: **`direct`** mode, explicitly declared (release mode `direct`, approver `n/a`, target branch `main`, ticket format `none`, terminal status `released`). Confirmed independently: `git remote -v` returns nothing — no remote configured. No PR/handoff step; terminal status is `released`.

## 1. Pre-Flight Checks

*Re-run fresh, this pass, not copied from `plan.md`/`review.md`/`qa.md`. Re-verified a second time immediately before the commit, since time had passed since this section was first prepared.*

- [x] `review.md` status is `passed`
- [x] `qa.md` status is `passed`
- [x] Full test suite green on the release commit — `make clean && make test-all`: **109 passed / 0 failed** in each of clang `-O2`, ASan+UBSan, and the g++-alias build; `test-negative` OK (both self-check assertions); 3× `BENCH OK` (dirty scan ~0.003 ms, text ~0.14-0.15 ms, game-loop replay ~0.0008-0.0009 ms — all under the 5 ms budget); 15 Python tests + 2 round-trip tests OK; `test-png-external` OK (`sips` cross-check 240×160); `make lint` OK (11 rule blocks, including the 3 game-state ones). Re-run and confirmed identical on the now-tagged commit (§3 smoke check).
- [x] Build succeeds from a clean state — `make clean` then full rebuild, no cached objects, all targets above compiled from scratch — done twice: once pre-commit, once post-tag.
- [x] No uncommitted changes belonging to this feature in the working tree — the release commit (`212e2e5`) contains exactly the 12 files listed in §3, verified by `git status` before staging (explicit `git add` of each path, never `-A`/`.`) and after staging (staged set matched the list exactly). The working tree still carries unrelated diffs, deliberately left untouched: `README.md` modified (excluded — belongs to the already-shipped `rendering-core` increment, per review F5/user ruling), `.spark/constitution.md` modified (a separate, unrelated `/charter` amendment from concurrent hardware bring-up work), and untracked `assets/Buttons.png`, `assets/fonts/`, `assets/sprites/`, `firmware/steamcore/include/steamcore/board_config.h`, `firmware/system/` (unrelated in-progress work). None of these were staged or committed; confirmed by `git status` immediately after tagging.

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

*Local commit + local annotated tag are the entire "publish" step in `direct` mode (no remote, no PR, no deploy). Executed on the user's explicit go: "go, commit und tag für game-state-management".*

| Action | Result |
|---|---|
| Version bump & tag | **Done.** Staged exactly the 12 files named in the prepared plan (5 new firmware files, `docs/host-tests.md`, `tools/check_constraints.sh`, and this feature's 5 `.spark/game-state-management/` ceremony docs) via explicit `git add <path>...` — no `-A`/`.`. Committed as `212e2e5` — "Add game state management: READY/PLAYING/GAME_OVER session-phase component". Tagged `v0.1.0` as an annotated tag: "v0.1.0: game state management (READY/PLAYING/GAME_OVER session-phase component)". |
| PR / merge | N/A — `direct` mode, no remote configured, no PR workflow |
| Deploy | N/A — no build/flash/deploy pipeline exists (no ESP-IDF toolchain, no board); this is a host-tested logic increment, same posture as the four prior increments |
| Post-release smoke check | **Done, green.** `git log -1 --stat` on `212e2e5` shows exactly the 12 expected files (1308 insertions, 3 deletions). `make clean && make test-all` on the tagged commit: 109/0 passed across all three test builds, `test-negative` OK, all 3 benches under the 5 ms budget, 15+2 Python tests OK, `test-png-external` OK, `make lint` OK — identical to the pre-commit run. No running device/service to ping in this project; this stands in for "alive". |

**Version justification:** `git tag -l` returned nothing before this release and no `.spark/*/release.md` existed for any of the 4 prior increments — **no tagging/versioning convention existed yet** in this project; nothing to follow, so none was invented from a false precedent. Used the simplest sensible default for a solo/local repo: semver, starting at **v0.1.0** — pre-1.0 because the project is still assembling its Phase-2 engine checklist (README) with no hardware-verified build yet (constitution §4), not because this increment is unstable. This is an initial tag, not a "bump" — there was nothing before it to bump from.

**Rollback path** (local-only, no remote to unwind — nothing was ever pushed):
- Tag created but wrong: `git tag -d v0.1.0` — removes the local tag only, commit `212e2e5` is untouched.
- Commit itself needs undoing (before or after removing the tag): `git reset --soft HEAD~1` — restores every file from the commit back to staged/modified in the working tree; nothing is lost, no force-push, no remote coordination needed since none exists.
- `README.md`'s own pending diff, `.spark/constitution.md`'s pending amendment, and the concurrent hardware bring-up files (`board_config.h`, `firmware/system/`, `assets/*`) were never staged or touched by any of the above — excluded by design (F5 and out-of-scope concurrent work) and remain exactly as they were in the working tree, confirmed by `git status` after the commit and tag.

## 4. Learnings (Keep!)

- **What went well:** two-round `/peer-review` caught a real false-green gap (AC-3.2's "pressing" case, F2) with targeted mutation testing — injecting the exact defect class the AC exists to catch — before it ever reached QA; the gate did the job it exists for.
- **What we'd do differently:** new-test documentation (`docs/host-tests.md`) lagged behind new-lint-rule documentation in the same task (F8, accepted); folding "list the new test names" into the same DoD line as "list the new lint rules" would have caught it at `/increment` time instead of at review.
- **Patterns worth reusing:** the "mutate the transition to the specific wrong behavior an AC exists to forbid, confirm exactly the AC's own test fails and nothing else does" technique (T5(d), F2, F4) is a strong, repeatable template for any future edge-triggered or state-machine feature — candidate for `CLAUDE.md` / project memory as a standard `/peer-review` technique on this codebase. Also worth reusing: explicit, named-path `git add` (never `-A`/`.`) as standard practice whenever unrelated concurrent work (here: a hardware bring-up effort with its own uncommitted `/charter` amendment and new files) is sitting in the same working tree at release time.

---

## ✅ KEEP GATE

*All boxes checked → the loop is closed. The feature is done-done.*

- [x] All pre-flight checks passed at release time — 5/5, re-verified fresh on the release commit and again on the tagged commit
- [x] Changelog written in user-facing language
- [x] Release actions executed and verified — commit `212e2e5` and tag `v0.1.0` created on explicit user go; post-release smoke check green
- [x] Learnings recorded
- [x] Line budget respected: Ist 99 / Soll ~100 (excluding HTML comments)
- [x] Status set to `released`
