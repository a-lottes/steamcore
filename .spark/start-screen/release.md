# Release: start-screen

| | |
|---|---|
| **Phase** | Keep |
| **Owner** | Release Manager (`/go-live`) |
| **Input** | `review.md` (`passed`, round 2), `qa.md` (`passed`, round 1) |
| **Status** | `released` |
| **Version** | v0.4.0 |
| **Date** | 2026-09-05 |

**Handoff**
- **Status:** `released` — both gates green, fresh pre-flight green on host and
  device toolchains (both before and after the commit), version bumped,
  changelog and rollback path written, release commit `a8e590f` created,
  annotated tag `v0.4.0` created on that commit, post-release smoke check
  re-run against the tagged commit and reproduced every §1 number exactly.
- **Summary:** The console now shows its own title screen — "STEAMCORE" over
  "PRESS START" — while idle, gone the instant play begins; confirmed on the
  real physical panel (T12).
- **Open:** none. `direct` mode, no remote — nothing pushed anywhere, nothing
  outstanding.
- **Binding ruling:** §3 Release Actions and the KEEP GATE below carry the
  final ruling.
- **On conflict:** the numbered body below wins for everything except
  `Status`/`Version`; log the mismatch at the next `/go-live` and proceed.

## 0. Gates & Delivery Mode

- `review.md`: `passed`, round 2. `REVIEW GATE` checklist fully checked: no
  open Blocker/Major (F1/F2 both `fixed r2`, each re-verified by a
  mutation/probe the reviewer ran), every Must AC traced, all deviations
  documented, suite green, status `passed`.
- `qa.md`: `passed`, round 1. `QA GATE` checklist fully checked: every Must-
  story AC (US-1/US-2) and every applicable NFR verified and passed, all 9
  Must ACs + 2 Should ACs (US-3) genuinely captured (not parked — T12 ran on
  real hardware), no open Blocker/Major, status `passed`.
- Constitution §7: **`direct`** mode, explicitly declared (solo project,
  local repo, no remote, approver n/a, target branch `main`, ticket `none`,
  terminal status `released`). No `pr`-mode language applies anywhere in this
  report.
- Constitution §8 QA Method: standing project fact, not a per-feature
  override — `Browser-observable surface: no`; substitute method is
  framebuffer dump over USB-CDC decoded by the Python viewer plus the serial
  transcript, with hardware-independent logic covered by host-compiled unit
  tests. `qa.md` §1 already records this and verifies every `AC-`/`NFR-` ID
  under it.

## 1. Pre-Flight Checks

*Run fresh, right now, on the uncommitted working tree — not copied from
`review.md`/`qa.md`.*

- [x] `review.md` status is `passed`
- [x] `qa.md` status is `passed`
- [x] Full host test suite green — `make test` / `make test-asan` /
      `make test-gcc`: **171/171 all three.** `make lint`: clean.
      `make test-python`: **31/31.** `make bench`: title screen render
      3.44 µs/call (budget ≪ 16.67 ms). `make view`: `build/title_screen.png`
      viewed directly — STEAMCORE centred above, PRESS START centred below,
      all else BLACK.
- [x] Build succeeds from a clean state — from `firmware/system/`
      (`~/esp/esp-idf/export.sh` sourced), the seven device-touched sources
      (`app_main.cpp`, `title_screen_harness_game.h`, `CMakeLists.txt`,
      `font.cpp`, `title_screen.cpp`, `text.cpp`, `dump_format.cpp`) were
      `touch`ed to force a non-cached recompile, then `idf.py build`: green,
      all touched `.cpp` files rebuilt (`text.cpp.obj`, `dump_format.cpp.obj`,
      `title_screen.cpp.obj`, `font.cpp.obj`, `app_main.cpp.obj` all listed
      building), `steamcore_system.bin` 0x3c540 bytes, 76% partition free.
- [x] No unaccounted uncommitted changes — `git status` re-checked immediately
      before writing this report: every modified/untracked path maps to §3's
      file list below except the three pre-existing, unrelated untracked
      assets (`assets/Buttons.png`, `assets/fonts/`, `assets/sprites/`), which
      `review.md` §6 and `qa.md` both name as belonging to no task here and
      which stay excluded from the commit. No drift from the prepared list.
