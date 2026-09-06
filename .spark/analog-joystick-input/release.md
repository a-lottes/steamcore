# Release: analog-joystick-input

| | |
|---|---|
| **Phase** | Keep |
| **Owner** | Release Manager (`/go-live`) |
| **Input** | `review.md` (`passed`, round 3), `qa.md` (`passed`, round 1) |
| **Status** | `released` |
| **Version** | v0.6.0 |
| **Date** | 2026-09-06 |

**Handoff**
- **Status:** `released` — both gates green, pre-flight re-run fresh at
  prepare time and reproduced again post-tag (§1b). Commit
  `c5c01b0732f7e5d3949a40dae5b8a18d954c3f3e` tagged `v0.6.0`. No push/PR/
  deploy exist for this project (direct mode, no remote) — local commit +
  tag is the entire publish action, both executed on the caller's explicit
  go.
- **Summary:** a second `Source`, `AnalogJoystickSource`, reads the real
  ADC stick (deadzone-thresholded, host-proven) and three buttons into
  `GameInput`, alongside the untouched `GpioInputSource`; US-3's on-device
  confirmation (T8) stays honestly `blocked` — nothing is wired yet.
- **Open:** none. The two items open at prepare time (commit, tag) are
  done; the post-release smoke check reproduced pre-flight exactly (§1b).
  US-3/T8 remains its own, plan-anticipated `blocked` state — not a release
  blocker (Should, not a Must).
- **Binding ruling:** §3 Release Actions and the KEEP GATE below carry the
  final ruling.
- **On conflict:** the numbered body below wins for everything except
  `Status`/`Version`; log the mismatch at the next `/go-live` and proceed.

## 0. Gates & Delivery Mode

- `review.md`: `passed`, round 3 — closing round, 0 open findings at any
  severity (F1–F11 all `fixed`, verified not merely read). Every Must AC
  (AC-1.1–1.6, AC-2.1–2.7) traced to code; NFR-8 re-derived (`gpio_input_source.*`,
  `input.h`, `game_loop.h` byte-identical to `v0.3.0`).
- `qa.md`: passed, round 1, 0 Blockers/Majors/Minors. QA ran by the method
  `.spark/constitution.md` §8 declares as a standing project fact
  (`Browser-observable surface: no`; substitute: framebuffer dump + serial
  transcript for rendering output, plus host-compiled unit tests for
  hardware-independent logic). This feature has no rendering/framebuffer
  surface at all, so only the unit-test/build half of that substitute
  applies — `qa.md` §1 states this is by design, not a gap — and every
  Must AC/NFR is still recorded individually under its own ID; US-3's
  AC-3.1–3.4 are recorded `not capturable` (T8 hardware-gated Should), never
  passed, per this project's own `CLAUDE.md` convention.
- Constitution §7: **`direct`** mode, explicitly declared (approver n/a,
  branch `main`, ticket `none`, terminal status `released`). No `pr`-mode
  language applies anywhere in this report.

## 1. Pre-Flight Checks

*Run fresh, right now, on the uncommitted working tree — not copied from
`review.md`/`qa.md`.*

- [x] `review.md` status is `passed`
- [x] `qa.md` status is `passed`
- [x] Full host test suite green — `make test-all`: `make test`/`test-asan`/
      `test-gcc` **206/206 all three**, exit 0 (`test-negative`'s own
      `0 passed, 1 failed` and `0 passed, 0 failed` lines are its deliberate
      self-checks, not regressions). `make lint`: OK, all blocks including
      this feature's four (ADC-literal ban, VRX/VRY-pin ban, feature-scoped
      digit block, clock/RNG/alloc ban).
- [x] Build succeeds from a clean state — `source ~/esp/esp-idf/export.sh`,
      `idf.py fullclean && idf.py build` from `firmware/system/` (ESP-IDF
      v5.4.4): green, `[1060/1060]`, zero warnings, `steamcore_system.bin`
      0x32530 bytes (80% partition free) — both
      `analog_joystick_source.cpp.obj` and `gpio_input_source.cpp.obj`
      confirmed present under `build/esp-idf/main/CMakeFiles/__idf_main.dir/`.
- [x] No unaccounted uncommitted changes — `git status --short` re-checked
      immediately before writing this report (not trusted from the task
      description): every modified/untracked path maps to §3's file list
      below exactly, except four pre-existing, unrelated items —
      `.spark/start-screen/release.md`, `CLAUDE.md` (both modified in an
      earlier, unrelated session) and `assets/Buttons.png`/`assets/fonts/`/
      `assets/sprites/` (untracked, from a different in-progress effort,
      not referenced by any file in this feature) — all four stay excluded
      from the commit.
- [x] `git diff --exit-code v0.3.0 -- firmware/steamcore/port/esp32/gpio_input_source.{h,cpp} firmware/steamcore/include/steamcore/{input,game_loop}.h`:
      empty — re-derived myself, not copied from `review.md`'s NFR-8 row.

## 1b. Post-Release Smoke Check (re-run on the tagged commit)

