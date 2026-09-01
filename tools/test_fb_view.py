"""Unit tests for fb_view.py. Stdlib unittest only (constitution NFR-4).

Malformed inputs are synthesized by hand here -- only the *valid* fixture
must be engine-produced (spec C3); a malformed file is malformed by
construction regardless of which language wrote it.
"""

import os
import struct
import subprocess
import sys
import tempfile
import time
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fb_view  # noqa: E402

FIXTURE_DUMP = os.environ.get("STEAMCORE_FIXTURE_DUMP")


def make_dump(magic=b"SCFB", version=1, width=1, height=1, payload=None):
    if payload is None:
        payload = bytes([0]) * (width * height)
    return magic + struct.pack("<HHH", version, width, height) + payload


class TestRejectionPaths(unittest.TestCase):
    """One case per T6 rejection path. Each asserts: non-zero exit, the
    CLI's stderr names the specific cause, and no output file appears."""

    def setUp(self):
        self.tmpdir = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmpdir.cleanup)
        self.out_path = os.path.join(self.tmpdir.name, "out.png")

    def _run(self, dump_path):
        return subprocess.run(
            [sys.executable, "-B", os.path.join(
                os.path.dirname(os.path.abspath(__file__)), "fb_view.py"),
             dump_path, self.out_path],
            capture_output=True, text=True,
        )

    def _write_dump(self, data):
        path = os.path.join(self.tmpdir.name, "input.scfb")
        with open(path, "wb") as f:
            f.write(data)
        return path

    def assertRejected(self, result, expected_substring):
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(expected_substring, result.stderr)
        self.assertNotIn("Traceback", result.stderr)
        self.assertFalse(os.path.exists(self.out_path))

    def test_bad_magic(self):
        dump = self._write_dump(make_dump(magic=b"XXXX"))
        self.assertRejected(self._run(dump), "bad magic")

    def test_unsupported_version(self):
        dump = self._write_dump(make_dump(version=99))
        self.assertRejected(self._run(dump), "unsupported version")

    def test_size_mismatch(self):
        dump = self._write_dump(make_dump(width=10, height=10, payload=b"\x00" * 3))
        self.assertRejected(self._run(dump), "needs")

    def test_zero_width_is_rejected(self):
        dump = self._write_dump(make_dump(width=0, height=5, payload=b""))
        self.assertRejected(self._run(dump), "not valid")

    def test_zero_height_is_rejected(self):
        dump = self._write_dump(make_dump(width=5, height=0, payload=b""))
        self.assertRejected(self._run(dump), "not valid")

    def test_invalid_palette_index(self):
        dump = self._write_dump(
            make_dump(width=2, height=1, payload=bytes([0, 5]))
        )
        result = self._run(dump)
        self.assertRejected(result, "invalid palette index")
        self.assertIn("offset 1", result.stderr)

    def test_missing_file(self):
        missing = os.path.join(self.tmpdir.name, "does_not_exist.scfb")
        result = self._run(missing)
        # Asserts the tool's own contract (its "fb_view: " prefix and the
        # path it was given), not the OS's English strerror() wording,
        # which is C-library output and not portable (review F5).
        self.assertRejected(result, "fb_view: ")
        self.assertIn(missing, result.stderr)

    def test_directory_as_input(self):
        # A distinct subdirectory, not self.tmpdir.name itself -- out_path
        # lives inside tmpdir.name too, so asserting that substring alone
        # would also accept a (wrong) message naming the output path
        # instead of the input (review F16).
        input_dir = os.path.join(self.tmpdir.name, "a_directory_not_a_file")
        os.mkdir(input_dir)
        result = subprocess.run(
            [sys.executable, "-B", os.path.join(
                os.path.dirname(os.path.abspath(__file__)), "fb_view.py"),
             input_dir, self.out_path],
            capture_output=True, text=True,
        )
        self.assertRejected(result, "fb_view: ")
        self.assertIn(input_dir, result.stderr)

    def test_no_arguments(self):
        result = subprocess.run(
            [sys.executable, "-B", os.path.join(
                os.path.dirname(os.path.abspath(__file__)), "fb_view.py")],
            capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, 2)
        self.assertIn("usage", result.stderr.lower())
        self.assertNotIn("Traceback", result.stderr)


class TestHappyPath(unittest.TestCase):
    def setUp(self):
        if not FIXTURE_DUMP or not os.path.exists(FIXTURE_DUMP):
            self.skipTest(
                "STEAMCORE_FIXTURE_DUMP not set or missing -- run `make test` "
                "first, or invoke via `make test-python`"
            )
        self.tmpdir = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmpdir.cleanup)

    def test_success_prints_output_path_and_exits_zero(self):
        out_path = os.path.join(self.tmpdir.name, "out.png")
        result = subprocess.run(
            [sys.executable, "-B", os.path.join(
                os.path.dirname(os.path.abspath(__file__)), "fb_view.py"),
             FIXTURE_DUMP, out_path],
            capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout.strip(), out_path)
        self.assertTrue(os.path.exists(out_path))

    def test_decode_is_byte_identical_across_runs(self):
        out_a = os.path.join(self.tmpdir.name, "a.png")
        out_b = os.path.join(self.tmpdir.name, "b.png")
        fb_view.decode_to_png(FIXTURE_DUMP, out_a)
        fb_view.decode_to_png(FIXTURE_DUMP, out_b)
        with open(out_a, "rb") as fa, open(out_b, "rb") as fb:
            self.assertEqual(fa.read(), fb.read())

    def test_decode_completes_within_one_second(self):
        out_path = os.path.join(self.tmpdir.name, "timed.png")
        start = time.monotonic()
        fb_view.decode_to_png(FIXTURE_DUMP, out_path)
        elapsed = time.monotonic() - start
        self.assertLess(elapsed, 1.0, f"decode took {elapsed:.3f}s, budget is < 1s (NFR-1)")

    def test_overwrite_is_silent(self):
        out_path = os.path.join(self.tmpdir.name, "overwrite.png")
        with open(out_path, "wb") as f:
            f.write(b"not a real png")
        fb_view.decode_to_png(FIXTURE_DUMP, out_path)
        with open(out_path, "rb") as f:
            self.assertTrue(f.read().startswith(fb_view.PNG_SIGNATURE))


if __name__ == "__main__":
    unittest.main()
