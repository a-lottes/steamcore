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
