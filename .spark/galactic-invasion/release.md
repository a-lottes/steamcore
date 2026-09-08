# Release: galactic-invasion

| | |
|---|---|
| **Phase** | Keep |
| **Owner** | Release Manager (`/go-live`) |
| **Input** | `review.md` (`passed`, round 2), `qa.md` (`passed`, round 1) |
| **Status** | `preparing` |
| **Version** | v0.7.0 (proposed) |
| **Date** | 2026-09-08 |

**Handoff**
- **Status:** `preparing` — both gates verified green, pre-flight re-run fresh (host + device), nothing committed/tagged/pushed yet. Awaiting the caller to relay the user's explicit go-ahead.
- **Summary:** SteamCore's first playable game — a Galaga/Space-Invaders homage reachable directly from the title screen: move, shoot, 3 lives with respawn/invulnerability, a distinct win and loss screen, restart in one button.
- **Open:** `2 outstanding` — (1) execute the prepared commit/tag on explicit go; (2) two questions for the user, listed below, unrelated to whether this release can proceed.
- **Binding ruling:** §3 Release Actions and the KEEP GATE below carry the final ruling.
- **On conflict:** the numbered body below wins for everything except `Status`/`Version`; log the mismatch at the next `/go-live` and proceed.

## 0. Gates & Delivery Mode

- `review.md`: `passed`, round 2 — 0 open Blocker/Major findings (F1 Blocker and F2/F3 Major all `fixed r2`, re-derived by the reviewer via mutation, not accepted on prose). REVIEW GATE checklist: all 7 boxes checked. One open Minor, F8, is informational-only — precisely this staging question, addressed in §3 below.
- `qa.md`: `passed`, round 1 — 0 Blockers/Majors/Minors this round; every Must AC (US-1–US-7, US-10–US-12) and both Should ACs (US-8, US-9) verified by actually running the named test. QA GATE checklist: all 7 boxes checked.
- Constitution §7 Delivery & Handoff: **`direct`** mode, explicitly declared (approver n/a, branch `main`, ticket `none`, terminal status `released`). No `pr`-mode language applies anywhere in this report.
- Constitution §8 QA Method (standing project fact, decided at `/charter`, not a per-feature waiver): browser-observable surface `no`; declared substitute is framebuffer-dump + serial-log for the hardware half, plus host-compiled unit tests for hardware-independent logic. `qa.md`'s `passed` status rests on the **unit-test half**, which ran and is recorded under every AC/NFR ID; the framebuffer/serial half is explicitly deferred this feature per spec.md §1's own scoping (no board session this round) — not silently skipped, and not a claim of on-device confirmation.

## 1. Pre-Flight Checks

*Run fresh, right now, on the uncommitted working tree — not copied from `review.md`/`qa.md`.*

- [x] `review.md` status is `passed` (round 2, checklist fully checked)
- [x] `qa.md` status is `passed` (round 1, checklist fully checked)
- [x] Full host test suite green — `make clean && make test-all` re-run by me just now: `make test`/`test-asan`/`test-gcc`/`test-png-external` **255/255 all four**, exit 0 (`test-negative`'s `0 passed, 1 failed` and `0 passed, 0 failed` are its own deliberate self-checks, not regressions). Six `BENCH OK` (galactic-invasion tick 5.05 µs/tick, ratio 1.63x, budget 16.6667 ms — same order of magnitude as `review.md`'s 4.63 µs and `qa.md`'s 5.09 µs, host-timing variance). `test-python` 31/31, `test-roundtrip` 2/2. `make lint`: **OK**.
- [x] Build succeeds from a clean state — host: covered by the clean `make test-all` above. Device: `source ~/esp/esp-idf/export.sh && idf.py fullclean && idf.py build` from `firmware/system/` (ESP-IDF v5.4.4), re-run by me just now (review round 2 had explicitly *not* re-run this, citing round 1): green, **`[1061/1061]`**, zero warnings, `steamcore_system.bin` 0x39860 bytes (78% partition free); `games/galactic_invasion/galactic_invasion.cpp.obj` confirmed present under `build/esp-idf/main/CMakeFiles/__idf_main.dir/`.
- [ ] No uncommitted changes in the working tree — **not yet true, by design**: this is prepare-only, nothing is committed yet. `git status --short` re-checked immediately before writing this report: every modified/untracked path maps to §3's file list below exactly, **except four pre-existing, unrelated items that stay excluded** — `.spark/start-screen/release.md`, `CLAUDE.md` (both modified in start-screen's own earlier, uncommitted session), `docs/wiring-analog-joystick.md` (analog-joystick-input's own uncommitted wiring note), and untracked `assets/Buttons.png`/`assets/fonts/`/`assets/sprites/` (pre-existing concept assets, not produced by this feature — this game's own art is hand-authored `Color` arrays in `galactic_invasion_art.h`, per spec A14). This is `review.md` F8, informational only. This box will read `[x]` once the prepared commit below is actually created.

