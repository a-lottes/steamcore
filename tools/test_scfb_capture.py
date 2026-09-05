"""Unit tests for scfb_capture.py. Stdlib unittest only (constitution
NFR-4). Exercises log noise, a truncated block and a byte-count-mismatched
block, and multiple valid blocks in one transcript -- start-screen T11's
own DoD, not just the happy path.
"""

import os
import struct
import tempfile
import unittest

import scfb_capture  # noqa: E402


def make_scfb_bytes(width=2, height=2, payload=None):
    if payload is None:
        payload = bytes(range(width * height))
    header = b"SCFB" + struct.pack("<HHH", 1, width, height)
    return header + payload


class ExtractBlocksTest(unittest.TestCase):
    def test_extracts_valid_block_ignoring_surrounding_noise(self):
        valid = make_scfb_bytes()
        hexstr = valid.hex()
        lines = [
            "I (100) some_tag: unrelated boot message\n",
            "I (200) some_tag: SCFB-DUMP-BEGIN\n",
            hexstr[:8] + "\n",
            hexstr[8:] + "\n",
            "I (300) some_tag: SCFB-DUMP-END\n",
            "I (400) some_tag: more noise\n",
        ]
        blocks = list(scfb_capture.extract_blocks(lines))
        self.assertEqual(len(blocks), 1)
        self.assertEqual(scfb_capture.validate_and_decode(blocks[0]), valid)

    def test_drops_a_begin_with_no_matching_end(self):
        valid = make_scfb_bytes()
        lines = [
            "SCFB-DUMP-BEGIN\n",
            valid.hex() + "\n",
            # No END marker, no more lines -- truncated capture.
        ]
        blocks = list(scfb_capture.extract_blocks(lines))
        self.assertEqual(blocks, [])

    def test_drops_a_block_containing_non_hex_noise(self):
        valid = make_scfb_bytes()
        lines = [
            "SCFB-DUMP-BEGIN\n",
            valid.hex()[:8] + "\n",
            "I (250) some_tag: an interleaved log line\n",
            valid.hex()[8:] + "\n",
            "SCFB-DUMP-END\n",
        ]
        blocks = list(scfb_capture.extract_blocks(lines))
        self.assertEqual(blocks, [])

    def test_extracts_multiple_valid_blocks_in_one_transcript(self):
        first = make_scfb_bytes(payload=bytes([0, 0, 0, 0]))
        second = make_scfb_bytes(payload=bytes([3, 3, 3, 3]))
        lines = (
            ["SCFB-DUMP-BEGIN\n", first.hex() + "\n", "SCFB-DUMP-END\n"]
            + ["SCFB-DUMP-BEGIN\n", second.hex() + "\n", "SCFB-DUMP-END\n"]
        )
        blocks = list(scfb_capture.extract_blocks(lines))
        self.assertEqual(len(blocks), 2)
        self.assertEqual(scfb_capture.validate_and_decode(blocks[0]), first)
        self.assertEqual(scfb_capture.validate_and_decode(blocks[1]), second)

    def test_a_second_begin_discards_the_open_incomplete_block(self):
        # review F8: an abandoned BEGIN (e.g. a reset mid-dump) followed by
        # a fresh, complete BEGIN/END must not leak the first block's
        # partial hex into the second one's payload.
        valid = make_scfb_bytes()
        lines = [
            "SCFB-DUMP-BEGIN\n",
            "ffffffff\n",  # partial hex from an abandoned first attempt
            "SCFB-DUMP-BEGIN\n",  # reset -- discards the line above
            valid.hex() + "\n",
            "SCFB-DUMP-END\n",
        ]
        blocks = list(scfb_capture.extract_blocks(lines))
        self.assertEqual(len(blocks), 1)
        self.assertEqual(scfb_capture.validate_and_decode(blocks[0]), valid)


