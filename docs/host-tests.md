# Host Tests

SteamCore's rendering core has no device dependency: every acceptance
criterion in `.spark/rendering-core/spec.md` is provable on this machine
with a host C++ compiler alone — no ESP-IDF, no cmake, no ninja, no board.
See constitution §4 for why this is the project's only enforceable gate
today, and §8 for how QA records the device-side half that this gate does
not cover.

The `framebuffer-viewer` story (see `docs/dump-format.md`) adds a second,
Python-only gate for the same reason: this project has no wired display,
so `make view` is currently the only way to actually *see* what the
engine renders.

`display-driver` is the first story with a genuine split: its pixel
conversion, tile mapping and retry/commit contract (AC-1.1–1.3, AC-2.1–
2.3, AC-4.1–4.3) are host-CI-verified below like everything else, but the
actual SPI transmission structurally cannot be (constitution §4 — it
needs `driver/spi_master.h`). `docs/device-build.md` states exactly which
ACs still need the physical board and how to run them.

`input-driver` splits the same way, in the other direction: the debounce
core, its seven-signal independence and bounce immunity, its composition
with the unmodified `GameSession`, and the pin table's own no-bare-literal
correctness (AC-1.1–1.4, AC-2.1–2.4, AC-3.1) are all host-CI-verified below
with zero ESP-IDF. AC-3.2 and AC-3.3 are **document-verified, not
host-CI-verified**: nothing in `make test`/`lint` reads `docs/wiring-input.md`
— confirming its wording states the right topology and status is a human
(or QA) reading it directly (review F6). AC-4.5 (the real `GpioInputSource`
+ on-device harness using `GameSession`/`GameLoop`'s shipped APIs
unmodified) is verified structurally — by building `firmware/system/` and
reading the harness source — and needs no human at the controls. Only
AC-4.1–4.4, which need a person physically pressing wired buttons, are
unreachable from this
gate; `docs/wiring-input.md` states nothing is wired yet, and `qa.md`
records those four as `blocked` until it is.

`start-screen` is fully host-CI-verifiable for its Musts, with no split at
all: `drawTitleScreen` composes only the shipped `drawText`, so AC-1.1–1.6
(pixel-exact layout, ink colour, disjoint bounding boxes, ASan edge safety)
and AC-2.1–2.3 (disappears on `start`, no flicker while held, determinism)
are all proven below with zero ESP-IDF, the same way `rendering-core`/
`text-rendering`/`game-loop` were. AC-3.1 (the device-dumped screen matches
the documented layout) and AC-3.2 (hardware unavailable is recorded, not
substituted) are the only device-only ACs — see `docs/device-build.md`.

## Commands

