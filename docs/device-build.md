# Device Build

The display driver's SPI half (`firmware/steamcore/port/esp32/`) cannot run
on the host — see constitution §4 and display-driver plan.md §1 Decision 1.
This is how to build, flash and watch it on the real ESP32-S3-N16R8 board.
`docs/host-tests.md` covers everything that *can* run without the board.

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
