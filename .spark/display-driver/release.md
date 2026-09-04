# Release: display-driver

| | |
|---|---|
| **Phase** | Keep |
| **Owner** | Release Manager (`/go-live`) |
| **Input** | `review.md` (`passed`, round 2), `qa.md` (`passed`, round 1) |
| **Status** | `released` |
| **Version** | v0.2.0 |
| **Date** | 2026-09-04 |

**Handoff**
- **Status:** `released` — both gates green, fresh pre-flight green on host and
  device toolchains before the commit, release committed and tagged, and the
  identical pre-flight re-run clean on the tagged commit itself.
- **Summary:** the engine's `Framebuffer`/`DirtyTracker` output now reaches the real
  ILI9488 panel over SPI, tick-driven by `GameLoop`, at 40 MHz, dirty-tile-only, with
  graceful retry on transient failures — the cabinet's screen, finally driven by the
  engine and not a disconnected hardware spike.
- **Open:** `1 item for the orchestrator` — the F12/F13 constitution/README
  staleness flag (§5). Does not block the gate; needs a human call
  (`/charter` for F12, a scheduled README pass for F13).
- **Binding ruling:** §3 Release Actions and the KEEP GATE below carry the final ruling.
- **On conflict:** the numbered body wins for everything except `Status`/`Version`;
  log the mismatch at the next `/go-live` and proceed.

## 0. Gates & Delivery Mode

- `review.md`: `passed`, round 2. 0 open Blocker/Major — F1, F2, F5, F8, F9
  confirmed `fixed r2` on real hardware / an empirical C++17 probe, not from fix
  descriptions; F3/F4 fixed round 1. F10–F15 open, Minor/Nit, non-blocking.
- `qa.md`: `passed`, round 1. 16/16 Must ACs verified via the constitution's
  declared substitute method (§8: no browser surface; framebuffer/serial substitute)
  as a **standing project fact**, not a per-feature waiver. 0 bugs. The three
  human-eye checks (AC-3.1/3.2/5.1) were closed by the orchestrator relaying the
  user's live confirmation on a further run of the same flashed build.
- Constitution §7: **`direct`** mode, explicitly declared (approver n/a, branch
  `main`, ticket `none`, terminal status `released`). Confirmed independently:
  `git remote -v` returns nothing. Terminal status is `released`, not `handed-off`.

## 1. Pre-Flight Checks

*Run twice this pass: once before the commit (working tree), once again after,
on the tagged release commit itself — not copied from prior docs either time.*

- [x] `review.md` status `passed`
- [x] `qa.md` status `passed`
- [x] Full suite green, both halves, **before and after the commit** — **host:**
      `make clean && make test-all`: 124/124 x3 builds (clang, ASan/UBSan, g++),
      `test-negative` OK, 3 benches OK, 15+2 Python OK, `test-png-external` OK,
      `make lint` OK (17 blocks, incl. 5 display-driver). **Device:** `idf.py
      fullclean && idf.py build` in `firmware/system/` (ESP-IDF exported first) —
      links clean, 78% flash free, both passes identical.
- [x] Build succeeds from clean — `make clean` and `idf.py fullclean` both run
      before both builds, before and after the commit.
- [x] No unaccounted uncommitted changes — every uncommitted path mapped to §3's
      file list except three untracked leftovers confirmed (`git log --all`) to
      predate this feature: `assets/Buttons.png`, `assets/fonts/`, `assets/sprites/` —
      excluded from the commit, same discipline as every prior release. Confirmed
      absent from `git status` after the commit too.

## 2. Changelog

### Added
- The cabinet's screen is finally driven by the engine, not a disconnected hardware
  spike: whatever `GameLoop` renders now shows up on the real physical display,
  tick after tick, live.
- Only the parts of the picture that actually changed are sent each tick, so
  updates are fast and the display never blocks redrawing what hasn't moved.
- The screen updates at the fastest, most responsive speed the hardware reliably
  supports, with no visible corruption or tearing.
- A brief hiccup sending data to the screen no longer freezes or crashes the
  console — the affected patch of screen just catches up the very next tick.

