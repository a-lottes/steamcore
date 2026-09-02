#!/usr/bin/env python3
"""Regenerates font_test.cpp's kAnchors[] array from independently-authored
glyph design data -- never from font.cpp's kGlyphArt (review F1, F10).

Run it and paste the printed block over the current kAnchors[] entries
(everything between the leading comment and the '~' tofu line) whenever a
glyph's pixel data in font.cpp changes. It recomputes and re-verifies from
scratch every time; nothing here is cached or hand-edited.

The design data below (FIVE_BY_SEVEN, DIGITS, PUNCT, and the three
directly-specified glyphs) is the same source that was hand-transcribed
into font.cpp's kGlyphArt table -- this script's only job is to (1) mechanize
that transcription so it can be checked against font.cpp, and (2) compute a
minimal set of (col, row, on/off) anchor points per glyph such that swapping
ANY two glyphs' art is guaranteed to disagree with at least one anchor of
the glyph being substituted into. That guarantee is verified by brute force
below, not assumed.
"""

# The classic 5x7 dot-matrix alphabet (the shapes used in countless
# HD44780/LCD character-generator-style fonts) -- the actual design choice.
# Embedded into the engine's 8x8 cell by to_cell() below (1px left margin,
# 2px right margin, 1px blank baseline row).
FIVE_BY_SEVEN = {
    'A': [".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    'B': ["####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."],
    'C': [".####", "#....", "#....", "#....", "#....", "#....", ".####"],
    'D': ["####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####."],
    'E': ["#####", "#....", "#....", "####.", "#....", "#....", "#####"],
    'F': ["#####", "#....", "#....", "####.", "#....", "#....", "#...."],
    'G': [".####", "#....", "#....", "#.###", "#...#", "#...#", ".####"],
    'H': ["#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    'I': ["#####", "..#..", "..#..", "..#..", "..#..", "..#..", "#####"],
    'J': ["..###", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##.."],
    'K': ["#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#"],
    'L': ["#....", "#....", "#....", "#....", "#....", "#....", "#####"],
    'M': ["#...#", "##.##", "#.#.#", "#...#", "#...#", "#...#", "#...#"],
    'N': ["#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#", "#...#"],
    'O': [".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    'P': ["####.", "#...#", "#...#", "####.", "#....", "#....", "#...."],
    'Q': [".###.", "#...#", "#...#", "#...#", "#.#.#", "#..#.", ".##.#"],
    'R': ["####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"],
    'S': [".####", "#....", "#....", ".###.", "....#", "....#", "####."],
    'T': ["#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."],
    'U': ["#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    'V': ["#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.."],
    'W': ["#...#", "#...#", "#...#", "#.#.#", "#.#.#", "##.##", "#...#"],
    'X': ["#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#"],
    'Y': ["#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.."],
    'Z': ["#####", "....#", "...#.", "..#..", ".#...", "#....", "#####"],
}

DIGITS = {
    '0': [".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###."],  # slashed, distinct from O
    '1': ["..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."],
    '2': [".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####"],
    '3': [".###.", "#...#", "....#", "..##.", "....#", "#...#", ".###."],
    '4': ["...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#."],
    '5': ["#####", "#....", "#....", "####.", "....#", "#...#", ".###."],
    '6': [".###.", "#....", "#....", "####.", "#...#", "#...#", ".###."],
    '7': ["#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..."],
    '8': [".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."],
    '9': [".###.", "#...#", "#...#", ".####", "....#", "....#", ".###."],
}
FIVE_BY_SEVEN.update(DIGITS)


def to_cell(rows5x7):
    assert len(rows5x7) == 7
    out = []
    for r in rows5x7:
        assert len(r) == 5
        row8 = ' ' + r.replace('.', ' ') + '  '
        assert len(row8) == 8, row8
        assert set(row8) <= {' ', '#'}
        out.append(row8)
    out.append(' ' * 8)  # blank baseline row
    assert len(out) == 8
    return out


# Direct 8x8 designs (not 5x7-embedded, matching '.'/':' precedent) for the
# four punctuation marks that don't fit the 5x7 grid, per the Designer's
# §8 shape rules and review F3/F8's corrections.
PUNCT = {
    '!': [  # continuous vertical stroke through most of the cell + a
            # baseline dot, clear gap between them (distinct from ':').
        "  ##    ",
        "  ##    ",
        "  ##    ",
        "  ##    ",
        "  ##    ",
        "        ",
        "        ",
        "  ##    ",
    ],
    '-': [  # single horizontal bar, mid-cell.
        "        ",
        "        ",
        "        ",
        " #####  ",
        "        ",
        "        ",
        "        ",
        "        ",
    ],
    '>': [  # right-pointing chevron. Inset one column from the left
            # edge, matching every letter/digit (review F8).
        " #      ",
        "  #     ",
        "   #    ",
        "    #   ",
        "   #    ",
        "  #     ",
        " #      ",
        "        ",
    ],
    '?': [  # a curve over a baseline dot -- dot at the SAME baseline
            # row as '.' (review F3), not one row above it. Inset one
            # column from the left edge, matching every letter/digit
            # (review F8).
        "  ###   ",
        " #   #  ",
        "     #  ",
        "    #   ",
        "   #    ",
        "        ",
        "        ",
        "   #    ",
    ],
}

SPACE = ["        "] * 8

DOT = [
    "        ",
    "        ",
    "        ",
    "        ",
    "        ",
    "        ",
    "        ",
    "  ##    ",
]

COLON = [
    "        ",
    "        ",
    "  ##    ",
    "        ",
    "        ",
    "  ##    ",
    "        ",
    "        ",
]

# Full 43-glyph on/off grid: char -> 8 strings of 8 chars each (' '/'#').
GRID = {ch: to_cell(rows7) for ch, rows7 in FIVE_BY_SEVEN.items()}
GRID.update(PUNCT)
GRID[' '] = SPACE
GRID['.'] = DOT
GRID[':'] = COLON
assert len(GRID) == 43, len(GRID)


def on(ch, row, col):
    return GRID[ch][row][col] == '#'


def derive_anchors(chars):
    """Greedy per-glyph selection of (row, col, on/off) anchors such that,
    combined, no OTHER glyph agrees with this glyph on every anchor picked
    for it -- guaranteeing a whole-glyph swap disagrees with at least one
    anchor of the glyph being substituted into (review F1)."""
    anchors = {}
    all_coords = [(r, c) for r in range(8) for c in range(8)]
    for ch in chars:
        chosen = []
        candidates = set(chars) - {ch}
        for r, c in all_coords:
            if not candidates:
                break
            val = on(ch, r, c)
            still_confused = {o for o in candidates if on(o, r, c) == val}
            if len(still_confused) < len(candidates):
                chosen.append((r, c, val))
                candidates = still_confused
        anchors[ch] = chosen
        if candidates:
            raise AssertionError(f"{ch!r} still confused with {candidates}")
    return anchors


def verify_pairwise(chars, anchors):
    """For every ordered pair (a, b), a != b: does swapping a's art into
    b's slot get caught by b's own anchors? Must hold for all pairs."""
    problems = []
    for a in chars:
        for b in chars:
            if a == b:
                continue
            if not any(on(a, r, c) != val for (r, c, val) in anchors[b]):
                problems.append((a, b))
    return problems


def emit_cpp(chars, anchors):
    esc = {"'": "\\'", '\\': '\\\\'}
    lines = []
    for ch in chars:
        c = esc.get(ch, ch)
        entries = ", ".join(
            f"{{'{c}', {col}, {row}, {'true' if val else 'false'}}}"
            for (row, col, val) in anchors[ch]
        )
        lines.append(f"    {entries},")
    return "\n".join(lines)


def main():
    chars = sorted(GRID.keys())  # ASCII order, matching kGlyphArt's storage order
    anchors = derive_anchors(chars)
    problems = verify_pairwise(chars, anchors)
    total = sum(len(v) for v in anchors.values())
    print(f"# {total} anchors across {len(chars)} glyphs, avg {total / len(chars):.2f} per glyph")
    if problems:
        print(f"# UNCAUGHT SWAPS: {problems}")
        raise SystemExit(1)
    print("# All pairwise whole-glyph swaps are caught by the target's own anchors.")
    print()
    print(emit_cpp(chars, anchors))


if __name__ == "__main__":
    main()
