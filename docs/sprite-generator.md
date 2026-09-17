# Sprite Generator

`tools/generate_sprite_data.py` reduces regions of a concept-art sheet to
compile-time C++ sprite data for `games/galactic_invasion/`.

It is an **offline** step. Its output is committed and the firmware build
never reads an image — with `assets/` deleted from the working tree,
`make test` and `idf.py build` both stay green. That is the line
`galactic-invasion` drew and this feature keeps.

Standard library only: `tools/check_constraints.sh` enforces a stdlib
allowlist over `tools/*.py`, so Pillow is not an option here — and
`fb_view.py` already hand-builds PNGs with `zlib`+`struct`, which makes
this decoder its inverse rather than new territory.

## The alpha finding — measured, not assumed

The background rule leads with an alpha gate, but whether that gate can
do the work is a property of the source file, so it is **measured once
and recorded** rather than guessed:

```
python3 tools/generate_sprite_data.py --probe assets/sprites/galactic_invation.png
```

For `assets/sprites/galactic_invation.png`
(sha256 `5164bfbe912bfecde43272cad030df0369715acb0fe78c8640a44c31c69cf96a`,
1536×1024, bit depth 8, colour type 6/RGBA, non-interlaced), measured
2026-09-16:

> **verdict: alpha-separated** (77% below the gate, 20% above it, 1%
> straddling)

The distribution is bimodal: 64% of pixels sit at alpha 0–15 and 12% at
240–255, with only thin antialiasing between. **The sheet is already
cut out.** What looks like a solid brown panel behind the artwork is
transparent in the file; its brown RGB values simply sit underneath an
alpha of 1–26.

Two traps this measurement avoids, both of which produced a wrong
reading first:

- **Counting only the exact extremes is misleading.** This file reports
  *0%* at exactly alpha 255 and 9% at exactly 0, which reads like "not
  separated at all" — while in fact being cleanly separated. The verdict
  is therefore taken from the 16-bucket distribution, never from the
  exact-value counts.
- **Reading the panel as opaque from how the sheet *looks*.** Planning
  expected an opaque background (risk R1) precisely because the rendered
  sheet shows a filled brown panel. It does not have one.

Consequence for the background rule: the alpha gate is the primary and
exact separator here, and the chroma flood's role is reduced to clearing
glow-halo remnants. Both paths stay implemented and tested, because the
next sheet may well be the opaque case this one turned out not to be.

## Failure behaviour

The generator **fails loudly and writes nothing** rather than producing
plausible-looking wrong art. Each of these exits non-zero naming which
case occurred:

- the source file is missing or unreadable;
- the source is not a PNG, or is outside the supported subset (bit depth
  other than 8, colour type other than 2/RGB or 6/RGBA, interlaced, an
  unknown scanline filter type, corrupt image data);
- a requested region extends beyond the source image's bounds;
- a target size is larger than its source region (upscaling);
- the output path is not writable.

Output is written to a temp file and renamed into place, so an abort
mid-run can never leave a half-written or partially overwritten header
behind — a previous good file stays intact.

## Background rule

Per target cell, a source pixel is background if its alpha is below
`--alpha-min`, or (for pixels that survive the alpha gate) if its RGB
falls within `--bg-tol` of the region's border-seeded reference colour, or
within the wider `--halo-tol` *and* is 4-connected to an already-background
pixel (a fixpoint BFS, so halo closure is order-independent). All three
tolerances are integer, per-channel Euclidean-ish distance checks — no
floats anywhere in the pipeline, which is what makes re-running on
identical input byte-identical (AC-3.3) rather than merely usually so.

## Fit policy: quantize, then vote

Each target cell maps to an integer box of source pixels (`(ty*h)//target_h`
.. `((ty+1)*h)//target_h`, and the width equivalent) — anisotropic, so the
box "fills its frozen box" rather than preserving the source's aspect ratio
inside the cell. A cell becomes background (`BLACK`) unless at least
`--coverage-min` percent of its source pixels are non-background.
Otherwise: **every non-background source pixel in the box is quantized to
a palette index individually, and the cell takes the index with the most
votes** — ties break to the lowest `Color` enumerator (quantize()'s own
tie-break, reused rather than re-invented).

This replaced an original mean-then-quantize design (plan §1 decision 4)
after a parameter sweep (two source regions, ~40 bg/halo/coverage
combinations each) found zero combinations free of isolated single-pixel
artifacts on this sheet's anti-aliased art — a minority ink blended into
the mean at a cell boundary routinely quantized to a colour matching
neither neighbour. Quantizing first and voting is resistant to that: a
handful of anti-aliased outlier pixels cannot outvote a cell's actual
majority ink. User-corrected 2026-09-16 (T9); see `fit_region()`'s own
docstring and plan.md's Deviations entry for the full sweep writeup.

## Quantisation and per-side ink exclusion

Nearest of the four palette entries by squared Euclidean RGB distance;
ties break to the lowest `Color` enumerator (index 0 is `BLACK`, so a
genuine tie always favours it). This sheet predates the project's
per-side ink reservation (AC-2.3: `BRIGHT_ORANGE` is player-exclusive),
so quantizing its highlights against the full four-color palette put
`BRIGHT_ORANGE` on enemy-side pixels — found generating the enemy
fighter (T10). `--enemy-artifact` (same spec syntax as `--artifact`)
routes through `quantize(..., exclude=(3,))`, restricting the candidate
palette so that ink is structurally impossible on that artifact's output
rather than merely reviewed for afterwards. The mechanism stays
implemented and tested (`tools/test_generate_sprite_data.py`'s
`QuantisationTest`/`EmitHeaderTest` exclusion cases) even though no
artifact in the currently-committed header uses it — see "Known
limitation" below for why the enemy itself ended up hand-authored
instead, which made the flag's *use* here temporary without making the
flag itself dead code.

