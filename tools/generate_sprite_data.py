#!/usr/bin/env python3
"""Reduce regions of a concept-art sheet to compile-time C++ sprite data.

This is an offline art *reduction* step, not an asset pipeline: it runs by
hand, its output is committed, and nothing the firmware builds or runs ever
reads an image. See docs/sprite-generator.md.

Standard library only, deliberately. tools/check_constraints.sh enforces a
stdlib allowlist over tools/*.py, so pulling in Pillow here would fail
`make lint` -- and fb_view.py already hand-builds PNGs with zlib+struct,
which makes the decoder below its inverse rather than new territory.
PALETTE is reused out of fb_view for the same reason: one Python-side
palette, already cross-checked against the C++ one by test_roundtrip.py.

(Both paragraphs above are worded to keep the words "import" and "from"
off the start of a line: that allowlist check deliberately matches
unanchored, so it sees indented imports inside try/except blocks -- and
prose in a docstring alike.)
"""

import argparse
import hashlib
import os
import re
import struct
import sys
import tempfile
import zlib
from collections import deque

from fb_view import PALETTE, PNG_SIGNATURE


class GeneratorError(Exception):
    """A named, expected failure. Printed to stderr; exit code 1.

    Every path that raises this leaves no output file written or partially
    overwritten (AC-3.6) -- the emitter writes to a temp file and renames,
    so an abort mid-run cannot leave a half-written header behind.
    """


# --------------------------------------------------------------------------
# PNG decoding
#
# Supported subset, verified against the actual source on 2026-09-16
# (IHDR: 1536x1024, bit depth 8, colour type 6, interlace 0): colour type 6
# (RGBA) and 2 (RGB), bit depth 8, non-interlaced. Colour type 2 is not
# speculative -- it is exactly what fb_view.encode_png writes, so the unit
# tests can build their own fixtures without a second encoder.
#
# Anything outside that subset is a named failure, never a best-effort
# guess: silently mis-decoding an image would produce plausible-looking
# wrong art, which is worse than not running.
# --------------------------------------------------------------------------

_CHANNELS = {2: 3, 6: 4}


def _iter_chunks(data):
    offset = len(PNG_SIGNATURE)
    while offset + 8 <= len(data):
        (length,) = struct.unpack(">I", data[offset : offset + 4])
        kind = data[offset + 4 : offset + 8]
        start = offset + 8
        end = start + length
        if end + 4 > len(data):
            raise GeneratorError("truncated PNG: chunk extends past end of file")
        yield kind, data[start:end]
        offset = end + 4  # skip CRC


def _paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def _unfilter(raw, width, height, channels):
    """Undo the per-scanline filter (PNG spec 9.2), types 0-4."""
    stride = width * channels
    out = bytearray(stride * height)
    previous = bytearray(stride)
    pos = 0
    for row in range(height):
        if pos >= len(raw):
            raise GeneratorError("truncated PNG: image data ended early")
        filter_type = raw[pos]
        pos += 1
        line = bytearray(raw[pos : pos + stride])
        if len(line) != stride:
            raise GeneratorError("truncated PNG: image data ended early")
        pos += stride

        if filter_type == 0:
            pass
        elif filter_type == 1:
            for i in range(channels, stride):
                line[i] = (line[i] + line[i - channels]) & 0xFF
        elif filter_type == 2:
            for i in range(stride):
                line[i] = (line[i] + previous[i]) & 0xFF
        elif filter_type == 3:
            for i in range(stride):
                left = line[i - channels] if i >= channels else 0
                line[i] = (line[i] + ((left + previous[i]) >> 1)) & 0xFF
        elif filter_type == 4:
            for i in range(stride):
                left = line[i - channels] if i >= channels else 0
                upper_left = previous[i - channels] if i >= channels else 0
                line[i] = (line[i] + _paeth(left, previous[i], upper_left)) & 0xFF
        else:
            raise GeneratorError(
                f"unsupported PNG scanline filter type {filter_type} "
                f"(supported: 0-4)"
            )

        out[row * stride : (row + 1) * stride] = line
        previous = line
    return out


class Image:
    """A decoded RGBA image. Alpha is 255 everywhere for colour type 2."""

    def __init__(self, width, height, pixels):
        self.width = width
        self.height = height
        self.pixels = pixels  # bytearray, 4 bytes per pixel, RGBA

    def at(self, x, y):
        i = (y * self.width + x) * 4
        p = self.pixels
        return p[i], p[i + 1], p[i + 2], p[i + 3]