*Executed on the explicit caller go, on commit `c5c01b073` / tag `v0.6.0`,
right after tagging — not copied from §1.*

- [x] `git rev-parse HEAD` and `git rev-list -n1 v0.6.0` both resolve to
      `c5c01b0732f7e5d3949a40dae5b8a18d954c3f3e` — the tag points at the
      commit that was actually built and tested.
- [x] `make test-all`: exit 0. `make test` (clang): **206 passed,
      0 failed** — matches §1. `make test-asan`: **206 passed, 0 failed** —
      matches §1. `make test-gcc`: **206 passed, 0 failed** — matches §1.
      `make test-negative`: both deliberate self-checks reported exactly as
      §1 (`0 passed, 1 failed`, `0 passed, 0 failed`) — matches.
- [x] `make lint`: **OK** — all blocks clean, including this feature's four
      (ADC-literal ban, VRX/VRY-pin ban, feature-scoped digit block,
      clock/RNG/alloc ban) — matches §1.
- [x] Device build: `source ~/esp/esp-idf/export.sh`, `idf.py fullclean &&
      idf.py build` from `firmware/system/` (ESP-IDF v5.4.4, genuine clean
      rebuild, not incremental): green, **`[1060/1060]`**, zero warnings —
      identical step count to §1. `steamcore_system.bin` **0x32530 bytes
      (80% partition free)** — byte-identical size to §1. Both
      `analog_joystick_source.cpp.obj` and `gpio_input_source.cpp.obj`
      re-confirmed present under
      `build/esp-idf/main/CMakeFiles/__idf_main.dir/`.

**Conclusion:** the tagged commit reproduces the same green result recorded
at pre-flight — identical pass counts on all three host toolchains, identical
lint outcome, identical device-build step count and binary size. Nothing
regressed between prepare and publish. This project ships no rendering
surface for this feature and has no deploy pipeline, so "alive" here means
the flashable artifact reproducibly builds — there is no running service to
poll; on-device confirmation of the physical stick/buttons stays out of
scope, honestly `blocked` (T8, unwired hardware, plan-anticipated).

## 2. Changelog

### Added
- The console can now read a real analog joystick — push it to any of the
  four directions, or a diagonal, and it drives the same directional input
  a game already understands, with a dead zone around center so it reads
  as neutral until you actually push it.
- Two more physical buttons now work as a third control option: one for
  start, one for fire, plus the joystick's own click for select — wired
  the same simple, no-tools way as the buttons already supported.
- A wiring guide (`docs/wiring-analog-joystick.md`) shows exactly which
  pin each stick axis and button connects to, and states plainly it must
  be powered from 3.3V, never 5V, before anything is energized.

### Changed
- Nothing about the console's existing digital-switch controls changes —
  they keep working exactly as before, side by side with the new stick.

### Fixed
- (none — new capability, not a bug fix)

**Honest status note:** the stick and button logic is proven correct
against synthetic values and builds cleanly for the real hardware, but
on-device confirmation with a human moving the physical stick and pressing
the physical buttons has **not** happened yet — nothing is wired to the
board yet (plan.md T8, reported `blocked`, an intentional, plan-anticipated
stopping point, not a defect).

## 3. Release Actions

*Executed on the caller's explicit go. Direct mode, no remote — the two
local commands below are the entire publish action for this project; no
push, no PR, no deploy exist for it.*

| Action | Result |
|---|---|
| Commit | **Done.** Commit `c5c01b0732f7e5d3949a40dae5b8a18d954c3f3e`, message `feat: add analog joystick input -- real stick and buttons reach GameInput alongside GpioInputSource` + `Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>` trailer (matches this repo's convention). Staged via explicit `git add <path>` per file (§3a's exact 17-path list), never `-A`/`.`. |
| Version bump & tag | **Done.** `v0.6.0` — `git tag -a v0.6.0 -m "analog-joystick-input: second Source reads real ADC stick + 3 buttons into GameInput, GpioInputSource untouched; US-3 on-device confirmation pending wiring (T8 blocked)" c5c01b0732f7e5d3949a40dae5b8a18d954c3f3e` (tag object `d4001996997f02c69b6d50c7e29499302e543647`). |
| PR / merge | N/A — `direct` mode, no remote configured (`git remote -v` empty). |
| Deploy | N/A — no deploy pipeline; the device flash is the artifact, already exercised (fresh `idf.py fullclean && idf.py build`, §1 and re-confirmed §1b) pre- and post-tag. |
| Post-release smoke check | **Done** — see §1b. Same green result as pre-flight, reproduced on the actual tagged commit: 206/206 x3 host toolchains, lint OK, device build `[1060/1060]` with byte-identical binary size. |

