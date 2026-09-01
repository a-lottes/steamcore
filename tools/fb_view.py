#!/usr/bin/env python3
"""Decodes an SCFB dump (docs/dump-format.md) into a PNG image.

Standard library only -- no pip install, no Pillow (constitution NFR-4,
spec A5/C8). The PNG encoder below hand-builds the smallest legal PNG:
colour type 2 (truecolour RGB), 8-bit depth, filter 0 on every scanline,
one IDAT chunk, no ancillary chunks.
"""

import argparse
import os
import struct
import sys
import tempfile
import zlib

MAGIC = b"SCFB"
HEADER_SIZE = 10
SUPPORTED_VERSION = 1

# docs/dump-format.md "Payload" -- index -> (R, G, B).
PALETTE = {
    0: (0x00, 0x00, 0x00),  # BLACK
    1: (0x4D, 0x26, 0x00),  # DARK_ORANGE
    2: (0xB3, 0x59, 0x00),  # ORANGE
    3: (0xFF, 0x99, 0x33),  # BRIGHT_ORANGE
}

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


class DumpError(Exception):
    """A dump file is malformed. The message is shown to the user."""


def read_dump(path):
    with open(path, "rb") as f:
        data = f.read()

    if len(data) < HEADER_SIZE:
        raise DumpError(f"file too short to contain an SCFB header: {path}")

    magic = data[0:4]
    if magic != MAGIC:
        raise DumpError(f"bad magic {magic!r}, expected {MAGIC!r}: {path}")

    version = struct.unpack_from("<H", data, 4)[0]
    if version != SUPPORTED_VERSION:
        raise DumpError(
            f"unsupported version {version}, this tool only reads "
            f"version {SUPPORTED_VERSION}: {path}"
        )

    width = struct.unpack_from("<H", data, 6)[0]
    height = struct.unpack_from("<H", data, 8)[0]

    # A PNG must have positive dimensions (the PNG spec forbids 0 in
    # IHDR), and a real Framebuffer dump is never 0x0 either -- a
    # declared width or height of 0 is a malformed file, not an edge
    # case to render as an empty image (review F2).
    if width == 0 or height == 0:
        raise DumpError(f"declared dimensions {width}x{height} are not valid "
                         f"(width and height must both be >= 1): {path}")

    payload = data[HEADER_SIZE:]
    if len(payload) != width * height:
        raise DumpError(
            f"declared {width}x{height} needs {width * height} payload "
            f"bytes, file has {len(payload)}: {path}"
        )

    for offset, index in enumerate(payload):
        if index not in PALETTE:
            raise DumpError(
                f"invalid palette index {index} at payload offset "
                f"{offset} (valid range is 0-{len(PALETTE) - 1}): {path}"
            )

    return width, height, payload


def payload_to_rgb_rows(width, height, payload):
    rows = []
    for y in range(height):
        row = bytearray()
        for x in range(width):
            index = payload[y * width + x]
            r, g, b = PALETTE[index]
            row += bytes((r, g, b))
        rows.append(bytes(row))
    return rows


def _chunk(chunk_type, data):
    return (
        struct.pack(">I", len(data))
        + chunk_type
        + data
        + struct.pack(">I", zlib.crc32(chunk_type + data) & 0xFFFFFFFF)
    )


def encode_png(width, height, rgb_rows):
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)

    raw = bytearray()
    for row in rgb_rows:
        raw.append(0)  # filter type 0 (None) on every scanline
        raw += row

    idat = zlib.compress(bytes(raw), 9)

    return (
        PNG_SIGNATURE
        + _chunk(b"IHDR", ihdr)
        + _chunk(b"IDAT", idat)
        + _chunk(b"IEND", b"")
    )


def decode_to_png(dump_path, output_path):
    width, height, payload = read_dump(dump_path)
    rows = payload_to_rgb_rows(width, height, payload)
    png_bytes = encode_png(width, height, rows)

    out_dir = os.path.dirname(os.path.abspath(output_path)) or "."
    try:
        fd, tmp_path = tempfile.mkstemp(dir=out_dir, prefix=".fb_view_", suffix=".tmp")
    except OSError as exc:
        # mkstemp's own failure never created a temp path worth naming;
        # report the user's actual output path instead (review F4).
        raise OSError(f"cannot create output at {output_path}: {exc.strerror}") from exc

    try:
        try:
            with os.fdopen(fd, "wb") as f:
                f.write(png_bytes)
            os.replace(tmp_path, output_path)
        except OSError as exc:
            # Report the user-facing output path, not the internal temp
            # file name, which means nothing to whoever reads stderr.
            raise OSError(f"cannot write output to {output_path}: {exc.strerror}") from exc
    except BaseException:
        if os.path.exists(tmp_path):
            os.remove(tmp_path)
        raise


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dump", help="path to an .scfb dump file")
    parser.add_argument("output", help="path to write the decoded .png to")
    args = parser.parse_args(argv)

    try:
        decode_to_png(args.dump, args.output)
    except (DumpError, OSError) as exc:
        print(f"fb_view: {exc}", file=sys.stderr)
        return 1

    print(args.output)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
