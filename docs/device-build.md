# Device Build

`firmware/steamcore/port/esp32/` cannot run on the host — see constitution
§4 and display-driver plan.md §1 Decision 1. This is how to build, flash
and watch it on the real ESP32-S3-N16R8 board. `docs/host-tests.md` covers
everything that *can* run without the board.

`firmware/system/main/app_main.cpp` currently runs the
**highscore-system** device-build-insurance harness (highscore-system
plan.md T14): drives a real
`HighscoreGame<GalacticInvasion, HighscoreStore<NvsHighscoreBackend>>`
through the completely unmodified `GameLoop<Game>`, pushed to the
already-wired ILI9488 panel, with synthetic `fire`/`start` square-wave
pulses (no buttons needed — see `highscore_harness_game.h`'s own doc
comment for why one mechanism serves both "start the first round" and
"restart after a finished TABLE screen"). This is compile insurance only
— see "What's verified this way" below — not a claim that any of this has
been played, or that an entry has survived a power cycle, on hardware.
Flashing this build no longer runs the galactic-invasion harness that
produced v0.7.0's release evidence — that harness is throwaway by
construction and its source is preserved verbatim in git history at the
v0.7.0 tag, and every prior harness (`title_screen_harness_game.h`,
`galactic_invasion_harness_game.h`, and now `highscore_harness_game.h`)
stays on disk (the same posture every prior harness swap took, most
recently galactic-invasion plan.md T16).

## Reading the analog-joystick harness log (superseded, kept for history)

Every tick (20 ms, `kTickDelayMs`), one line reports both axes' raw ADC
counts alongside all four derived direction booleans:

```
I (1234) analog_joystick_harness: axes: rawX=2047 rawY=2047 up=0 down=0 left=0 right=0
```