**Version justification.** `git tag -l` (sorted): `v0.0.1`–`v0.0.4`
(retroactive catch-ups, per `CLAUDE.md`), `v0.1.0`–`v0.5.0` as same-day
full-loop releases tagged on their own release commits. This feature is
the same shape — entire source uncommitted, spec'd/planned/built/
reviewed/QA'd today (2026-09-06) — so it continues that forward sequence
as **v0.6.0**, not a backfill. **Minor** bump (v0.5.0→v0.6.0): purely
additive — one new header (`analog_axis.h`), one new port type
(`AnalogJoystickSource`), five new named pin constants, zero change to any
shipped public symbol (`GameInput`, `InputReader`, `Debouncer`,
`GpioInputSource`, `input.h`, `game_loop.h` all confirmed byte-identical to
`v0.3.0`, §1). Not PATCH (new functionality, not a bug fix); not MAJOR
(nothing removed or resignatured).

**Exact file list staged (never `-A`/`.`), matched against a fresh
`git status --short` re-checked immediately before staging — no drift found,
matches the prepared §3a list exactly:**

```
firmware/steamcore/include/steamcore/analog_axis.h
firmware/steamcore/test/analog_axis_test.cpp
firmware/steamcore/test/fake_analog_source.h
firmware/steamcore/port/esp32/analog_joystick_source.h
firmware/steamcore/port/esp32/analog_joystick_source.cpp
docs/wiring-analog-joystick.md
firmware/steamcore/include/steamcore/board_config.h
firmware/system/main/CMakeLists.txt
firmware/system/main/app_main.cpp
docs/host-tests.md
docs/device-build.md
tools/check_constraints.sh
.spark/analog-joystick-input/spec.md
.spark/analog-joystick-input/plan.md
.spark/analog-joystick-input/review.md
.spark/analog-joystick-input/qa.md
.spark/analog-joystick-input/release.md
```

**Explicitly excluded (pre-existing, unrelated, confirmed still present
and untouched after re-checking at execution time):** `.spark/start-screen/
release.md`, `CLAUDE.md`, `assets/Buttons.png`, `assets/fonts/`,
`assets/sprites/`.

**Rollback path** (local-only — nothing pushed, nothing to unwind on a
remote; `git remote -v` is empty):
- Committed and tagged as `c5c01b0732f7e5d3949a40dae5b8a18d954c3f3e` /
  `v0.6.0`. If found wrong: `git tag -d v0.6.0` (delete the tag first), then
  `git reset --soft HEAD~1` — restores every file to the working tree,
  nothing lost, and re-exposes the untracked/modified state exactly as it
  was pre-commit.
- Tag wrong but commit fine: `git tag -d v0.6.0` only, then re-tag once
  corrected.
- Nothing is deployed anywhere (no remote, no pipeline), so no rollback
  step beyond the two above is ever needed for this release.

## 4. Learnings (Keep!)

- **What went well:** the pure/impure split (§1 Decision 2/4/5 of `plan.md`)
  again paid off exactly as `input-driver`/`display-driver` predicted — all
  Musts (US-1, US-2) shipped fully host-verified while T8 sat honestly
  `blocked` on unwired hardware, costing the release nothing. Round-by-round
  review (F4→F8→F9) progressively cut the per-tick ADC read count from 8 to
  4 to 2 without ever changing the public shape — mutation-verified each
  time, not just re-read.
- **What we'd do differently:** F8 shows a self-contradicting inline comment
  (claiming "2 reads" the same sentence it says "halves" from 8) surviving
  one full review round before being caught — a comment stating a concrete
  count is worth a one-line grep-style sanity check against the code it
  describes before calling a fix `done`.
- **Patterns worth reusing:** this is the third feature in a row
  (`display-driver` US-5, `input-driver` US-4, now `analog-joystick-input`
  US-3) to split a hardware-gated Should into a buildable half and a
  blocked half rather than deferring the whole story — `CLAUDE.md` already
  documents this pattern from the first two; nothing new to add, but a
  third clean application is worth noting as confirmation it generalizes.
  Also new this pass: re-running a device build post-tag as a genuine
  `fullclean` rather than trusting an incremental ninja cache caught nothing
  wrong here, but is the only way the `[1060/1060]`-step-count match in §1b
  is actually meaningful — an incremental rebuild would have reused
  pre-flight's cached objects and silently proven nothing new.

---

## ✅ KEEP GATE

*All boxes checked → the loop is closed. The feature is done-done.*

- [x] All pre-flight checks passed at release time — full suite green fresh
      at prepare time (206/206 x3, lint clean, clean-state device build
      `[1060/1060]`), and reproduced again post-tag on the actual released
      commit (§1b): same pass counts, same lint outcome, same device-build
      step count and binary size, no uncommitted changes belonging to this
      feature.
- [x] Changelog written in user-facing language
- [x] Release actions executed and verified — commit `c5c01b073`, tag
      `v0.6.0`, and the post-release smoke check are all done (§3, §1b); no
      push/PR/deploy exist for this project (direct mode, no remote).
- [x] Learnings recorded
- [x] Line budget respected: Ist 197 / Soll ~100 (excluding HTML comments)
      — 97 over; reason: §1b (post-release smoke-check detail, required by
      this pass), the explicit never-`-A` 17-path file list, the exact
      commit/tag identifiers, and the version-justification paragraph
      together account for the overage, not prose padding
- [x] Status set to `released`
