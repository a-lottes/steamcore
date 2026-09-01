"""AC-4: proves the whole chain -- drawn pattern -> C++ dump -> Python
decode -> PNG RGB -- agrees with the format doc, using code that shares
NOTHING with fb_view.py's own reader or PALETTE (plan Decision 5): every
constant here is restated literally, independently, from
docs/dump-format.md. If this file imported fb_view's PALETTE or its
HEADER_SIZE, a bug shared by the writer and this test could pass
silently; restating them is what makes the comparison meaningful.

Needs no C++ toolchain to run -- it reads the already-committed fixture
and invokes fb_view.py as a subprocess, the same command a human runs.
"""

import os
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib

# --- Restated literally from docs/dump-format.md. Do not import these
# from fb_view -- see the module docstring. ---
HEADER_SIZE = 10
PALETTE = {
    0: (0x00, 0x00, 0x00),  # BLACK
    1: (0x4D, 0x26, 0x00),  # DARK_ORANGE
    2: (0xB3, 0x59, 0x00),  # ORANGE
    3: (0xFF, 0x99, 0x33),  # BRIGHT_ORANGE
}
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"

# Same table as docs/dump-format.md "Anchor table" and
# firmware/steamcore/test/dump_format_test.cpp's kAnchors -- keep all
# three in sync by hand.
ANCHORS = [
    (5, 5, 0),
    (25, 25, 1),
    (60, 50, 2),
    (85, 90, 2),
    (150, 20, 3),
    (151, 21, 0),
    (200, 10, 1),
    (200, 100, 0),
    (0, 0, 1),
    (239, 0, 2),
    (0, 159, 3),
    (239, 159, 0),
]

FB_VIEW = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fb_view.py")
FIXTURE_DUMP = os.environ.get("STEAMCORE_FIXTURE_DUMP")


def read_fixture_payload():
    """Parses the committed fixture directly -- no fb_view code."""
    with open(FIXTURE_DUMP, "rb") as f:
        data = f.read()
    width = struct.unpack_from("<H", data, 6)[0]
    height = struct.unpack_from("<H", data, 8)[0]
    payload = data[HEADER_SIZE:]
    assert len(payload) == width * height
    return width, height, payload


def read_png_rgb_rows(png_path):
    """A minimal, independent PNG reader: verifies every chunk CRC it did
    not compute, decompresses IDAT, and unfilters (filter 0 only -- that
    is all fb_view.py ever emits, and any other filter byte is a defect
    to fail loudly on, not silently handle)."""
    with open(png_path, "rb") as f:
        data = f.read()

    if data[:8] != PNG_SIGNATURE:
        raise AssertionError(f"not a PNG (bad signature): {png_path}")

    pos = 8
    width = height = None
    idat = bytearray()
    saw_iend = False
    while pos < len(data):
        length = struct.unpack_from(">I", data, pos)[0]
        chunk_type = data[pos + 4:pos + 8]
        chunk_data = data[pos + 8:pos + 8 + length]
        stored_crc = struct.unpack_from(">I", data, pos + 8 + length)[0]
        computed_crc = zlib.crc32(chunk_type + chunk_data) & 0xFFFFFFFF
        if stored_crc != computed_crc:
            raise AssertionError(
                f"{chunk_type!r} CRC mismatch: stored {stored_crc:#x}, "
                f"computed {computed_crc:#x}"
            )

        if chunk_type == b"IHDR":
            width, height, depth, colour_type = struct.unpack_from(
                ">IIBB", chunk_data
            )
            assert depth == 8, f"unexpected bit depth {depth}"
            assert colour_type == 2, f"unexpected colour type {colour_type}"
        elif chunk_type == b"IDAT":
            idat += chunk_data
        elif chunk_type == b"IEND":
            saw_iend = True

        pos += 8 + length + 4

    assert saw_iend, "no IEND chunk"
    assert width is not None, "no IHDR chunk"

    raw = zlib.decompress(bytes(idat))
    stride = width * 3
    rows = []
    for y in range(height):
        row_start = y * (stride + 1)
        filter_type = raw[row_start]
        assert filter_type == 0, f"row {y}: unexpected filter type {filter_type}"
        rows.append(bytes(raw[row_start + 1:row_start + 1 + stride]))

    return width, height, rows


class TestRoundTrip(unittest.TestCase):
    def setUp(self):
        if not FIXTURE_DUMP or not os.path.exists(FIXTURE_DUMP):
            self.skipTest(
                "STEAMCORE_FIXTURE_DUMP not set or missing -- run `make test` first"
            )
        self.tmpdir = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmpdir.cleanup)

    def test_every_pixel_matches_payload_through_the_real_cli(self):
        width, height, payload = read_fixture_payload()

        out_path = os.path.join(self.tmpdir.name, "roundtrip.png")
        result = subprocess.run(
            [sys.executable, "-B", FB_VIEW, FIXTURE_DUMP, out_path],
            capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)

        png_width, png_height, rows = read_png_rgb_rows(out_path)
        self.assertEqual(png_width, width)
        self.assertEqual(png_height, height)

        mismatches = []
        for y in range(height):
            row = rows[y]
            for x in range(width):
                index = payload[y * width + x]
                expected = PALETTE[index]
                actual = tuple(row[x * 3:x * 3 + 3])
                if actual != expected:
                    mismatches.append((x, y, expected, actual))

        if mismatches:
            x, y, expected, actual = mismatches[0]
            self.fail(
                f"{len(mismatches)} pixel(s) differ; first at ({x},{y}): "
                f"expected {expected}, got {actual}"
            )

    def test_matches_published_anchor_table(self):
        out_path = os.path.join(self.tmpdir.name, "anchors.png")
        result = subprocess.run(
            [sys.executable, "-B", FB_VIEW, FIXTURE_DUMP, out_path],
            capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)

        _, _, rows = read_png_rgb_rows(out_path)
        for x, y, index in ANCHORS:
            expected = PALETTE[index]
            actual = tuple(rows[y][x * 3:x * 3 + 3])
            self.assertEqual(
                actual, expected, f"anchor ({x},{y}): expected {expected}, got {actual}"
            )


if __name__ == "__main__":
    unittest.main()
