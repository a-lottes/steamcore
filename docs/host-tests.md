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

## Commands

| Command | What it does |
|---|---|
| `make test` | Builds and runs the full suite. Prints `<passed> passed, <failed> failed` and exits 0 iff everything passed. |
| `make test FILTER=<substring>` | Runs only tests whose name contains `<substring>` (AC-1.4), e.g. `make test FILTER=ac_2_9`. A filter that matches **no** test prints `ERROR: no test matched filter` and exits non-zero — a typo'd or renamed filter can never record a criterion as passed on zero executed tests. |
| `make test-negative` | Builds a binary containing one deliberate failure and asserts the harness reports it correctly (name + `file:line`) and exits non-zero. Also asserts that a zero-match `FILTER` exits non-zero. Proves the harness's failure path actually works, not just its pass path. |
| `make test-asan` | Rebuilds and runs the full suite under `-fsanitize=address,undefined -fno-sanitize-recover=all`. Exits non-zero on any finding. Verified against a real injected out-of-bounds read on 2026-09-01 (aborted with exit code 2); the injected fault was removed afterwards. |
| `make test-gcc` | Runs the full suite with `CXX=g++`. See *Toolchain reality* below for what this does and does not prove on this host. |
| `make bench` | Runs three budgets, each in its own binary, stopping at the first failure: the NFR-1 dirty-scan benchmark (a full scan of a completely changed 240x160 framebuffer, < 5 ms with `-O2`), text-rendering's NFR-1 (drawing a full 30x20/600-character screen of text, also < 5 ms), and game-loop's NFR-1 (two independent `GameLoop` replays of `replay_fixture.h`'s deterministic consumer, `kReplayTicks` ticks each, also < 5 ms — the budget names the `tick` calls only; the determinism-oracle comparison cost is measured and printed on its own non-gating line). Exits non-zero if any is exceeded. |
| `make lint` | Greps `firmware/steamcore/{include,src}` for dynamic allocation (`new`/`*alloc`/`strdup`/`std::vector`/`string`/`map`/`deque`/`list`/`function`/smart pointers), ESP-IDF/FreeRTOS/driver/`hal`/`soc`/`sdkconfig` headers, resolution literals (`240`/`160`/`480`/`320`), the tile-size literal `16` outside `config.h`, and the glyph-metric literals `8`/`43` outside `font.h` (constitution §3/§4/§6, NFR-4). Also checks every `tools/*.py` import against a stdlib-only allowlist. For the game-loop mechanism and its test/fixture files specifically (`game_loop.h`, `game_loop_test.cpp`, `game_loop_determinism_test.cpp`, `fb_compare.h`, `fb_compare_test.cpp`, `replay_fixture.h`), additionally rejects any wall-clock read or unseeded-RNG call (`<chrono>`/`<ctime>`/`std::chrono`/`clock(`/`time(`/`gettimeofday`/`rand(`/`srand(`/`random_device`/`<random>`) — deliberately excluding `bench_game_loop.cpp`, which legitimately uses `<chrono>` the same way the other two benches do — and extends the dynamic-allocation grep to that same file set plus the bench, since the include/src-only grep never reaches `test/` (AC-1.3, AC-4.4, NFR-2). Automates what would otherwise be reviewer diligence. |
| `make test-python` | Discovers and runs **every** `tools/test_*.py` under stdlib `unittest` — currently `test_fb_view.py` *and* `test_roundtrip.py` (15 tests). From `test_fb_view.py`: every decoder rejection path (bad magic, bad version, size mismatch, zero width/height, invalid palette index, missing/directory input, no arguments), determinism, the < 1s decode budget (NFR-1), and silent overwrite. `-B` suppresses `__pycache__` so a stale `.pyc` can never satisfy an import (the Python analogue of the F11 staleness class). |
| `make test-roundtrip` | Runs `tools/test_roundtrip.py`: proves the whole chain (drawn pattern → C++ dump → `fb_view.py` → PNG) against the committed fixture, with a PNG reader and palette table that share no code with `fb_view.py` itself (docs/dump-format.md). Needs no C++ toolchain — reads the already-committed fixture. |
| `make test-png-external` | Decodes the fixture with `fb_view.py`, then asks the pre-installed macOS `sips` to independently confirm the PNG's *declared pixel dimensions* (its IHDR chunk) — an oracle that shares no code with our own PNG reader or writer. `sips` reads IHDR only; it does not decode IDAT, so a pixel-content bug is `test-roundtrip`'s job, not this one's. Prints `SKIPPED` and stays green on non-macOS hosts, never a silent pass. |
| `make view` | One command from a clean checkout to two PNGs: `build/pattern.png` (the rendering-core fixture) and `build/text_pattern.png` (the text-rendering fixture — all 43 defined characters, permuted). Runs the real C++ test suite first (so both fixtures reflect the *current* source tree, not a stale committed one, and the text dump doesn't exist until that run writes it), then decodes both. `VIEWER_PNG`/`TEXT_VIEWER_PNG` are overridable. |
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
