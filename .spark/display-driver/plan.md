# Plan: display-driver

| | |
|---|---|
| **Phase** | Plan |
| **Owner** | Engineering Manager (`/sprint-plan`) |
| **Input** | `.spark/display-driver/spec.md` (`approved`) |
| **Status** | `approved` |
| **Date** | 2026-09-03 |

**Handoff**
- **Status:** `approved` — user approved as proposed, including both flagged judgment calls: the NFR-7 surface reading (§1 Decision 6) and AC-3.1's log-verification reading (§1 Decision 7).
- **Summary:** The ESP-IDF boundary is a **directory**, not an `#ifdef`: everything pure stays in `firmware/steamcore/{include,src}` (unchanged host `make test`, unchanged absolute no-ESP-IDF-header lint rule), and the SPI half lives in a new `firmware/steamcore/port/esp32/` subtree the host build never globs. The seam between them is a **compile-time transmitter template** (`TilePusher<Transmitter>`), not a virtual interface — which pulls the scan→push→commit-only-transferred retry logic (US-4) back onto the host gate, leaving only bus setup, the init sequence and the actual `spi_device_transmit` device-only.
- **Open:** `0 tasks not done` — all 11 tasks `done`. `make clean && make test-all` green (124 C++ tests × 4 configs, benches, 15 Python + 2 round-trip, `sips`, 17 lint blocks incl. 5 new display-driver ones); `idf.py build` green. Every Must-story AC verified on real hardware 2026-09-03 with logged transcripts and human visual confirmation (T2/T6/T7/T8/T11). Three deviations recorded in their own task rows, none architectural: T2's `app_main.cpp` had to change (not in the original file list — an unavoidable consequence of T6 moving `transmitTile` private, not a scope change); T6 found and fixed a real stack-overflow bug (Framebuffer+DirtyTracker+Ili9488Display as plain `app_main` locals vs. a 3584-byte task stack); T10's NFR-7 audit found `expandTile` missing from plan §2's own public-surface list and corrected it in place. SPI clock tuned to the full constitution-budget **40 MHz** (T11), stable, human-confirmed.
- **Binding ruling:** §3 Task Breakdown for current task status; a plan revision after review/QA findings updates §1/§3 in place, never a new section
- **On conflict:** the numbered body below wins for everything except `Status`; log the mismatch as a finding at the next `/peer-review` and proceed — don't stop on it.

## 1. Architecture Decision