- [x] `git diff --exit-code HEAD -- firmware/steamcore/include/steamcore/game_state.h
      firmware/steamcore/src/game_state.cpp firmware/steamcore/include/steamcore/game_loop.h
      firmware/steamcore/include/steamcore/input.h`: empty — all four
      released types confirmed byte-identical.

## 2. Changelog

### Added
- The console now shows its own title screen when powered on and idle —
  "STEAMCORE" above a "PRESS START" prompt, centred on an otherwise black
  screen.

### Changed
- (none — purely additive; every previously shipped screen and behavior is
  unaffected)

### Fixed
- (none — new capability, not a bug fix; two incidental device-build
  compatibility fixes to already-shipped font code are recorded in `plan.md`
  §Deviations and did not change any glyph's pixels, so they carry no
  user-facing changelog entry)

**Confirmed on real hardware.** The title screen was flashed to the physical
panel; both the READY frame (title + prompt) and the PLAYING frame (both
gone) were dumped, decoded, and matched byte-exact to the host-rendered
image, and a human independently confirmed the physical panel shows the same
thing (plan.md T12, 2026-09-04).

## 3. Release Actions

*`direct` mode. Executed with the user's explicit go ("ja, veröffentlichen"),
relayed by the caller, for exactly the plan below — nothing more.*

| Action | Result |
|---|---|
| Version bump & tag | **Done.** Annotated tag `v0.4.0` created on release commit `a8e590f`: `git tag -a v0.4.0 -m "start-screen: STEAMCORE title screen over PRESS START, real-hardware confirmed"`. `git rev-parse v0.4.0` → `a8e590f`. |
| Commit | **Done.** Commit `a8e590f` on `main`: `feat: add start screen — STEAMCORE title over PRESS START, gone the instant play begins` (+ Co-Authored-By trailer). 24 files changed, exactly §3's prepared file list — re-checked against a fresh `git status` immediately before staging, no drift found, all 24 paths staged via explicit `git add <path>`, none via `-A`/`.`. |
| PR / merge | N/A — `direct` mode, no remote configured. |
| Deploy | N/A — no deploy pipeline; the device flash is the release artifact, already exercised (clean `idf.py build` pre-commit and again post-commit against `a8e590f`, both green) and previously flashed live during `/increment` (plan.md T12). |
| Post-release smoke check | **Done.** Re-ran the identical host+device pre-flight against the tagged commit `a8e590f` itself: `make test`/`make test-asan`/`make test-gcc` all **171/171**, `make lint` clean, `make test-python` **31/31**, `idf.py build` green with all five device-touched `.cpp` files rebuilt (forced by `touch`) and an identical `steamcore_system.bin` size of `0x3c540` bytes, 76% partition free — every §1 number reproduced exactly on the committed, tagged tree. |

**Version justification.** Prior tags: `v0.0.1`–`v0.0.4` (retroactive
catch-ups on historical commits, per `CLAUDE.md`), `v0.1.0`
(`game-state-management`), `v0.2.0` (`display-driver`), `v0.3.0`
(`input-driver`, commit `79ea407`) — all same-day full-loop releases tagged
on their own release commits. `start-screen` continues that forward
sequence as **v0.4.0**, not a `v0.0.x` backfill. **Minor** bump
(v0.3.0→v0.4.0): the change is purely additive — one new public header
(`title_screen.h`) exposing exactly four new symbols (`drawTitleScreen`,
`TitleBounds`, `kTitleWordmarkBounds`, `kTitlePromptBounds`), with zero
change to any existing released symbol. `GameState`, `GameSession`,
`GameLoop<Game>` and `GameInput` are all confirmed byte-identical to `HEAD`
(§1, re-derived independently of the reviewer's and QA's own repeated
confirmations of the same fact). Not a PATCH (new functionality, not a bug
fix); not a MAJOR (nothing existing removed or resignatured).

**Exact file list staged (explicit `git add <path>` calls, never
`-A`/`.`; re-checked against a fresh `git status` immediately before
staging — no drift from the prepared list):**
- Modified: `Makefile`, `docs/device-build.md`, `docs/dump-format.md`,
  `docs/host-tests.md`, `firmware/steamcore/src/font.cpp`,
  `firmware/system/main/CMakeLists.txt`,
  `firmware/system/main/app_main.cpp`, `tools/check_constraints.sh`
