# Release: input-driver

| | |
|---|---|
| **Phase** | Keep |
| **Owner** | Release Manager (`/go-live`) |
| **Input** | `review.md` (`passed`, round 2), `qa.md` (`passed`, round 1) |
| **Status** | `released` |
| **Version** | v0.3.0 |
| **Date** | 2026-09-04 |

**Handoff**
- **Status:** `released` — both gates green, fresh pre-flight green on host
  and device toolchains before the commit, release actions executed
  (commit `79ea407761e3f0fe6a7ef75d0a698ec1998d99d2`, tag `v0.3.0`), and the
  post-release smoke check re-run green on the tagged commit itself.
- **Summary:** `GameInput` grows from two fields to all seven README commits
  to (joystick + SELECT), driven by one debounced core proven host-side
  against a simulated source, plus the real GPIO driver and a wiring guide —
  US-4's on-device confirmation (T10) stays honestly `blocked`, no hardware
  wired yet.
- **Open:** `0 outstanding` — all release actions executed and verified;
  nothing pending.
- **Binding ruling:** §3 Release Actions and the KEEP GATE below carry the
  final ruling.
- **On conflict:** the numbered body below wins for everything except
  `Status`/`Version`; log the mismatch at the next `/go-live` and proceed.

## 0. Gates & Delivery Mode

- `review.md`: `passed`, round 2. 0 open Blocker/Major. F1–F9 all
  `verified-fixed r2` (three probed by mutation, not read-only). F10 (`qa.md`
  currency) `fixed` via a dated note, cheaper option chosen over a full
  `/demo-day` re-run, per the finding's own suggestion.
- `qa.md`: `passed`, round 1, dated post-QA note added 2026-09-04 covering the
  F1–F9 fix diff. Every Must AC (US-1/US-2/US-3) verified, per the QA method
  `.spark/constitution.md` §8 declares as a standing project fact (surface
  `Browser-observable: no`; substitute: host-compiled unit tests for
  hardware-independent logic + framebuffer/serial transcript for device
  output — this feature is log-only, no framebuffer touched). US-4's four
  hardware-gated ACs are honestly recorded `not capturable`, not claimed.
- Constitution §7: **`direct`** mode, explicitly declared (approver n/a,
  branch `main`, ticket `none`, terminal status `released`). No `pr` mode
  language applies anywhere in this report.

## 1. Pre-Flight Checks

*Run fresh, right now, on the uncommitted working tree — not copied from
`review.md`/`qa.md`.*

- [x] `review.md` status is `passed`
- [x] `qa.md` status is `passed`
- [x] Full host test suite green — `make test` / `make test-asan` /
      `make test-gcc`: **154/154 all three.** `make lint`: clean (19 blocks).
- [x] Build succeeds from a clean state — `idf.py fullclean && idf.py build`
      from `firmware/system/` (ESP-IDF v5.4.4 exported): green, zero
      warnings, `steamcore_system.bin` 0x2fef0 bytes, 81% partition free.
- [x] No unaccounted uncommitted changes — `git status` re-checked immediately
      before writing this report (not trusted from the orchestrator's task
      description): every modified/deleted/untracked path maps to §3's file
      list below except the three pre-existing, unrelated untracked assets
      (`assets/Buttons.png`, `assets/fonts/`, `assets/sprites/`), which
      `review.md` §1 names as belonging to no task here and which stay
      excluded from the commit.
- [x] `git diff --exit-code v0.1.0 -- game_state.{h,cpp}`: empty — `GameSession`
      still byte-identical to its shipped v0.1.0 form.

## 2. Changelog

### Added
- The console can now sense the full planned control set — the joystick's
  four directions and a SELECT button, not just START and FIRE — with clean,
  bounce-free presses on every one of them.
- A wiring guide with a pin table tells you exactly which pin each button and
  joystick direction connects to and how to wire it, before you buy or solder
  a single part.

