# SCFB Dump Format (v1)

`SCFB` ("SteamCore FrameBuffer") is a small file format for dumping the
exact contents of a `steamcore::Framebuffer` to bytes, so they can be
decoded off-device into a viewable image. It exists because rendering-core
proved the engine's logic entirely through unit-test assertions — nobody
has ever looked at a frame it produced. See constitution §4/§8.

**Non-goal:** this document defines a *file* format only. It says nothing
about how those bytes might one day travel over a wire (USB-CDC framing,
sync bytes, a checksum, a baud rate). That is explicitly out of scope
here and belongs to a future device-side story, once ESP-IDF and a wired
panel exist. A framing layer can wrap this format later without changing
it — see the version field below.

## Byte layout

All multi-byte fields are **little-endian**. This is true of both hosts
this format is built to move between today (this Mac) and the eventual
target (ESP32-S3, Xtensa LX7) — but it is stated explicitly rather than
left as a coincidence a reader has to discover.

| Offset | Size (bytes) | Field | Value |
|---|---|---|---|
| 0 | 4 | magic | ASCII `SCFB` = `0x53 0x43 0x46 0x42` |
| 4 | 2 | version | `uint16`, currently `1` |
| 6 | 2 | width | `uint16`, pixels, must be `>= 1` |
| 8 | 2 | height | `uint16`, pixels, must be `>= 1` |
| 10 | width × height | payload | row-major, 1 byte per pixel |

Total file size is exactly `10 + width * height` bytes. There is no
separate length field — the payload's length is `width * height`; a
second, independently-stated length could disagree with the first and
there would be no way to say which one is wrong. There are no reserved or
padding bytes anywhere in the header: it must be written and read
**field by field** (four separate byte-order-aware writes/reads), never
as a single `memcpy` of a C struct — a compiler is free to insert padding
between struct members, and that padding is not part of this format.

## Payload

Each payload byte is one pixel, in row-major order (row 0 first, each row
left-to-right), and must be a valid `steamcore::Color` index:

| Index | Name | Display colour |
|---|---|---|
| `0` | `BLACK` | `#000000` |
| `1` | `DARK_ORANGE` | `#4D2600` |
| `2` | `ORANGE` | `#B35900` |
| `3` | `BRIGHT_ORANGE` | `#FF9933` |

A payload byte outside `0`–`3` is invalid. A decoder must reject the file
rather than clamp, mask, or guess.

## Reading this format

A conforming reader:

1. Reads the file's actual byte size first.
2. Verifies the first 4 bytes equal the magic `SCFB`.
3. Reads `version` (offset 4) and rejects any value it does not
   implement (currently only `1`).
4. Reads `width` and `height` (offsets 6 and 8) and rejects the file if
   either is `0` — a viewable image needs positive dimensions, and a real
   `Framebuffer` dump is never `0x0` either.
5. Verifies the file's actual size (step 1) equals
   `10 + width * height` — **before allocating any buffer sized from the
   declared dimensions**. A corrupted or hostile header claiming an
   enormous frame must not be allowed to force a large allocation from a
   small file.
6. Reads the payload and verifies every byte is in `0`–`3`.

Any of these checks failing means the file is rejected outright — no
partial decode, no best-effort output.

## Fixture path

The one committed, engine-produced example of this format lives at the
path named by the `FIXTURE_DUMP` variable in the repository's root
`Makefile`. Nothing outside the `Makefile` hard-codes that path a second
time; both the C++ and Python test suites receive it from there
(`-DSTEAMCORE_FIXTURE_DUMP` for C++, the `STEAMCORE_FIXTURE_DUMP`
environment variable for Python).

## Anchor table

A small set of absolute `(x, y) → colour` truths about the fixture
pattern, asserted independently by both the C++ writer test
(`firmware/steamcore/test/dump_format_test.cpp`) and the Python
round-trip test (`tools/test_roundtrip.py`). Their purpose is to catch a
bug class no amount of comparing-the-two-sides-to-each-other can: an
identical offset, axis swap, or transposition error made independently
on both sides.

The pattern itself (drawn against a cleared `BLACK` background, 240×160):
two unequal, overlapping filled rects (`DARK_ORANGE` then `ORANGE` on
top), an asymmetric blitted "F" glyph (`BRIGHT_ORANGE`), a one-pixel
vertical line spanning only the top third of the screen (`DARK_ORANGE`),
and four differently-coloured corner pixels (the fourth corner is left as
untouched background `BLACK`, not a fifth colour).

| x | y | Colour | Why this point |
|---|---|---|---|
| 5 | 5 | `BLACK` | untouched background |
| 25 | 25 | `DARK_ORANGE` | inside rect A only |
| 60 | 50 | `ORANGE` | rect A/B overlap — B drawn last, wins |
| 85 | 90 | `ORANGE` | inside rect B only |
| 150 | 20 | `BRIGHT_ORANGE` | glyph top-left, an "on" pixel |
| 151 | 21 | `BLACK` | glyph interior, a transparent hole |
| 200 | 10 | `DARK_ORANGE` | on the vertical line |
| 200 | 100 | `BLACK` | below the line's span (line stops at y=53) |
| 0 | 0 | `DARK_ORANGE` | corner: top-left |
| 239 | 0 | `ORANGE` | corner: top-right |
| 0 | 159 | `BRIGHT_ORANGE` | corner: bottom-left |
| 239 | 159 | `BLACK` | corner: bottom-right (untouched) |