### Changed
- (none — new output path only)

### Fixed
- (none — new capability, not a bug fix)

## 3. Release Actions

*`direct` mode, executed with explicit user go-ahead ("Ja", relayed by the
orchestrator in direct response to this file's prepared §3). Local-only —
nothing pushed, no remote configured.*

| Action | Result |
|---|---|
| Commit | **Executed.** `113f943ea27ce9386ec27d232437b738557c8312` — "Add display driver: engine output now reaches the real ILI9488 panel", 24 files changed, on `main`. |
| Version bump & tag | **Executed.** `v0.2.0`, annotated tag object `9e61155546da7da6f507f496a010c56452620f9e`, pointing at release commit `113f943ea27ce9386ec27d232437b738557c8312` (no historical commit exists to point at — see justification). Verified via `git rev-parse v0.2.0^{commit}` == the commit hash above. |
| PR / merge | N/A — `direct` mode, no remote |
| Deploy | N/A — no deploy pipeline; the device flash *is* the artifact, already exercised live in `/increment`/`/peer-review`/`/demo-day` |
| Post-release smoke check | **Executed, green.** Host: `make clean && make test-all` on the tagged commit — 124/124 x3, `test-negative` OK, 3 benches OK, 15+2 Python OK, `test-png-external` OK, `make lint` OK (17 blocks). Device: `idf.py fullclean && idf.py build` from `firmware/system/` on the tagged commit — links clean, 78% flash free. |

**Correction applied during execution.** The plan drafted in this file's
`preparing` pass omitted `tools/check_constraints.sh` from the commit's file
list, even though `git status` showed it genuinely modified (T9 added 5 new
lint rules there: no `#include` from `port/` inside `include/`/`src/`, no GPIO
literal outside `board_config.h`, no resolution/tile-size literal in
`port/esp32`, no bare scale-factor literal in the display-driver pixel/tile
math, no full-frame SPI transaction outside `ili9488_display.cpp`, no dynamic
allocation in `port/esp32`). Left out, T9's entire lint-rule deliverable would
have silently shipped uncommitted while the code it protects went in. Corrected
before staging; `tools/check_constraints.sh` is included in the commit and
confirmed present in `git show --stat` for `113f943e`.

**Version justification.** `git tag -l`: `v0.0.1`–`v0.0.4` are retroactive
catch-ups tagged on historical source commits (per `CLAUDE.md`); `v0.1.0`
(`game-state-management`) is the first same-day full-loop release, tagged on its
own release commit since no historical commit exists. `display-driver` is the same
shape — entire source uncommitted, written/QA'd today — so it continues that
forward sequence rather than backfilling `v0.0.x`. **Minor** bump (v0.1.0→v0.2.0):
purely additive surface (`toPanelPixel`, `tileWindow`, `expandTile`,
`Ili9488Display::init`/`pushDirty`, 6 `config.h` constants); `game_loop.h` and every
other existing public API confirmed unchanged (`git diff --exit-code`, clean, per
`review.md`/`qa.md`).

**Exact commit file list (24 paths, explicit `git add <path>` calls, never
`-A`/`.`):** `docs/host-tests.md`, `docs/device-build.md`,
`firmware/steamcore/include/steamcore/config.h`, `.../panel_format.h`,
`.../tile_pusher.h`, `firmware/steamcore/src/panel_format.cpp`,
`firmware/steamcore/port/esp32/ili9488_display.{h,cpp}`,
`firmware/steamcore/test/dump_format_test.cpp`, `.../fixture_pattern.h`,
`.../panel_format_test.cpp`, `.../tile_pusher_test.cpp`,
`firmware/system/CMakeLists.txt`, `firmware/system/main/CMakeLists.txt`,
`firmware/system/main/app_main.cpp`, `.../harness_consumer.h`, the two
**deletions** `firmware/system/main/ili9488_display.{h,cpp}`,
`tools/check_constraints.sh` (added by correction — see above), plus
`.spark/display-driver/{spec,plan,review,qa,release}.md`.
**Explicitly excluded:** `assets/Buttons.png`, `assets/fonts/`, `assets/sprites/`
(confirmed pre-existing and unrelated; still untracked after the commit).

**Rollback path** (local-only, nothing pushed):
- Commit wrong: `git reset --soft HEAD~1` — restores files to working tree,
  nothing lost. Run `git tag -d v0.2.0` first if the tag was also created.
- Tag wrong but commit fine: `git tag -d v0.2.0` — local tag only, commit
  untouched, re-tag once corrected.
- Both wrong: `git tag -d v0.2.0` then `git reset --soft HEAD~1`.
- Nothing has been pushed to any remote (`git remote -v` empty), so no
  force-push or remote cleanup is ever needed for this release.

## 4. Learnings (Keep!)

- **What went well:** the `port/esp32/` directory boundary plus the compile-time
  `TilePusher<Transmitter>` seam pulled the whole retry contract (US-4) onto the
  host gate — fault-injection-proven on a Mac, not just hardware. Round-2 review
  re-deriving F1/F5 from a live flash and a dialect probe, not fix descriptions, is
  the discipline that makes `passed` trustworthy at release time.
- **What we'd do differently:** F15 (the on-device anchor log restates constants,
  never reads the actual framebuffer) shipped as an accepted Minor — revisit before
  a second visual feature reuses that log pattern, so "proves the mapping" and
  "proves what was drawn" stay distinct. Also: the prepared file list in a
  `preparing`-status release.md should be cross-checked against a fresh `git
  status` at execution time, not trusted as-is — `tools/check_constraints.sh`
  was genuinely modified (T9's lint rules) and was missing from the original
  plan; caught only because the orchestrator re-diffed before staging.
- **Patterns worth reusing:** pairing `idf.py fullclean && idf.py build` with `make
  clean && make test-all` at release time is worth standardizing as this project's
  two-halves pre-flight for every future `port/`-touching feature. Candidate for
  `CLAUDE.md`. Re-running that same two-halves pre-flight a second time, on the
  tagged commit, after the commit — not just before it — is worth keeping as
  standard practice too.

## 5. Flagged for the orchestrator: F12/F13 staleness

`review.md` F12/F13 both name language this release makes false, surfaced here
rather than silently absorbed into the commit:
- **F12 (constitution §3/§4):** both still call the production driver "still
  unbuilt, planned future work" / "remains future planned work" — false as of this
  release. Only `/charter` amends the constitution; not done here. **Recommend:**
  run `/charter` to replace both passages with a factual update (driver shipped,
  `display-driver` v0.2.0, dirty-tile SPI DMA proven at 40 MHz on real hardware) and
  log it in the Amendments table.
- **F13 (`README.md:608, 625-629`):** Phase-2 checklist still says the display
  driver "ist noch offen" — not a regression this feature caused (Game
  Loop/Text Rendering are similarly stale per `review.md`). **Recommend:** a
  one-line check-off alongside a future pass that reconciles the whole checklist,
  rather than a one-off edit that leaves the others stale.

Neither edit was made by this agent — named precisely so the orchestrator can
decide whether to run `/charter` (F12) and schedule the README pass (F13).

---

## ✅ KEEP GATE

*All boxes checked → the loop is closed. The feature is done-done.*

- [x] All pre-flight checks passed at release time — 3/3, host + device, from
      clean, run twice (pre-commit and post-tag), both clean
- [x] Changelog written in user-facing language
- [x] Release actions executed and verified — commit `113f943e`, tag `v0.2.0`
      (annotated, on that commit), post-release smoke check green on both halves
- [x] Learnings recorded
- [x] Line budget respected: Ist 176 / Soll ~100 (excluding HTML comments) — 76
      over; reason: the explicit never-`-A` 24-path file list (grown by one for
      the correction), dual host+device pre-flight run twice, the correction
      note, and the required F12/F13 flag section together account for it, not
      prose
- [x] Status set to `released`, or `handed-off` in declared `pr` mode —
      `direct` mode, status is `released`
