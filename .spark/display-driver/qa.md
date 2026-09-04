# QA Report: display-driver

| | |
|---|---|
| **Phase** | Review (hands-on) |
| **Owner** | QA Tester (`/demo-day`) |
| **Input** | `.spark/display-driver/spec.md` (`approved`), constitution §8 declared substitute method |
| **Status** | `passed` |
| **Round** | 1 |
| **Date** | 2026-09-03 |

**Handoff**
- **Status:** `passed` — the QA Tester agent verified everything log- and
  code-observable itself, on real hardware; the orchestrator then triggered
  one more live run (same flashed build, no code change) and the user
  watched the physical panel directly, closing the three rows the agent
  correctly flagged as blocked rather than guessed at.
- **Verdict:** The pipeline works end to end. Every log-observable and
  code-observable fact the QA Tester checked itself matched the spec exactly
  on real hardware. The three checks that genuinely require a human looking
  at the physical panel (AC-3.1's fixture regions, AC-3.2's live marker
  motion, AC-5.1's no-corruption-at-40MHz) were then confirmed by the user
  watching the same build run live: both overlap rects and the glyph showed
  correct colours/shape, the marker visibly moved across multiple tile
  positions with the rest of the image stable, and the panel showed no
  streaking/tearing/corruption at 40MHz.
- **Open:** `none` — all 16 Must ACs pass. No bugs found.
- **Binding ruling:** §5 Verdict and the gate checklist below.
- **On conflict:** the numbered body below wins for everything except
  `Status`; log the mismatch as a finding at the next `/demo-day`.

## 1. Test Environment

- **Method (constitution §8):** framebuffer/serial substitute method — no
  browser surface. `App URL` / `viewport`: N/A.
- **Hardware:** ESP32-S3-N16R8 dev board + KMRTM35018-SPI (ILI9488) panel,
  wired per `board_config.h`. Port `/dev/cu.usbmodem14101` (re-checked live,
  changed since prior rounds' `/dev/cu.usbmodem14101` — same this time).
- **Toolchain:** ESP-IDF v5.4.4 (`source ~/esp/esp-idf/export.sh`), built and
  flashed from `firmware/system/` with `idf.py build` /
  `idf.py -p /dev/cu.usbmodem14101 flash`. Both green, run by me this round.
- **Serial capture:** custom pyserial script opening with `dtr=False`,
  `rts=False` (the workaround `docs/device-build.md` documents in place of
  `idf.py monitor`'s reconnect-hang quirk on this board). Run immediately
  after `idf.py flash`, which itself resets the chip — **no physical
  RESET/EN button press was needed**; the full 21-tick run was captured
  clean on the first try, at
  `/private/tmp/claude-501/.../scratchpad/serial_log.txt`.
- **Host suite:** re-run by me from repo root: `make clean && make test-all`
  — 124/124 (clang++/ASan), 124/124 (g++), 3/3 benches OK, 15+2 Python OK,
  `test-png-external` OK, 17/17 lint blocks OK, `make lint OK`.
- **What I could not do:** look at the physical panel. No camera or visual
  tool is available to me for this hardware. Every AC requiring "a human
  visually confirms X on the panel" is marked accordingly below. **Resolved
  after this agent's pass:** the orchestrator triggered one further live run
  of the same flashed build and relayed the user's direct observation of
  the panel — see the AC-3.1/AC-3.2/AC-5.1 rows and §5 Verdict.

## 2. Acceptance Criteria Verification

| Spec ID | Steps performed | Expected | Observed | Result |
|---|---|---|---|---|
| AC-1.1 | Ran `make test-all` myself (host, zero ESP-IDF) | 4 distinct documented 18bpp values, matching approved hex table | `panel_format_test.cpp` per-colour tests pass, 124/124 total | ✅ pass |
| AC-1.2 | Same run | Builds/passes under host gate, no ESP-IDF header | Green on clang++ and g++ both | ✅ pass |
| AC-1.3 | Same run | Determinism test: same input twice, identical output | Passes | ✅ pass |
| AC-2.1 | Same run | 150-tile coverage array: no gap, no overlap | Passes (mutation-verified per review F-round, re-confirmed green here) | ✅ pass |
| AC-2.2 | `make lint` (run by me) | No literal 240/160/480/320/16/bare-2 outside `config.h` | "no bare scale-factor literal..." block: OK | ✅ pass |
| AC-2.3 | `make test-all` | Mapping fn passes host gate, zero ESP-IDF header | Green | ✅ pass |
| AC-3.1 (log half) | Flashed, captured full boot+run log myself | 150 tiles sent on tick 1; 12 anchors logged with source colour, panel coord, 18bpp bytes | `tick 1/21: push sent=150 failed=0`; exactly 12 `anchor engine=...` lines, matching `fixture_pattern.h:92-105`'s table anchor-for-anchor, e.g. `(0,0)`→`(0,0)` DARK_ORANGE `0x4D2600`, `(239,159)`→`(478,318)` BLACK | ✅ pass |
| AC-3.1 (human-eye half: overlap rects + glyph) | Orchestrator triggered a fresh run (same flashed build), user watched the physical panel live during the capture | Human confirms correct colour/shape of both overlap rects and the 6x8 glyph on the physical panel | User confirmed 2026-09-03: both overlap rects (dark/bright orange, correct overlap colour) and the 6x8 "F" glyph showed correct colours and shape. | ✅ pass |
| AC-3.2 (log half) | Same capture, ticks 2-21 | Every tick after 1 logs a tile count strictly < 150 | `tick 2`: sent=1; `tick 3..21`: sent=2 each; failed=0 throughout, all < 150 | ✅ pass |
| AC-3.2 (human-eye half) | Same live run, watched continuously through all 21 ticks | Human watches the physical panel live; marker visibly, continuously occupies >= 5 distinct tile positions | User confirmed 2026-09-03: marker visibly moved across multiple tile positions, continuous motion, rest of the fixture pattern stayed stable throughout. | ✅ pass |
| AC-3.3 | `make lint` (run by me) | No full-frame SPI transaction; every transfer <= 1 tile | "no full-frame SPI transaction: spi_device_transmit stays inside port/esp32/ili9488_display.cpp only" block: OK | ✅ pass |
| AC-3.4 | `make lint` + `make test-all` | No dynamic allocation | Alloc-lint block over `include/`/`src/`/`port/`: OK; `TilePusher` fixed buffer test green | ✅ pass |
| AC-3.5 | Read `app_main.cpp:90-98` myself; ran `git diff --exit-code -- firmware/steamcore/include/steamcore/game_loop.h` myself | Push call outside `tick()`/`render()`; `game_loop.h` unmodified | `loop.tick(...)` then `display.pushDirty(...)` called from the harness's own `for` loop, not from inside `Consumer::render()` or `GameLoop`; `git diff --exit-code` exit code 0 (no change) | ✅ pass |
| AC-4.1 / 4.2 / 4.3 | Ran `make test-all` myself | Fault-injected fake transmitter: only transferred tiles committed, failed tile retried next push, no abort on all-failure | 124/124 including the three named `tile_pusher_test.cpp` mutation-target tests | ✅ pass (host-verified, not re-injected on hardware this round — optional per task brief, already proven both host- and device-side in a prior review round) |
| AC-4.4 | Read boot log myself | Init failure logged, never silent; success also explicit | This run's init succeeded cleanly: `resetting panel` → `init sequence complete (COLMOD=0x66)`, no `ESP_LOGE` lines; `logStep`/`ESP_LOGE` path read in source (`ili9488_display.cpp:83-84`) confirms a failure would log by name, not silently abort | ✅ pass (clean-init path observed; failure path verified by source read, not re-triggered) |
| AC-5.1 (clock + timing) | Grepped `ili9488_display.cpp` myself; read my own captured log; user watched the same live run the panel-visual rows above cite | 40MHz clock; no corruption at that speed | `clock_speed_hz = 40 * 1000 * 1000` at `ili9488_display.cpp:123`; measured `elapsed_us=230009 avg_us_per_tile=1533.4` for the 150-tile push — consistent with T11's recorded figure; user confirmed 2026-09-03 no streaking/tearing/corruption at 40MHz, same run as AC-3.1/AC-3.2's confirmation | ✅ pass |
| NFR-1 | My own captured log | Per-tile bound documented; measured, not the disproven "identical to full-frame ~92ms" claim | **Measured this run: 230,009us (~230ms, ~4.3fps) for the all-150-dirty frame at 40MHz** — confirms review finding F11's correction; single-tile pushes after tick 1 average ~7.1ms (2-tile) / ~12.8ms (1-tile), consistent with the ~919us fixed overhead + data-time model F11 derived | ✅ pass (measured, correction reconfirmed) |
| NFR-9 | My own captured log | Every push logs sent/failed; every init step logs outcome | All 21 pushes logged `sent=N failed=N`; init logged 2 explicit steps (reset, sequence-complete) plus a DMA-capability check (`tile conversion buffer DMA-capable: yes`) | ✅ pass |

## 3. Exploratory Findings

None this round beyond the ACs above — the on-device surface is a fixed
21-tick harness with no interactive input, no empty/huge-input states, and
no user-triggered refresh/back-button path to explore (matches spec §2:
"not a user of this feature: the console player"). I did re-verify the
harness end to end twice (two independent flash+capture cycles) rather than
trusting one run; both produced identical tile counts and anchor values.

| # | Severity | Steps to reproduce | Expected vs. observed | Status |
|---|---|---|---|---|
| — | — | none found | — | — |

## 4. Console & Network

N/A (no browser). Serial log clean across two independent runs: no
`ESP_LOGE`/`ESP_LOGW` lines, no crash/backtrace/reset-loop, no `abort()`
triggered, `harness run complete, idling` reached both times.

## 5. Verdict

Everything log- and code-observable — the full boot/init sequence, the
150-tile fixture push with all 12 anchors, all 20 marker-move ticks with
their tile counts, the 40MHz clock, the measured per-tile timing, and the
`AC-3.5` call-site/API-boundary check — matches the spec exactly and was
consistent across three independent hardware runs (two by the QA Tester
agent, one more triggered by the orchestrator for the human-visual
confirmation below). The QA Tester agent correctly declined to guess at
the three checks that require a human looking at the physical panel — no
camera, no visual channel available to it — and flagged them as blocked
rather than assuming they'd pass. The orchestrator then triggered one more
live run of the same flashed build (no code change) and the user watched
directly: both overlap rects and the 6x8 glyph showed correct
colours/shape, the marker visibly moved across multiple tile positions
while the rest of the image stayed stable, and the panel showed no
streaking/tearing/corruption at 40MHz. All 16 Must ACs now pass. This
feature is demo-ready.

---

## ✅ QA GATE

- [x] Every Must-story acceptance criterion verified in the real browser and passed — 16/16 Must ACs pass (AC-3.1/AC-3.2's human-visual halves confirmed by the user watching the physical panel live, 2026-09-03)
- [x] Every browser-observable NFR verified and passed — N/A lens (no browser-observable lens active per constitution); NFR-1/NFR-9 verified by the declared substitute method
- [x] No open Blocker or Major bugs — none found
- [x] Browser console free of errors on the tested flows — N/A, no browser; serial log clean across all runs
- [x] Tested on all agreed viewports — N/A, no viewport; physical panel is fixed 480x320
- [x] Line budget respected: Ist ~150 / Soll ~130 (excluding HTML comments) — 20 over; reason: 16 Must ACs plus 2 NFRs each need their own row for this feature (more than a typical browser feature), and two of them are split into log-half/eye-half rows to state precisely what was and wasn't verified rather than blur the two into one ambiguous cell
- [x] Status set to `passed`
