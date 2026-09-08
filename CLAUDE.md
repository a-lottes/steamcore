# CLAUDE.md

Project-specific notes for Claude Code. The standing rules for this project
(product principles, technical constraints, quality bars, QA method) live in
`.spark/constitution.md` and are read by every SPARK ceremony — this file is
for patterns Claude Code itself should remember across sessions that don't
belong there.

## Retroactive SPARK catch-up releases

Some early increments (`rendering-core`, `framebuffer-viewer`, `text-rendering`,
`game-loop`) shipped their source code before the SPARK loop was fully run
against them — spec/plan/review happened, but `/demo-day` and `/go-live` were
only run later, catching up a backlog. When releasing one of these:

- The release commit contains only the missing ceremony artifacts
  (`qa.md`, `release.md`) — never a source diff, since the source already
  shipped in an earlier commit.
- The version tag is an **annotated tag pointing at the original historical
  source commit**, not at the catch-up commit that adds the QA/release docs.
  Example: `rendering-core`'s code is commit `e5d4be3`; its tag `v0.0.1` was
  created with `git tag -a v0.0.1 e5d4be3 -m "..."`, so the tag's position in
  the commit graph matches its version number instead of sitting after later,
  higher-numbered releases.
- Numbering: `v0.1.0` was already taken by the first feature to go through
  the full loop same-day (`game-state-management`). Retroactive catch-ups for
  older, pre-existing commits use `v0.0.1`, `v0.0.2`, ... in their original
  commit order, staying below `v0.1.0` rather than continuing past it.

## Re-check `git status` before staging a prepared release

A `/go-live` prepare-only pass writes its exact file list into
`release.md` §3 while the increment's work is still fresh in context — but
execution can happen turns later, after other files in the working tree
have changed (a concurrent effort, a stray edit, a file the increment
itself touched without the preparer noticing). Trusting that prepared list
verbatim at execution time is how a genuinely-modified file gets silently
left out of the release commit.

Example: `display-driver`'s (`v0.2.0`) prepared plan omitted
`tools/check_constraints.sh`, even though T9 had added 5 new lint rules to
it — the file simply wasn't in the preparer's list. Caught only because the
orchestrator ran a fresh `git status` before staging and diffed it against
the prepared list, not because the plan itself flagged the gap.

**Rule:** immediately before running the staging commands from a
`release.md`, re-run `git status` and reconcile it against §3's file list —
every modified/untracked path that belongs to the feature should appear in
the list, and every path in the list should still exist and still be
relevant. Add what's missing before committing; never assume the prepared
list from an earlier turn is still complete.

## Hardware-gated Should: split the buildable half from the blocked half

When a story depends on physical hardware that isn't acquired/wired yet
(`display-driver`'s US-5 clock-speed tuning, `input-driver`'s US-4 on-device
confirmation), don't let the whole story slip to "later" as one lump, and
don't silently drop it either. Split it into two tasks at `/sprint-plan`
time:

- **The buildable half** — anything a compiler (host or cross) can verify
  without the physical part actually being present: writing the real
  device-side driver/source, wiring it into the build, confirming
  `idf.py build` (or the equivalent) is green, and confirming the shipped
  public APIs it touches are used unmodified by reading the source. This
  task is executable today and should be, not deferred alongside the part
  that genuinely can't be.
- **The blocked half** — the actual on-device confirmation with a human at
  the real controls. Ordered last, explicitly hardware-gated in its own
  Definition of Done ("executable only if X is physically wired/acquired by
  `/increment` time; if not, report `blocked` with the reason and record
  its ACs as explicitly unverified — never as passed, never satisfied by a
  substitute").

`blocked` is a legitimate, plan-anticipated terminal state for that one
task — not a failure, not grounds to hold the whole feature back from
`/peer-review`/`/demo-day`/`/go-live`, provided (as both these features'
specs did) the story carrying the hardware-gated ACs is scoped as a
**Should**, not a Must, so the loop's "every Must AC verified" gates are
satisfiable without it. Record the block plainly everywhere a reader would
look: the plan's task row, the wiring/setup doc's own status line, and
`qa.md`'s AC table (as `not capturable`, with the reason — never quietly
omitted).

## A host-only module can still hide a device-build bug

`font.cpp`/`text.cpp` were released by `text-rendering` and passed every
host gate (`make test`/`test-asan`/`test-gcc`) cleanly for two whole
features (`display-driver`, `input-driver`) before anything on the device
side ever `#include`d them. `start-screen` (T11) was the first feature
whose device harness composed `drawText`, and `idf.py build` immediately
surfaced two latent bugs neither host compiler had ever been able to
catch: a compile-time art validator that used `throw` (host clang/g++
never disable exceptions; ESP-IDF's device build does, via
`-fno-exceptions`), and a bare `size_t` relying on `<array>` to pull in
`<cstddef>` transitively (true of host libc++/libstdc++, not guaranteed
of the ESP-IDF toolchain's). Both were small, behavior-preserving fixes
once found — the cost was entirely in how long they went undetected.

**Pattern:** a module living in `include/`/`src/` is host-tested, but
"host-tested" is not "device-buildable" until something on the device
side actually includes it. The gap between "released" and "first
compiled for the device" can span multiple features. If a module is
likely to end up on-device eventually (anything feeding a real display or
composed into a real harness), a cheap insurance step is compiling it
against the ESP-IDF toolchain at least once — even a throwaway
`#include` in an existing harness — before a future feature discovers the
incompatibility as an unplanned blocker mid-increment.

## Derive on-screen layout from font metrics, never a literal

`start-screen`'s two elements (`kTitleWordmarkBounds`/`kTitlePromptBounds`,
`title_screen.h`) compute their width as
`(sizeof(text) - 1) * kGlyphAdvance` and their `y` as a `kGlyphHeight`
multiple, rather than any literal — the same `sizeof(string)`-based
convention `text_fixture_test.cpp` already used, just promoted from a test
fixture to a real screen's layout. This keeps a string edit and its
on-screen footprint from ever silently disagreeing, and (paired with the
existing glyph-metric lint rule) makes a bare `8`/`43` outside `font.h` a
build failure rather than a coincidence.

For two or more elements sharing a screen, prove disjointness **twice**:
once as a header `static_assert` (compile-time, using the general
four-way separating-axis expression — not "A is above B", which silently
stops being checked if the layout is ever reordered) and once as a
runtime host test with its own independent intersection helper. Worth
reusing for any future screen composed purely of `drawText`/`Sprite`
elements.