## Invocation

```
--probe SOURCE               dimensions, sha256 and alpha distribution; writes nothing
--print-source-hash SOURCE   the sha256 the provenance banner records
--source PATH                the concept sheet (required for --preview/--emit)
--preview X,Y,W,H            print the quantised region as ASCII at 1:1, no file written
--emit                       run the whole pipeline for every --artifact/--enemy-artifact
                              and atomically write --output
--output PATH                header file to write (with --emit)
--artifact NAME=X,Y,W,H,TH[,TW]
                              an artifact to emit; repeatable. Five numbers preserve
                              the region's aspect ratio (the logo); six take the
                              target width literally (a cast member filling its
                              frozen box)
--enemy-artifact NAME=X,Y,W,H,TH[,TW]
                              same spec syntax as --artifact, quantised excluding
                              BRIGHT_ORANGE (AC-2.3); repeatable
--alpha-min N                 alpha below N becomes BLACK (default 128)
--bg-tol N                    hard background tolerance, per-channel (default 60)
--halo-tol N                  soft, contiguity-gated halo tolerance (default 110)
--coverage-min N              percent of a target cell that must be non-background
                               for it to be lit (default 50)
```

`--print-source-hash` exists so the banner and the host gate's drift
check share **one** hash implementation and can never disagree.

The exact invocation that produced the committed
`galactic_invasion_generated_art.h` is also recorded verbatim in that
file's own banner comment (`_build_regenerate_command`), so it never
drifts from what's documented here:

```
python3 tools/generate_sprite_data.py --emit \
  --source assets/sprites/galactic_invation.png \
  --output games/galactic_invasion/galactic_invasion_generated_art.h \
  --bg-tol 100 --halo-tol 200 --coverage-min 15 \
  --artifact Logo=45,20,287,95,56 \
  --artifact PlayerGenerated=557,49,56,63,12,12
```

An earlier version of this invocation (T10) also generated the enemy as
`--enemy-artifact EnemyGenerated=39,355,53,60,10,12`. T13's design
review reverted the enemy to hand-authored art (see "Known limitation"
below), so the committed header now emits only `Logo` and
`PlayerGenerated` — `--enemy-artifact` remains a real, tested capability
of the tool for the next sprite that needs it, it just isn't exercised
by the current committed output.

## Provenance warning (A13)

`tools/check_constraints.sh` recomputes `assets/sprites/galactic_invation.png`'s
sha256 via `--print-source-hash` and compares it to the `// Source sha256:`
line in the generated header's banner. A mismatch **prints a warning and
keeps `make lint` green** — it does not fail the build. This was an
explicit scope decision (against the Product Owner's recommendation of a
hard failure): the generator is a manual, offline step a developer runs
deliberately, not something CI re-derives automatically, so a stale
header is a "someone should regenerate this" nudge, not a broken build.
If `assets/` is absent (the normal state once this feature ships — see
above), the check is skipped with an explicit message rather than
silently passing.

## Known limitation: the enemy ended up hand-authored, not generated

T10 adopted a generated enemy silhouette (the concept sheet's FIGHTER
region) after it passed the plan's mechanical adoption gate (≥2 outline
concavities), with a qualitative concern flagged and deferred to the
design review (T13): the generated shape was a symmetric diamond, not
clearly "the opposite shape family from the player's nose-up wedge"
AC-2.4a requires. T13's `/look-and-feel` review confirmed the concern —
the diamond shares the player's own bilateral symmetry rather than
contrasting with it — and, per AC-2.7 ("keep the shipped sprite if the
redraw isn't visibly better"), the enemy was reverted to this project's
original shipped invader silhouette, hand-edited into this feature's
shading language (an `ORANGE` outline, a `DARK_ORANGE` interior core).
It carries its own `AC-3.11` hand-authored note in
`galactic_invasion_art.h` rather than a generator provenance banner.
This is exactly what a downscale-and-quantize pipeline cannot fix by
tuning parameters: distinguishability by silhouette family is a design
judgement about what the source region *is*, not a property any
background/fit/quantisation setting can produce from it.

## Known limitation: shot-behind-enemy compositing ambiguity

The redrawn enemy and enemy-shot art (T7) share `ORANGE` ink post-recolor
(AC-2.3's per-side reservation). The shot sprite's transparent gap-rows
mean that when a shot overlaps an enemy body of the identical colour, the
composited framebuffer is bit-for-bit indistinguishable from "no shot
present" — genuine information loss in `GalacticInvasion::render`'s draw
order (enemies drawn before shots), not a detection-algorithm bug. Found
and root-caused via direct ground-truth debugging, not merely suspected.

Two test-fixture consequences, both `#if 0`-disabled with a full
explanation at the site rather than deleted:
`galactic_invasion_threshold_failsafe_still_fires_after_a_coincident_hit`
and `galactic_invasion_dodge_run_reaches_threshold_with_lives_intact` in
`firmware/steamcore/test/galactic_invasion_round_test.cpp`.

User decision (2026-09-16): document as a known gap and close T7 rather
than fix it here — a real fix means either reverting AC-3.10's ink choice
or changing render order, both of which would need a fresh `/sprint-plan`
pass, not a mid-increment patch.
