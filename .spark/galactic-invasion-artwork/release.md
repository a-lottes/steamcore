# Release: galactic-invasion-artwork

| | |
|---|---|
| **Phase** | Keep |
| **Owner** | Release Manager (`/go-live`) |
| **Input** | `review.md` (`passed`), `qa.md` (`passed`) |
| **Status** | `released` |
| **Version** | v0.9.0 |
| **Date** | 2026-09-16 |

**Handoff**
- **Status:** `released`.
- **Summary:** Galactic Invasion gets its own logo/title screen and a redrawn player ship, enemy and both shot sprites, generated from concept art by a new offline, stdlib-only PNG-to-sprite tool that never ships in the firmware build.
- **Open:** `none`.
- **Binding ruling:** §3 Release Actions and the KEEP GATE below carry the final ruling.
- **On conflict:** the numbered body below wins for everything except `Status`/`Version`; log the mismatch as a finding at the next `/go-live` and proceed — don't stop on it.

## 1. Pre-Flight Checks

- [x] `review.md` status is `passed` (F1 user-waived 2026-09-16; F2-F6 fixed and re-verified)
- [x] `qa.md` status is `passed` (independently re-performed by the QA Tester, not read off review)
- [x] Full test suite green on the release commit — `make test`/`test-asan`/`test-gcc` 339/339 each, `make lint` OK, re-run *after* the release commit (`0077866`) as the post-release smoke check, not just before
- [x] Build succeeds from the release commit — `idf.py build` green (ESP-IDF v5.4.4)
- [x] No uncommitted changes belonging to this feature — the working tree does carry two unrelated, pre-existing, already-uncommitted items this feature deliberately left untouched: `firmware/system/main/app_main.cpp` (a different feature's throwaway playtest harness, fenced off by this feature's own plan R8/T6 and review.md's Scope section) and a few untracked `assets/` files outside spec A14's one committed asset (`assets/sprites/galactic_invation.png`, which *is* in this release). Both predate this feature and are out of its scope, not leftovers from it.

## 2. Changelog

### Added
- Galactic Invasion now shows its own "GALACTIC INVASION" logo and a "PRESS START" prompt when you select it, instead of the generic system title screen.

### Changed
- The player's ship, the enemy formation, and both shot types now share one shaded, hand-designed look instead of flat single-colour blocks.
- The enemy's return fire has its own distinctive shape, so you can tell it apart from your own shot at a glance — even by silhouette alone.

### Fixed
- *(none — this is a new-capability release, not a bugfix release)*

## 3. Release Actions

| Action | Result |
|---|---|
| Version bump & tag | `v0.9.0` — annotated tag on commit `0077866`, same-commit tag (this project's normal same-day-full-loop convention, `v0.1.0`-`v0.8.0`; the `v0.0.x` retroactive-catch-up convention doesn't apply, since this feature's source was never committed before this release) |
| PR / merge | N/A — `direct` release mode (constitution §7): solo project, no remote configured, committed straight to `main` |
| Deploy | N/A — embedded firmware project; "deploy" is `idf.py flash` to physical hardware, performed on request, not as part of a release pipeline |
| Post-release smoke check | Performed after the commit and tag existed, not before: `make test` (339/339) and `make lint` (`make lint OK`) re-run against the actual release commit `0077786` — confirms the released tree is alive, not just that an earlier gate was green |

## 4. Learnings (Keep!)

- **What went well:** The T9 (fit algorithm) and T13 (enemy silhouette) course-corrections both worked exactly as the SPARK loop intends — a real defect was found by systematic evidence (a parameter sweep; a design review), routed to the user with the alternatives actually weighed, and the user's decision was implemented without re-litigating it. Neither turned into a stalled negotiation. `/peer-review`'s independent re-derivation of every AC (not just reading the project's own assertions) caught four real, if minor, defects (F1-F4, F6) that a lighter review would have missed — most notably F1, a genuine regression-guard gap two tasks deep in the increment.
- **What we'd do differently:** T7 and T10 each disabled a test for the identical root cause without an explicit user ruling on the second occurrence — only `/peer-review` caught that the second waiver was never actually asked for. When a `#if 0`-disable recurs for a reason already ruled on once, get the explicit waiver at the same task, not two ceremonies later. Also: `script -q`'s doubled `\r\r\n` when capturing `idf.py monitor` output cost real time to diagnose (F1's capture, T15) — now documented in `docs/device-build.md`, but worth checking for on the *first* on-device capture of a future feature, not rediscovering it.
- **Patterns worth reusing:** (1) Majority-vote-after-individual-quantisation for downscaling anti-aliased pixel art beats mean-then-quantise — mean blending across a cell boundary reliably produces an isolated wrong-ink pixel; quantising first and voting is robust to a handful of anti-aliased outliers. (2) An `exclude`-parameter on a shared quantiser (`quantize(rgb, exclude=(BRIGHT_ORANGE,))`) is a clean way to enforce a per-side/per-role ink reservation structurally, rather than reviewing generated output for a rule violation after the fact. (3) Template-matching a located sprite's own pixel data (`findEnemyShots` against `kEnemyShotSprite`) survives art changes that break a fixed-colour locator — worth defaulting to over colour-based detection whenever entities can legitimately share an ink.

---

## ✅ KEEP GATE

- [x] All pre-flight checks passed at release time
- [x] Changelog written in user-facing language
- [x] Release actions executed and verified (`direct` mode: version bump + tag executed, PR/Deploy correctly N/A, post-release smoke check performed against the actual release commit)
- [x] Learnings recorded
- [x] Line budget respected: Ist 78 / Soll ~100 (excluding HTML comments)
- [x] Status set to `released`