class ValidateAndDecodeTest(unittest.TestCase):
    def test_accepts_a_well_formed_payload(self):
        valid = make_scfb_bytes()
        self.assertEqual(scfb_capture.validate_and_decode(valid.hex()), valid)

    def test_rejects_a_byte_count_short_of_the_declared_size(self):
        short = make_scfb_bytes()[:-1]
        self.assertIsNone(scfb_capture.validate_and_decode(short.hex()))

    def test_rejects_a_byte_count_longer_than_the_declared_size(self):
        long = make_scfb_bytes() + b"\x00"
        self.assertIsNone(scfb_capture.validate_and_decode(long.hex()))

    def test_rejects_wrong_magic(self):
        wrong = b"XXXX" + make_scfb_bytes()[4:]
        self.assertIsNone(scfb_capture.validate_and_decode(wrong.hex()))

    def test_rejects_invalid_hex(self):
        self.assertIsNone(scfb_capture.validate_and_decode("not hex data"))

    def test_rejects_zero_width_or_height(self):
        zero_width = b"SCFB" + struct.pack("<HHH", 1, 0, 4)
        self.assertIsNone(scfb_capture.validate_and_decode(zero_width.hex()))


class MainTest(unittest.TestCase):
    """review F8: main() itself was untested -- argv arity, a missing
    input file, the no-valid-block exit path, and the --which selection
    F2 added all go through main(), not just the helper functions above.
    """

    def setUp(self):
        self.tmpdir = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmpdir.cleanup)

    def _write_log(self, name, lines):
        path = os.path.join(self.tmpdir.name, name)
        with open(path, "w", encoding="utf-8") as f:
            f.writelines(lines)
        return path

    def test_missing_positional_argument_exits_nonzero(self):
        with self.assertRaises(SystemExit) as cm:
            scfb_capture.main(["only_one_arg.log"])
        self.assertNotEqual(cm.exception.code, 0)

    def test_missing_log_file_is_reported_and_exits_one(self):
        out_path = os.path.join(self.tmpdir.name, "out.scfb")
        missing_path = os.path.join(self.tmpdir.name, "does_not_exist.log")
        result = scfb_capture.main([missing_path, out_path])
        self.assertEqual(result, 1)
        self.assertFalse(os.path.exists(out_path))

    def test_no_valid_block_exits_one_and_writes_nothing(self):
        log_path = self._write_log("noise.log", ["just some noise\n"])
        out_path = os.path.join(self.tmpdir.name, "out.scfb")
        result = scfb_capture.main([log_path, out_path])
        self.assertEqual(result, 1)
        self.assertFalse(os.path.exists(out_path))

    def test_default_which_writes_the_last_valid_block(self):
        first = make_scfb_bytes(payload=bytes([0, 0, 0, 0]))
        second = make_scfb_bytes(payload=bytes([3, 3, 3, 3]))
        log_path = self._write_log(
            "two_blocks.log",
            ["SCFB-DUMP-BEGIN\n", first.hex() + "\n", "SCFB-DUMP-END\n",
             "SCFB-DUMP-BEGIN\n", second.hex() + "\n", "SCFB-DUMP-END\n"],
        )
        out_path = os.path.join(self.tmpdir.name, "out.scfb")

        result = scfb_capture.main([log_path, out_path])

        self.assertEqual(result, 0)
        with open(out_path, "rb") as f:
            self.assertEqual(f.read(), second)

    def test_which_first_writes_the_first_valid_block(self):
        first = make_scfb_bytes(payload=bytes([0, 0, 0, 0]))
        second = make_scfb_bytes(payload=bytes([3, 3, 3, 3]))
        log_path = self._write_log(
            "two_blocks.log",
            ["SCFB-DUMP-BEGIN\n", first.hex() + "\n", "SCFB-DUMP-END\n",
             "SCFB-DUMP-BEGIN\n", second.hex() + "\n", "SCFB-DUMP-END\n"],
        )
        out_path = os.path.join(self.tmpdir.name, "out.scfb")

        result = scfb_capture.main([log_path, out_path, "--which", "first"])

        self.assertEqual(result, 0)
        with open(out_path, "rb") as f:
            self.assertEqual(f.read(), first)


if __name__ == "__main__":
    unittest.main()
