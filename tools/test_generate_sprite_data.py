"""Unit tests for generate_sprite_data.py. Stdlib unittest only
(constitution NFR-4).

Every fixture here is built with fb_view.encode_png rather than checked in
as a binary, so the tests carry no asset of their own and AC-3.2 (the build
and the suite run with assets/ absent) holds for them too. That encoder
writes colour type 2, which is exactly why the decoder supports 2 alongside
the sheet's own 6.

The emphasis is the failure taxonomy (AC-3.6): each named failure must exit
non-zero, say which one occurred, and leave no output file written or
partially overwritten. A generator that silently produces plausible-looking
wrong art is worse than one that refuses to run.
"""

import os
import struct
import tempfile
import unittest
import zlib

import fb_view
import generate_sprite_data as gen


def make_image(width, height, pixel_fn):
    """An in-memory gen.Image built directly from pixel_fn(x, y) -> RGBA,
    bypassing PNG encode/decode entirely. The background/fit/quantise
    functions below operate on gen.Image, so their tests need a way to
    construct one without round-tripping through a real file -- decoding
    is DecoderTest's job above, not this one's."""
    pixels = bytearray(width * height * 4)
    for y in range(height):
        for x in range(width):
            r, g, b, a = pixel_fn(x, y)
            i = (y * width + x) * 4
            pixels[i : i + 4] = bytes((r, g, b, a))
    return gen.Image(width, height, pixels)


def solid_rgb_png(width, height, rgb):
    """A colour-type-2 PNG, every pixel the same colour."""
    row = bytes(rgb) * width
    return fb_view.encode_png(width, height, [row] * height)


def rgba_png(width, height, pixel_fn, filter_type=0):
    """A colour-type-6 PNG built by hand, so alpha and the scanline filter
    type are both under the test's control."""
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    raw = bytearray()
    previous = bytearray(width * 4)
    for y in range(height):
        line = bytearray()
        for x in range(width):
            line += bytes(pixel_fn(x, y))
        raw.append(filter_type)
        if filter_type == 0:
            raw += line
        elif filter_type == 2:  # Up
            raw += bytes((line[i] - previous[i]) & 0xFF for i in range(len(line)))
        else:
            raise AssertionError("fixture supports filter types 0 and 2 only")
        previous = line
    return (
        fb_view.PNG_SIGNATURE
        + fb_view._chunk(b"IHDR", ihdr)
        + fb_view._chunk(b"IDAT", zlib.compress(bytes(raw), 9))
        + fb_view._chunk(b"IEND", b"")
    )


class TempPng:
    def __init__(self, data):
        self.data = data

    def __enter__(self):
        fd, self.path = tempfile.mkstemp(suffix=".png")
        with os.fdopen(fd, "wb") as f:
            f.write(self.data)
        return self.path

    def __exit__(self, *exc):
        try:
            os.unlink(self.path)
        except OSError:
            pass


class DecoderTest(unittest.TestCase):
    def test_reads_colour_type_2(self):
        with TempPng(solid_rgb_png(4, 3, (0xB3, 0x59, 0x00))) as path:
            img = gen.read_png(path)
        self.assertEqual((img.width, img.height), (4, 3))
        # Colour type 2 has no alpha channel; it decodes as fully opaque.
        self.assertEqual(img.at(0, 0), (0xB3, 0x59, 0x00, 255))
        self.assertEqual(img.at(3, 2), (0xB3, 0x59, 0x00, 255))

    def test_reads_colour_type_6_with_alpha(self):
        data = rgba_png(3, 2, lambda x, y: (x * 10, y * 20, 7, 255 if x else 0))
        with TempPng(data) as path:
            img = gen.read_png(path)
        self.assertEqual(img.at(0, 0), (0, 0, 7, 0))
        self.assertEqual(img.at(2, 1), (20, 20, 7, 255))

    def test_reads_a_filtered_scanline(self):
        """Filter type 2 (Up) must be undone, not taken literally -- the real
        sheet uses several filter types across its scanlines."""
        plain = rgba_png(4, 4, lambda x, y: (x * 5, y * 6, 9, 200), filter_type=0)
        filtered = rgba_png(4, 4, lambda x, y: (x * 5, y * 6, 9, 200), filter_type=2)
        self.assertNotEqual(plain, filtered)  # genuinely different encodings
        with TempPng(plain) as a, TempPng(filtered) as b:
            self.assertEqual(gen.read_png(a).pixels, gen.read_png(b).pixels)

    def test_rejects_non_png(self):
        with TempPng(b"this is not a png") as path:
            with self.assertRaises(gen.GeneratorError) as ctx:
                gen.read_png(path)
        self.assertIn("signature", str(ctx.exception))

    def test_rejects_interlaced(self):
        ihdr = struct.pack(">IIBBBBB", 2, 2, 8, 6, 0, 0, 1)  # interlace = 1
        data = (
            fb_view.PNG_SIGNATURE
            + fb_view._chunk(b"IHDR", ihdr)
            + fb_view._chunk(b"IDAT", zlib.compress(b"\x00" * 18, 9))
            + fb_view._chunk(b"IEND", b"")
        )
        with TempPng(data) as path:
            with self.assertRaises(gen.GeneratorError) as ctx:
                gen.read_png(path)
        self.assertIn("interlaced", str(ctx.exception))

    def test_rejects_unsupported_bit_depth(self):
        ihdr = struct.pack(">IIBBBBB", 2, 2, 16, 6, 0, 0, 0)
        data = (
            fb_view.PNG_SIGNATURE
            + fb_view._chunk(b"IHDR", ihdr)
            + fb_view._chunk(b"IDAT", zlib.compress(b"\x00" * 34, 9))
            + fb_view._chunk(b"IEND", b"")
        )
        with TempPng(data) as path:
            with self.assertRaises(gen.GeneratorError) as ctx:
                gen.read_png(path)
        self.assertIn("bit depth", str(ctx.exception))

    def test_rejects_palette_colour_type(self):
        ihdr = struct.pack(">IIBBBBB", 2, 2, 8, 3, 0, 0, 0)  # 3 = palette
        data = (
            fb_view.PNG_SIGNATURE
            + fb_view._chunk(b"IHDR", ihdr)
            + fb_view._chunk(b"IDAT", zlib.compress(b"\x00" * 6, 9))
            + fb_view._chunk(b"IEND", b"")
        )
        with TempPng(data) as path:
            with self.assertRaises(gen.GeneratorError) as ctx:
                gen.read_png(path)
        self.assertIn("colour type", str(ctx.exception))