def read_png(path):
    try:
        with open(path, "rb") as f:
            data = f.read()
    except OSError as exc:
        raise GeneratorError(f"cannot read source image {path}: {exc}")

    if not data.startswith(PNG_SIGNATURE):
        raise GeneratorError(f"not a PNG file (bad signature): {path}")

    width = height = depth = colour_type = interlace = None
    idat = bytearray()
    for kind, payload in _iter_chunks(data):
        if kind == b"IHDR":
            (width, height, depth, colour_type, _comp, _filt, interlace) = (
                struct.unpack(">IIBBBBB", payload)
            )
        elif kind == b"IDAT":
            idat += payload
        elif kind == b"IEND":
            break

    if width is None:
        raise GeneratorError(f"PNG has no IHDR chunk: {path}")
    if depth != 8:
        raise GeneratorError(
            f"unsupported PNG bit depth {depth} (only 8 is supported): {path}"
        )
    if colour_type not in _CHANNELS:
        raise GeneratorError(
            f"unsupported PNG colour type {colour_type} "
            f"(only 2 = RGB and 6 = RGBA are supported): {path}"
        )
    if interlace != 0:
        raise GeneratorError(
            f"interlaced PNGs are not supported (Adam7): {path}"
        )
    if not idat:
        raise GeneratorError(f"PNG has no image data: {path}")

    try:
        raw = zlib.decompress(bytes(idat))
    except zlib.error as exc:
        raise GeneratorError(f"corrupt PNG image data in {path}: {exc}")

    channels = _CHANNELS[colour_type]
    flat = _unfilter(raw, width, height, channels)

    rgba = bytearray(width * height * 4)
    if channels == 4:
        rgba[:] = flat
    else:
        for i in range(width * height):
            rgba[i * 4 + 0] = flat[i * 3 + 0]
            rgba[i * 4 + 1] = flat[i * 3 + 1]
            rgba[i * 4 + 2] = flat[i * 3 + 2]
            rgba[i * 4 + 3] = 255
    return Image(width, height, rgba)


def source_sha256(path):
    try:
        with open(path, "rb") as f:
            return hashlib.sha256(f.read()).hexdigest()
    except OSError as exc:
        raise GeneratorError(f"cannot read source image {path}: {exc}")


# --------------------------------------------------------------------------
# Regions
# --------------------------------------------------------------------------


def parse_region(text):
    parts = text.split(",")
    if len(parts) != 4:
        raise GeneratorError(
            f"region must be X,Y,W,H (four integers), got: {text}"
        )
    try:
        x, y, w, h = (int(p) for p in parts)
    except ValueError:
        raise GeneratorError(f"region must be four integers, got: {text}")
    if w <= 0 or h <= 0:
        raise GeneratorError(f"region width and height must be positive: {text}")
    return x, y, w, h


def check_region(image, region):
    x, y, w, h = region
    if x < 0 or y < 0 or x + w > image.width or y + h > image.height:
        raise GeneratorError(
            f"region {x},{y},{w},{h} extends beyond the source image "
            f"({image.width}x{image.height})"
        )


def check_target(region, target_w, target_h):
    _x, _y, w, h = region
    if target_w > w or target_h > h:
        raise GeneratorError(
            f"target size {target_w}x{target_h} is larger than the source "
            f"region {w}x{h}; upscaling is not supported"
        )


# --------------------------------------------------------------------------
# Background rule (plan §1 decision 3 / spec D1): alpha gate, then a
# border-seeded chroma flood, then a halo closure -- all integer
# arithmetic, in this fixed order, applied before fit and quantisation.
# --------------------------------------------------------------------------


def _dist2(p, q):
    return (p[0] - q[0]) ** 2 + (p[1] - q[1]) ** 2 + (p[2] - q[2]) ** 2