| Command | What it does |
|---|---|
| `make test` | Builds and runs the full suite. Prints `<passed> passed, <failed> failed` and exits 0 iff everything passed. |
| `make test FILTER=<substring>` | Runs only tests whose name contains `<substring>` (AC-1.4), e.g. `make test FILTER=ac_2_9`. A filter that matches **no** test prints `ERROR: no test matched filter` and exits non-zero — a typo'd or renamed filter can never record a criterion as passed on zero executed tests. |
| `make test-negative` | Builds a binary containing one deliberate failure and asserts the harness reports it correctly (name + `file:line`) and exits non-zero. Also asserts that a zero-match `FILTER` exits non-zero. Proves the harness's failure path actually works, not just its pass path. |
| `make test-asan` | Rebuilds and runs the full suite under `-fsanitize=address,undefined -fno-sanitize-recover=all`. Exits non-zero on any finding. Verified against a real injected out-of-bounds read on 2026-09-01 (aborted with exit code 2); the injected fault was removed afterwards. |
| `make test-gcc` | Runs the full suite with `CXX=g++`. See *Toolchain reality* below for what this does and does not prove on this host. |
| `make bench` | Runs five budgets, each in its own binary, stopping at the first failure: the NFR-1 dirty-scan benchmark (a full scan of a completely changed 240x160 framebuffer, < 5 ms with `-O2`), text-rendering's NFR-1 (drawing a full 30x20/600-character screen of text, also < 5 ms), game-loop's NFR-1 (two independent `GameLoop` replays of `replay_fixture.h`'s deterministic consumer, `kReplayTicks` ticks each, also < 5 ms — the budget names the `tick` calls only; the determinism-oracle comparison cost is measured and printed on its own non-gating line), start-screen's NFR-1 (`drawTitleScreen` in `READY` averaged over 10,000 iterations, reported as µs/call against the 16.67 ms 60 Hz tick budget rather than the 5 ms full-screen figure — a two-line draw is far too cheap for a single measured call to clear timer noise), and collision-system's NFR-1/NFR-12 (`overlaps()` over 8,000,000 pairs and `sweepCollisions()` over 400,000 passes across a 32-entity array, both against 10% of the 16.67 ms 60 Hz tick; every input comes from a fixed-seed arithmetic generator read through a `volatile` seed and every accumulation goes through a `volatile` sink, since `overlaps()` is pure and `constexpr` and `-O2` would otherwise delete or hoist the measurement entirely — each figure is checked to roughly double when the iteration count doubles, the proof that the loop actually ran). Exits non-zero if any is exceeded. |
| `make lint` | Greps `firmware/steamcore/{include,src}` for dynamic allocation (`new`/`*alloc`/`strdup`/`std::vector`/`string`/`map`/`deque`/`list`/`function`/smart pointers), ESP-IDF/FreeRTOS/driver/`hal`/`soc`/`sdkconfig` headers, resolution literals (`240`/`160`/`480`/`320`), the tile-size literal `16` outside `config.h`, and the glyph-metric literals `8`/`43` outside `font.h` (constitution §3/§4/§6, NFR-4). Also checks every `tools/*.py` import against a stdlib-only allowlist. A shared clock/RNG token pattern (`<chrono>`/`<ctime>`/`<time.h>`/`<sys/time.h>`/`std::chrono`/`steady_clock`/`system_clock`/`high_resolution_clock`/`clock(`/`clock_gettime(`/`time(`/`gettimeofday`/`rand(`/`srand(`/`random_device`/`<random>`) and a shared allocation pattern are each applied to two feature-scoped file sets so the token lists cannot drift apart between them: the game-loop mechanism and its test/fixture files (`game_loop.h`, `game_loop_test.cpp`, `game_loop_determinism_test.cpp`, `fb_compare.h`, `fb_compare_test.cpp`, `replay_fixture.h`, plus `bench_game_loop.cpp` for the allocation check only — the bench legitimately uses `<chrono>`), and the game-state mechanism and its test/fixture files (`game_state.h`, `game_state.cpp`, `game_state_test.cpp`, `game_state_determinism_test.cpp`, `session_replay_fixture.h`), since the include/src-only grep never reaches `test/` (AC-1.3/AC-4.4 and AC-5.3, NFR-2). Also rejects a `GameState` enumerator given an explicit value, or any `static_cast<GameState>`, in `game_state.h` — a state is named, never numbered, and reached only through `GameSession::advance` (NFR-4). display-driver adds five more checks, the first that reach outside `include/`/`src/` into `firmware/steamcore/port/esp32/` and `firmware/system/main/` (the ESP-IDF-dependent half the host gate structurally cannot compile, spec A2): no `#include` from `port/` inside `include/`/`src/`; no GPIO pin literal (this board's actual pins) near a gpio/spi/io_num/pin token outside `board_config.h` — the first automated check for constitution §6's GPIO rule, every prior increment left it to reviewer diligence; no resolution/tile-size literal in `port/esp32`; no bare `*2`/`2*` scale-factor multiplication in `panel_format.{h,cpp}`/`tile_pusher.h` (AC-2.2); and `spi_device_transmit`/`spi_device_polling_transmit` confined to the single file that owns hardware transmission, `port/esp32/ili9488_display.cpp` — the structural guard against a full-frame push (AC-3.3) is `TilePusher`'s one-tile-at-a-time `Transmitter` concept itself; this lint rule guards against a second, ad-hoc SPI call site reappearing elsewhere, the shape the deleted bring-up spike's own scanline-fill function had. input-driver (T7) extends the GPIO-literal digit set from 9/10/11/12/13/14 (display) to also cover 4/5/6/7/15/17/18 (input, `board_config.h`), and adds three more checks: the shared clock/RNG and allocation patterns applied to the input mechanism and its test/fixture files (`input.h`, `input_test.cpp`, `input_session_test.cpp`, `fake_input_source.h`, and — once T9 adds them — `port/esp32/gpio_input_source.{h,cpp}`) — the one mechanism-lint block that reaches into `port/`, unlike display-driver's SPI-wait exemption, because `GpioInputSource`'s output feeds game logic directly (NFR-3); and a presence check that `game_loop.h`'s `static_assert(sizeof(GameInput) == 7 * sizeof(bool))` size guard has not been silently deleted (NFR-8). start-screen adds five more checks over `title_screen.h`/`title_screen.cpp` and their test/fixture files: the shared clock/RNG and allocation patterns (AC-2.3, NFR-2); no `assets/`, `.png` or `.ttf` reference in the two source files, automating "no decoded PNG/TTF byte" (AC-1.4); no `setPixel`/`fillRect`/`blit` call in `title_screen.cpp`, so AC-1.3's "composes only `drawText`" is structural rather than reviewed; and a presence check (anchored on its own message text, not just "a `static_assert` exists") that the `kTitleWordmarkBounds`/`kTitlePromptBounds` disjointness guard has not been silently deleted (AC-1.6). collision-system adds four more checks over `collision.h` and its test/fixture files (`collision_test.cpp`, `collision_overflow_test.cpp`, `collision_dispatch_test.cpp`, `collision_sweep_test.cpp`): the shared clock/RNG pattern (NFR-5) — deliberately **not** applied to `bench_collision.cpp`, the same exemption `bench_game_loop.cpp` already has, since measuring elapsed time with `<chrono>` is that file's entire legitimate purpose; the shared allocation pattern, which **is** applied to `bench_collision.cpp` too (NFR-2) and, because the pattern already matches `std::function`, doubles as the automated half of AC-2.4's dispatch-mechanism ban; and two presence-only checks (correctness is T3/T4's job, not lint's) that `static_assert(sizeof(Entity) == 4 * sizeof(int32_t))` and the `int64_t` widening have not been silently deleted (R4/R6). analog-joystick-input adds four more checks: the shared clock/RNG and allocation patterns over `analog_axis.h`, `analog_axis_test.cpp`, `fake_analog_source.h` and both `analog_joystick_source.{h,cpp}` port files (NFR-2/NFR-3) — extends the global GPIO-literal digit set (already 4/5/6/7/9/10/11/12/13/14/15/17/18 — never `16`, which no scheme uses) with `8`/`21`/`47`, this feature's three digital pins, re-verified against the unmodified tree for zero false positives before landing; a feature-scoped block additionally bans `1`/`2`/`8`/`21`/`47` near a gpio/spi/pin token in `analog_axis.h` and the port files specifically, since `1`/`2` (VRX/VRY's own pins) cannot join the global digit set without flooding it with false positives on ordinary small integers — the residual gap (any other file naming GPIO1/2 near a pin token) is recorded in the script rather than left for a reader to assume full coverage; and a ban on `ADC_CHANNEL_[0-9]`/`ADC_UNIT_[0-9]` anywhere under `port/` or `firmware/system/main/`, making "the ADC channel is derived via `adc_oneshot_io_to_channel`, never written down" structural rather than a convention — this rule caught a genuine violation during development (a placeholder member-default of `ADC_CHANNEL_0`, fixed to value-initialization). Automates what would otherwise be reviewer diligence. |
| `make test-python` | Discovers and runs **every** `tools/test_*.py` under stdlib `unittest` — currently `test_fb_view.py`, `test_roundtrip.py` *and* `test_scfb_capture.py` (31 tests). From `test_scfb_capture.py` (start-screen T11): marker extraction against surrounding log noise, a `BEGIN` with no matching `END`, a second `BEGIN` discarding the open incomplete block, non-hex noise interleaved into a block, multiple blocks in one transcript, every `validate_and_decode` rejection path (short/long byte count, wrong magic, invalid hex, zero width), and `main()` itself — argv arity, a missing log file, the no-valid-block exit-1 path, and both `--which first`/`--which last` selections. From `test_fb_view.py`: every decoder rejection path (bad magic, bad version, size mismatch, zero width/height, invalid palette index, missing/directory input, no arguments), determinism, the < 1s decode budget (NFR-1), and silent overwrite. `-B` suppresses `__pycache__` so a stale `.pyc` can never satisfy an import (the Python analogue of the F11 staleness class). |
| `make test-roundtrip` | Runs `tools/test_roundtrip.py`: proves the whole chain (drawn pattern → C++ dump → `fb_view.py` → PNG) against the committed fixture, with a PNG reader and palette table that share no code with `fb_view.py` itself (docs/dump-format.md). Needs no C++ toolchain — reads the already-committed fixture. |
| `make test-png-external` | Decodes the fixture with `fb_view.py`, then asks the pre-installed macOS `sips` to independently confirm the PNG's *declared pixel dimensions* (its IHDR chunk) — an oracle that shares no code with our own PNG reader or writer. `sips` reads IHDR only; it does not decode IDAT, so a pixel-content bug is `test-roundtrip`'s job, not this one's. Prints `SKIPPED` and stays green on non-macOS hosts, never a silent pass. |
| `make view` | One command from a clean checkout to three PNGs: `build/pattern.png` (the rendering-core fixture), `build/text_pattern.png` (the text-rendering fixture — all 43 defined characters, permuted) and `build/title_screen.png` (start-screen's `READY` title screen). Runs the real C++ test suite first (so every fixture reflects the *current* source tree, not a stale committed one, and neither the text nor the title dump exists until that run writes it), then decodes all three. `VIEWER_PNG`/`TEXT_VIEWER_PNG`/`TITLE_VIEWER_PNG` are overridable. |
| `make test-all` | Chains `test`, `test-negative`, `test-asan`, `test-gcc`, `bench`, `test-python`, `test-roundtrip`, `test-png-external`, `lint` in that order; stops at the first failure. |
| `make clean` | Removes `build/`. |

## Benchmark result (NFR-1)

Measured 2026-09-01 on the reference host below: **~0.0015 ms** per full
150-tile dirty scan, three consecutive runs. Budget is < 5 ms — over
3000x headroom, so no further optimisation is warranted for this
increment.

- **Host:** macOS 13.7.8, Intel Core i5-7360U @ 2.30GHz, Apple clang
  14.0.3, `-O2`.
- **Target-device (ESP32-S3) timing is not measured and is not claimed.**
  The Xtensa core, its cache behaviour and clock speed are unrelated to
  this host; a device measurement is out of scope for this increment
  (constitution §4 honest-status rule).

### game-loop (NFR-1)

Measured 2026-09-02 on the same reference host, same flags: **~0.0009 ms**
for `kReplayTicks` (53) ticks x 2 independent `GameLoop::tick()` runs —
the budget names the `tick` calls only (review F4), and this measurement
is comfortably inside the 5 ms budget. The separately-printed, non-gating
`framebuffersEqual` comparison cost (one full 240x160-buffer scan) was
**~0.25 ms per call** on the same run — reported for context only, not
part of the budget: AC-4.1's determinism test performs `kReplayTicks`
such comparisons, one per tick, which at this per-call cost totals well
outside 5 ms and was never meant to fit it (review F12). Target-device
timing is not measured and is not claimed, same posture as the dirty-scan
result above.

`CXX` defaults to `clang++` and is overridable: `make test CXX=g++`.

## Toolchain reality (2026-09-01)

A host C++ toolchain is present — Apple clang 14.0.3, `/usr/bin/g++`,
`/usr/bin/make`. **`/usr/bin/g++` on this machine is Apple clang**, not a
real GNU GCC: `make test-gcc` runs and must pass, but the two-compiler
portability claim (constitution NFR-4 equivalent) stays **unverified**
until a genuine GCC — or the ESP-IDF xtensa toolchain — is available.
Nothing here is reported as verified beyond what actually ran.
