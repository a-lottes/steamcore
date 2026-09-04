# Device Build

`firmware/steamcore/port/esp32/` cannot run on the host — see constitution
§4 and display-driver plan.md §1 Decision 1. This is how to build, flash
and watch it on the real ESP32-S3-N16R8 board. `docs/host-tests.md` covers
everything that *can* run without the board.

`firmware/system/main/app_main.cpp` currently runs the **input-driver**
harness (`InputReader<GpioInputSource>` + a real `GameSession`, log-only —
plan.md T9): flashing this build no longer runs the display-driver harness
that produced v0.2.0's release evidence — that harness is throwaway by
construction and its source is preserved verbatim in git history at the
v0.2.0 tag (input-driver plan.md §5, accepted deliberately).

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