def _median(values):
    # A deterministic, integer, per-channel median: the lower of the two
    # middle values on an even count, never an averaged (and possibly
    # fractional) one. Documented in the banner alongside the other tie
    # rules, so this specific choice is a recorded parameter, not a
    # library default someone has to go look up.
    s = sorted(values)
    return s[len(s) // 2]


def _reference_colour(rgb_values):
    # Materialised once: rgb_values may be a single-use generator, and
    # each channel below needs its own independent pass over it.
    values = list(rgb_values)
    return (
        _median([v[0] for v in values]),
        _median([v[1] for v in values]),
        _median([v[2] for v in values]),
    )


def _border_coords(w, h):
    coords = []
    for i in range(w):
        coords.append((i, 0))
        if h > 1:
            coords.append((i, h - 1))
    for j in range(1, h - 1):
        coords.append((0, j))
        if w > 1:
            coords.append((w - 1, j))
    return coords


class BackgroundResult:
    """mask[j][i] is True iff pixel (i, j) of the region becomes BLACK.
    rgb[j][i] is that pixel's original colour (meaningless where mask is
    True) -- fit_region uses both. mode is "alpha" when every border pixel
    was already cleared by the alpha gate (decisions (b)-(d) never ran, no
    reference colour exists) or "chroma" otherwise; reference is that
    run's B, or None in alpha mode. Both are recorded in the provenance
    banner (AC-3.4), never left implicit."""

    def __init__(self, mask, rgb, mode, reference):
        self.mask = mask
        self.rgb = rgb
        self.mode = mode
        self.reference = reference


def background_mask(image, region, alpha_min, bg_tol, halo_tol):
    x0, y0, w, h = region
    rgb = [[(0, 0, 0)] * w for _ in range(h)]
    alpha_gated = [[False] * w for _ in range(h)]
    for j in range(h):
        for i in range(w):
            r, g, b, a = image.at(x0 + i, y0 + j)
            rgb[j][i] = (r, g, b)
            alpha_gated[j][i] = a < alpha_min

    border = _border_coords(w, h)
    surviving_border = [(i, j) for (i, j) in border if not alpha_gated[j][i]]

    # (b), collapsed: nothing on the border survived the alpha gate, so
    # there is no reference colour to build and (c)/(d) have nothing left
    # to do -- recorded as its own mode, not run as a no-op chroma pass.
    if not surviving_border:
        mask = [row[:] for row in alpha_gated]
        return BackgroundResult(mask, rgb, "alpha", None)

    reference = _reference_colour(rgb[j][i] for (i, j) in surviving_border)

    # Border-purity guard: this crop's border must actually be background,
    # or the reference colour (and everything downstream of it) is
    # meaningless. A starfield speckle or a mis-nudged crop shows up here
    # as a loud abort, never as speckled art (mirrors AC-3.6's postures
    # for the other four named failures).
    bad = sum(
        1
        for (i, j) in surviving_border
        if _dist2(rgb[j][i], reference) > bg_tol * bg_tol
    )
    if bad * 100 > 25 * len(surviving_border):
        raise GeneratorError(
            f"crop border is not clean background: {bad} of "
            f"{len(surviving_border)} alpha-surviving border pixels lie "
            f"further than --bg-tol {bg_tol} from the reference colour "
            f"{reference} -- nudge the region or the tolerance"
        )

    # (c): 4-connected BFS from every border pixel. An already-alpha-gated
    # border pixel is itself a seed (it is background, just not via
    # colour); a surviving one is accepted, and becomes a seed in turn,
    # iff it is within bg_tol of the reference. Seeded from one global
    # reference colour rather than growing outward from a running local
    # average, so the result does not depend on visit order -- only on
    # which pixels are reachable through background-coloured territory.
    mask = [row[:] for row in alpha_gated]
    visited = [[False] * w for _ in range(h)]
    queue = deque()

    def visit(i, j):
        if visited[j][i]:
            return
        visited[j][i] = True
        if mask[j][i]:
            queue.append((i, j))
            return
        if _dist2(rgb[j][i], reference) <= bg_tol * bg_tol:
            mask[j][i] = True
            queue.append((i, j))

    for (i, j) in border:
        visit(i, j)
    while queue:
        i, j = queue.popleft()
        for (ni, nj) in ((i - 1, j), (i + 1, j), (i, j - 1), (i, j + 1)):
            if 0 <= ni < w and 0 <= nj < h:
                visit(ni, nj)

    # (d): halo closure to fixpoint. A surviving pixel within the softer
    # halo_tol that touches (8-adjacent) an already-BLACK pixel joins it
    # too. This is what erases a glow ring the bg_tol-only flood left
    # behind -- and it is the reason an interior dark mass, walled off by
    # ink the flood never reached, survives: nothing BLACK is ever
    # adjacent to it.
    changed = True
    while changed:
        changed = False
        for j in range(h):
            for i in range(w):
                if mask[j][i]:
                    continue
                if _dist2(rgb[j][i], reference) > halo_tol * halo_tol:
                    continue
                for (ni, nj) in (
                    (i - 1, j - 1), (i, j - 1), (i + 1, j - 1),
                    (i - 1, j), (i + 1, j),
                    (i - 1, j + 1), (i, j + 1), (i + 1, j + 1),
                ):
                    if 0 <= ni < w and 0 <= nj < h and mask[nj][ni]:
                        mask[j][i] = True
                        changed = True
                        break

    return BackgroundResult(mask, rgb, "chroma", reference)


# --------------------------------------------------------------------------
# Fit (plan §1 decision 4 / spec D2): integer box mapping + coverage rule.
# Sprites fill their frozen box (anisotropic scaling permitted); the logo
# preserves aspect by construction of the region and target it is called
# with, not by any special case in here.
# --------------------------------------------------------------------------


def fit_region(w, h, mask, rgb, target_w, target_h, coverage_min, exclude=()):
    """Returns a target_h x target_w grid; each cell is None (background,
    becomes BLACK) or the majority-vote palette index (ties break to the
    lowest Color enumerator, quantize()'s own rule) among its
    non-background source pixels' individually-quantized colours.

    User-corrected 2026-09-16 (galactic-invasion-artwork T9) from plan §1
    decision 4's original "integer mean of non-background pixels, then
    quantize the mean": that order does not survive detailed,
    anti-aliased source art at a large downscale factor. A systematic
    sweep (two different source regions, ~40 bg/halo/coverage
    combinations each) found *zero* parameter choices free of isolated
    single-pixel artifacts -- a minority ink blended into the mean at a
    cell boundary routinely pulled the quantized result to a colour
    matching neither neighbour, exactly what T8's own structural
    assertions (D11) exist to catch. Quantising every source pixel first
    and voting is far more resistant to that: a handful of anti-aliased
    outliers cannot outvote the cell's actual majority ink. The
    background rule, coverage threshold, box mapping and tie-break are
    all unchanged -- only the mean-then-quantize step became
    quantize-then-vote.
    """
    grid = [[None] * target_w for _ in range(target_h)]
    for ty in range(target_h):
        y0 = (ty * h) // target_h
        y1 = ((ty + 1) * h) // target_h
        for tx in range(target_w):
            x0 = (tx * w) // target_w
            x1 = ((tx + 1) * w) // target_w
            total = 0
            non_bg = 0
            votes = {}
            for y in range(y0, y1):
                for x in range(x0, x1):
                    total += 1
                    if not mask[y][x]:
                        non_bg += 1
                        idx = quantize(rgb[y][x], exclude=exclude)
                        votes[idx] = votes.get(idx, 0) + 1
            if non_bg * 100 < coverage_min * total:
                grid[ty][tx] = None
            else:
                best_idx, best_count = None, -1
                for idx in sorted(votes.keys()):
                    if votes[idx] > best_count:
                        best_idx, best_count = idx, votes[idx]
                grid[ty][tx] = best_idx
    return grid


# --------------------------------------------------------------------------
# Quantisation (plan §1 decision 5 / AC-3.8): nearest palette entry by
# squared Euclidean RGB distance on integers; ties break to the lowest
# Color enumerator. Total, float-free, so re-running on identical input is
# byte-identical (AC-3.3) by construction, not by convention.
# --------------------------------------------------------------------------


def _nearest_palette_index(rgb, palette):
    # Iterating indices ascending and replacing only on a strict '<' is
    # the entire tie-break rule: the first (lowest) index seen at the
    # minimum distance is the one that survives every later tie.
    best_index = None
    best_dist = None
    for index in sorted(palette.keys()):
        d = _dist2(rgb, palette[index])
        if best_dist is None or d < best_dist:
            best_dist = d
            best_index = index
    return best_index


def quantize(rgb, exclude=()):
    """Nearest of the four palette entries; index 0 is BLACK.

    `exclude` restricts the candidate set (AC-2.3): the concept sheet
    predates this project's per-side ink reservation, so its highlights
    quantise to BRIGHT_ORANGE regardless of which entity they belong to
    -- found generating the enemy fighter, whose brightest pixels
    otherwise landed on the one ink enemy-side data may never contain.
    Excluding it here is what makes that structurally impossible rather
    than merely reviewed for, mirroring how AC-2.3 is enforced on
    hand-authored art in galactic_invasion_art.h.
    """
    palette = {i: c for i, c in PALETTE.items() if i not in exclude}
    return _nearest_palette_index(rgb, palette)


def generate_artifact_grid(image, region, target_w, target_h,
                            alpha_min, bg_tol, halo_tol, coverage_min,
                            exclude=()):
    """The whole per-artifact pipeline: background rule, then fit (which
    now quantises and votes per source pixel internally). Returns (grid,
    mode, reference) -- grid is target_h x target_w of palette indices
    (0 = BLACK), mode/reference are the background rule's own findings
    for this artefact's provenance banner (AC-3.4). `exclude` forwards to
    quantize() -- AC-2.3's per-side ink reservation, enforced structurally
    for enemy-side artifacts rather than left to what the source happens
    to contain."""
    check_region(image, region)
    check_target(region, target_w, target_h)
    _x, _y, w, h = region
    bg = background_mask(image, region, alpha_min, bg_tol, halo_tol)
    fitted = fit_region(w, h, bg.mask, bg.rgb, target_w, target_h,
                         coverage_min, exclude=exclude)
    grid = [[0] * target_w for _ in range(target_h)]
    for ty in range(target_h):
        for tx in range(target_w):
            idx = fitted[ty][tx]
            grid[ty][tx] = 0 if idx is None else idx
    return grid, bg.mode, bg.reference


# --------------------------------------------------------------------------
# Alpha finding (T2's measured verdict, recorded rather than assumed)
# --------------------------------------------------------------------------


def alpha_histogram(image, region=None):
    """Counts of fully opaque, fully transparent and partial pixels.

    Exact counts alone proved actively misleading on the real sheet, which
    reports 0% at exactly 255 and 9% at exactly 0 while being, in fact,
    cleanly separated: its background sits at alpha 1-26 rather than at 0.
    alpha_buckets() below is what the verdict is actually read from.
    """
    if region is None:
        x0, y0, w, h = 0, 0, image.width, image.height
    else:
        x0, y0, w, h = region
    opaque = transparent = partial = 0
    for y in range(y0, y0 + h):
        for x in range(x0, x0 + w):
            a = image.at(x, y)[3]
            if a == 255:
                opaque += 1
            elif a == 0:
                transparent += 1
            else:
                partial += 1
    return opaque, transparent, partial


def alpha_buckets(image, region=None, step=1):
    """Alpha distribution in 16 buckets -- the shape, not just the extremes.

    A sheet exported from a paint program is rarely 0-or-255: what matters
    for the background rule is whether the distribution is *bimodal* (a
    background cluster low, an artwork cluster high, little in between,
    which an alpha gate separates cleanly) or flat (which it cannot).
    """
    if region is None:
        x0, y0, w, h = 0, 0, image.width, image.height
    else:
        x0, y0, w, h = region
    buckets = [0] * 16
    for y in range(y0, y0 + h, step):
        for x in range(x0, x0 + w, step):
            buckets[image.at(x, y)[3] * 16 // 256] += 1
    return buckets


def alpha_verdict(buckets, alpha_min):
    """'alpha-separated' or 'not alpha-separated', with the reason.

    Separated means: most pixels fall clearly on one side or the other of
    the gate, with little mass straddling it. The straddling band is the
    part an alpha gate cannot classify, so it is what decides whether the
    chroma flood has to carry the work instead.
    """
    total = sum(buckets) or 1
    gate = alpha_min * 16 // 256
    below = sum(buckets[:gate])
    above = sum(buckets[gate + 1 :])
    straddling = buckets[gate]
    if below > 0 and above > 0 and straddling * 100 // total < 5:
        return (
            "alpha-separated",
            f"{below * 100 // total}% below the gate, "
            f"{above * 100 // total}% above it, "
            f"{straddling * 100 // total}% straddling",
        )
    return (
        "not alpha-separated",
        f"{straddling * 100 // total}% of pixels sit in the gate's own "
        f"bucket; the chroma flood must carry the background rule",
    )


def _atomic_write(path, text):
    out_dir = os.path.dirname(os.path.abspath(path)) or "."
    if not os.path.isdir(out_dir):
        raise GeneratorError(f"output directory does not exist: {out_dir}")
    try:
        fd, tmp = tempfile.mkstemp(dir=out_dir, prefix=".gen_sprite_", suffix=".tmp")
    except OSError as exc:
        raise GeneratorError(f"cannot write near {path}: {exc}")
    try:
        with os.fdopen(fd, "w") as f:
            f.write(text)
        os.replace(tmp, path)
    except OSError as exc:
        try:
            os.unlink(tmp)
        except OSError:
            pass
        raise GeneratorError(f"cannot write output file {path}: {exc}")


# --------------------------------------------------------------------------
# Emitter (plan §1 decision 2 / AC-3.1, AC-3.4, AC-3.5): row-string art in
# the existing convention, widened to four symbols, plus a machine-written
# provenance banner. Output format matches what a human would hand-author
# -- diffable and reviewable, not a Color[] blob (plan §1, rejected
# alternatives).
# --------------------------------------------------------------------------

PALETTE_CHARS = {0: " ", 1: ".", 2: "+", 3: "#"}


def render_ascii(grid):
    return "\n".join("".join(PALETTE_CHARS[c] for c in row) for row in grid)


def parse_artifact_spec(text):
    """NAME=X,Y,W,H,TH[,TW].

    Five numbers: TW is derived to preserve the source region's aspect
    ratio -- the logo's policy (D2). Six numbers: TW is taken literally --
    "sprites fill their frozen box" (D2), used for every cast member with
    a fixed, possibly non-square hitbox.

    Returns a 6-tuple ending in an `exclude` palette-index tuple, always
    `()` here -- main() overrides it to `(3,)` for --enemy-artifact
    entries (AC-2.3, quantize()'s own doc comment).
    """
    if "=" not in text:
        raise GeneratorError(
            f"artifact spec must be NAME=X,Y,W,H,TH[,TW]: {text}"
        )
    name, rest = text.split("=", 1)
    if not re.fullmatch(r"[A-Z][A-Za-z0-9]*", name):
        raise GeneratorError(
            f"artifact name must be a PascalCase identifier fragment, "
            f"e.g. Logo: {name}"
        )
    parts = rest.split(",")
    if len(parts) not in (5, 6):
        raise GeneratorError(
            f"artifact spec must be NAME=X,Y,W,H,TH[,TW], got "
            f"{len(parts)} numbers after '=': {text}"
        )
    try:
        nums = [int(p) for p in parts]
    except ValueError:
        raise GeneratorError(f"artifact spec numbers must be integers: {text}")
    x, y, w, h, target_h = nums[:5]
    if w <= 0 or h <= 0:
        raise GeneratorError(
            f"artifact region width/height must be positive: {text}"
        )
    if target_h <= 0:
        raise GeneratorError(f"artifact target height must be positive: {text}")
    if len(nums) == 6:
        target_w = nums[5]
        if target_w <= 0:
            raise GeneratorError(
                f"artifact target width must be positive: {text}"
            )
        aspect_preserved = False
    else:
        # Round-half-up, integer only -- decision 4 is integer arithmetic
        # throughout, and Python's round() would use banker's rounding.
        target_w = (target_h * w + h // 2) // h
        aspect_preserved = True
    return name, (x, y, w, h), target_w, target_h, aspect_preserved, ()


def _comment_block(lines):
    return "\n".join((f"// {line}" if line else "//") for line in lines)


def _shared_converter_lines():
    return [
        "namespace detail {",
        "",
        "// The four-symbol row-string convention this generator emits:",
        "// ' '=BLACK, '.'=DARK_ORANGE, '+'=ORANGE, '#'=BRIGHT_ORANGE.",
        "// Mirrors galactic_invasion_art.h's own hand-authored",
        "// spritePixel()/rowIsExactWidth() convention (font.cpp's",
        "// original), widened from two symbols to four -- named",
        "// distinctly (generatedXxx) so this file and",
        "// galactic_invasion_art.h can be included in the same",
        "// translation unit without a redefinition.",
        "",
        "constexpr bool generatedRowIsExactWidth(const char* row, int32_t width) {",
        "  return row[width] == '\\0';",
        "}",
        "",
        "[[noreturn]] inline void reportInvalidGeneratedArt() { std::abort(); }",
        "",
        "constexpr Color generatedSpritePixel(const char* row, int32_t col,",
        "                                      int32_t width) {",
        "  if (!generatedRowIsExactWidth(row, width)) {",
        "    reportInvalidGeneratedArt();",
        "    return Color::BLACK;",
        "  }",
        "  switch (row[col]) {",
        "    case ' ': return Color::BLACK;",
        "    case '.': return Color::DARK_ORANGE;",
        "    case '+': return Color::ORANGE;",
        "    case '#': return Color::BRIGHT_ORANGE;",
        "    default: break;",
        "  }",
        "  reportInvalidGeneratedArt();",
        "  return Color::BLACK;",
        "}",
        "",
        "}  // namespace detail",
    ]


def _render_standalone_artifact(name, region, target_w, target_h,
                                 aspect_preserved, grid, mode, reference):
    """A self-contained artifact: owns its own width/height constants and
    exports a ready-to-blit Sprite (the logo's shape -- T5 consumes
    kLogoSprite directly)."""
    x, y, w, h = region
    rows = [render_ascii([r]) for r in grid]  # one string per source row

    lines = []
    lines.append(_comment_block([
        f"Artifact: {name}",
        f"  region: x={x} y={y} w={w} h={h} "
        f"(aspect {'preserved' if aspect_preserved else 'filled (anisotropic)'})",
        f"  target: {target_w}x{target_h}",
        f"  background mode: {mode}"
        + (f", reference colour: {reference}" if reference is not None else ""),
    ]))
    lines.append(f"inline constexpr int32_t k{name}Width = {target_w};")
    lines.append(f"inline constexpr int32_t k{name}Height = {target_h};")
    lines.append("")
    lines.append("namespace detail {")
    lines.append("")
    lines.append("// clang-format off")
    lines.append(f"inline constexpr const char* k{name}Rows[k{name}Height] = {{")
    for row_str in rows:
        lines.append(f'    "{row_str}",')
    lines.append("};")
    lines.append("// clang-format on")
    lines.append("")
    lines.append(f"constexpr std::array<Color, k{name}Width * k{name}Height> "
                 f"build{name}Pixels() {{")
    lines.append(f"  std::array<Color, k{name}Width * k{name}Height> pixels{{}};")
    lines.append("  int32_t offset = 0;")
    lines.append(f"  for (int32_t row = 0; row < k{name}Height; ++row) {{")
    lines.append(f"    for (int32_t col = 0; col < k{name}Width; ++col) {{")
    lines.append("      pixels[static_cast<size_t>(offset++)] =")
    lines.append(f"          generatedSpritePixel(k{name}Rows[row], col, "
                 f"k{name}Width);")
    lines.append("    }")
    lines.append("  }")
    lines.append("  return pixels;")
    lines.append("}")
    lines.append("")
    lines.append(f"inline constexpr std::array<Color, k{name}Width * k{name}Height> "
                 f"k{name}Pixels = build{name}Pixels();")
    lines.append("")
    lines.append(f"static_assert(sizeof(k{name}Pixels) / sizeof(Color) ==")
    lines.append(f"                  static_cast<size_t>(k{name}Width * "
                 f"k{name}Height),")
    lines.append(f'              "k{name}Pixels must hold exactly '
                 f'k{name}Width*k{name}Height cells");')
    lines.append("")
    lines.append("}  // namespace detail")
    lines.append("")
    lines.append(f"inline constexpr Sprite k{name}Sprite{{detail::k{name}Pixels.data(),")
    lines.append(f"                                       k{name}Width, k{name}Height,")
    lines.append(f"                                       /*stride=*/k{name}Width}};")
    lines.append("")
    lines.append(f"static_assert(k{name}Sprite.width == k{name}Width &&")
    lines.append(f"                  k{name}Sprite.height == k{name}Height &&")
    lines.append(f"                  k{name}Sprite.stride == k{name}Width,")
    lines.append(f'              "k{name}Sprite must be tightly packed at its '
                 f'declared size");')
    return "\n".join(lines)


def _render_cast_artifact(name, region, target_w, target_h, aspect_preserved,
                           grid, mode, reference):
    """A cast member: pixels only, no width/height constants or Sprite of
    its own -- galactic_invasion_art.h already owns kPlayerWidth/Height
    etc. (frozen, A6) and assigns this array to kPlayerPixels directly.
    Always fills its box (aspect_preserved is only ever False for these,
    asserted by the caller), matching D2's "sprites fill their frozen
    box" policy exactly."""
    x, y, w, h = region
    rows = [render_ascii([r]) for r in grid]

    lines = []
    lines.append(_comment_block([
        f"Artifact: {name} (cast member -- pixels only, no Sprite of its "
        f"own; consumed by galactic_invasion_art.h)",
        f"  region: x={x} y={y} w={w} h={h} (box filled, anisotropic)",
        f"  target: {target_w}x{target_h}",
        f"  background mode: {mode}"
        + (f", reference colour: {reference}" if reference is not None else ""),
    ]))
    lines.append("namespace detail {")
    lines.append("")
    lines.append("// clang-format off")
    lines.append(f"inline constexpr const char* k{name}Rows[{target_h}] = {{")
    for row_str in rows:
        lines.append(f'    "{row_str}",')
    lines.append("};")
    lines.append("// clang-format on")
    lines.append("")
    lines.append(f"constexpr std::array<Color, {target_w} * {target_h}> "
                 f"build{name}Pixels() {{")
    lines.append(f"  std::array<Color, {target_w} * {target_h}> pixels{{}};")
    lines.append("  int32_t offset = 0;")
    lines.append(f"  for (int32_t row = 0; row < {target_h}; ++row) {{")
    lines.append(f"    for (int32_t col = 0; col < {target_w}; ++col) {{")
    lines.append("      pixels[static_cast<size_t>(offset++)] =")
    lines.append(f"          generatedSpritePixel(k{name}Rows[row], col, "
                 f"{target_w});")
    lines.append("    }")
    lines.append("  }")
    lines.append("  return pixels;")
    lines.append("}")
    lines.append("")
    lines.append(f"inline constexpr std::array<Color, {target_w} * {target_h}> "
                 f"k{name}Pixels = build{name}Pixels();")
    lines.append("")
    lines.append("}  // namespace detail")
    return "\n".join(lines)


def _build_regenerate_command(args):
    """Reconstructed in a fixed, canonical order from the parsed values --
    never sys.argv verbatim -- so the banner's regenerate command is
    stable regardless of how the run was actually typed (flag order,
    relative vs. absolute paths a shell happened to expand)."""
    parts = [
        "python3 tools/generate_sprite_data.py --emit",
        f"--source {args.source}",
        f"--output {args.output}",
        f"--alpha-min {args.alpha_min}",
        f"--bg-tol {args.bg_tol}",
        f"--halo-tol {args.halo_tol}",
        f"--coverage-min {args.coverage_min}",
    ]
    for artifact in args.artifact:
        parts.append(f"--artifact {artifact}")
    for artifact in args.enemy_artifact:
        parts.append(f"--enemy-artifact {artifact}")
    return " ".join(parts)


def emit_header(output_path, source_path, artifact_specs, alpha_min, bg_tol,
                 halo_tol, coverage_min, regenerate_cmd):
    """Runs the whole pipeline for every artifact and atomically writes one
    header holding all of them (AC-3.1, AC-3.3, AC-3.5). Returns the text
    written, so callers (and tests) can inspect it without re-reading the
    file.

    Routes each spec by aspect_preserved, exactly the distinction
    parse_artifact_spec's NAME=X,Y,W,H,TH[,TW] forms already encode: five
    numbers (aspect preserved) is a standalone artifact with its own
    Sprite and width/height constants (the logo); six (target width
    literal) is a cast member -- pixels only, filling a box whose
    dimensions are already frozen elsewhere (galactic_invasion_art.h's
    kPlayerWidth/Height etc., A6). Each spec's trailing `exclude` tuple
    (added by main() for --enemy-artifact entries) restricts quantize()'s
    candidate palette -- AC-2.3, see quantize()'s own doc comment."""
    image = read_png(source_path)
    source_hash = source_sha256(source_path)

    artifact_blocks = []
    for (name, region, target_w, target_h, aspect_preserved,
         exclude) in artifact_specs:
        grid, mode, reference = generate_artifact_grid(
            image, region, target_w, target_h, alpha_min, bg_tol, halo_tol,
            coverage_min, exclude=exclude)
        renderer = (_render_standalone_artifact if aspect_preserved
                    else _render_cast_artifact)
        artifact_blocks.append(renderer(
            name, region, target_w, target_h, aspect_preserved, grid, mode,
            reference))

    banner = _comment_block([
        "GENERATED -- do not edit by hand. Produced by",
        "tools/generate_sprite_data.py (galactic-invasion-artwork plan §1",
        "decisions 2-5). A hand edit here is invisible to",
        "tools/check_constraints.sh's provenance check (AC-3.9) and will",
        "silently drift from the source it claims to come from.",
        "",
        f"Source: {source_path}",
        f"Source sha256: {source_hash}",
        "",
        "The parameters below are shared by every artifact in this file;",
        "each artifact's own extracted region and background finding are",
        "recorded just above its row data.",
        "",
        f"Background rule: alpha gate first (alpha < {alpha_min} -> BLACK),",
        f"then a border-seeded chroma flood (bg-tol {bg_tol}) and a halo",
        f"closure to fixpoint (halo-tol {halo_tol}), all integer",
        "arithmetic, applied before fit and quantisation.",
        f"Fit: integer box mapping, coverage-min {coverage_min}%; a cast",
        "sprite fills its frozen box (anisotropic scaling permitted), the",
        "logo instead preserves its source region's aspect ratio.",
        "Quantisation: nearest of the four palette colours by squared",
        "Euclidean RGB distance on integers; ties break to the lowest",
        "Color enumerator (BLACK < DARK_ORANGE < ORANGE < BRIGHT_ORANGE).",
        "",
        "Regenerate with:",
        f"  {regenerate_cmd}",
    ])

    parts = [
        banner,
        "",
        "#pragma once",
        "",
        "#include <array>",
        "#include <cstddef>",
        "#include <cstdint>",
        "#include <cstdlib>",
        "",
        '#include "steamcore/color.h"',
        '#include "steamcore/sprite.h"',
        "",
        "namespace steamcore::games {",
        "",
    ]
    parts.append("\n".join(_shared_converter_lines()))
    parts.append("")
    for block in artifact_blocks:
        parts.append(block)
        parts.append("")
    parts.append("}  // namespace steamcore::games")
    parts.append("")

    text = "\n".join(parts)
    _atomic_write(output_path, text)
    return text


def main(argv=None):
    parser = argparse.ArgumentParser(
        prog="generate_sprite_data.py",
        description=(
            "Reduce regions of a concept-art sheet to compile-time C++ sprite "
            "data. Offline step: its output is committed and the firmware "
            "build never reads an image."
        ),
        epilog=(
            "Background rule: an alpha gate runs first (a < --alpha-min "
            "becomes BLACK); the reference background colour is then the "
            "per-channel median of the crop's 1px border over the pixels "
            "that survived it, a 4-connected flood from the border clears "
            "everything within --bg-tol of it, and a halo closure pass "
            "repeats until fixpoint for pixels within --halo-tol that touch "
            "already-cleared ones. Quantisation maps each surviving pixel to "
            "the nearest palette entry by squared euclidean RGB distance, "
            "ties breaking to the lowest Color enumerator. All integer "
            "arithmetic, so re-running on identical input is byte-identical. "
            "Provenance drift is reported by the host gate as a warning, "
            "never a failure."
        ),
    )
    parser.add_argument("--probe", metavar="SOURCE",
                        help="report SOURCE's dimensions and alpha histogram, "
                             "then exit without writing anything")
    parser.add_argument("--print-source-hash", metavar="SOURCE",
                        help="print SOURCE's sha256 and exit -- the one hash "
                             "implementation, shared by the banner and the "
                             "host gate's drift check")
    parser.add_argument("--source", metavar="PATH",
                        help="the concept sheet (required for --preview/--emit)")
    parser.add_argument("--preview", metavar="X,Y,W,H",
                        help="print the quantised region as ASCII (using "
                             "--alpha-min/--bg-tol/--halo-tol/--coverage-min) "
                             "at 1:1, so a crop can be chosen and nudged "
                             "without writing a file")
    parser.add_argument("--emit", action="store_true",
                        help="run the whole pipeline for every --artifact "
                             "and atomically write --output")
    parser.add_argument("--output", metavar="PATH",
                        help="header file to write (with --emit)")
    parser.add_argument("--artifact", metavar="NAME=X,Y,W,H,TH[,TW]",
                        action="append", default=[],
                        help="an artifact to emit (with --emit); repeatable. "
                             "Five numbers preserve the region's aspect "
                             "ratio (the logo); six take the target width "
                             "literally (a cast member filling its frozen "
                             "box)")
    parser.add_argument("--enemy-artifact", metavar="NAME=X,Y,W,H,TH[,TW]",
                        action="append", default=[],
                        help="same spec syntax as --artifact, but quantised "
                             "against a palette excluding BRIGHT_ORANGE "
                             "(AC-2.3: player-exclusive ink) -- the concept "
                             "sheet predates the per-side ink reservation, "
                             "so its highlights would otherwise land there "
                             "regardless of which side the artifact is on")
    parser.add_argument("--alpha-min", type=int, default=128,
                        help="alpha below this becomes BLACK (default: 128)")
    parser.add_argument("--bg-tol", type=int, default=60,
                        help="hard background tolerance, per-channel distance "
                             "(default: 60)")
    parser.add_argument("--halo-tol", type=int, default=110,
                        help="soft, contiguity-gated halo tolerance "
                             "(default: 110)")
    parser.add_argument("--coverage-min", type=int, default=50,
                        help="percent of a target cell that must be "
                             "non-background for it to be lit (default: 50)")
    args = parser.parse_args(argv)

    if args.print_source_hash:
        print(source_sha256(args.print_source_hash))
        return 0

    if args.probe:
        image = read_png(args.probe)
        opaque, transparent, partial = alpha_histogram(image)
        total = image.width * image.height
        print(f"source: {args.probe}")
        print(f"dimensions: {image.width}x{image.height}")
        print(f"sha256: {source_sha256(args.probe)}")
        print(f"alpha == 255 (opaque):      {opaque} ({100 * opaque // total}%)")
        print(f"alpha == 0   (transparent): {transparent} "
              f"({100 * transparent // total}%)")
        print(f"alpha in between (partial): {partial} "
              f"({100 * partial // total}%)")
        # The exact counts above are not the verdict: an export can be
        # fully separated while reporting 0% at exactly 255.
        buckets = alpha_buckets(image, step=3)
        bucket_total = sum(buckets) or 1
        print(f"distribution (16 buckets, every 3rd pixel), gate at "
              f"--alpha-min {args.alpha_min}:")
        for i, count in enumerate(buckets):
            if not count:
                continue
            marker = " <-- gate" if i == args.alpha_min * 16 // 256 else ""
            print(f"  alpha {i * 16:3d}-{i * 16 + 15:3d}: {count:8d} "
                  f"({100 * count // bucket_total:3d}%){marker}")
        verdict, reason = alpha_verdict(buckets, args.alpha_min)
        print(f"verdict: {verdict} ({reason})")
        return 0

    if args.preview:
        if not args.source:
            parser.error("--preview requires --source")
        image = read_png(args.source)
        region = parse_region(args.preview)
        x, y, w, h = region
        grid, mode, reference = generate_artifact_grid(
            image, region, w, h, args.alpha_min, args.bg_tol, args.halo_tol,
            args.coverage_min)
        print(f"region: x={x} y={y} w={w} h={h} (1:1, no fit applied)")
        print(f"background mode: {mode}"
              + (f", reference colour: {reference}" if reference else ""))
        print(render_ascii(grid))
        return 0

    if args.emit:
        if not args.source:
            parser.error("--emit requires --source")
        if not args.output:
            parser.error("--emit requires --output")
        if not args.artifact and not args.enemy_artifact:
            parser.error("--emit requires at least one --artifact or "
                         "--enemy-artifact")
        artifact_specs = [parse_artifact_spec(a) for a in args.artifact]
        artifact_specs += [parse_artifact_spec(a)[:5] + ((3,),)
                           for a in args.enemy_artifact]
        names = [spec[0] for spec in artifact_specs]
        if len(names) != len(set(names)):
            raise GeneratorError(f"duplicate artifact name(s) in: {names}")
        regenerate_cmd = _build_regenerate_command(args)
        emit_header(args.output, args.source, artifact_specs,
                    args.alpha_min, args.bg_tol, args.halo_tol,
                    args.coverage_min, regenerate_cmd)
        print(f"wrote {args.output} ({len(names)} artifact(s): "
              f"{', '.join(names)})")
        return 0

    parser.error("nothing to do: pass --probe, --print-source-hash, "
                  "--preview or --emit")
    return 2


if __name__ == "__main__":
    try:
        sys.exit(main())
    except GeneratorError as exc:
        print(f"generate_sprite_data: {exc}", file=sys.stderr)
        sys.exit(1)