- **Context:** This is the first feature that structurally cannot be 100% host-testable (A2, C1). Constitution §4
  makes that load-bearing, not cosmetic: the host gate is the *only* gate this project has, and the rule that
  protects it (`check_constraints.sh`'s "no ESP-IDF/FreeRTOS/driver header in `include/` or `src/`") is currently
  **absolute**. Whatever boundary I pick, the cost is measured in how much of that absoluteness it spends. Three
  facts narrow the field: the spike's init sequence is hardware-proven and must be reused (A8); `DirtyTracker`'s
  header already sketches the exact scan→push→commit-only-transferred loop the driver owes it; and US-4's
  fault-injection ACs are pure sequencing logic that happens to sit next to SPI, not SPI itself.

- **Decision:**
  1. **The boundary is a directory, and it costs the lint rule nothing.** New subtree
     `firmware/steamcore/port/esp32/` holds the ESP-IDF-dependent driver (`ili9488_display.{h,cpp}`, header
     *and* source). The host `Makefile` globs `$(SRC_DIR)/*.cpp` only, so it never sees it — **no Makefile
     change, no `filter-out`, no `#ifdef`**. `check_constraints.sh`'s ESP-IDF-header rule keeps scanning
     `include/`+`src/` and stays absolute; `port/` becomes the one declared place ESP-IDF headers are legal,
     and T9 adds the inverse rule (nothing in `include/`/`src/` may include from `port/`).
  2. **The driver is engine code, not app code** — it lives under `firmware/steamcore/`, is `steamcore`-
     namespaced, and NFR-7/NFR-8's `library` discipline binds it in full. `firmware/system/main/` keeps only
     what is genuinely app/throwaway: `app_main`, the synthetic consumer, the harness loop.
  3. **The pure/impure seam is a template parameter, not a virtual interface.**
     `template <typename Transmitter> class TilePusher` (header-only, `include/steamcore/tile_pusher.h`) owns
     the one fixed `uint8_t buffer_[kPanelTileBytes]` and the whole loop: walk the dirty `TileMask`, expand each
     tile, call `tx.transmitTile(window, bytes, count)`, accumulate what succeeded, `tracker.commit(fb,
     transferred)`. Host tests instantiate it with a fake transmitter that fails chosen tiles — so **AC-4.1,
     AC-4.2, AC-4.3 and AC-3.4 become host-CI-verifiable**, and the device-only residue shrinks to bus/GPIO
     setup, the init sequence, address-window bytes and `spi_device_transmit`. Same compile-time-binding
     rationale `GameLoop<Game>` already set (no vtable, no allocation, nothing virtual exists in this codebase).
  4. **A full-frame push is structurally unreachable, not merely grepped-against.** The transmitter concept's
     only method takes one window and is called once per dirty tile with exactly `kPanelTileBytes`; there is no
     API through which a 460,800-byte transaction could be expressed (AC-3.3 then also gets its grep, T9).
  5. **Panel geometry extends `config.h`, derived not spelled out** (A7): one new literal, `kPanelScale = 2`,
     plus `kPanelWidth/Height = kScreenWidth/Height * kPanelScale`, `kPanelTileSize`, `kPanelBytesPerPixel`,
     `kPanelTileBytes`, pinned by `static_assert`s naming 480/320/32/3072 in the one file the resolution lint
     already excludes. `PanelPixel` is a `{r,g,b}` struct, not a `uint8_t[3]`, so no index literal `2` is needed
     anywhere and AC-2.2's bare-`2` ban is satisfiable rather than aspirational.
  6. **Recorded interpretation of NFR-7 (flagged for approval).** The three named entry points are
     `toPanelPixel`, `tileWindow` and `Ili9488Display::pushDirty(fb, tracker)` — one caller-visible push call
     site, exactly as NFR-7 says. Realizing it needs four supporting symbols: `PanelPixel`, `PanelWindow` and
     `PushResult` (return types the entry points must have) and `TilePusher` (the internal composition, in
     `include/` and therefore reachable — the same convention-vs-reachability nuance game-loop review F7 raised
     about `detail`). They are declared here and audited in §2 rather than discovered at review.
  7. **Recorded interpretation of AC-3.1's log half (flagged).** MISO is unused (§6, write-only driver), so no
     log can prove what the *panel latched*. The harness logs, per fixture anchor, the source colour, the panel
     coordinate it mapped to, and the 18bpp bytes handed to a transaction that returned success. That is
     "landed at their exact expected coordinate and colour" as far as this driver can honestly claim it; the
     large regions are what the human eye confirms actually arrived.
  8. **The spike is retired, not extended.** `firmware/system/main/ili9488_display.{h,cpp}` are **deleted** in
     T2. Its init sequence (reset pulse, SWRESET, SLPOUT, COLMOD=0x66, MADCTL=0x28, INVOFF, DISPON), its
     command constants, its DC-toggle-per-transaction approach and its SPI2_HOST config port over verbatim
     where they are proven; three things change because they are wrong for a tile driver, not because they are
     ugly: `max_transfer_sz` (960 — smaller than one 3,072-byte tile), the hardcoded `320`/`480` locals, and
     `ESP_ERROR_CHECK` on steady-state transmits (AC-4.3 forbids the abort; init keeps it, AC-4.4).

- **Alternatives considered:**

  | Alternative | Why rejected |
  |---|---|
  | Keep the driver in `src/` and exclude it with `#ifdef ESP_PLATFORM` / a Makefile `filter-out` | Both punch a hole in the one lint rule constitution §4 calls load-bearing: the ESP-IDF-header grep over `src/` would need a named exception, and every future reviewer inherits "except that file". A directory the host build structurally cannot see needs no exception at all |
  | Put SPI behind a thin **virtual** interface so host tests can mock it | Buys the same testability as Decision 3 and costs a vtable, the first virtual dispatch in the codebase, and a runtime indirection on the hot per-tile path — for a seam with exactly one production implementation. game-loop already rejected runtime polymorphism for this same reason; deviating from that would itself be an architecture decision |
  | Put the whole driver in `firmware/system/main/` beside the spike | Makes the driver app code: it falls outside `firmware/steamcore/`'s naming, review and NFR-7/NFR-8 discipline, and every future game would depend on a symbol that lives in the "system bring-up" project. The spec asks for a `steamcore`-namespaced driver |
  | Pure module emits bytes; the driver re-implements the scan/commit loop itself | The retry contract (US-4) is the subtlest logic in the feature and would land entirely on the device side, verifiable only by fault injection on real hardware. Exactly the wrong half to push out of the gate |
  | Register `firmware/steamcore/` as a real ESP-IDF component with its own `CMakeLists.txt` | More idiomatic IDF, but introduces a second source-of-truth build file inside the host-tested tree that can silently drift from the Makefile's globs. One explicit SRCS list in `main/CMakeLists.txt` fails loudly (undefined reference) when it drifts |
  | Add an iterator/`forEachSetTile` to `TileMask` for the push loop | A new public symbol on a type this feature does not own (§6, NFR-7). `test(col, row)` over `kTileCols × kTileRows` already reads every bit |
  | Have the driver own `scan()` only and let the caller commit | Makes "only transferred tiles are ever committed" caller discipline instead of a structural guarantee — the exact class of weakness `render(Framebuffer&)` was chosen to remove in game-loop |
  | Second, larger on-device fixture so the human can eyeball everything | Explicitly out of scope (§6, F1's resolution): small elements route through the log instead |

- **Consequences:** *Easier* — `make test`/`test-asan`/`test-gcc` stay green with zero ESP-IDF exposure and gain
  real coverage of the retry contract; a future panel (Phase 4, ×3 or its own resolution) changes `config.h` and
  one `port/` directory; a second port (a different controller) is a sibling directory, not a rewrite.
  *Harder* — there are now two build systems that must both know about a new engine `.cpp` (host glob vs.
  explicit IDF `SRCS`), and `port/esp32/` is real production code that no CI gate compiles, so it can only rot
  in one direction: silently, until someone flashes. T9's lint rules and `docs/device-build.md` are the
  counterweight. *Deliberately not decided here* — task-level threading/DMA-completion overlap (single-threaded
  contract inherited, A10), and any 60 Hz pacing (no scheduler exists; NFR-5's decoupling holds).

## 2. Affected Components

Scoped by hand — no tool file was passed with this task, so no blast-radius query was run and none is cited here.

- **New (pure, host-gated):** `include/steamcore/panel_format.h`, `src/panel_format.cpp`,
  `include/steamcore/tile_pusher.h`; tests `test/panel_format_test.cpp`, `test/tile_pusher_test.cpp`;
  `test/fixture_pattern.h` (extracted, see below).
- **New (ESP-IDF, device-only):** `firmware/steamcore/port/esp32/ili9488_display.{h,cpp}`,
  `firmware/system/main/harness_consumer.h`, `docs/device-build.md`.
- **Modified:** `include/steamcore/config.h` (panel constants, Decision 5); `firmware/system/main/app_main.cpp`
  (spike loop → US-3 harness); `firmware/system/main/CMakeLists.txt` (engine + port sources, C++17);
  `test/dump_format_test.cpp` (pattern moves to `fixture_pattern.h`, included back — one definition, §6);
  `tools/check_constraints.sh` (T9); `docs/host-tests.md`.
- **Deleted:** `firmware/system/main/ili9488_display.{h,cpp}` — the spike, superseded (Decision 8). Leaving it
  would mean two drivers for one panel.
- **Untouched, by design:** `framebuffer.*`, `dirty_tracker.*`, `game_loop.h`, `game_state.*`, `color.h`,
  `board_config.h` (pins reused, none added), `text.*`, `font.*`, `dump_format.*`, `tools/fb_view.py`, `Makefile`.
- **New dependencies: none.** ESP-IDF v5.4.4 is already installed and already used by `firmware/system/`;
  `driver/spi_master.h`, `driver/gpio.h`, `esp_log.h`, `esp_timer.h`, FreeRTOS delays are all in-tree. No new
  package, no new service.
- **Public API surface added (NFR-7), audited:** entry points `toPanelPixel(Color)`, `tileWindow(col,row)`,
  `Ili9488Display::init()` + `Ili9488Display::pushDirty(fb, tracker)`. Supporting types `PanelPixel`,
  `PanelWindow`, `PushResult`, and `TilePusher<Transmitter>` (Decision 6, flagged). Also public, per A7
  (Decision 5) and correctly not flagged as drift: `config.h`'s six new constants `kPanelScale`,
  `kPanelWidth`, `kPanelHeight`, `kPanelTileSize`, `kPanelBytesPerPixel`, `kPanelTileBytes` — spec-sanctioned,
  named here explicitly per review round-1 F8. Not counted, per A13:
  `harness_consumer.h`'s consumer and `test/fixture_pattern.h`. **T10 audit correction:** `expandTile(fb, col,
  row, out)` is also public in `panel_format.h` and was missing from this list — a genuine drift, not a new
  symbol added during T10. Added here rather than hidden behind a `detail` namespace: it is clean, pure,
  directly unit-tested math (T1/T4) that a caller composing their own tile-push loop without `TilePusher`
  could legitimately want, the same standing as `toPanelPixel`/`tileWindow`. `TilePusher::buffer()` is a
  diagnostics-only accessor (doc comment says so explicitly) and, like `push()`, is a member of an
  already-counted type, not a separate new symbol.

## 3. Task Breakdown

| # | Task | Story | Covers (AC / NFR) | Depends on | Status | Definition of Done |
|---|---|---|---|---|---|---|
| T1 | Walking skeleton, host half: panel constants + `toPanelPixel`/`tileWindow`/`expandTile` + first test | US-1, US-2 | AC-1.2, AC-2.3, NFR-4 | – | `done` | `config.h` gains `kPanelScale`, `kPanelWidth/Height`, `kPanelTileSize`, `kPanelBytesPerPixel`, `kPanelTileBytes`, all derived from `kScreenWidth`/`kScreenHeight`/`kTileSize` with `static_assert`s pinning 480, 320, 32 and 3072; `panel_format.{h,cpp}` declares `PanelPixel{r,g,b}`, `PanelWindow{x0,y0,x1,y1}`, `toPanelPixel(Color)`, `tileWindow(col,row)` and `expandTile(const Framebuffer&, col, row, uint8_t* out)`; one test converts one colour, maps one tile and expands one tile end to end; `make test`, `make test-gcc`, `make test-asan` and `make lint` green with no Makefile change and no ESP-IDF header — files: firmware/steamcore/include/steamcore/config.h, firmware/steamcore/include/steamcore/panel_format.h, firmware/steamcore/src/panel_format.cpp, firmware/steamcore/test/panel_format_test.cpp |
| T2 | Walking skeleton, device half: `port/esp32/` driver with ported init + one real tile on the panel | US-3 | AC-3.3, NFR-9 | T1 | `done` | The spike's `ili9488_display.{h,cpp}` are deleted and replaced by `steamcore::port::esp32::Ili9488Display`, carrying the spike's proven init sequence verbatim except: `max_transfer_sz = kPanelTileBytes` (was 960, smaller than one tile), no local panel-size literals (`config.h` instead), and a `transmitTile(window, bytes, count)` that sets the address window and issues exactly one `spi_device_transmit`, returning `bool` instead of `ESP_ERROR_CHECK`-aborting so steady-state failures (a later task) can be non-fatal. `main/CMakeLists.txt` compiles `framebuffer.cpp`, `panel_format.cpp` and the port source; `app_main` pushes one hardcoded `BRIGHT_ORANGE` tile at `(col=7, row=4)`. **Verified on real hardware 2026-09-03**: `idf.py build` then `idf.py -p /dev/cu.usbmodem14101 flash`, physical RESET, log captured (pyserial, dtr=False/rts=False on open — `idf.py monitor` repeatedly hung on this board's reconnect quirk, see docs/device-build.md): `I (298) ili9488: resetting panel` / `I (728) ili9488: init sequence complete (COLMOD=0x66)` / `I (728) display_driver_main: tile push at (7,4) window x[224,255] y[128,159]: OK`. Human confirmation: "ich sehe ein orangenes Viereck in der Mitte" — matches the window's expected screen position exactly. `docs/device-build.md` written with the build/flash/monitor commands and the manual-RESET quirk — files: firmware/steamcore/port/esp32/ili9488_display.h, firmware/steamcore/port/esp32/ili9488_display.cpp, firmware/system/main/app_main.cpp, firmware/system/main/CMakeLists.txt, docs/device-build.md |
| T3 | Conversion battery: all four palette colours, distinct, matching the approved hex table | US-1 | AC-1.1, AC-1.3 | T1 | `done` | One named test per `Color` asserts `toPanelPixel` returns the 3-byte value derived from `docs/dump-format.md`'s already-approved table (`#000000`, `#4D2600`, `#B35900`, `#FF9933`) with the documented 18bpp truncation rule stated in the header, no independently invented mapping; a further test asserts all four results are pairwise distinct; a determinism test calls each conversion twice and asserts byte-identical output; `make test-asan` reports zero findings for the file — files: firmware/steamcore/test/panel_format_test.cpp, firmware/steamcore/include/steamcore/panel_format.h |
| T4 | Mapping battery: all 150 tiles, full coverage, no gap, no overlap, correct x2 expansion | US-2 | AC-2.1, NFR-4, NFR-5 | T1 | `done` | A test iterates every `(col,row)` in `[0,kTileCols) x [0,kTileRows)` and asserts `tileWindow` returns exactly `[col*kPanelTileSize, +kPanelTileSize-1] x [row*...]`; a second test paints a 480x320 coverage array from all 150 windows and asserts every panel pixel is claimed exactly once (no gap, no overlap); a third test writes a distinguishable engine tile and asserts `expandTile` emits each source pixel as a `kPanelScale x kPanelScale` block in row-major panel order, `kPanelTileBytes` bytes exactly, with the first and last byte checked explicitly; the mapping code reads no clock and no RNG — files: firmware/steamcore/test/panel_format_test.cpp |
| T5 | `TilePusher<Transmitter>` + fault injection: commit only what transferred, retry, never abort | US-4 | AC-4.1, AC-4.2, AC-4.3, AC-3.4, NFR-2, NFR-3 | T4 | `done` | Mutation-verified: replacing the transmit-result check with an unconditional commit fails exactly the 3 tests that should catch it (`tile_pusher_failed_tile_stays_dirty_others_clean`, `tile_pusher_retries_previously_failed_tile`, `tile_pusher_all_failure_returns_normally_no_abort`), reverted, 124/124 green. | `tile_pusher.h` defines a header-only `TilePusher<Transmitter>` owning one `uint8_t buffer_[kPanelTileBytes]` member (asserted by `static_assert` on `sizeof`) whose `push(Framebuffer&, DirtyTracker&, Transmitter&)` scans, expands and transmits per tile, then calls `commit` with only the succeeded tiles and returns `PushResult{sent, failed}`; tests with a fake transmitter prove: all-success commits everything and the next scan reports zero dirty; a transmitter failing tile *k* leaves exactly tile *k* dirty on the next scan and every other tile clean; a second push with the same failure attempts tile *k* again; a transmitter failing *every* tile returns normally with `sent == 0` and no abort; a transmitter that succeeds on the retry clears the tile; no `new`/container/`std::function` anywhere in the file — files: firmware/steamcore/include/steamcore/tile_pusher.h, firmware/steamcore/test/tile_pusher_test.cpp |
| T6 | Real SPI transmitter behind `TilePusher`; honest init failure; per-push logging | US-3, US-4 | AC-3.3, AC-4.4, NFR-9, NFR-1 | T2, T5 | `done` | **Deviation**: `init()` changed from T2's `void`-that-aborts to `bool`-that-logs-and-returns — every step logs its own outcome (`logStep`) and the caller (`app_main`) decides whether to abort, still never silent (AC-4.4). `app_main.cpp` had to change (not in the original file list) since `transmitTile` moved to the private nested `SpiTransmitter` and is no longer directly callable — a small, obvious, unavoidable consequence of T6 itself, not a scope change; T7/T8 replace this interim harness anyway. **Found and fixed on device**: the interim harness's first attempt hung silently (identical truncated boot log across two independent fresh boots, stopping right after `Calling app_main()`, no crash message) — `Framebuffer`+`DirtyTracker`+`Ili9488Display` as plain locals in `app_main()` total ~80KB against `CONFIG_ESP_MAIN_TASK_STACK_SIZE=3584`, a silent stack overflow. Fixed by making all three `static`; worth carrying into T7/T8's real harness design, not just this scratch one. **Verified on real hardware 2026-09-03** with a temporary forced-failure test hook (reverted before this entry, confirmed by rebuild): `push 1: sent=150 failed=0 elapsed_us=507915` (fresh tracker, all tiles) / `push 2: TEST HOOK forcing failure on call #151 (tile at 448,256)` → `sent=0 failed=1` / `push 3: sent=1 failed=0` — the same tile recovered on the very next push, exactly the AC-4.1/AC-4.2 contract proven on-device, not just on the host. `esp_ptr_dma_capable` check logged `yes`. | `Ili9488Display` gains `bool init()` and `pushDirty(Framebuffer&, DirtyTracker&)` delegating to a `TilePusher` instantiated with its private SPI transmitter; every steady-state transmit failure returns false to the pusher and never calls `ESP_ERROR_CHECK`/`abort`; init logs each step's result explicitly and logs a named error before any abort (AC-4.4), including an `esp_ptr_dma_capable` check on the conversion buffer with an explicit log line; every push logs tiles sent and tiles failed/retried plus the measured microseconds for one tile via `esp_timer` (NFR-1 raw data); flashed, and a forced single-tile failure (temporary injected error return) is observed on device leaving that tile stale for one frame and recovering on the next, with the transcript recorded — files: firmware/steamcore/port/esp32/ili9488_display.h, firmware/steamcore/port/esp32/ili9488_display.cpp |
| T7 | Fixture extraction + first-tick on-device proof: 150 tiles, large regions by eye, anchors by log | US-3 | AC-3.1, NFR-9 | T6 | `done` | `drawFixturePattern`/`kAnchors` extracted to `test/fixture_pattern.h` (`steamcore::test` namespace, `inline`, header-only, matching `session_replay_fixture.h`'s established convention); `dump_format_test.cpp` now includes it and its own 5 tests are unchanged in content, re-verified: 124/124 host tests, `test-asan` 124/124, `test-roundtrip` OK (byte-identical fixture, confirming the extraction changed no pixel), `test-png-external` OK. **Verified on real hardware 2026-09-03**: log confirms `fixture push: sent=150 failed=0`; all 12 anchors logged with engine coord, panel coord (exactly engine×2, confirmed e.g. `(239,159)`→`(478,318)`, inside the 480×320 bounds) and 18bpp wire bytes matching `docs/dump-format.md`'s table. Human visual confirmation: both overlap rects (dark/bright orange, correct overlap colour) and the 6×8 "F" glyph block showed their correct colours and shape on the physical panel. | `drawFixturePattern` and the anchor table move from `dump_format_test.cpp`'s anonymous namespace into `test/fixture_pattern.h`, included by both `dump_format_test.cpp` (whose tests stay green, unchanged in content) and the device harness, so exactly one definition exists (§6); the harness's first `tick()` draws that pattern into a fresh `Framebuffer`, pushes it, and logs exactly `150` tiles sent plus, per anchor, its source colour, its mapped panel coordinate and the 18bpp bytes handed to a transaction that returned success (Decision 7); a human confirms on the physical panel that both overlap rects and the 6x8 glyph block show their correct colour, and the transcript plus the human's confirmation are recorded verbatim in the task note — files: firmware/steamcore/test/fixture_pattern.h, firmware/steamcore/test/dump_format_test.cpp, firmware/system/main/app_main.cpp, firmware/system/main/CMakeLists.txt |
| T8 | Live `GameLoop<Consumer>` harness: >= 20 further ticks, moving full-tile marker, unmodified API | US-3 | AC-3.2, AC-3.5, NFR-5 | T7 | `done` | `harness_consumer.h`'s `HarnessConsumer` (throwaway, `steamcore::test`-namespaced, not public) draws the shared fixture on tick 1 then moves one `BRIGHT_ORANGE` full-tile marker per tick along a fixed `step % kTileCols, step / kTileCols` path. `app_main` constructs `GameLoop<HarnessConsumer>(consumer, fb)` with the unmodified signature and calls `pushDirty` from its own loop after `tick()` returns (AC-3.5) — `game_loop.h` untouched, confirmed by `git diff --exit-code` showing no change. **Verified on real hardware 2026-09-03**, 21 ticks: log shows `tick 1: sent=150` (fixture), `tick 2: sent=1` (first marker placement, nothing to restore yet), `tick 3..21: sent=2` each (restore previous tile + draw new marker), 0 failures throughout — every post-first-tick count strictly < 150 as required. Human confirmation: continuous visible motion across multiple tile positions, rest of the fixture pattern stayed stable throughout. | `harness_consumer.h` defines a throwaway consumer whose `update(const GameInput&)` advances a tile-aligned marker by exactly one tile along a fixed path that is a pure function of its own tick counter (no clock, no RNG) and whose `render(Framebuffer&)` redraws the fixture-coloured tile it left and fills its entire current tile solid `BRIGHT_ORANGE`; `app_main` constructs `steamcore::GameLoop<Consumer>` with `game_loop.h`'s unmodified signature and, for at least 21 ticks, calls `tick(GameInput{})` and then `display.pushDirty(fb, tracker)` from the harness loop — never from inside `render()`, never from inside `GameLoop`, and `game_loop.h` is byte-identical to its shipped version; every post-first tick logs a tile count strictly less than 150 matching tiles-left plus tiles-entered; a human watches the run live and confirms the marker occupies at least 5 different tile positions and that unvisited tiles keep their AC-3.1 colour; transcript and confirmation recorded — files: firmware/system/main/harness_consumer.h, firmware/system/main/app_main.cpp, firmware/system/main/CMakeLists.txt |
| T9 | Lint rules: the port boundary, GPIO literals, panel-geometry literals, no full-frame transaction | US-1, US-2, US-3, US-4 | AC-2.2, AC-3.3, AC-3.4, NFR-2, NFR-6 | T8 | `done` | 5 new named blocks in `check_constraints.sh`: (a) no `#include` from `port/` inside `include/`/`src/`; (b) no GPIO literal (9/10/11/12/13/14) outside `board_config.h`, scanning `include/`, `src/`, `port/`, `firmware/system/main` — first automation of constitution §6's GPIO rule; (c) no resolution/tile-size literal in `port/esp32`; (d) no bare `*2`/`2*` scale-factor multiplication in `panel_format.{h,cpp}`/`tile_pusher.h` (AC-2.2) — narrowed from "any bare 2" after a real false positive on `out[index + 2]`'s RGB byte offset; (e) `spi_device_transmit`/`spi_device_polling_transmit` confined to `port/esp32/ili9488_display.cpp` alone (AC-3.3, recorded interpretation: guards the call site, not a transfer-size check grep can't reliably do). Each of the 5 (not 4 — (b) was two DoD bullets in one rule) demonstrated failing once against a temporarily inserted violation, reverted, confirmed gone via grep and `git diff --stat`: `make lint`/`make test` both green afterward (124/124). `docs/host-tests.md` updated. | `check_constraints.sh` gains one named display-driver block reusing the existing shared `SCOPED_ALLOC_PATTERN` and the existing missing-file-is-a-failure posture, enforcing: (a) no `#include` from `port/` anywhere in `include/`/`src/`, and `port/esp32` is the only directory under `firmware/steamcore` allowed to include an ESP-IDF header; (b) no GPIO literal — the first automation of constitution §6's rule — by rejecting any bare 9/10/11/12/13/14 next to a `gpio`/`spi`/`io_num`/`pin` token outside `board_config.h`, over `include/`, `src/`, `port/` and `firmware/system/main/`; (c) no `240`/`160`/`480`/`320`/`16` in `port/` or the display file set, and no bare `2` in `panel_format.{h,cpp}`/`tile_pusher.h` outside `config.h` (AC-2.2); (d) no allocation and no `spi_device_transmit`/`spi_device_polling_transmit` call outside the single `transmitTile` function in `port/esp32/` (AC-3.3); the block states in a comment why the clock/RNG rule deliberately does **not** extend to `port/` (NFR-5: display-output timing is decoupled, and NFR-1 measures it); each of the four rules is demonstrated failing once against a temporarily inserted violation and the observation recorded; `make lint` green on the real tree — files: tools/check_constraints.sh, docs/host-tests.md |
| T10 | Contract documentation and the NFR-7 surface audit | US-1, US-2, US-3, US-4 | NFR-7, NFR-8 | T9 | `done` | All three headers now state, explicitly: single-threaded/nothing-throws/no-error-code, determinism (NFR-5), the retry/commit-only-transferred contract (US-4), init-may-abort-vs-steady-state-never (A6/AC-4.4), display-output timing decoupled from game-logic determinism, and one usage example each in `dirty_tracker.h`'s style. **NFR-7 audit found and corrected one real drift**: `expandTile` is public in `panel_format.h` but was missing from plan §2's own entry-point list — added there with justification (clean, pure, directly tested math a caller could legitimately want without going through `TilePusher`), not hidden behind a `detail` namespace. `docs/host-tests.md` and `docs/device-build.md` both now state the exact AC split (AC-1.1–1.3/AC-2.1–2.3/AC-4.1–4.3 host-CI-verifiable; AC-3.1/AC-3.2/AC-3.5-runtime/AC-4.4/AC-5.1/NFR-1/NFR-9 need the board). `make test` (124/124) and `idf.py build` both re-verified green after the doc-only header edits. | `ili9488_display.h`'s doc comment states, each explicitly: the inherited single-threaded, nothing-throws, no-error-code contract; that only actually-transferred tiles are ever committed and a failed tile stays dirty for the next push; that init failure is the one case that may abort while a steady-state failure never does; that waiting for transfer completion is display-output timing and never feeds the 60 Hz game-logic determinism guarantee; and one usage example in `dirty_tracker.h`'s style showing construct → `init()` → per-tick `tick()` then `pushDirty()`; `panel_format.h` and `tile_pusher.h` carry the same treatment for their own contracts; the header set is audited symbol by symbol against NFR-7 and §2's audit list is corrected in place if it drifted; `docs/host-tests.md` and `docs/device-build.md` state exactly which ACs the host gate covers and which need the board — files: firmware/steamcore/port/esp32/ili9488_display.h, firmware/steamcore/include/steamcore/panel_format.h, firmware/steamcore/include/steamcore/tile_pusher.h, docs/host-tests.md, docs/device-build.md |
| T11 | SPI clock tuned toward 40 MHz, with the per-tile transfer time measured | US-5 | AC-5.1, NFR-1 | T10 | `done` | 10 MHz already extensively verified (T2/T6/T7/T8); stepped 10→20→40 MHz on real hardware, re-running T7's 150-tile frame + T8's 21-tick run at each step. **20 MHz**: avg 2147.9 µs/tile (150-tile push), 0 failures across all 21 ticks, human-confirmed clean (no streaking/corruption). **40 MHz**: avg 1533.4 µs/tile, 0 failures across all 21 ticks, human-confirmed clean — the full constitution §3 budget reached and kept, no step failed. **Result: 40 MHz**, recorded in the driver's own comment (plan.md is the source for `qa.md` to cite once `/demo-day` runs). Final `make test` (124/124), `make lint`, and `idf.py build` all re-verified green with the clock at its final value. | The device clock is raised from the spike's 10 MHz toward 40 MHz in steps (10 → 20 → 40, IOMUX pins per `board_config.h` make >26 MHz reachable), and at each step T7's 150-tile frame and T8's 20+-tick run are re-run and inspected for corruption, streaking or tearing; the highest speed that shows none is kept, the measured single-tile (3,072-byte) transfer time at that speed is recorded, and both the chosen speed and the reason it was chosen (including any step that failed) are written up for `qa.md`; if 40 MHz proves unstable the achieved value is reported honestly as the result, not as a shortfall — files: firmware/steamcore/port/esp32/ili9488_display.cpp, docs/device-build.md |

## 4. Test Strategy

- **Host-CI-verifiable at `/increment` time (`make test`, `test-asan`, `test-gcc`, `lint` — zero ESP-IDF):**
  AC-1.1, AC-1.2, AC-1.3 (T3); AC-2.1, AC-2.3 (T4); AC-2.2, AC-3.4 partly (T9 grep); AC-4.1, AC-4.2, AC-4.3
  (T5, via the fake transmitter); NFR-2, NFR-3, NFR-4, NFR-5, NFR-6. This is the payoff of Decision 3 — the
  retry contract, the subtlest logic in the feature, is proven by fault injection on a Mac.
- **Needs the physical board in hand (human-run `idf.py build flash monitor`, serial log + eyes, A5/C4):**
  AC-3.1, AC-3.2, AC-3.5 in its runtime half, AC-4.4, AC-5.1, NFR-1's measurement, NFR-9's actual log output.
  `make test` cannot and must not pretend to cover these; nothing in this plan reports them as passed until a
  transcript exists (constitution §6, honest status).
- **US-1** — T3, one named test per colour so QA can record AC-1.1 per criterion with `make test FILTER=`. The
  distinctness test is what catches a truncation bug that maps two palette steps onto one wire value.
- **US-2** — T4. Per-tile assertions alone would pass for a mapping that overlaps or leaves gaps, so the
  coverage-array test (every panel pixel claimed exactly once) is the one that actually carries AC-2.1.
- **US-3** — split deliberately. The *mechanism* (expansion bytes, tile count, mask handling) is host-proven in
  T4/T5; only what genuinely needs a panel — that light of the right colour appears in the right place, tick
  after tick — is left to the board (T7, T8). AC-3.5's structural half is checkable by reading the harness and
  by `git diff --exit-code` on `game_loop.h`; its runtime half needs the run.
- **US-4** — T5 is the primary proof and it is negative-first: the fake transmitter must be observed *failing*
  tiles and the dirty state observed surviving, or the suite proves nothing (the false-green class that
  dominated game-loop's review rounds). T6 repeats one injected failure on device once, as confirmation that
  the real transmitter reports failure the same way the fake one does — not as the main proof.
- **US-5** — device-only by nature (T11); a clock speed has no host meaning. Dropped first if the story shrinks.
- **Sanitizers:** `make test-asan` over T4's 150-tile loop and T5's 3,072-byte buffer writes is where an
  off-by-one in `expandTile` actually gets caught; assertions alone would read in-bounds garbage.
- **Deliberately not automated, with reasons:** camera/pixel capture from the panel (§6 — human inspection is
  the accepted method); an ESP-IDF unit-test app (`unity`/`pytest-embedded`) — real, but it would introduce a
  third test framework and a second CI concept for the one thing a human must look at anyway; `/demo-day` in a
  browser — no browser-observable surface (constitution §8, `no`); real GCC — `/usr/bin/g++` is Apple clang, so
  NFR-4's two-compiler claim stays honestly reported as nominal, unchanged from prior increments.

## 5. Risks & Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| Spike-to-production reuse carries over a value that was only correct for a full-frame fill — `max_transfer_sz = 960` is smaller than one 3,072-byte tile and would fail every transfer | High: the driver would fail at exactly the moment it first does its real job, and the failure would look like a wiring problem | Named explicitly in Decision 8 and in T2's DoD; T2 is a walking skeleton that pushes a real tile through the real path before any other device work exists, so this surfaces in task 2, not task 8 |
| The conversion buffer is not in DMA-capable internal RAM (ESP-IDF rejects such a `tx_buffer`), or the object drifts into PSRAM | High: every push fails with `ESP_ERR_INVALID_ARG` and the panel stays blank | No dynamic allocation means the buffer is in `.bss` (internal RAM); T6 additionally logs an explicit `esp_ptr_dma_capable` check at init rather than discovering it as a mystery error |
| `port/esp32/` is production code that **no gate compiles** — it can rot silently between flashes | High and permanent: this is the cost of the boundary, honestly stated | T9's lint rules cover what grep can (allocation, literals, transaction shape, boundary direction); `docs/device-build.md` makes the build reproducible; the code kept there is deliberately minimized by Decision 3 |
| AC-3.2's live-tick timing: the harness ticks faster than a human can follow, or so slowly the run is unwatchable | Medium: the human check either blurs or drags, and a pass/fail is recorded on a bad observation | The harness paces ticks with an explicit `vTaskDelay` in the *harness loop only* (never in `GameLoop`, never feeding logic — NFR-5), tuned in T8 so the marker's motion is trackable by eye; the serial log carries the per-tick truth independently of what the eye caught |
| Two build systems must both learn about a new engine `.cpp` (host glob vs. explicit IDF `SRCS`) | Medium: a future increment's source compiles on host and fails to link on device | Accepted, with a loud failure mode (undefined reference at link, never silent); a comment in `main/CMakeLists.txt` states the rule; the alternative (a globbing IDF component) was rejected as the quieter failure |
| NFR-7 surface reading (Decision 6) — four supporting symbols beyond the three the spec names | Medium: a review finding on shape after the code exists | Declared up front in §1/§2 with each symbol's justification, and flagged in this Handoff for the approval conversation rather than discovered at `/peer-review` |
| AC-3.1's log cannot prove the panel *latched* a pixel, only that a successful transaction carried it (MISO unused) | Medium: an AC could be recorded as passed on weaker evidence than its wording implies | Recorded as an interpretation in Decision 7 and repeated in T7's DoD, so QA records what was actually observed; the large regions still get genuine visual confirmation |
| 40 MHz proves unstable on prototype wiring (long jumpers, no ground plane) | Low: US-5 is a Should and the first thing dropped | T11 steps 10 → 20 → 40 and keeps the highest clean value, recording the failed step as the result rather than as a shortfall |
| Extracting the fixture from `dump_format_test.cpp` changes framebuffer-viewer's committed `.scfb` fixture | Low-Medium: a silently changed committed artifact would break `make test-roundtrip`/`test-png-external` | T7 moves the code without altering a single drawing call and requires `make test-all` green, including the round-trip and `sips` checks against the unchanged committed fixture |

---

## ✅ PLAN GATE

*All boxes checked → `/increment` may start. Any box open → back to `/sprint-plan`.*

- [x] Spec status is `approved` (never plan against a draft)
- [x] Architecture decision includes rejected alternatives (8 recorded, §1)
- [x] Architecture respects the constitution's technical constraints (§3 dirty-tile-only push — structurally unreachable full frame, Decision 4; no dynamic allocation — one fixed member buffer; no GPIO literal — `board_config.h` reused, none added, and T9 automates the rule for the first time; no resolution literal — `config.h` extended by derivation, Decision 5; C++17 on both toolchains — T2 pins `-std=gnu++17` for the IDF build; §4's no-ESP-IDF-header-in-logic rule left absolute, not excepted, Decision 1; `steamcore` namespace, `snake_case`, English) — no conflict found
- [x] Every task maps to a user story — no orphan tasks, no story without tasks
- [x] Every Must AC and every applicable NFR is covered by at least one task (AC-1.1…1.3, AC-2.1…2.3, AC-3.1…3.5, AC-4.1…4.4, AC-5.1; NFR-1…NFR-9; NFR-10/NFR-11 are N/A per the spec)
- [x] Every task has a checkable definition of done
- [x] Task order respects dependencies (walking skeleton first, and deliberately in two halves: T1 host, T2 real hardware — one tile on the physical panel before any AC battery exists, so the integration risk dies in task 2)
- [x] Test strategy covers every Must story, and states per AC whether it is host-CI-verifiable or needs the board
- [x] Line budget respected: Ist 195 / Soll ~300 (excluding HTML comments) — 105 under
- [x] Status set to `approved` by the user