### Changed
- Existing START/FIRE behavior is unchanged: any code that already read those
  two signals keeps working exactly as before.

### Fixed
- (none — new capability, not a bug fix)

**Honest status note:** the joystick/SELECT wiring and debounce logic are
proven against a simulated source and build cleanly for the real hardware,
but on-device confirmation with a human pressing the real, physically wired
controls has **not** happened yet — no buttons, joystick or SELECT switch are
wired to the board (plan.md T10, reported `blocked`, an intentional,
plan-anticipated stopping point, not a defect). START/FIRE's real-device
behavior was already confirmed by earlier releases and is unaffected.

## 3. Release Actions

*`direct` mode. Executed 2026-09-04, immediately after re-verifying `git
status` showed no drift from the file list below and re-running the full
pre-flight (§1) fresh.*

| Action | Result |
|---|---|
| Commit | **Executed.** `79ea407761e3f0fe6a7ef75d0a698ec1998d99d2` — "feat: add input driver — joystick, SELECT and debounced GPIO reads reach GameInput" (Conventional Commits, constitution §5; Co-Authored-By trailer included). 22 files changed (21 add/modify + 1 delete), matching the file list below exactly; verified via `git status`/`git show --stat` before and after. |
| Version bump & tag | **Executed.** Annotated tag `v0.3.0` (tag object `fe7326d6bec6878a8ab70a84416074c03b7385cd`) created with the prepared message, pointing at commit `79ea407761e3f0fe6a7ef75d0a698ec1998d99d2` — confirmed via `git rev-parse v0.3.0^{commit}`. |
| PR / merge | N/A — `direct` mode, no remote configured (`git remote -v` empty). |
| Deploy | N/A — no deploy pipeline; the device flash is the artifact, already exercised (clean `idf.py build`) pre- and post-commit in this pass and earlier in `/peer-review`/`/demo-day`. |
| Post-release smoke check | **Executed, green.** Re-ran the identical host+device pre-flight on the tagged commit itself (`git rev-parse HEAD` = `79ea407...`, working tree clean of anything but the three unrelated untracked assets): `make test`/`test-asan`/`test-gcc` **154/154 all three**, `make lint` clean (19 blocks), `idf.py fullclean && idf.py build` from `firmware/system/` green, `steamcore_system.bin` 0x2fef0 bytes, 81% partition free — every number reproduces exactly against the pre-commit run in §1, confirming what's tagged is what was verified. |

**Version justification.** `git tag -l` shows `v0.0.1`–`v0.0.4` (retroactive
catch-ups on historical commits, per `CLAUDE.md`), `v0.1.0`
(`game-state-management`) and `v0.2.0` (`display-driver`, commit `113f943e`,
released earlier today) as same-day full-loop releases tagged on their own
release commits. `input-driver` is the same shape — entire source
uncommitted, spec'd/planned/built/reviewed/QA'd today — so it continues that
forward sequence as **v0.3.0**, not a `v0.0.x` backfill. **Minor** bump
(v0.2.0→v0.3.0): the change is purely additive to `GameInput`'s public
surface — five new fields, declaration order keeps `start`/`fire` first so
every existing positional `GameInput{a, b}` literal keeps its original
meaning (NFR-8, audited: zero call sites take more than two positional args)
— plus new files (`input.h`, `gpio_input_source.{h,cpp}`, docs). Not a PATCH
(new functionality, not a bug fix); not a MAJOR/breaking change (`GameSession`
confirmed byte-identical to v0.1.0, no existing symbol removed or
resignatured).

**Exact file list committed (explicit `git add <path>` calls, never
`-A`/`.`; re-checked against a fresh `git status` immediately before staging,
per `CLAUDE.md`'s re-check rule — no drift found, matched the prepared list
exactly):**
- Modified: `docs/device-build.md`, `docs/host-tests.md`,
  `firmware/steamcore/include/steamcore/board_config.h`,
  `firmware/steamcore/include/steamcore/game_loop.h`,
  `firmware/steamcore/test/game_loop_test.cpp`,
  `firmware/system/main/CMakeLists.txt`, `firmware/system/main/app_main.cpp`,
  `tools/check_constraints.sh`