class FailureTaxonomyTest(unittest.TestCase):
    """AC-3.6's four named cases."""

    def test_missing_source_is_named(self):
        with self.assertRaises(gen.GeneratorError) as ctx:
            gen.read_png("/nonexistent/definitely/not/here.png")
        self.assertIn("cannot read source image", str(ctx.exception))

    def test_region_out_of_bounds_is_named(self):
        with TempPng(solid_rgb_png(8, 8, (1, 2, 3))) as path:
            img = gen.read_png(path)
        with self.assertRaises(gen.GeneratorError) as ctx:
            gen.check_region(img, (4, 4, 8, 8))
        self.assertIn("extends beyond", str(ctx.exception))
        # A region exactly filling the image is fine, not off-by-one rejected.
        gen.check_region(img, (0, 0, 8, 8))

    def test_upscaling_is_named(self):
        with self.assertRaises(gen.GeneratorError) as ctx:
            gen.check_target((0, 0, 10, 10), 12, 4)
        self.assertIn("larger than the source region", str(ctx.exception))
        with self.assertRaises(gen.GeneratorError):
            gen.check_target((0, 0, 10, 10), 4, 12)
        # Equal size is not upscaling.
        gen.check_target((0, 0, 10, 10), 10, 10)

    def test_unwritable_output_is_named_and_writes_nothing(self):
        with self.assertRaises(gen.GeneratorError) as ctx:
            gen._atomic_write("/nonexistent/definitely/not/here/out.h", "x")
        self.assertIn("does not exist", str(ctx.exception))

    def test_malformed_region_string_is_named(self):
        for bad in ["1,2,3", "a,b,c,d", "1,2,3,4,5", "1,2,0,4", "1,2,3,-1"]:
            with self.assertRaises(gen.GeneratorError):
                gen.parse_region(bad)
        self.assertEqual(gen.parse_region("45,20,287,95"), (45, 20, 287, 95))


class AtomicWriteTest(unittest.TestCase):
    def test_write_replaces_atomically(self):
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "out.h")
            gen._atomic_write(path, "first")
            gen._atomic_write(path, "second")
            with open(path) as f:
                self.assertEqual(f.read(), "second")
            # No temp files left behind.
            self.assertEqual(os.listdir(d), ["out.h"])

    def test_failed_write_leaves_previous_content_intact(self):
        """AC-3.6: no output file written *or partially overwritten*."""
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "out.h")
            gen._atomic_write(path, "original")
            with self.assertRaises(gen.GeneratorError):
                gen._atomic_write(os.path.join(d, "nope", "out.h"), "x")
            with open(path) as f:
                self.assertEqual(f.read(), "original")


