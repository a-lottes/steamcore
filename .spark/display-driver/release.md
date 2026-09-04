# Release: display-driver

| | |
|---|---|
| **Phase** | Keep |
| **Owner** | Release Manager (`/go-live`) |
| **Input** | `review.md` (`passed`, round 2), `qa.md` (`passed`, round 1) |
| **Status** | `preparing` |
| **Version** | v0.2.0 (proposed) |
| **Date** | 2026-09-04 |

**Handoff**
- **Status:** `preparing` — both gates green, fresh pre-flight green on host and
  device toolchains, release fully staged but **no commit/tag created**; no
  outward-facing or irreversible action taken.
- **Summary:** the engine's `Framebuffer`/`DirtyTracker` output now reaches the real
  ILI9488 panel over SPI, tick-driven by `GameLoop`, at 40 MHz, dirty-tile-only, with
  graceful retry on transient failures — the cabinet's screen, finally driven by the
  engine and not a disconnected hardware spike.
- **Open:** `2 items for the orchestrator` — (1) explicit go-ahead to run the
  prepared commit/tag (§3); (2) the F12/F13 constitution/README staleness flag (§5).
  Neither blocks the gate; both need a human call before publishing.
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

*Fresh this pass, on current HEAD + uncommitted tree — not copied from prior docs.*

- [x] `review.md` status `passed`
- [x] `qa.md` status `passed`
- [x] Full suite green, both halves — **host:** `make clean && make test-all`:
      124/124 x3 builds (clang, ASan/UBSan, g++), `test-negative` OK, 3 benches OK,
      15+2 Python OK, `test-png-external` OK, `make lint` OK (17 blocks, incl. 5
      display-driver). **Device:** `idf.py fullclean && idf.py build` in
      `firmware/system/` (ESP-IDF exported first) — links clean, 78% flash free.
- [x] Build succeeds from clean — `make clean` and `idf.py fullclean` both run
      before their builds this pass.
- [x] No unaccounted uncommitted changes — every uncommitted path maps to §3's file
      list except three untracked leftovers confirmed (`git log --all`) to predate
      this feature: `assets/Buttons.png`, `assets/fonts/`, `assets/sprites/` —
      excluded from the prepared commit, same discipline as every prior release.

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

*`direct` mode, **prepare-only**: commit + local tag are drafted, not executed — no
go-ahead relayed yet. Nothing below has left the working tree.*

| Action | Result |
|---|---|
| Version bump & tag | **Prepared, not executed.** `v0.2.0`, annotated on the new release commit (no historical commit exists to point at — see justification). Staged: `git tag -a v0.2.0 -m "v0.2.0: display driver (Framebuffer/DirtyTracker reaches the real ILI9488 panel over SPI, GameLoop-driven, 40MHz)"` |
| PR / merge | N/A — `direct` mode, no remote |
| Deploy | N/A — no deploy pipeline; the device flash *is* the artifact, already exercised live in `/increment`/`/peer-review`/`/demo-day` |
| Post-release smoke check | **Not yet run** — pending the commit/tag; will re-run host + `idf.py build` on the tagged commit once authorized |

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

**Exact commit file list (explicit paths, never `-A`/`.`):** `docs/host-tests.md`,
`docs/device-build.md`, `firmware/steamcore/include/steamcore/config.h`,
`.../panel_format.h`, `.../tile_pusher.h`, `firmware/steamcore/src/panel_format.cpp`,
`firmware/steamcore/port/esp32/ili9488_display.{h,cpp}`,
`firmware/steamcore/test/dump_format_test.cpp`, `.../fixture_pattern.h`,
`.../panel_format_test.cpp`, `.../tile_pusher_test.cpp`,
`firmware/system/CMakeLists.txt`, `firmware/system/main/CMakeLists.txt`,
`firmware/system/main/app_main.cpp`, `.../harness_consumer.h`, and the two
**deletions** `firmware/system/main/ili9488_display.{h,cpp}` (staged via `git add`
on the removed path), plus `.spark/display-driver/{spec,plan,review,qa,release}.md`.
Then `git commit -m "Add display driver: ..."` and the tag command above.
**Explicitly excluded:** `assets/Buttons.png`, `assets/fonts/`, `assets/sprites/`.

**Rollback path** (local-only, nothing pushed):
- Pre-authorization: nothing to roll back, no command has run.
- Commit wrong: `git reset --soft HEAD~1` — restores files to working tree, nothing lost.
- Tag wrong: `git tag -d v0.2.0` — local tag only, commit untouched.
- Both wrong: delete the tag first, then soft-reset.

## 4. Learnings (Keep!)

- **What went well:** the `port/esp32/` directory boundary plus the compile-time
  `TilePusher<Transmitter>` seam pulled the whole retry contract (US-4) onto the
  host gate — fault-injection-proven on a Mac, not just hardware. Round-2 review
  re-deriving F1/F5 from a live flash and a dialect probe, not fix descriptions, is
  the discipline that makes `passed` trustworthy at release time.
- **What we'd do differently:** F15 (the on-device anchor log restates constants,
  never reads the actual framebuffer) shipped as an accepted Minor — revisit before
  a second visual feature reuses that log pattern, so "proves the mapping" and
  "proves what was drawn" stay distinct.
- **Patterns worth reusing:** pairing `idf.py fullclean && idf.py build` with `make
  clean && make test-all` at release time is worth standardizing as this project's
  two-halves pre-flight for every future `port/`-touching feature. Candidate for
  `CLAUDE.md`.

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

- [x] All pre-flight checks passed at release time — 3/3, host + device, from clean
- [x] Changelog written in user-facing language
- [ ] Release actions executed and verified — **prepared, not executed**: commit and
      tag staged (§3), awaiting explicit go-ahead
- [x] Learnings recorded
- [x] Line budget respected: Ist 170 / Soll ~100 (excluding HTML comments) — 38
      over; reason: the explicit never-`-A` file list, dual host+device pre-flight,
      and the required F12/F13 flag section together account for it, not prose
- [ ] Status set to `released`, or `handed-off` in declared `pr` mode — **neither**:
      mode is `direct`, status is `preparing`, awaiting go for §3's commit/tag