## 2. Changelog

### Added
- The console now has its first real game: press START at the title screen and a round begins immediately — no menu, no game selection.
- Move your ship left and right, and fire straight up — one shot in the air at a time, so timing your shots matters.
- A formation of enemies sweeps side to side and creeps closer the longer the round runs; shoot them down to score points, shown live on screen. If the formation ever reaches you, the round ends there and then, however many lives you have left.
- You get 3 lives. Getting hit costs one and instantly puts you back in your starting spot; for a couple of seconds afterward your ship flickers to show you're safe from another hit while you get your bearings.
- Clear every enemy and you get a distinct "YOU WIN" screen; run out of lives (or get caught by the advancing formation) and you get a "GAME OVER" screen instead — both show your final score, and both let you jump straight into a fresh round with one more press of START.
- Enemies occasionally shoot back at you, and the formation moves a little faster each time you thin it out — the round gets tenser the longer you survive.
- Everything on screen — your ship, the enemies, the shots — is real hand-drawn pixel art now, not placeholder boxes.

### Changed
- None — this is new, additive gameplay; nothing about any previously released screen or control changes.

### Fixed
- None (new capability, not a bug-fix release; see the review's own fix history in `review.md` §3 for defects caught and closed before this release, none of which ever reached a player since the feature never shipped before now).

## 3. Release Actions

*Prepared, not executed. Every row below is a plan awaiting the caller's explicit relay of the user's go-ahead — direct mode, no remote, so the two local commands under "Version bump & tag" are this project's entire publish action.*

| Action | Result |
|---|---|
| Commit | **Prepared, not run.** Exact 29-path `git add` list in §3a below (never `-A`/`.`). Proposed message: `feat: add galactic-invasion -- SteamCore's first playable game` + body (see §3b) + `Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>` trailer, matching this repo's convention. |
| Version bump & tag | **Proposed only.** `v0.7.0`, justified in §3c. Tag command (to run against the release commit once created, not before): `git tag -a v0.7.0 -m "galactic-invasion: first playable game -- movement, single-shot firing, 3x6 formation, 3 lives with respawn/invulnerability, win/loss screens, hand-authored sprites; hardware framebuffer/serial confirmation deferred per spec scoping" <release-commit-sha>` |
| PR / merge | N/A — `direct` mode, no remote configured (`git remote -v` empty). |
| Deploy | N/A — no deploy pipeline for this project; "deploy" here means the commit lands on `main`. On-device flashing/play-testing is a separate, further step not requested this pass and not covered by this release's QA scope (framebuffer/serial half explicitly deferred, §0). |
| Post-release smoke check | **Not run — nothing released yet.** Once executed, the equivalent of `analog-joystick-input`'s own §1b (re-run `make test-all` + `idf.py fullclean && idf.py build` on the actual tagged commit, confirm identical pass counts/binary size) should be repeated before calling this `released`. |

### 3a. Exact file list to stage (never `-A`/`.`), reconciled against a fresh `git status --short` immediately before writing this report

```
Makefile
docs/device-build.md
docs/host-tests.md
tools/check_constraints.sh
firmware/system/main/CMakeLists.txt
firmware/system/main/app_main.cpp
games/galactic_invasion/galactic_invasion.h
games/galactic_invasion/galactic_invasion.cpp
games/galactic_invasion/galactic_invasion_art.h
firmware/steamcore/test/galactic_invasion_art_test.cpp
firmware/steamcore/test/galactic_invasion_combat_test.cpp
firmware/steamcore/test/galactic_invasion_determinism_test.cpp
firmware/steamcore/test/galactic_invasion_dump_test.cpp
firmware/steamcore/test/galactic_invasion_enemy_fire_test.cpp
firmware/steamcore/test/galactic_invasion_fixture.h
firmware/steamcore/test/galactic_invasion_formation_test.cpp
firmware/steamcore/test/galactic_invasion_hud_test.cpp
firmware/steamcore/test/galactic_invasion_lives_test.cpp
firmware/steamcore/test/galactic_invasion_projectile_test.cpp
firmware/steamcore/test/galactic_invasion_round_test.cpp
firmware/steamcore/test/galactic_invasion_speedup_test.cpp
firmware/steamcore/test/galactic_invasion_test.cpp
firmware/steamcore/test/bench_galactic_invasion.cpp
firmware/system/main/galactic_invasion_harness_game.h
.spark/galactic-invasion/spec.md
.spark/galactic-invasion/plan.md
.spark/galactic-invasion/review.md
.spark/galactic-invasion/qa.md
.spark/galactic-invasion/release.md
```
29 paths. **Explicitly excluded** (pre-existing, unrelated, confirmed still present and untouched at the time of this reconciliation): `CLAUDE.md`, `.spark/start-screen/release.md`, `docs/wiring-analog-joystick.md`, `assets/Buttons.png`, `assets/fonts/`, `assets/sprites/` — see §0/`review.md` F8. If any of those three modified files' own content should also be committed, that is a separate decision for the user (see §4 questions below), not folded into this commit.

### 3b. Proposed commit message body

```
feat: add galactic-invasion -- SteamCore's first playable game

Composes only already-shipped primitives (GameLoop, GameSession,
collision.h, Framebuffer/drawText/Sprite/blit, drawTitleScreen) into a
Galaga/Space-Invaders homage: horizontal movement, single-active-shot
firing, a 3x6 enemy formation with edge-reversal and descent, 3 lives
with fixed-position respawn and a 120-tick invulnerability window
(with sprite flicker), a formation-threshold failsafe that ends the
round regardless of lives, distinct win/loss screens, and hand-authored
Color-array sprite art for the ship/enemy/projectile. No change to any
composed primitive's public contract (NFR-5); GameState gains no new
enumerator -- win/loss is this game's own private state.

Should-scope US-8 (enemy return fire) and US-9 (formation speedup) both
shipped. Hardware framebuffer/serial confirmation stays explicitly
deferred per spec.md's own scoping; the unit-test half of the project's
declared QA method (constitution SS8) is what backs this release.

255/255 host tests (clang/ASan/gcc), idf.py build green on the device
harness (galactic_invasion_harness_game.h, T16 -- the first time this
feature's code is compiled by the ESP-IDF toolchain, per CLAUDE.md's
"a host-only module can still hide a device-build bug").
```

### 3c. Version justification

`git tag -l --sort=v:refname`: `v0.0.1`–`v0.0.4` (retroactive catch-ups, per `CLAUDE.md`), `v0.1.0`–`v0.6.0` as same-day full-loop releases each tagged on its own release commit, one per feature, always a MINOR bump, never PATCH or MAJOR. This feature continues that unbroken sequence as **v0.7.0**.

**Why MINOR, not a bigger jump, despite this being far larger than any prior increment** (17 tasks, ~255 total test cases up from `v0.6.0`'s 206, the engine's first real `Game`-conforming type): semver's leading `0.y.z` explicitly means "initial development, public API may change at any time, not yet considered stable." That description is still accurate here — `review.md` F5 is an open, accepted finding that the engine's namespace convention for games (`Enemy`/`isAlive`/generic constants sitting at `steamcore::games` scope) is *not yet settled* and is explicitly deferred to be ruled on "before game #2." Tagging `v1.0.0` now would assert API stability the project's own review just declined to assert. Separately, this release changes **zero** public engine symbols — `GameLoop`, `GameSession`, `GameInput`, `Framebuffer`, `Sprite`, `collision.h` are all composed exactly as already shipped (NFR-5, confirmed by `/peer-review`) — so nothing about the *engine's* surface justifies a MAJOR bump either. The feature's size is real and is reflected in the changelog's breadth, not in the version-number arithmetic. Not PATCH (new functionality, not a bug fix).

### Rollback path (local-only — nothing pushed, nothing on a remote to unwind; `git remote -v` is empty)

- Once committed and tagged: if found wrong, `git tag -d v0.7.0` (delete the tag first), then `git reset --soft HEAD~1` — restores every file to the working tree exactly as it is now, nothing lost.
- Tag wrong but commit fine: `git tag -d v0.7.0` only, then re-tag once corrected.
- Nothing is deployed anywhere (no remote, no pipeline, no device flash performed this pass), so no rollback step beyond the two above would ever be needed.
- Currently: nothing has been executed yet, so there is nothing to roll back — this section documents the path for after the caller's go.

## 4. Learnings (Keep!)

- **What went well:** the review's own re-derivation discipline (planting F1's exact latch back, removing F2's guards, planting a lint-evading file) caught that its Round-1 "fixed" claims were genuinely fixed rather than fixed-in-prose — the same rigor this release pass applied to pre-flight (re-running the suite and the device build fresh rather than citing `review.md`'s numbers).
- **What we'd do differently:** F9 (F2's write-once guard has no regression detector) and F3's original gap (T9's threshold rule had none either) are the same class of miss occurring twice in one feature — a Major-level bug fix landing with zero mutation coverage. Worth a standing `/peer-review` habit: any fix to a Blocker/Major finding gets a mutation check against its own guard, not just its target bug, before the finding is marked closed.
- **Patterns worth reusing:** T16's device-build insurance step (compiling this feature's game code through `idf.py build` before release, even with no hardware wired) is exactly the CLAUDE.md pattern from `text-rendering`'s font.cpp incident, applied proactively this time instead of being discovered as a mid-increment surprise — worth keeping as a standard task on every future game feature's plan, not just a lesson learned after the fact.