- New: `.spark/start-screen/spec.md`, `.spark/start-screen/plan.md`,
  `.spark/start-screen/review.md`, `.spark/start-screen/qa.md`,
  `.spark/start-screen/release.md` (this file),
  `firmware/steamcore/include/steamcore/title_screen.h`,
  `firmware/steamcore/src/title_screen.cpp`,
  `firmware/steamcore/test/title_screen_test.cpp`,
  `firmware/steamcore/test/title_screen_session_test.cpp`,
  `firmware/steamcore/test/title_screen_determinism_test.cpp`,
  `firmware/steamcore/test/title_screen_dump_test.cpp`,
  `firmware/steamcore/test/title_screen_game.h`,
  `firmware/steamcore/test/bench_title_screen.cpp`,
  `firmware/system/main/title_screen_harness_game.h`,
  `tools/scfb_capture.py`, `tools/test_scfb_capture.py`

**Explicitly excluded:** `assets/Buttons.png`, `assets/fonts/`,
`assets/sprites/` — untracked, pre-existing, unrelated to this feature
(flagged by both `review.md` and `qa.md`). Confirmed still untracked
immediately after the commit — did not ride along.

**Rollback path** (local-only, nothing pushed, nothing to unwind on a
remote — `git remote -v` is empty):
- Commit made but wrong: `git tag -d v0.4.0` (the tag exists — delete it
  first), then `git reset --soft HEAD~1` — restores every file to the
  working tree, nothing lost.
- Tag wrong but commit fine: `git tag -d v0.4.0` — local tag only, commit
  untouched, re-tag once corrected.
- Both wrong: `git tag -d v0.4.0` then `git reset --soft HEAD~1`.
- No force-push or remote cleanup is ever needed for this release — there is
  no remote to unwind.

## 4. Learnings (Keep!)

- **What went well:** the round-2 review discipline (re-breaking F1/F2 by
  mutation rather than trusting their `fixed` labels) caught a real gap — the
  first draft's tests imported the title strings from the code under test
  instead of restating them, so AC-1.2's wording could have regressed
  silently. Re-verifying independently at release time (fresh 171/171 x3,
  clean lint, forced-recompile device build, both pre- and post-commit)
  reproduced every number the gates cited with no surprises.
- **What we'd do differently:** two latent `font.cpp` device-build
  incompatibilities (a `throw` under `-fno-exceptions`, an implicit
  `<cstddef>` dependency) went undetected since `text-rendering` shipped,
  because no prior harness had ever compiled `drawText`/`font.cpp` for the
  device target. Compiling every released host-only module against the
  device toolchain at least once, even before a feature needs it on-device,
  would have caught this earlier.
- **Patterns worth reusing:** deriving every layout constant from
  `sizeof(string) - 1` and `kGlyphAdvance`/`kGlyphHeight` rather than a
  literal, then proving disjointness twice (a `static_assert` and a runtime
  test) is a clean, cheap template for any future screen composed purely of
  `drawText` calls — worth naming in `CLAUDE.md` for the next UI-composing
  feature.

---

## ✅ KEEP GATE

*All boxes checked → the loop is closed. The feature is done-done.*

- [x] All pre-flight checks passed at release time — 171/171 x3, lint clean,
      31/31 Python, bench and view both green, `idf.py build` green from a
      forced non-cached recompile, all four released engine types confirmed
      byte-identical, `git status` re-checked immediately before this report
- [x] Changelog written in user-facing language
- [x] Release actions executed and verified — commit `a8e590f` and annotated
      tag `v0.4.0` created; post-release smoke check re-run against the
      tagged commit reproduced every §1 number exactly. Deploy and PR/merge
      correctly N/A for `direct` mode with no pipeline
- [x] Learnings recorded
- [x] Line budget respected: Ist 151 / Soll ~100 (excluding HTML comments) —
      51 over; reason: the never-`-A` explicit file list, the version
      justification paragraph and the dual gate-recap in §0 together account
      for it, not prose (same overage class as `input-driver`'s release)
- [x] Status set to `released`, or `handed-off` in declared `pr` mode —
      `direct` mode, commit `a8e590f` and tag `v0.4.0` are the complete
      publish action for this project (no remote, nothing pushed)