class AlphaFindingTest(unittest.TestCase):
    def test_histogram_counts_each_class(self):
        data = rgba_png(4, 1, lambda x, y: (0, 0, 0, [0, 128, 255, 255][x]))
        with TempPng(data) as path:
            img = gen.read_png(path)
        opaque, transparent, partial = gen.alpha_histogram(img)
        self.assertEqual((opaque, transparent, partial), (2, 1, 1))

    def test_verdict_separated_when_bimodal(self):
        """The real sheet's shape: a large low cluster, a smaller high one,
        almost nothing straddling the gate."""
        buckets = [0] * 16
        buckets[0] = 770
        buckets[15] = 200
        buckets[8] = 5
        verdict, _reason = gen.alpha_verdict(buckets, 128)
        self.assertEqual(verdict, "alpha-separated")

    def test_verdict_not_separated_when_mass_straddles_the_gate(self):
        buckets = [0] * 16
        buckets[8] = 500  # the gate's own bucket holds most of the image
        buckets[0] = 250
        buckets[15] = 250
        verdict, reason = gen.alpha_verdict(buckets, 128)
        self.assertEqual(verdict, "not alpha-separated")
        self.assertIn("chroma flood", reason)

    def test_verdict_not_separated_when_everything_is_opaque(self):
        """An opaque export -- the case the plan expected to find. The alpha
        gate cannot separate anything, so the flood must."""
        buckets = [0] * 16
        buckets[15] = 1000
        verdict, _reason = gen.alpha_verdict(buckets, 128)
        self.assertEqual(verdict, "not alpha-separated")


class SourceHashTest(unittest.TestCase):
    def test_hash_is_stable_and_content_dependent(self):
        a = solid_rgb_png(2, 2, (1, 2, 3))
        b = solid_rgb_png(2, 2, (4, 5, 6))
        with TempPng(a) as pa, TempPng(b) as pb:
            self.assertEqual(gen.source_sha256(pa), gen.source_sha256(pa))
            self.assertNotEqual(gen.source_sha256(pa), gen.source_sha256(pb))

    def test_missing_source_is_named(self):
        with self.assertRaises(gen.GeneratorError):
            gen.source_sha256("/nonexistent/definitely/not/here.png")


# --------------------------------------------------------------------------
# Background rule (T3, plan §1 decision 3 / spec D1). Built directly on
# in-memory Image objects (make_image), not real PNG bytes -- decoding
# is DecoderTest's concern, not this one's.
# --------------------------------------------------------------------------

BROWN = (100, 60, 20)  # a synthetic "panel fill" reference colour