**Two questions for the user** (do not block this release; they're about scope of what else gets committed, not about whether galactic-invasion is ready):
1. `.spark/start-screen/release.md` is modified in the working tree to reflect its actual already-completed state (`released`, tag `v0.4.0` on `a8e590f`) but was never committed — this project's own pattern is a small separate follow-up commit for exactly this (see "Record ... release outcome" commits). Should that follow-up commit be made (separately from this release)?
2. `CLAUDE.md`'s two uncommitted sections (from `start-screen`) and `docs/wiring-analog-joystick.md`'s uncommitted troubleshooting note (from `analog-joystick-input`) — same question: worth their own small housekeeping commits, separately from this release?

---

## ✅ KEEP GATE

*All boxes checked → the loop is closed. The feature is done-done.*

- [x] All pre-flight checks passed at release time — 4 of 5 boxes checked; the 5th ("no uncommitted changes") is honestly unchecked because nothing has been committed yet in this prepare-only pass, with the exact reconciled file list and the four excluded items named.
- [x] Changelog written in user-facing language
- [ ] Release actions executed and verified (or `aborted` with reason) — **not executed**; prepared only, awaiting the caller's explicit relay of the user's go-ahead (see §3). This is a normal, reportable state, not a failure.
- [x] Learnings recorded
- [x] Line budget respected: Ist 161 / Soll ~100 — 61 over; reason: the never-`-A` 29-path file list, the version-justification paragraph, the fresh device-build pre-flight detail, and the prepare-vs-execute Release Actions split this pass specifically requires together account for the overage, not prose padding (mirrors `v0.6.0`'s own precedent of a similar overage for the same reasons).
- [ ] Status set to `released`, or `handed-off` in declared `pr` mode — **not set**. Status stays `preparing`: both gates are green and pre-flight is fresh and clean, but no commit, tag, or push has been made. Awaiting the user's explicit go-ahead, to be relayed back through the caller.