`rawX`/`rawY` are the last successfully read ADC counts (0–4095, or the
previous value held over if that tick's read failed) — a *wired*, centred
stick reads at or near 2047 with all four booleans 0. Two cases that are
**not** 2047 and are easy to misread as a full deflection: if
`init()` failed, or before the very first successful read, no ADC read
happens at all and both counters stay at their initial `0` while every
boolean still reads 0 (never the `left`/`up` a raw 0 would otherwise
imply) — the `ADC init failed` error line at startup is what tells the two
apart. An ADC pin with nothing connected to it floats and drifts rather
than resting at any particular value. `start`/`fire`/`select` log only on
a debounced change, not every tick:

```
I (2000) analog_joystick_harness: start: pressed
I (2100) analog_joystick_harness: start: released
```

If `AnalogJoystickSource::init()` fails (no ADC available), the harness
logs that once at startup and every direction boolean stays 0 forever —
the three buttons still work independently, per the fail-safe policy
`analog_joystick_source.h`'s own contract documents.

## Prerequisites

ESP-IDF v5.4.4, installed at `~/esp/esp-idf` (see `.spark/constitution.md`
§4). Every command below assumes this has been sourced first, once per
shell:

```
source ~/esp/esp-idf/export.sh
```

## Build, flash, watch

From `firmware/system/`:

```
idf.py set-target esp32s3   # once per fresh build/ directory
idf.py build
idf.py -p /dev/cu.usbmodemXXXXX flash
```

Find the port with `ls /dev/cu.usbmodem*` — it changes across resets, so
re-check it if a flash fails with "port is busy or doesn't exist".

## The manual-RESET quirk

After `idf.py flash` finishes, the app does **not** reliably start on its
own on this board: opening a new connection to the ESP32-S3's native
USB-Serial/JTAG port — from `idf.py monitor`, a fresh `pyserial` session,
anything — triggers a `USB_UART_CHIP_RESET` that lands the chip back in
ROM download mode (`waiting for download`) instead of running the just-
flashed app. This reproduces with the official `esp-idf-monitor` tool
too, so it is not specific to any one script.

**Fix: press the physical RESET/EN button once** (not BOOT) while a
listener is already attached. A hardware EN pulse only touches reset, not
GPIO0/boot-mode selection, and boots straight to the app. Sequence that
works:

```
idf.py -p /dev/cu.usbmodemXXXXX monitor
# now press RESET/EN on the board once
```

If the monitor needs to run non-interactively (no real TTY available),
wrap it in a pseudo-terminal, e.g. `script -q /tmp/out.log idf.py -p
<port> monitor`, run it in the background, press RESET, then read the log
file.

## The silent-stack-overflow trap (engine objects in `app_main`)

Found the hard way during display-driver T6, recorded here so it is not
re-discovered from scratch by the next feature that puts engine objects on
the device.

`CONFIG_ESP_MAIN_TASK_STACK_SIZE` is **3584 bytes** in this project's
`sdkconfig`. A `Framebuffer` alone is 38,400 bytes and a `DirtyTracker`
another 38,400 — declaring either (let alone both plus an
`Ili9488Display`) as a **plain local** in `app_main()` overflows that
stack. The failure is silent: the boot log simply stops right after
`Calling app_main()`, with no crash message, no backtrace and no reset
loop, so it reads like a hang in the very first thing the function does.

**Rule: every engine object held by `app_main` (or by any task with the
default stack) is declared `static`**, which puts it in `.bss` instead —
see `firmware/system/main/app_main.cpp`. This costs nothing here (no
dynamic allocation is allowed anyway, constitution §6) and is what makes
the symptom impossible rather than merely unlikely.

## What's verified this way vs. by `make test`

Per display-driver plan.md §4, exactly:

- **Host-CI-verifiable** (`make test`/`test-asan`/`test-gcc`/`lint`, zero
  ESP-IDF, never touches the board): AC-1.1, AC-1.2, AC-1.3 (pixel
  conversion); AC-2.1, AC-2.3 (tile mapping); AC-2.2, AC-3.4 in part
  (lint greps); AC-4.1, AC-4.2, AC-4.3 (the retry/commit contract, proven
  by fault-injecting a fake `Transmitter` — this is the payoff of routing
  the pure scan/expand/commit loop through `TilePusher` instead of the
  real SPI driver).
- **Needs the physical board** (this document's steps — serial log plus a
  human looking at the physical panel, per constitution §8's declared
  substitute verification method): AC-3.1, AC-3.2, AC-3.5's runtime half,
  AC-4.4, AC-5.1, NFR-1's actual measurement, NFR-9's actual log output.
  `make test` cannot and must not pretend to cover these — nothing is
  reported as passed until a transcript exists.

`input-driver` splits the same way (plan.md §1 Decision 7, T9/T10):

- **Host-CI-verifiable**: AC-1.1–1.4 (per-signal independence, level-not-
  edge, cold start, simultaneous directions), AC-2.1–2.4 (bounce immunity,
  `GameSession` composition, zero-ESP-IDF build), AC-3.1 (pin table's
  no-bare-literal correctness, linted) — see `docs/host-tests.md`.
- **Document-verifiable, not host-CI-verifiable** (no test or lint rule
  reads it — a human or QA confirms by reading `docs/wiring-input.md`
  directly, review F6): AC-3.2 (per-signal topology, four independent
  switches stated), AC-3.3 ("nothing wired yet" stated).
- **Structurally verifiable without hardware** (T9, this document's `idf.py
  build` step above, no human and no wiring needed): AC-4.5 — read
  `firmware/steamcore/port/esp32/gpio_input_source.{h,cpp}` and
  `firmware/system/main/app_main.cpp`/`input_harness_game.h` to confirm
  `GameSession`'s and `GameLoop`'s shipped public APIs are used unmodified.
- **Needs the physical board, and does not exist yet** (`docs/wiring-input.md`
  states nothing is wired — plan.md T10, reported `blocked` until it is):
  AC-4.1, AC-4.2, AC-4.3, AC-4.4 — a human pressing real buttons/joystick
  while reading the serial log this harness produces.
  - **Reading AC-4.3's transition count off the log:** the harness also
    logs a synthetic `PLAYING -> GAME_OVER` on its own timer (every 150
    ticks — `input_harness_game.h`, `kSyntheticGameOverEveryTicks`), since
    no real game exists yet to end a session. That line is always marked
    `(synthetic sessionEnded trigger, not a real press)`; count only the
    unmarked transition lines against the human's counted deliberate START
    presses (review F3).
  - **The window NFR-1 wants recorded:** the harness logs its own
    effective debounce window at startup (`kDebounceSamples * tick`) —
    copy that line into `qa.md` rather than assuming the `input.h` design
    target (review F2).

`start-screen` needs no hardware split at all for its Musts (T11 plan §1
Decision 8):

- **Host-CI-verifiable**: AC-1.1–1.6 (pixel-exact layout, ink colour,
  disjoint bounding boxes, ASan edge safety), AC-2.1–2.3 (disappears on
  `start`, no flicker while held, determinism) — see `docs/host-tests.md`.
- **Needs the physical board**: AC-3.1 (the device-dumped screen matches
  the documented layout) and AC-3.2 (hardware unavailable is recorded, not
  substituted) — captured below. The panel itself is already wired and
  proven since v0.2.0, and this harness needs no buttons (a synthetic
  `start` pulse drives READY → PLAYING after ~3 seconds), so — unlike
  `input-driver`'s T10 — this one is expected to actually run.

`highscore-system` splits the same way (plan.md T14/T15):

- **Host-CI-verifiable** (`make test`/`test-asan`/`test-gcc`/`lint`, zero
  ESP-IDF, never touches the board): every AC this feature owns except
  AC-1.5 — the whole `HighscoreTable`/`HighscoreBlock` encode/decode/
  qualify/insert contract, `InitialsEntry`, both screens' layout, the
  composed `HighscoreGame` wiring against a `FakeFlashBackend`, and the
  end-to-end determinism replay — see `docs/host-tests.md`.
- **Structurally verifiable without hardware** (T14, this document's
  `idf.py build` step above, no human and no wiring needed): AC-1.4 (no
  ESP-IDF header outside `port/`) and NFR-10 — read
  `firmware/steamcore/port/esp32/nvs_highscore_backend.{h,cpp}` and
  `firmware/system/main/app_main.cpp`/`highscore_harness_game.h` to
  confirm `HighscoreStore<Backend>`'s and `GameLoop`'s shipped public APIs
  are used unmodified, and that `NvsHighscoreBackend` satisfies the
  `Backend` concept exactly (`bool read(uint8_t*, int32_t)`/
  `bool write(const uint8_t*, int32_t)`) with no other public surface.
- **Needs the physical board** (plan.md T15, a Should — a `blocked` result
  here does not hold the feature back, CLAUDE.md's hardware-gated-Should
  split): AC-1.5 — a real entry recorded through the real screens must
  survive an actual power cycle. The host's own "reconstruct a second
  `HighscoreStore` over the same backend bytes" proof (T4/T8) already
  covers AC-1.2 (the *persistence format* round-trips); it does not and
  cannot stand in for AC-1.5 (the *real NVS partition* round-trips across
  an actual power-off), which needs the board.

## Capturing an SCFB dump from the serial console (start-screen T11)

The start-screen harness prints each framebuffer once per `GameSession`
state change (plus the initial READY render) as an SCFB dump
(`docs/dump-format.md`), hex-encoded between sentinel markers, so it shares
the same UART as ordinary `ESP_LOGI` lines without those lines' timestamp/
tag prefix corrupting the hex payload:

```
SCFB-DUMP-BEGIN
<hex line 1 (64 hex characters = 32 bytes)>
<hex line 2>
...
SCFB-DUMP-END
```

1. Flash and attach a monitor per the sections above, and capture its
   output to a file — e.g. `script -q /tmp/title_screen.log idf.py -p
   <port> monitor` (see the manual-RESET quirk above; press RESET/EN once
   the listener is attached).
2. Let it run past at least one state change (~3 seconds at the
   `kSyntheticStartAtTick`/`kTickDelayMs` values `title_screen_harness_game.h`
   and `app_main.cpp` document) so both the READY and PLAYING screens are
   captured, then stop the capture.
3. Decode **both** dumps from the one captured transcript — `--which
   first` for the earliest block (READY), the default `--which last` for
   the most recent (PLAYING) (review F2: earlier revisions of this doc
   always decoded the last block, which is the wrong one for confirming
   the READY screen this step needs):

   ```
   python3 tools/scfb_capture.py /tmp/title_screen.log /tmp/title_screen_ready.scfb --which first
   python3 tools/scfb_capture.py /tmp/title_screen.log /tmp/title_screen_playing.scfb
   ```

   `scfb_capture.py` extracts every complete `BEGIN`/`END` block, rejects
   any whose byte count disagrees with its own header (a truncated capture)
   rather than decoding it partially, and prints how many valid blocks it
   found either way.
4. View both exactly like any other dump:

   ```
   python3 tools/fb_view.py /tmp/title_screen_ready.scfb /tmp/title_screen_ready.png
   python3 tools/fb_view.py /tmp/title_screen_playing.scfb /tmp/title_screen_playing.png
   ```
5. Confirm the READY image shows `STEAMCORE` at `(84, 48)` and
   `PRESS START` at `(76, 112)` with the rest black, and the PLAYING image
   is fully black (AC-3.1) — and that the physical panel shows the same
   two screens and blanks when the synthetic `start` pulse fires.

## galactic-invasion (T16): compile insurance, not a hardware claim

Per CLAUDE.md's "a host-only module can still hide a device-build bug" —
`font.cpp` passed every host gate for two whole features before anything on
the device side first `#include`d it, and that first inclusion immediately
surfaced two ESP-IDF-only incompatibilities (`throw` under
`-fno-exceptions`, a `size_t` relying on a transitive `<cstddef>`) neither
host compiler could ever have caught. T16 exists purely to close that same
gap for this feature's own `constexpr` sprite-art validator
(`games/galactic_invasion/galactic_invasion_art.h`) and its `collision.h`
usage, before some future feature discovers an incompatibility there as an
unplanned blocker.

- `firmware/system/main/CMakeLists.txt` gains
  `games/galactic_invasion/galactic_invasion.cpp` in `SRCS` and
  `../../../games` in `INCLUDE_DIRS` (mirroring the host Makefile's own
  `-I$(GAMES_DIR)`, so `#include "galactic_invasion/galactic_invasion.h"`
  resolves identically on both toolchains).
- `galactic_invasion_harness_game.h` wraps a real `GalacticInvasion`,
  supplying a synthetic one-tick `start` pulse (`kSyntheticStartAtTick`,
  same pattern as `title_screen_harness_game.h`) since `GameSession`
  requires a rising edge, not a held level — no physical button is wired to
  this harness.
- **Verified this way:** `idf.py build` completes with no warnings and no
  errors, which is this task's entire point — it is the first time this
  feature's code is compiled by the ESP-IDF toolchain rather than only host
  clang/g++.
- **Explicitly not verified this way, and not claimed:** anything about how
  the game actually plays, looks, or performs on the real board. This build
  was not flashed and the harness was not run on hardware — no AC depends
  on it (constitution §4 honest-status rule). `make test`/`test-asan`/
  `test-gcc`/`make bench` (`docs/host-tests.md`) remain the source of every
  correctness and performance claim this feature makes.
