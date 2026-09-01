# Host Tests

SteamCore's rendering core has no device dependency: every acceptance
criterion in `.spark/rendering-core/spec.md` is provable on this machine
with a host C++ compiler alone — no ESP-IDF, no cmake, no ninja, no board.
See constitution §4 for why this is the project's only enforceable gate
today, and §8 for how QA records the device-side half that this gate does
not cover.

## Commands

| Command | What it does |
|---|---|
| `make test` | Builds and runs the full suite. Prints `<passed> passed, <failed> failed` and exits 0 iff everything passed. |
| `make test FILTER=<substring>` | Runs only tests whose name contains `<substring>` (AC-1.4), e.g. `make test FILTER=ac_2_9`. A filter that matches **no** test prints `ERROR: no test matched filter` and exits non-zero — a typo'd or renamed filter can never record a criterion as passed on zero executed tests. |
| `make test-negative` | Builds a binary containing one deliberate failure and asserts the harness reports it correctly (name + `file:line`) and exits non-zero. Also asserts that a zero-match `FILTER` exits non-zero. Proves the harness's failure path actually works, not just its pass path. |
| `make test-asan` | Rebuilds and runs the full suite under `-fsanitize=address,undefined -fno-sanitize-recover=all`. Exits non-zero on any finding. Verified against a real injected out-of-bounds read on 2026-09-01 (aborted with exit code 2); the injected fault was removed afterwards. |
| `make test-gcc` | Runs the full suite with `CXX=g++`. See *Toolchain reality* below for what this does and does not prove on this host. |
| `make bench` | Runs the NFR-1 dirty-scan benchmark: a full scan of a completely changed 240x160 framebuffer must complete in < 5 ms with `-O2`. Exits non-zero if it doesn't. |
| `make lint` | Greps `firmware/steamcore/{include,src}` for dynamic allocation (`new`/`*alloc`/`strdup`/`std::vector`/`string`/`map`/`deque`/`list`/`function`/smart pointers), ESP-IDF/FreeRTOS/driver/`hal`/`soc`/`sdkconfig` headers, resolution literals (`240`/`160`/`480`/`320`) and the tile-size literal `16` outside `config.h` (constitution §3/§4/§6). Automates what would otherwise be reviewer diligence. |
| `make test-all` | Chains `test`, `test-negative`, `test-asan`, `test-gcc`, `bench`, `lint` in that order; stops at the first failure. |
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

`CXX` defaults to `clang++` and is overridable: `make test CXX=g++`.

## Toolchain reality (2026-09-01)

A host C++ toolchain is present — Apple clang 14.0.3, `/usr/bin/g++`,
`/usr/bin/make`. **`/usr/bin/g++` on this machine is Apple clang**, not a
real GNU GCC: `make test-gcc` runs and must pass, but the two-compiler
portability claim (constitution NFR-4 equivalent) stays **unverified**
until a genuine GCC — or the ESP-IDF xtensa toolchain — is available.
Nothing here is reported as verified beyond what actually ran.
