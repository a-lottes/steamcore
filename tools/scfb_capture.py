#!/usr/bin/env python3
"""Extracts an SCFB dump from a captured device serial transcript and
writes it as a standalone .scfb file that fb_view.py decodes unchanged.

The on-device harness (firmware/system/main/app_main.cpp, start-screen
T11) prints each framebuffer once per GameSession state change, hex-
encoded between "SCFB-DUMP-BEGIN"/"SCFB-DUMP-END" sentinel markers, so it
can share a UART with ordinary ESP_LOGI lines without those lines'
timestamp/tag prefixes corrupting the hex payload -- see
docs/dump-format.md for the underlying file format and
docs/device-build.md for how to capture the transcript this reads.

Stdlib only (constitution NFR-4): no new import, no allowlist edit.

Usage:
    python3 tools/scfb_capture.py <captured_log.txt> <output.scfb>
    python3 tools/scfb_capture.py <captured_log.txt> <output.scfb> --which first
"""

import argparse
import sys

BEGIN_MARKER = "SCFB-DUMP-BEGIN"
END_MARKER = "SCFB-DUMP-END"
HEADER_SIZE = 10  # steamcore::kDumpHeaderSize (docs/dump-format.md)
_HEX_DIGITS = set("0123456789abcdefABCDEF")


def extract_blocks(lines):
    """Yields the concatenated hex payload of each complete BEGIN/END block
    found in `lines`. Matches markers as a line *suffix* so a captured
    line still carrying an ESP-IDF log prefix (e.g. "I (1234) tag:
    SCFB-DUMP-BEGIN") is recognised. A BEGIN with no matching END before
    EOF or the next BEGIN, or a non-hex line appearing inside a block
    (e.g. an interleaved log line), invalidates that block -- it is
    dropped, never yielded as a truncated or corrupted payload.
    """
    in_block = False
    current_hex = []
    for raw_line in lines:
        line = raw_line.strip()
        if line.endswith(BEGIN_MARKER):
            in_block = True
            current_hex = []
            continue
        if line.endswith(END_MARKER):
            if in_block:
                yield "".join(current_hex)
            in_block = False
            current_hex = []
            continue
        if in_block:
            if line and all(c in _HEX_DIGITS for c in line):
                current_hex.append(line)
            else:
                in_block = False
                current_hex = []


def validate_and_decode(hex_str):
    """Returns the decoded bytes for a well-formed, complete SCFB payload,
    or None if it must be rejected: not valid hex, too short to hold a
    header, wrong magic, or a byte count that disagrees with the header's
    own declared width*height (a truncated capture) -- never a partial
    decode (docs/dump-format.md's own reader contract).
    """
    try:
        data = bytes.fromhex(hex_str)
    except ValueError:
        return None
    if len(data) < HEADER_SIZE:
        return None
    if data[0:4] != b"SCFB":
        return None
    width = int.from_bytes(data[6:8], "little")
    height = int.from_bytes(data[8:10], "little")
    if width == 0 or height == 0:
        return None
    if len(data) != HEADER_SIZE + width * height:
        return None
    return data


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", help="path to a captured serial transcript")
    parser.add_argument("output", help="path to write the decoded .scfb to")
    parser.add_argument(
        "--which", choices=["first", "last"], default="last",
        help="which valid block to write when the transcript holds more "
             "than one -- the harness dumps once per GameSession state "
             "change, so 'first' is the earliest screen captured (e.g. "
             "READY) and 'last' (the default) is the most recent (e.g. "
             "PLAYING). review F2: a fixed 'always last' default silently "
             "produced the wrong screen for docs/device-build.md's own "
             "walkthrough, which wants both.")
    args = parser.parse_args(argv)

    try:
        with open(args.log, "r", encoding="utf-8", errors="replace") as f:
            lines = f.readlines()
    except OSError as exc:
        print(f"scfb_capture: {exc}", file=sys.stderr)
        return 1

    valid_blocks = []
    for hex_str in extract_blocks(lines):
        decoded = validate_and_decode(hex_str)
        if decoded is not None:
            valid_blocks.append(decoded)

    if not valid_blocks:
        print("scfb_capture: no valid SCFB block found in the log -- a "
              "truncated or byte-count-mismatched block is rejected, "
              "never decoded partially", file=sys.stderr)
        return 1

    chosen = valid_blocks[0] if args.which == "first" else valid_blocks[-1]
    with open(args.output, "wb") as f:
        f.write(chosen)

    print(f"scfb_capture: wrote {args.output} ({len(chosen)} bytes), "
          f"'{args.which}' of {len(valid_blocks)} valid block(s) found "
          "in the log")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