class BackgroundRuleTest(unittest.TestCase):
    def test_fully_transparent_surround_is_alpha_mode_with_empty_outer_ring(self):
        """A fully transparent border means (b)-(d) never run at all --
        the flood is a recorded no-op, not merely a vacuous one."""
        w = h = 6

        def px(x, y):
            on_border = x == 0 or x == w - 1 or y == 0 or y == h - 1
            if on_border:
                return (0, 0, 0, 0)  # fully transparent
            return (255, 150, 50, 255)  # opaque "ink" interior

        img = make_image(w, h, px)
        result = gen.background_mask(img, (0, 0, w, h), alpha_min=128,
                                      bg_tol=60, halo_tol=110)
        self.assertEqual(result.mode, "alpha")
        self.assertIsNone(result.reference)
        for y in range(h):
            for x in range(w):
                on_border = x == 0 or x == w - 1 or y == 0 or y == h - 1
                self.assertEqual(result.mask[y][x], on_border,
                                  f"mismatch at ({x},{y})")

    def test_opaque_panel_with_glow_ramp_clears_to_an_empty_ring(self):
        """The other world: nothing is transparent. The border is exactly
        the reference colour (bg_tol distance 0), a one-pixel glow ring
        sits further away but within halo_tol, and the ink core is far
        enough from the reference to never qualify as background."""
        w = h = 10
        ink = (255, 150, 50)
        # Distance from BROWN chosen to land between bg_tol (60) and
        # halo_tol (110): (30,20,10) has dist2 = 900+400+100=1400,
        # sqrt~37 -- within bg_tol already, so nudge further out to land
        # strictly between the two thresholds (dist in (60,110]).
        glow = (BROWN[0] + 70, BROWN[1] + 40, BROWN[2])  # dist2 = 4900+1600=6500, dist~80.6

        def px(x, y):
            ring = max(abs(x - (w - 1) / 2), abs(y - (h - 1) / 2))
            if ring >= 4:
                return (*BROWN, 255)
            if ring >= 3:
                return (*glow, 255)
            return (*ink, 255)

        img = make_image(w, h, px)
        result = gen.background_mask(img, (0, 0, w, h), alpha_min=128,
                                      bg_tol=60, halo_tol=110)
        self.assertEqual(result.mode, "chroma")
        self.assertEqual(result.reference, BROWN)

        grid, mode, reference = gen.generate_artifact_grid(
            img, (0, 0, w, h), w, h, alpha_min=128, bg_tol=60, halo_tol=110,
            coverage_min=50)
        self.assertEqual(mode, "chroma")
        self.assertEqual(reference, BROWN)
        # The outer ring of the quantised artefact: zero non-BLACK pixels,
        # and in particular no DARK_ORANGE -- both the bg_tol-only border
        # and the halo-only glow ring are fully cleared.
        for x in range(w):
            self.assertEqual(grid[0][x], 0, f"top ring at x={x}")
            self.assertEqual(grid[h - 1][x], 0, f"bottom ring at x={x}")
        for y in range(h):
            self.assertEqual(grid[y][0], 0, f"left ring at y={y}")
            self.assertEqual(grid[y][w - 1], 0, f"right ring at y={y}")
        # The ink core survives as foreground (non-BLACK).
        self.assertNotEqual(grid[h // 2][w // 2], 0)

    def test_interior_dark_mass_unreachable_from_border_survives(self):
        """A 2x2 patch, deep inside a shell of ink, painted exactly the
        reference colour -- background by colour alone, but never
        connected to the border through anything the flood accepts, so it
        must NOT be marked. Checked in both modes."""
        w = h = 12
        ink = (255, 150, 50)

        def make(px_fn):
            return gen.background_mask(make_image(w, h, px_fn), (0, 0, w, h),
                                        alpha_min=128, bg_tol=60, halo_tol=110)

        def interior_cells():
            return [(x, y) for y in range(5, 7) for x in range(5, 7)]

        # Chroma mode: opaque border at the reference colour, ink shell,
        # interior patch equal to the reference colour but walled off.
        def chroma_px(x, y):
            if (x, y) in interior_cells():
                return (*BROWN, 255)
            return (*ink, 255) if 2 <= x <= 9 and 2 <= y <= 9 else (*BROWN, 255)

        chroma_result = make(chroma_px)
        self.assertEqual(chroma_result.mode, "chroma")
        for (x, y) in interior_cells():
            self.assertFalse(chroma_result.mask[y][x],
                              f"interior ({x},{y}) must survive, unreached by the flood")
        # And the flood did clear the actual border/background area.
        self.assertTrue(chroma_result.mask[0][0])

        # Alpha mode: transparent border, opaque ink shell, and an
        # interior patch that is *also opaque* (so the alpha gate alone
        # cannot touch it either) but happens to equal BROWN in colour --
        # irrelevant in this mode, since colour is never consulted.
        def alpha_px(x, y):
            on_border = x == 0 or x == w - 1 or y == 0 or y == h - 1
            if on_border:
                return (0, 0, 0, 0)
            if (x, y) in interior_cells():
                return (*BROWN, 255)
            return (*ink, 255)

        alpha_result = make(alpha_px)
        self.assertEqual(alpha_result.mode, "alpha")
        for (x, y) in interior_cells():
            self.assertFalse(alpha_result.mask[y][x])

    def test_border_impurity_aborts_rather_than_producing_speckled_art(self):
        """More than 25% of the alpha-surviving border pixels are far from
        the reference colour -- a starfield fleck or a mis-nudged crop.
        This must abort, never quantise a guess."""
        w = h = 8

        def px(x, y):
            on_border = x == 0 or x == w - 1 or y == 0 or y == h - 1
            if on_border and x < 3:
                # Nearly a third of the border pixels are a wildly
                # different colour -- more than the 25% threshold.
                return (10, 200, 10, 255)
            if on_border:
                return (*BROWN, 255)
            return (255, 150, 50, 255)

        img = make_image(w, h, px)
        with self.assertRaises(gen.GeneratorError) as ctx:
            gen.background_mask(img, (0, 0, w, h), alpha_min=128,
                                 bg_tol=60, halo_tol=110)
        self.assertIn("not clean background", str(ctx.exception))

    def test_clean_border_within_tolerance_does_not_abort(self):
        w = h = 8

        def px(x, y):
            on_border = x == 0 or x == w - 1 or y == 0 or y == h - 1
            return (*BROWN, 255) if on_border else (255, 150, 50, 255)

        img = make_image(w, h, px)
        result = gen.background_mask(img, (0, 0, w, h), alpha_min=128,
                                      bg_tol=60, halo_tol=110)
        self.assertEqual(result.mode, "chroma")


# --------------------------------------------------------------------------
# Fit (T3, plan §1 decision 4 / spec D2)
# --------------------------------------------------------------------------


class FitPolicyTest(unittest.TestCase):
    def test_anisotropic_fit_fills_target_touching_all_four_edges(self):
        """A 3:1 source (30x10), entirely non-background, fitted into a
        10x10 square target. Box-mapping applies each axis independently
        (3x reduction in x, 1x in y) rather than preserving the source's
        aspect ratio -- so the target is foreground edge to edge, not
        letterboxed with blank columns the way an aspect-preserving fit
        into a square would leave (D2's whole point: the frozen box, not
        the sheet's proportions, is the design constraint)."""
        w, h = 30, 10
        target_w = target_h = 10
        mask = [[False] * w for _ in range(h)]  # nothing is background
        rgb = [[(255, 150, 50)] * w for _ in range(h)]
        grid = gen.fit_region(w, h, mask, rgb, target_w, target_h,
                               coverage_min=50)
        for x in range(target_w):
            self.assertIsNotNone(grid[0][x], f"top edge at x={x}")
            self.assertIsNotNone(grid[target_h - 1][x], f"bottom edge at x={x}")
        for y in range(target_h):
            self.assertIsNotNone(grid[y][0], f"left edge at y={y}")
            self.assertIsNotNone(grid[y][target_w - 1], f"right edge at y={y}")

    def test_below_coverage_threshold_becomes_black(self):
        # A 2x1 -> 1x1 target cell where only one of two source pixels is
        # foreground: 50% coverage, at the default 50% minimum this is
        # exactly the boundary and must pass (not <). The foreground
        # pixel is exact BRIGHT_ORANGE so the winning index is unambiguous
        # (index 3), independent of the vote mechanics under test here.
        mask = [[False, True]]
        rgb = [[(255, 153, 51), (0, 0, 0)]]
        grid = gen.fit_region(2, 1, mask, rgb, 1, 1, coverage_min=50)
        self.assertEqual(grid[0][0], 3)

        # Below the threshold: only the background pixel's "coverage" (0%)
        # -- must become BLACK (None).
        mask_all_bg = [[True, True]]
        rgb_all_bg = [[(10, 20, 30), (10, 20, 30)]]
        grid_bg = gen.fit_region(2, 1, mask_all_bg, rgb_all_bg, 1, 1,
                                  coverage_min=50)
        self.assertIsNone(grid_bg[0][0])

    def test_vote_ignores_background_pixels(self):
        # 1x1 target cell covering a 1x3 source strip: one background
        # pixel (excluded) and two foreground pixels, both exact ORANGE.
        mask = [[True, False, False]]
        orange = (179, 89, 0)
        rgb = [[(999, 999, 999), orange, orange]]
        grid = gen.fit_region(3, 1, mask, rgb, 1, 1, coverage_min=0)
        # index 2 = ORANGE; the background pixel's absurd value never
        # enters the vote at all.
        self.assertEqual(grid[0][0], 2)

    def test_majority_vote_picks_the_most_common_quantised_colour(self):
        # Three foreground pixels: two quantise to ORANGE, one to
        # BRIGHT_ORANGE -- a mean-then-quantise approach would blend
        # these into some other colour entirely; voting on
        # already-quantised pixels picks the actual majority, ORANGE.
        mask = [[False, False, False]]
        rgb = [[(179, 89, 0), (179, 89, 0), (255, 153, 51)]]
        grid = gen.fit_region(3, 1, mask, rgb, 1, 1, coverage_min=0)
        self.assertEqual(grid[0][0], 2)

    def test_majority_vote_tie_breaks_to_the_lowest_enumerator(self):
        # One pixel each of BLACK-ish and DARK_ORANGE-ish (both quantise
        # cleanly, distinct indices, equal vote count of one each) --
        # ties break to the lower enumerator, exactly quantize()'s own
        # rule, applied consistently at the voting stage too.
        mask = [[False, False]]
        rgb = [[(0, 0, 0), (77, 38, 0)]]
        grid = gen.fit_region(2, 1, mask, rgb, 1, 1, coverage_min=0)
        self.assertEqual(grid[0][0], 0)


# --------------------------------------------------------------------------
# Quantisation (T3, plan §1 decision 5 / AC-3.8)
# --------------------------------------------------------------------------


class QuantisationTest(unittest.TestCase):
    def test_tie_breaks_to_the_lowest_enumerator(self):
        # A synthetic two-entry palette with an exact, constructed tie:
        # (5,0,0) is equidistant (dist2=25) from both (0,0,0) and
        # (10,0,0). Isolated from the real 4-colour palette on purpose,
        # so the tie-break logic is proven exactly rather than by luck of
        # PALETTE's actual RGB values.
        palette = {0: (0, 0, 0), 1: (10, 0, 0)}
        self.assertEqual(gen._nearest_palette_index((5, 0, 0), palette), 0)
        # Not a tie: closer to index 1.
        self.assertEqual(gen._nearest_palette_index((7, 0, 0), palette), 1)

    def test_ties_break_low_even_with_more_than_two_candidates(self):
        palette = {0: (0, 0, 0), 1: (10, 0, 0), 2: (5, 5, 5), 3: (5, -5, 0)}
        # (5,0,0): dist2 to 0 is 25, to 1 is 25, to 2 is 50, to 3 is 25.
        # Three-way tie among 0, 1, 3 -- lowest wins.
        self.assertEqual(gen._nearest_palette_index((5, 0, 0), palette), 0)

    def test_real_palette_classifies_each_colour_to_its_own_entry(self):
        # Exact palette values (fb_view.PALETTE / color.h) each quantise
        # to themselves -- the trivial, load-bearing case.
        for index, rgb in fb_view.PALETTE.items():
            self.assertEqual(gen.quantize(rgb), index)

    def test_real_palette_classifies_a_nearby_colour(self):
        # Slightly off ORANGE, still clearly closer to ORANGE than to any
        # other entry.
        self.assertEqual(gen.quantize((175, 92, 3)), 2)

    def test_exclude_removes_a_candidate_even_when_it_is_the_true_nearest(self):
        # T10, AC-2.3: exact BRIGHT_ORANGE (index 3) is by construction its
        # own nearest match -- excluding it must fall through to the next
        # nearest (ORANGE) rather than erroring or ignoring the exclusion.
        bright = (255, 153, 51)
        self.assertEqual(gen.quantize(bright), 3)
        self.assertEqual(gen.quantize(bright, exclude=(3,)), 2)

    def test_exclude_does_not_disturb_an_unrelated_classification(self):
        for index, rgb in fb_view.PALETTE.items():
            if index == 3:
                continue
            self.assertEqual(gen.quantize(rgb, exclude=(3,)), index)


# --------------------------------------------------------------------------
# End-to-end determinism (T3, AC-3.3/AC-3.8)
# --------------------------------------------------------------------------


class DeterminismTest(unittest.TestCase):
    def test_generate_artifact_grid_is_byte_identical_on_repeat_runs(self):
        w = h = 14
        ink = (255, 150, 50)
        glow = (BROWN[0] + 60, BROWN[1] + 35, BROWN[2] + 5)

        def px(x, y):
            ring = max(abs(x - (w - 1) / 2), abs(y - (h - 1) / 2))
            if ring >= 5:
                return (*BROWN, 255)
            if ring >= 4:
                return (*glow, 255)
            return (*ink, 255)

        img = make_image(w, h, px)
        first = gen.generate_artifact_grid(img, (0, 0, w, h), 12, 12,
                                            alpha_min=128, bg_tol=60,
                                            halo_tol=110, coverage_min=50)
        second = gen.generate_artifact_grid(img, (0, 0, w, h), 12, 12,
                                             alpha_min=128, bg_tol=60,
                                             halo_tol=110, coverage_min=50)
        self.assertEqual(first, second)


# --------------------------------------------------------------------------
# Emitter (T4, plan §1 decision 2 / AC-1.2, AC-3.1, AC-3.3, AC-3.4, AC-3.5)
# --------------------------------------------------------------------------


class ArtifactSpecTest(unittest.TestCase):
    def test_five_numbers_derive_aspect_preserving_width(self):
        name, region, tw, th, aspect, exclude = gen.parse_artifact_spec(
            "Logo=45,20,287,95,56")
        self.assertEqual(name, "Logo")
        self.assertEqual(region, (45, 20, 287, 95))
        self.assertEqual(th, 56)
        self.assertTrue(aspect)
        # (56*287 + 47) // 95 -- round-half-up, integer only.
        self.assertEqual(tw, 169)
        self.assertEqual(exclude, ())

    def test_six_numbers_take_target_width_literally(self):
        name, region, tw, th, aspect, exclude = gen.parse_artifact_spec(
            "Player=558,33,54,87,12,12")
        self.assertEqual(name, "Player")
        self.assertEqual((tw, th), (12, 12))
        self.assertFalse(aspect)
        self.assertEqual(exclude, ())

    def test_rejects_lowercase_or_non_identifier_name(self):
        for bad in ["logo=1,2,3,4,5", "123=1,2,3,4,5", "Lo go=1,2,3,4,5"]:
            with self.assertRaises(gen.GeneratorError):
                gen.parse_artifact_spec(bad)

    def test_rejects_missing_equals_or_wrong_count(self):
        for bad in ["Logo1,2,3,4,5", "Logo=1,2,3", "Logo=1,2,3,4,5,6,7"]:
            with self.assertRaises(gen.GeneratorError):
                gen.parse_artifact_spec(bad)

    def test_rejects_non_positive_dimensions(self):
        for bad in ["Logo=1,2,0,4,5", "Logo=1,2,3,4,0", "Logo=1,2,3,4,5,0"]:
            with self.assertRaises(gen.GeneratorError):
                gen.parse_artifact_spec(bad)


class RenderAsciiTest(unittest.TestCase):
    def test_maps_each_palette_index_to_its_symbol(self):
        grid = [[0, 1, 2, 3]]
        self.assertEqual(gen.render_ascii(grid), " .+#")

    def test_one_line_per_row(self):
        grid = [[0, 3], [3, 0]]
        self.assertEqual(gen.render_ascii(grid), " #\n# ")


class RegenerateCommandTest(unittest.TestCase):
    def test_is_stable_regardless_of_argv_order(self):
        argv_a = [
            "--emit", "--source", "S.png", "--output", "O.h",
            "--artifact", "Logo=1,2,3,4,5",
        ]
        argv_b = [
            "--artifact", "Logo=1,2,3,4,5",
            "--output", "O.h", "--source", "S.png", "--emit",
        ]
        parser_a, args_a = _parse_for_test(argv_a)
        parser_b, args_b = _parse_for_test(argv_b)
        self.assertEqual(gen._build_regenerate_command(args_a),
                          gen._build_regenerate_command(args_b))

    def test_includes_every_artifact_in_order(self):
        _parser, args = _parse_for_test([
            "--emit", "--source", "S.png", "--output", "O.h",
            "--artifact", "Logo=1,2,3,4,5",
            "--artifact", "Player=5,6,7,8,9,9",
        ])
        cmd = gen._build_regenerate_command(args)
        self.assertIn("--artifact Logo=1,2,3,4,5", cmd)
        self.assertIn("--artifact Player=5,6,7,8,9,9", cmd)
        self.assertLess(cmd.index("Logo"), cmd.index("Player"))


def _parse_for_test(argv):
    import argparse
    # A minimal stand-in for gen.main's parser, covering only the fields
    # _build_regenerate_command reads -- avoids invoking main() (which
    # would also require --alpha-min etc. defaults) just to get an
    # argparse.Namespace.
    p = argparse.ArgumentParser()
    p.add_argument("--emit", action="store_true")
    p.add_argument("--source")
    p.add_argument("--output")
    p.add_argument("--artifact", action="append", default=[])
    p.add_argument("--enemy-artifact", action="append", default=[])
    p.add_argument("--alpha-min", type=int, default=128)
    p.add_argument("--bg-tol", type=int, default=60)
    p.add_argument("--halo-tol", type=int, default=110)
    p.add_argument("--coverage-min", type=int, default=50)
    return p, p.parse_args(argv)


class EmitHeaderTest(unittest.TestCase):
    def _write_source(self, tmpdir, w=20, h=20):
        ink = (255, 150, 50)

        def px(x, y):
            on_border = x == 0 or x == w - 1 or y == 0 or y == h - 1
            return (0, 0, 0, 0) if on_border else (*ink, 255)

        img = make_image(w, h, px)
        # Round-trip through a real PNG (colour type 6) so this test also
        # exercises emit_header's own read_png call, not a pre-built Image.
        raw = bytearray()
        for y in range(h):
            raw.append(0)
            for x in range(w):
                raw += bytes(px(x, y))
        ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
        data = (
            fb_view.PNG_SIGNATURE
            + fb_view._chunk(b"IHDR", ihdr)
            + fb_view._chunk(b"IDAT", zlib.compress(bytes(raw), 9))
            + fb_view._chunk(b"IEND", b"")
        )
        path = os.path.join(tmpdir, "source.png")
        with open(path, "wb") as f:
            f.write(data)
        return path

    def test_emits_a_compilable_looking_header_with_full_banner(self):
        with tempfile.TemporaryDirectory() as d:
            source = self._write_source(d)
            output = os.path.join(d, "out.h")
            specs = [gen.parse_artifact_spec("Logo=1,1,18,18,10")]
            text = gen.emit_header(output, source, specs,
                                    alpha_min=128, bg_tol=60, halo_tol=110,
                                    coverage_min=50,
                                    regenerate_cmd="the regenerate command")
            with open(output) as f:
                self.assertEqual(f.read(), text)

            # Every line up to the first non-banner line is a comment --
            # the assets/-reference lint rule's own exemption.
            banner_lines = text.split("\n")
            for line in banner_lines:
                if line == "#pragma once":
                    break
                self.assertTrue(line == "" or line.startswith("//"),
                                 f"non-comment banner line: {line!r}")
            else:
                self.fail("banner never ended with #pragma once")

            self.assertIn("Source: " + source, text)
            self.assertIn(gen.source_sha256(source), text)
            self.assertIn("the regenerate command", text)
            self.assertIn("inline constexpr int32_t kLogoWidth", text)
            self.assertIn("inline constexpr int32_t kLogoHeight", text)
            self.assertIn("k{}Sprite".format("Logo").replace("k{}", "kLogo"),
                           text)
            self.assertIn("generatedSpritePixel", text)
            self.assertIn("std::abort()", text)
            self.assertIn("namespace steamcore::games {", text)

    def test_is_byte_identical_across_repeated_emits(self):
        with tempfile.TemporaryDirectory() as d:
            source = self._write_source(d)
            output = os.path.join(d, "out.h")
            specs = [gen.parse_artifact_spec("Logo=1,1,18,18,10")]
            first = gen.emit_header(output, source, specs, 128, 60, 110, 50,
                                     "cmd")
            second = gen.emit_header(output, source, specs, 128, 60, 110, 50,
                                      "cmd")
            self.assertEqual(first, second)

    def test_writes_multiple_artifacts_into_one_file(self):
        with tempfile.TemporaryDirectory() as d:
            source = self._write_source(d)
            output = os.path.join(d, "out.h")
            specs = [
                gen.parse_artifact_spec("Logo=1,1,18,18,10"),
                gen.parse_artifact_spec("Player=1,1,18,18,6,6"),
            ]
            text = gen.emit_header(output, source, specs, 128, 60, 110, 50,
                                    "cmd")
            # Logo: five-number spec, standalone -- owns its own Sprite and
            # width/height constants.
            self.assertIn("kLogoWidth", text)
            self.assertIn("kLogoSprite", text)
            # Player: six-number spec, a cast member -- pixels only, no
            # width/height constants or Sprite of its own (those are
            # already frozen in galactic_invasion_art.h, A6).
            self.assertIn("kPlayerPixels", text)
            self.assertNotIn("kPlayerWidth", text)
            self.assertNotIn("kPlayerSprite", text)
            # The shared converter appears exactly once, not once per
            # artifact -- both blocks must use the same definitions.
            self.assertEqual(text.count("generatedSpritePixel(const char*"),
                              1)

    def test_six_number_spec_emits_a_cast_member_without_own_dimensions(self):
        with tempfile.TemporaryDirectory() as d:
            source = self._write_source(d)
            output = os.path.join(d, "out.h")
            specs = [gen.parse_artifact_spec("Player=1,1,18,18,6,6")]
            text = gen.emit_header(output, source, specs, 128, 60, 110, 50,
                                    "cmd")
            self.assertIn("kPlayerRows", text)
            self.assertIn("kPlayerPixels", text)
            self.assertIn("std::array<Color, 6 * 6> kPlayerPixels", text)
            self.assertNotIn("kPlayerWidth", text)
            self.assertNotIn("kPlayerHeight", text)
            self.assertNotIn("kPlayerSprite", text)
            self.assertIn("cast member", text)

    def test_five_number_spec_emits_a_standalone_artifact_with_dimensions(self):
        with tempfile.TemporaryDirectory() as d:
            source = self._write_source(d)
            output = os.path.join(d, "out.h")
            specs = [gen.parse_artifact_spec("Logo=1,1,18,18,10")]
            text = gen.emit_header(output, source, specs, 128, 60, 110, 50,
                                    "cmd")
            self.assertIn("inline constexpr int32_t kLogoWidth", text)
            self.assertIn("inline constexpr int32_t kLogoHeight", text)
            self.assertIn("inline constexpr Sprite kLogoSprite", text)

    def test_rejects_duplicate_artifact_names_via_main(self):
        with tempfile.TemporaryDirectory() as d:
            source = self._write_source(d)
            output = os.path.join(d, "out.h")
            argv = [
                "--emit", "--source", source, "--output", output,
                "--artifact", "Logo=1,1,18,18,10",
                "--artifact", "Logo=1,1,10,10,5",
            ]
            with self.assertRaises(gen.GeneratorError) as ctx:
                gen.main(argv)
        self.assertIn("duplicate", str(ctx.exception))

    @staticmethod
    def _rows_block(text, name):
        """The row-string literals between k{name}Rows[...] = { and the
        matching }; -- isolated so a check of *which symbols an artifact
        actually used* doesn't accidentally match the shared converter's
        own switch-case, which always mentions every Color enumerator by
        name regardless of what any artifact contains."""
        start = text.index(f"k{name}Rows[")
        start = text.index("{", start)
        end = text.index("};", start)
        return text[start:end]

    def test_enemy_artifact_never_emits_a_bright_orange_symbol(self):
        # T10, AC-2.3: _write_source's fill ink (255,150,50) is close
        # enough to BRIGHT_ORANGE to quantise there unexcluded (the whole
        # point of this fixture) -- routed as --enemy-artifact, it must
        # land on the next-nearest ink instead, never the '#' symbol
        # (index 3, PALETTE_CHARS).
        with tempfile.TemporaryDirectory() as d:
            source = self._write_source(d)
            output = os.path.join(d, "out.h")
            argv = [
                "--emit", "--source", source, "--output", output,
                "--enemy-artifact", "Enemy=0,0,20,20,6,6",
            ]
            gen.main(argv)
            with open(output) as f:
                text = f.read()
            rows = self._rows_block(text, "Enemy")
            self.assertIn("+", rows)  # ORANGE: the fixture's fill is lit
            self.assertNotIn("#", rows)

    def test_plain_artifact_is_unaffected_by_the_enemy_exclusion(self):
        # The same fixture, routed as an ordinary --artifact, keeps
        # quantising to BRIGHT_ORANGE -- proving --enemy-artifact's
        # restriction is per-artifact, not global to the run.
        with tempfile.TemporaryDirectory() as d:
            source = self._write_source(d)
            output = os.path.join(d, "out.h")
            argv = [
                "--emit", "--source", source, "--output", output,
                "--artifact", "Player=0,0,20,20,6,6",
            ]
            gen.main(argv)
            with open(output) as f:
                text = f.read()
            rows = self._rows_block(text, "Player")
            self.assertIn("#", rows)


if __name__ == "__main__":
    unittest.main()
