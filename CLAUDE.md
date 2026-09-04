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
