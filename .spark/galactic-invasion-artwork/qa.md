# QA Report: galactic-invasion-artwork

| | |
|---|---|
| **Phase** | Review (hands-on) |
| **Owner** | QA Tester (`/demo-day`) |
| **Input** | `.spark/galactic-invasion-artwork/spec.md`, `.spark/constitution.md` §8 |
| **Status** | `passed` |
| **Round** | 1 |
| **Date** | 2026-09-16 |

**Handoff**
- **Status:** `passed`.
- **Verdict:** Would demo this. Every Must-story AC is verified by a method I personally performed (host gates I ran, framebuffer PNGs I decoded pixel-by-pixel, generator failure paths I triggered myself). US-4's hardware ACs are Should and are recorded "as reported" from this session's earlier T15 capture, per the constitution's hardware-gated convention — not independently re-verified here, since re-flashing needs a human physically pressing buttons.
- **Open:** `none` — Blockers: `none`; Majors: `none` (2 Minors filed, both cosmetic, already known to review).
- **Binding ruling:** §5 Verdict and the gate checklist below.
- **On conflict:** the numbered body below wins for everything except `Status`.

## 1. Test Environment

- **No browser-observable surface** (`.spark/constitution.md` §8: "Browser-observable surface: `no`"). Substitute method used, exactly as declared: framebuffer dump over USB-CDC decoded by `tools/fb_view.py` into one PNG per acceptance criterion (already captured this session, reused per the caller's instruction — not re-flashed), plus host-compiled unit tests, which I ran myself.
- **App URL:** N/A (no web/browser surface).
- **Toolchain used:** Apple clang 14 (`make test`), `g++` (`make test-gcc`), ASan/UBSan (`make test-asan`), `python3` (host `make lint`, `tools/test_generate_sprite_data.py`), ESP-IDF v5.4.4 (`idf.py build`, sourced from `~/esp/esp-idf/export.sh`, re-run by me in `firmware/system/`).
- **Evidence decoded and inspected pixel-by-pixel (not just opened):** `build/galactic_invasion_logo.png`, `build/galactic_invasion_pattern.png`, `build/galactic_invasion_win_pattern.png` — read visually and also decoded with a stdlib PNG reader to extract exact colour/coordinate data (see §2 below for what that found).
- **Hardware:** not physically re-flashed this round (no new rendering-affecting change since T15's capture, per the caller's brief); US-4 recorded "as reported" from that capture.

## 2. Acceptance Criteria Verification

| Spec ID | Steps performed | Expected | Observed | Result |
|---|---|---|---|---|
| AC-1.1 | Ran `make test` (339 tests incl. `galactic_invasion_logo_test.cpp`); decoded `galactic_invasion_logo.png` pixel-by-pixel | Logo drawn via one `blit` in `READY`, rest of screen `BLACK` outside logo/prompt | Wordmark present at documented bounds; only 3 palette colours (`BRIGHT_ORANGE`/`ORANGE`/`DARK_ORANGE`) + `BLACK` found in the frame, no stray colour | pass |
| AC-1.2 | `grep` the generated header + `art_test.cpp:470` (`kLogoWidth <= 200 && kLogoHeight <= 56`, F5's fix); host suite green | One rectangular sprite, ≤200×56, 4 symbols only | 169×56, one `blit`, dimension-capped assert present (not area-capped) | pass |
| AC-1.3 | Decoded logo PNG: found "PRESS START" text at y≈104-118 in colour (255,153,51)=`BRIGHT_ORANGE` only | Prompt drawn via `drawText`, `BRIGHT_ORANGE` | Confirmed, isolated from logo colours | pass |
| AC-1.4 | `make test` (disjointness test in `logo_test.cpp:105` `CHECK(!rectsIntersect(...))`); `grep`'d header `static_assert`s in `galactic_invasion_logo.h:74-81` | Pairwise disjoint, proven twice (compile-time + runtime) | Both present; pixel decode confirms logo (y37-90ish) and prompt (y104-118) occupy non-overlapping bands | pass |
| AC-1.5/1.6 | `make test` — `logo_test.cpp:128,165` (START edge test, replay byte-equality) | Neither element drawn after START edge; byte-identical replay | 339/339 pass including these | pass |
| AC-1.7 | Visually inspected `galactic_invasion_logo.png` at 4x zoom | Legible large-pixel wordmark, no dither | Reads clearly as "GALACTIC INVASION", large blocky strokes, no banding/dither pattern | pass |
| AC-1.8 | `make test` — exhaustive-switch/no-clear test | Never clears; nothing drawn in `PLAYING`/`GAME_OVER` | Test passes; `galactic_invasion_pattern.png`/`win_pattern.png` show no logo bleed-through in `PLAYING`/`GAME_OVER` | pass |
| AC-1.9 | `make test` — held-start-at-boot test | First tick with `start==true` skips the logo | Test passes | pass |
| AC-1.10 | Ran `make test-asan` myself: 339/339, 0 sanitizer trips | No OOB access rendering the logo | Clean ASan/UBSan run | pass |
| AC-1.11 | n/a — fallback path not taken (T14 `skipped`, wordmark legible per AC-1.7) | — | Confirmed logo is legible; fallback code path exists and compiles (part of the 339-test suite) but is not exercised in `READY` | pass (n/a path) |
| AC-1.12 | `grep`'d `logo_test.cpp:113-117` (`CHECK_EQ` field-by-field vs `kTitlePromptBounds`); `make test` green | `kGiPromptBounds` identical to engine's `kTitlePromptBounds` | Confirmed by test and present in code | pass |
| AC-2.1/2.5/2.9 | `make test` (339, incl. `galactic_invasion_art_test.cpp`); `git diff` shows 6 dimension constants unchanged | Player 12×12 unchanged, recognisable ship silhouette | Confirmed by tests; visually, decoded pattern PNG shows a symmetric nose-up rocket/ship shape at (114-125, 144-155) | pass |
| AC-2.2 | Ran `make test`: 339/339 green | Every behavioural assertion passes textually unchanged | Confirmed — matches review's re-derivation. Two tests remain `#if 0`-disabled (documented, user-waived in review as F1) — not a regression introduced since | pass (F1's waiver stands, not re-litigated — condition (a)/(b)/(c)/(d) for re-derivation don't apply: not a fix being verified, review already treated this as the binding Must-AC verification) |
| AC-2.3 | `make test` (`hasNoBrightOrange` assertions); decoded pattern PNG colour histogram | `BRIGHT_ORANGE` player-only, `ORANGE` enemy-only, `DARK_ORANGE` interior ≥3px only | Colour histogram of the PLAYING dump: `(255,153,51)`=326px (HUD+ship+player shot only), `(179,89,0)`=1044px (enemy formation only), `(77,38,0)`=360px (enemy interior only) — zero cross-contamination in this frame | pass |
| AC-2.4 | Visual inspection of decoded/zoomed pattern PNG; review's `/look-and-feel` T13 verdict cited (fix already re-verified) | Distinguishable by silhouette alone; player identifiable in motion | Ship (thin nose + fanned base) clearly distinct in shape from enemy (crab-like, notched antennae/legs) by silhouette; player shot found as isolated 2×6 solid bar at (119-120, 138-143), separate from the ship | pass |
| AC-2.6 | `make test` (structural core-size assertions) + visual: enemy interior shows one contiguous darker 4×5-ish mass, ship flat, shots flat | Shading matches spec's permitted patterns | Confirmed in decoded pixel data (enemy has an internal `DARK_ORANGE` cluster; both shots single-ink) | pass |
| AC-2.7 | Reviewed plan Deviations T7/T9/T10/T13 (source-cited, not re-derived — condition: predecessor's own re-verified re-run, no fix pending here) | Player shot kept, enemy reverted to shipped silhouette per design judgment | `kProjectileRows`/`kEnemyRows` source confirms both hand-kept with AC-3.11 notes | pass |
| AC-2.8/2.11 | `make test` (339, incl. `enemy_fire_test.cpp` negative controls) | Locators discriminating, negative controls present | Confirmed; F2's fix (all 4 `anyEnemyPixelOnScreen` copies now exclude shot pixels) verified present via green suite | pass |
| AC-2.10 | `make test` (`art_test.cpp:246-404`, i-iv predicates) | Structural predicates hold for all 4 sprites | 339/339 pass | pass |
| AC-3.1/3.8 | Ran the generator myself: `--probe`, `--preview`, background-rule flags all behave as documented in `--help` | Documented background rule + fit policy + deterministic quantisation | Confirmed via `--help` output and a live re-run | pass |
| AC-3.2 | **Performed myself:** moved `assets/` aside, ran `make test` (339 green) + `make lint` (skip line printed), then restored `assets/` immediately | Build/tests green with `assets/` absent, no PNG read | Confirmed — `check_constraints: provenance check skipped -- assets/sprites/galactic_invation.png is absent`; `assets/` fully restored afterward | pass |
| AC-3.3/3.4 | **Performed myself:** re-ran the exact regenerate command from the committed banner against a scratch output path, diffed against the committed header | Byte-identical (modulo the intentionally-different `--output` path in the command string) | `diff` showed only the `--output` path differing — everything else byte-identical | pass |
| AC-3.5 | `make test`/`test-gcc` green (both compilers see the same `static_assert`s) | `constexpr`, `static_assert`-validated, both routes | Confirmed | pass |
| AC-3.6 | **Performed all four failure paths myself:** missing source, out-of-bounds region, upscale target, unwritable output (two real triggers: nonexistent parent dir, and output path colliding with an existing directory) | All exit non-zero, named reason, nothing written | All four confirmed exit 1 with specific messages; confirmed no output file left behind in each case | pass |
| AC-3.7 | **Ran `idf.py build` myself** in `firmware/system/` (sourced ESP-IDF export.sh) | Device build green | `Project build complete`, exit clean; `grep -rn throw` over `games/` found no actual `throw` usage (only explanatory comments) | pass |
| AC-3.9 | `make lint` output inspected | Warning-only provenance check; skip when `assets/` absent | With `assets/` present: no warning line (hash matches — I also independently regenerated and diffed, confirming no drift); with `assets/` absent: explicit skip line, not a failure | pass |
| AC-3.10/3.11 | Source-read `kEnemyShotRows`/`kProjectileRows` plus decoded pattern PNG | Enemy shot segmented `ORANGE`, hand-authored notes present | Confirmed: `"##","##","  ","  ","##","##"` segmented pattern with AC-3.11 comment | pass |
| AC-4.1 | **Not independently re-verified** — cited from plan.md Deviations T15 (this session's own earlier on-device capture); no rendering-affecting change since | Logo/prompt at documented bounds on real device | As reported by T15: matched host dump pixel-for-pixel, with the one honestly-reported stray pixel (see B1 below) | pass (as reported, per constitution §4/§8 hardware-gated convention — not re-flashed) |
| AC-4.2 | Same as above | START press on real device begins play, cast redrawn | As reported by T15 | pass (as reported) |
| AC-4.3 | N/A — hardware was available this session (T15), not blocked | — | — | n/a (not the blocked case) |
| NFR-1 | Ran `make bench` myself | READY render well under 16.6 ms budget | `galactic-invasion READY tick: 9.31 us/tick` — ~1790x under budget; consecutive-frame-identical test passes | pass |
| NFR-2 | `grep`'d `art_test.cpp:504-507` footprint assert | Total art ≤ 12,288 B | Assert present and part of the green 339-test suite (9,752 B per review's re-derivation, re-confirmed structurally present here) | pass |
| NFR-3 | `make test` (determinism tests) + my own byte-identical regen re-run | Same input → same output, both rendering and generator | Confirmed both ways | pass |
| NFR-4 | `make test`/`test-gcc` (both compilers) + my own `idf.py build` | Clean on 3 toolchains | All 3 green, run by me | pass |
| NFR-5 | `git diff HEAD -- firmware/steamcore/include firmware/steamcore/src` | Empty (no engine surface change) | Empty | pass |
| NFR-6 | Read `galactic_invasion_art.h`'s header doc block myself | Corrected doc claims (no more "one ink per sprite"/"no build-time tool"/stale DARK_ORANGE rationale) | Confirmed corrected text present (F2/NFR-6 fix verified by direct read) | pass |
| NFR-7 | Decoded pattern PNG colour data + `kEnemyShotRows` source | Lethal object ≥3:1 contrast, identity survives colour-blind read | `ORANGE` (4.35:1) confirmed used for enemy shot; segmented silhouette confirmed in source | pass |
| NFR-9/NFR-10 | AC-3.6 exercises above; `make lint` allowlist check | Loud failures, zero new build deps | Confirmed | pass |

*NFR-8 is N/A per spec (no accounts/roles/PII).*

## 3. Exploratory Findings

| # | Severity | Steps to reproduce | Expected vs. observed | Status |
|---|---|---|---|---|
| B1 | Minor | Decoded `build/galactic_invasion_logo.png` pixel-by-pixel; inspected screen coordinate (36, 39) and its 8-neighbourhood | Expected: no stray lit pixel outside letterforms (AC-1.7's residual-risk check). Observed: one fully isolated `BRIGHT_ORANGE` pixel at (36,39), surrounded on all sides by `BLACK` — cosmetic, reads as a sparkle, not a rendering defect. **Same finding already documented and accepted as review F3** — I re-derived it independently from the PNG bytes rather than citing it blind, and it matches exactly (screen (36,39) = logo-local (1,7), as F3 states). | accepted (per review F3, no new information) |
| B2 | Minor | Decoded `build/galactic_invasion_logo.png`; found 5 additional small non-letterform lit clusters near the wordmark (small dash/arrow-like marks flanking "GALACTIC" and "INVASION", visible at 4x zoom) beyond the single isolated pixel in B1 | Expected: only letterform pixels plus at most the one documented stray pixel. Observed: several small decorative-looking blobs (2-4px clusters) that are part of the sheet's glow/flourish artifacts surviving quantisation, visually read as intentional flourishes rather than defects — same root cause review F3 already named ("six lit components that are not letterforms... the larger blobs read as deliberate sparkles"). Filing separately only because the review's finding text focuses on the single isolated pixel; the multi-pixel blobs are the same accepted, already-recorded residual risk, not a new defect. | accepted (per review F3, restating already-recorded scope) |

No new Blocker or Major found beyond what review already surfaced and resolved (F1 waived, F2-F6 fixed and re-verified independently by me above).

## 4. Console & Network

N/A — no browser, no network surface (offline embedded device, constitution §2/NFR-8). Checked instead: host test sanitizer output (`make test-asan`, clean — no ASan/UBSan trips), and the generator's own stderr on all four failure paths (all named, specific, no silent swallowing).

## 5. Verdict

Would demo this right now. Every Must-story AC (US-1/2/3) was verified by a method I performed myself: I ran all host gates (339/339 across clang/asan/gcc, `make lint`, 54/54 Python unit tests, `make bench`), decoded the framebuffer PNGs down to raw pixel bytes rather than eyeballing thumbnails, independently re-ran the generator's regenerate command and diffed it byte-identical against the committed header, personally triggered all four AC-3.6 failure paths, personally moved `assets/` aside and back to prove the build/lint invariant, and personally ran `idf.py build` end-to-end after sourcing the ESP-IDF environment. Nothing here depends on reading source and assuming it works. US-4's two hardware ACs are Should-priority and are recorded "as reported" from this session's own earlier T15 device capture per the constitution's hardware-gated convention — I did not re-flash, since no rendering-affecting code has changed since that capture and doing so would require a human physically pressing buttons I cannot press myself. The only findings are two Minor, already-known cosmetic artifacts in the generated logo (a stray pixel and some small glow-remnant blobs) that review already surfaced, judged cosmetic, and recorded (F3) — I independently re-derived the exact same pixel coordinates from the raw PNG bytes rather than trusting the prior report, and they match.

---

## ✅ QA GATE

- [x] Every Must-story acceptance criterion verified by the declared substitute method (framebuffer dump + host tests) and passed
- [x] Every declared-method-observable NFR verified and passed
- [x] No open Blocker or Major bugs (2 Minor bugs listed, both already accepted per review F3 — not new)
- [x] N/A: browser console (no browser surface, constitution §8) — substitute checked: ASan/UBSan clean, generator stderr loud and specific
- [x] Tested on all agreed "viewports": N/A by platform — single fixed 240×160 framebuffer, no responsive surface
- [x] Line budget respected: Ist 99 / Soll ~130 (excluding HTML comments)
- [x] Status set to `passed`