- Deleted: `firmware/system/main/harness_consumer.h`
- New: `.spark/input-driver/spec.md`, `.spark/input-driver/plan.md`,
  `.spark/input-driver/review.md`, `.spark/input-driver/qa.md`,
  `.spark/input-driver/release.md` (this file), `docs/wiring-input.md`,
  `firmware/steamcore/include/steamcore/input.h`,
  `firmware/steamcore/port/esp32/gpio_input_source.h`,
  `firmware/steamcore/port/esp32/gpio_input_source.cpp`,
  `firmware/steamcore/test/fake_input_source.h`,
  `firmware/steamcore/test/input_session_test.cpp`,
  `firmware/steamcore/test/input_test.cpp`,
  `firmware/system/main/input_harness_game.h`

**Explicitly excluded:** `assets/Buttons.png`, `assets/fonts/`,
`assets/sprites/` — untracked, pre-existing, unrelated to this feature
(`review.md` §1); confirmed still untracked after the commit — did not ride
along.

**Rollback path** (local-only, nothing pushed, nothing to unwind on a remote):
- Commit made but wrong: `git reset --soft HEAD~1` — restores every file to
  the working tree, nothing lost. Run `git tag -d v0.3.0` first if the tag
  was also created.
- Tag wrong but commit fine: `git tag -d v0.3.0` — local tag only, commit
  untouched, re-tag once corrected.
- Both wrong: `git tag -d v0.3.0` then `git reset --soft HEAD~1`.
- `git remote -v` is empty — no force-push or remote cleanup is ever needed
  for this release.

## 4. Learnings (Keep!)

- **What went well:** the review round-2 discipline (probing F1/F9 by mutation
  rather than trusting `fixed` labels) carried straight through to this
  release with zero surprises at pre-flight — every number `review.md`/`qa.md`
  cited (154/154, clean lint, `game_state` byte-identical) reproduced exactly
  on a from-scratch `idf.py fullclean && idf.py build` and a fresh host run,
  both before the commit and again on the tagged commit itself.
- **What we'd do differently:** the F2 finding (harness tick rate silently
  widening the debounce window) shows the value of stating a design assumption
  in samples, not milliseconds, from the start — the fix was one comment and
  one log line because the underlying math never needed to change.
- **Patterns worth reusing:** splitting a hardware-gated Should into a
  buildable half (T9, host+device-compiler verifiable) and a blocked half
  (T10, needs real hardware) rather than letting the whole story slip is
  exactly `display-driver`'s US-5 precedent applied a second time — worth
  naming explicitly in `CLAUDE.md` as this project's standard move for any
  future Should gated on physical parts not yet acquired.

---

## ✅ KEEP GATE

*All boxes checked → the loop is closed. The feature is done-done.*

- [x] All pre-flight checks passed at release time — 154/154 x3, lint clean,
      `idf.py fullclean && idf.py build` green from clean, `game_state` diff
      empty, `git status` re-checked immediately before staging, and every
      number reproduced identically on the post-release smoke check of the
      tagged commit itself
- [x] Changelog written in user-facing language
- [x] Release actions executed and verified — commit
      `79ea407761e3f0fe6a7ef75d0a698ec1998d99d2`, annotated tag `v0.3.0`
      (`fe7326d6bec6878a8ab70a84416074c03b7385cd`) pointing at it, and the
      post-release smoke check all executed and green
- [x] Learnings recorded
- [x] Line budget respected: Ist 148 / Soll ~100 (excluding HTML comments) —
      48 over; reason: the explicit never-`-A` 22-path file list, the version
      justification paragraph and the dual gate-recap in §0 together account
      for it, not prose
- [x] Status set to `released`, or `handed-off` in declared `pr` mode —
      `direct` mode, all release actions executed and verified: `released`
