# Plan: framebuffer-viewer

| | |
|---|---|
| **Phase** | Plan |
| **Owner** | Engineering Manager (`/sprint-plan`) |
| **Input** | `.spark/framebuffer-viewer/spec.md` (`approved`) |
| **Status** | `approved` |
| **Date** | 2026-09-01 |

**Handoff**
- **Status:** `approved` — approved by the user on 2026-09-01 in the `/sprint-plan` session.
- **Summary:** One allocation-free C++ free function serializes a `Framebuffer` into a caller-supplied buffer in a 10-byte-header `.scfb` format documented once in `docs/dump-format.md`; a single stdlib-only `tools/fb_view.py` decodes it to a truecolour PNG; the fixture path lives once in the `Makefile` and reaches both suites by `-D` and env var.
- **Open:** none — all 10 tasks `done`. `make test-all` green: 48 C++ tests + 15 Python tests (13 from `test_fb_view.py` incl. the two zero-dimension rejection cases added during review, 2 from `test_roundtrip.py`), sips external PNG oracle OK, lint OK (incl. new tools/*.py stdlib-import allowlist). Two deviations from plan, both found and fixed during `/peer-review`, not by the plan's own author: (1) T9's `make view` was written to depend on `$(TEST_BIN)`, which only links the binary without running it — since the fixture is committed to git and survives `make clean`, `view` would have silently shown a stale/last-committed fixture instead of the current source tree; fixed to depend on `test`. (2) `test-png-external` had the same dependency bug (fixed identically) plus a second issue: its recipe is one `;`-joined shell command with no `set -e`, so a failing decode step didn't stop the target — `sips` would then measure a stale `build/sips_check.png` left over from an earlier successful run and report OK. Fixed with `set -e` plus removing any stale file before regenerating. See review.md for the full finding history.
- **Binding ruling:** §3 Task Breakdown for current task status; a plan revision after review/QA findings updates §1/§3 in place, never a new section.
- **On conflict:** the numbered body below wins for everything except `Status`; log the mismatch as a finding at the next `/peer-review` and proceed — don't stop on it.

## 1. Architecture Decision

- **Context:** rendering-core delivered `Framebuffer` with no accessor for its raw
  bytes, and NFR-5 forbids adding one. The spec needs a C++ writer, a Python
  decoder, and a proof that both agree — across two languages with no shared
  schema, on a host with no ESP-IDF and no third-party Python packages (A5).
  Existing gate culture: every binary target is `.PHONY` and always relinks
  (rendering-core review F11), exactly one `-I` path (F12), lint refuses to
  report a false OK (F15).

- **Decision:**
  1. **Format `SCFB` v1**, extension `.scfb`, all multi-byte fields little-endian
     (A8), serialized field-by-field (never a `memcpy` of a C struct — padding):

     | Offset | Size | Field |
     |---|---|---|
     | 0 | 4 | magic `SCFB` (`0x53 0x43 0x46 0x42`) |
     | 4 | 2 | version `uint16` = 1 |
     | 6 | 2 | width `uint16` |
     | 8 | 2 | height `uint16` |
     | 10 | w×h | payload, row-major, 1 byte/pixel, value 0–3 |

     No length field (w×h is the length; a second one could disagree), no
     reserved bytes, no packing (AC-1.6). Authoritative doc: `docs/dump-format.md`
     — a new file, because the future device-side story links *the format*, not a
     host-test catalogue.
  2. **Writer = one free function**, `steamcore::serializeDump(const Framebuffer&,
     uint8_t* out, size_t capacity) -> size_t` (0 = capacity too small), living in
     `include/steamcore/dump_format.h` + `src/dump_format.cpp`. It reads only the
     existing public `pixel()` — **no raw-byte accessor is added to `Framebuffer`**
     (NFR-5). It allocates nothing and touches no file, so it needs neither A6's
     exemption nor any Makefile change: `ENGINE_SRCS` is a wildcard, the one
     include path already resolves it, and `make lint` covers it for free. All
     file I/O lives in the test (`fopen`/`fwrite`), where A6's exemption applies.
     Public surface: one function plus `kDumpHeaderSize` and `kDumpFormatVersion`
     — both consumer-named (the test sizes its static buffer and asserts the
     version byte) and both the format contract expressed in code.
  3. **Decoder = one stdlib-only module** `tools/fb_view.py`, invoked
     `python3 tools/fb_view.py <dump> <out.png>` (positional, `argparse`; exit 0
     ok, 1 input/decode error, 2 usage — A12/A10). PNG: colour type 2 (truecolour
     RGB), 8-bit, filter byte 0 on every scanline, `zlib.compress(level=9)` for
     IDAT, `zlib.crc32` for chunk CRCs, no `tIME`/ancillary chunks (so AC-3.5
     determinism is structural). Output is written to a temp file in the target
     directory and `os.replace`d — that makes AC-3.3's "no PNG on failure" and
     A13's silent overwrite both true by construction. Validation order: magic →
     version → actual file size vs. declared w×h (**before** any buffer of that
     size exists, AC-1.8) → every payload byte in 0–3, all before the output path
     is touched.
  4. **Fixture path has exactly one literal** (A9): `Makefile` variable
     `FIXTURE_DUMP := firmware/steamcore/test/fixtures/reference_pattern.scfb`,
     passed to C++ as `-DSTEAMCORE_FIXTURE_DUMP='"$(FIXTURE_DUMP)"'` and to Python
     as the exported env var `STEAMCORE_FIXTURE_DUMP`. Both sides refuse to run
     without it (`#error` / explicit check), so neither can silently fall back to
     a second hard-coded string.
  5. **Round-trip proof splits the chain in two halves that never share code**
     (AC-4.1): the C++ test proves *drawn pattern → dump payload* (compares the
     serialized payload against `fb.pixel(x,y)` at every coordinate); the Python
     test proves *dump payload → PNG RGB* (compares each decoded PNG pixel against
     `PALETTE[payload[y*w+x]]`, slicing the payload with the header size stated
     literally in the test, not imported from `fb_view`). Both halves additionally
     assert the **same anchor table** published in `docs/dump-format.md` — ~10
     absolute (x, y, colour) truths about the pattern. The anchors are what catch
     the one bug class the transitive chain cannot: an identical offset/axis error
     on both sides.
  6. **Python tests use stdlib `unittest`**, run as
     `python3 -B -m unittest discover -s tools -p 'test_*.py'` (`-B`: no
     `__pycache__`, so no stale-bytecode analogue of the F11 staleness class).

- **Alternatives considered:**
  | Alternative | Why rejected |
  |---|---|
  | Pillow / any pip package for PNG | Hard constraint (A5/C8/§6); also no venv story on this machine. Hand-rolled PNG is ~40 lines and the risk is mitigated in §5. |
  | Raw-byte accessor `Framebuffer::data()` for the writer | NFR-5 forbids a new public method; an escape hatch to the raw store outlives its one debug consumer and becomes what the future display driver reaches for instead of a designed tile API. 38,400 `pixel()` reads cost nothing on a debug path. |
  | Writer as a file-writing function in a new host-only `firmware/steamcore/tools/` tree | Needs a second `-I` path (breaks the F12 one-path invariant) or ugly `../` includes, plus a second directory named `tools/`. Buffer-out keeps the function allocation-free, lint-covered and directly reusable by the future USB-CDC sender, which wants bytes, not a file. |
  | Format documented inline (module docstring / `docs/host-tests.md`) | NFR-6 wants one authoritative contract both implementations were built *against*. A docstring makes the C++ side read Python source; `host-tests.md` is a make-target catalogue the device story would not link. |
  | PNG colour type 3 (PLTE + indices) | Smaller, but AC-4.1 asks for *actual RGB values*; type 2 makes the assertion literal and the test-side reader trivial (no palette indirection to get wrong twice). |
  | Golden-file fixture: write to `build/` and byte-compare against the committed one | AC-2.3 says the test writes the checked-in fixture, and §6 rules out golden-diff workflows. Drift stays visible in the review diff (§5 R4). |
  | Hand-rolled Python assert-script harness mirroring the C++ one | The C++ harness exists only because C++ has no stdlib runner. Python has one, and `unittest` is stdlib, so A5 holds. |
  | Python re-implements the drawn pattern to compute expected pixels | Duplicates drawing logic in a second language. The payload-vs-PNG comparison plus the shared anchor table proves the same chain with one pattern definition. |

- **Consequences:** Easier — zero new build wiring for the C++ half; the writer is
  device-reusable as-is; the format has one owner file; a decoder change that
  breaks the contract fails `make test-all`. Harder — we own a PNG encoder *and*
  a test-side PNG reader (risk R1/R2); the committed fixture is rewritten by every
  full `make test` run, so its diff must be read, not skimmed (R4); `make lint`
  grows a Python-specific check and therefore a second language to maintain.

## 2. Affected Components

- **New:** `docs/dump-format.md`; `firmware/steamcore/include/steamcore/dump_format.h`;
  `firmware/steamcore/src/dump_format.cpp`; `firmware/steamcore/test/dump_format_test.cpp`;
  `firmware/steamcore/test/fixtures/reference_pattern.scfb` (committed binary,
  38,410 bytes); `tools/fb_view.py`; `tools/test_fb_view.py`; `tools/test_roundtrip.py`.
- **Modified:** `Makefile` (`FIXTURE_DUMP`, `-D`, targets `test-python`,
  `test-roundtrip`, `test-png-external`, `view`, extended `test-all`);
  `tools/check_constraints.sh`; `docs/host-tests.md`.
- **Untouched (deliberately):** `framebuffer.{h,cpp}`, `sprite.h`, `color.h`,
  `config.h`, `dirty_tracker.*` — §6 forbids any rendering-core API change; this
  story only reads an existing instance.
- **Dependencies:** none added. C++: existing clang++/C++17 gate. Python: stdlib
  only (`argparse`, `os`, `struct`, `sys`, `tempfile`, `zlib`, `unittest`,
  `subprocess`) — NFR-4. `sips` (T4) is a pre-installed macOS binary, used as an
  optional external oracle, never a build dependency.
- **Scoping note:** no tool file was passed, so no blast-radius query was run;
  the component list above was scoped by hand from the spec and the repo tree.

## 3. Task Breakdown

| # | Task | Story | Covers (AC / NFR) | Depends on | Status | Definition of Done |
|---|---|---|---|---|---|---|
| T1 | Write the authoritative format doc: header table with offsets/sizes, explicit little-endian statement, "serialize field-by-field, never memcpy a struct", payload layout, the 4 palette hex values, the non-goal that no wire/framing protocol is defined here, and a pointer to the `Makefile`'s `FIXTURE_DUMP` as the fixture path's single source | US-1 | AC-1.1, AC-1.6, AC-1.7, NFR-6 | – | `done` | A reader who has never seen the C++ or Python source can decode a `.scfb` by hand from this doc alone: every field's offset, size, byte order and legal value range is stated, and §6's wire-protocol exclusion is restated as an explicit non-goal — files: docs/dump-format.md |
| T2 | Walking skeleton, C++ half: `serializeDump()` + a test that clears a `Framebuffer` to a known colour, serializes into a static buffer, writes the fixture file, re-reads it and asserts the header bytes; add `FIXTURE_DUMP` to the Makefile and pass it as `-DSTEAMCORE_FIXTURE_DUMP` | US-2 | AC-2.1, AC-2.3, AC-2.5, NFR-4, NFR-5 | T1 | `done` | `make test` builds and runs it green; the header bytes read back match T1's table exactly; the test fails loudly (naming the path and the expected repo-root cwd) if `fopen` fails; the source contains no ESP-IDF header, no allocation, no new public method on `Framebuffer`, and the fixture path appears as a literal nowhere but the Makefile (`#error` if the macro is undefined) — files: firmware/steamcore/include/steamcore/dump_format.h, firmware/steamcore/src/dump_format.cpp, firmware/steamcore/test/dump_format_test.cpp, firmware/steamcore/test/fixtures/reference_pattern.scfb, Makefile |
| T3 | Walking skeleton, Python half: `fb_view.py` happy path — parse a valid dump, map indices to the §8 palette, encode a truecolour PNG (filter 0, `zlib` IDAT, `zlib.crc32`), positional-argument CLI, print the output path to stdout, exit 0 | US-3 | AC-3.1, AC-3.2, AC-3.6 | T1, T2 | `done` | `python3 tools/fb_view.py <fixture> build/skeleton.png` prints the output path and exits 0; the PNG's IHDR carries the dump's declared width/height; `import` names are stdlib only — files: tools/fb_view.py |
| T4 | External PNG-validity oracle: `make test-png-external` decodes the T3 output with macOS `sips` and asserts the reported pixel dimensions equal the dump's declared ones; prints `SKIPPED (sips not found)` and stays green where `sips` is absent | US-3 | AC-3.1, NFR-2 | T3 | `done` | With `sips` present the target exits non-zero if `sips` cannot read the file or reports the wrong dimensions; the skip path prints the word `SKIPPED` so a green run can never be mistaken for a run that actually checked — files: Makefile |
| T5 | Replace the cleared frame with the real fixture pattern: `clear` BLACK, two unequal overlapping `fillRect`s (DARK_ORANGE, ORANGE), a blitted asymmetric "F" glyph (BRIGHT_ORANGE), a one-pixel vertical line spanning only the top third, and four **differently coloured corner pixels**; assert the payload at every coordinate and against the anchor table; assert two serializations are byte-identical | US-2 | AC-2.2, AC-2.4, AC-1.6, NFR-3 | T2 | `done` | The pattern uses all four palette indices and is invariant under no horizontal flip, vertical flip or transpose (the four corners alone prove it); the test compares `payload[y*w+x]` against `fb.pixel(x,y)` for every pixel, additionally asserts every anchor from the doc's table, and asserts `serializeDump` called twice yields identical bytes; the anchor table is published in the format doc — files: firmware/steamcore/test/dump_format_test.cpp, firmware/steamcore/test/fixtures/reference_pattern.scfb, docs/dump-format.md |
| T6 | Decoder validation and failure paths: bad magic, unsupported version, declared-size vs. actual-file-size mismatch (checked before any w×h buffer is created), payload byte outside 0–3 (reporting the first offending offset), unopenable/missing/directory input, no arguments; atomic output via temp file + `os.replace` | US-3, US-1 | AC-1.2, AC-1.3, AC-1.4, AC-1.5, AC-1.8, AC-3.3, AC-3.4, NFR-2, NFR-7 | T3 | `done` | Each rejection exits non-zero, names its specific cause on stderr (bad marker / unsupported version / size mismatch / invalid palette index / cannot open input), prints no traceback and leaves no file at the output path; the size check reads only the file's byte count, never a w×h allocation — files: tools/fb_view.py |
| T7 | Python unit tests + `make test-python`: one synthetic malformed dump per T6 rejection case, argument-shape cases driven through `subprocess`, byte-identity of two decodes of the same dump, and a wall-clock assertion that a full 240×160 decode completes well inside 1s | US-3 | AC-3.3, AC-3.4, AC-3.5, AC-3.6, NFR-1, NFR-2, NFR-3, NFR-7 | T6 | `done` | `make test-python` runs `python3 -B -m unittest discover -s tools -p 'test_*.py'` with `STEAMCORE_FIXTURE_DUMP` exported and exits non-zero on any failure; every T6 case is asserted on exit code, on the stderr text and on the absence of an output file; no `__pycache__` is created — files: tools/test_fb_view.py, Makefile |
| T8 | Round-trip proof + `make test-roundtrip`: a test-only minimal PNG reader (verifying chunk CRCs it did not compute, `zlib.decompress`, filter-0 unfiltering) compares every decoded pixel against `PALETTE[payload[y*w+x]]` from the committed fixture, plus the doc's anchor table; the payload offset is stated literally in the test, not imported from `fb_view` | US-4 | AC-4.1, AC-4.2, AC-3.2, NFR-3 | T5, T7 | `done` | `make test-roundtrip` runs only this module, needs no C++ toolchain (reads the committed fixture), and on any mismatch exits non-zero naming the first differing coordinate with expected and actual RGB; deliberately swapping two palette entries or transposing the row index makes it fail — files: tools/test_roundtrip.py, Makefile |
| T9 | `make view` (US-5): rebuild and run the fixture-producing C++ test, then decode the fixture to `build/pattern.png` in one command; `VIEWER_PNG` overridable | US-5 | AC-5.1 | T5, T6 | `done` | From a clean checkout `make view` produces the PNG with no other manual step and exits non-zero if either half fails; the C++ half runs through the always-relinking `.PHONY` test binary, so the PNG can never come from a stale build — files: Makefile |
| T10 | Gate and docs closure: extend `check_constraints.sh` with a stdlib-only import check over `tools/*.py` (allowlist) and add `src/dump_format.cpp` to the resolution-literal check; wire `test-python`, `test-roundtrip`, `test-png-external` into `test-all`; document every new target and the new format doc | US-1, US-3 | NFR-4, NFR-6, NFR-2 | T4, T7, T8, T9 | `done` | `make lint` fails on an injected `import requests` in a `tools/*.py` file and on an injected `240` in `dump_format.cpp`, and states explicitly in a comment why the allocation/ESP-IDF checks stay scoped to `include`/`src`; `make test-all` runs the Python gates and stops at the first failure; `docs/host-tests.md` lists every new target with what it proves — files: tools/check_constraints.sh, Makefile, docs/host-tests.md, docs/dump-format.md |

## 4. Test Strategy

- **US-1 (format):** no runtime of its own. It is verified indirectly — T2/T5 assert
  the emitted header against the doc's table, T6/T7 assert every rejection rule, and
  T8 is the executable cross-check that both implementations read the same doc the
  same way. Reviewer check for NFR-6: the doc must stand alone.
- **US-2 (writer):** existing `make test` gate (also `test-asan`, `test-gcc`), no new
  harness. Per-pixel payload comparison against `fb.pixel()` (serializer correctness)
  plus the anchor table (pattern correctness). **Why the fixture can catch a real
  bug:** the four corners carry four *different* palette indices, so any horizontal
  flip, vertical flip, 180° rotation or transposed index arithmetic changes at least
  two corners; the "F" glyph has no symmetry axis at all; the unequal overlapping
  rects catch a swapped width/height; the one-pixel vertical line catches a stride or
  row-length error that a filled region would hide. A uniform or symmetric pattern —
  the rendering-core review's finding — cannot fail here by construction.
- **US-3 (decoder):** stdlib `unittest`, gated by `make test-python`. Malformed inputs
  are synthesized in the test (hand-built bytes are fine for *invalid* files; only the
  *valid* fixture must be engine-produced, C3). Every failure case asserts three
  things: non-zero exit, the specific stderr wording, and that no output file exists.
- **US-4 (round-trip), mechanically:** `make test-roundtrip` is pure Python and needs
  no C++ step, because the fixture is committed (AC-2.3). It (1) reads the fixture
  bytes, (2) invokes `tools/fb_view.py` as a subprocess into a `tempfile` directory —
  the same command a human runs, so the CLI contract is under test too, (3) parses the
  resulting PNG with the test-local reader, (4) compares pixel by pixel against
  `PALETTE[payload[…]]` and against the anchor table. The comparison step is Python.
  The C++ half of the chain is asserted separately by T5 under `make test`; the two
  halves meet at the fixture file and share no code. `make view` (US-5) is the human
  convenience path, not the gate.
- **Gate wiring:** `make test-all` = `test`, `test-negative`, `test-asan`, `test-gcc`,
  `bench`, `test-python`, `test-png-external`, `lint`. **Staleness discipline:** every
  C++ binary stays `.PHONY` and always relinks (F11); `make view` reaches C++ only
  through that path. Python has no compiled artifact, but `-B` suppresses
  `__pycache__` so no stale `.pyc` can satisfy an import; no new target caches an
  output, and each rewrites its PNG rather than reusing one.
- **Not automated / deliberately manual:** looking at the PNG. `/demo-day` opens
  `build/pattern.png` once and confirms a human sees the drawn pattern — the one thing
  no assertion in this story can prove, and the reason the story exists. NFR-8 and
  NFR-9 are N/A per the spec and get no task.

## 5. Risks & Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| R1 Hand-rolled PNG is subtly invalid (chunk CRC, zlib framing, missing filter byte) | The PNG opens nowhere, or worse opens in one viewer only | Only filter 0, only `zlib.crc32`/`zlib.compress`, no ancillary chunks — the smallest legal PNG. T4's `sips` check confirms IHDR (dimensions) independently; T8's `test-roundtrip` decodes IDAT with its own independent reader and is what actually proves pixel content is correct — `sips` alone does not decode pixels (review F3, R1 corrected post-review) |
| R2 Our test-side PNG reader shares a bug with our writer → false green | The round-trip test passes on a broken PNG | The reader verifies CRCs it did not compute; T4's external oracle sits outside both; the anchor table is absolute truth derived from neither |
| R3 C++ and Python drift apart — no shared schema language | A dump the writer emits stops being decodable, silently | `docs/dump-format.md` is authoritative and both sides assert its constants; T8 is the executable cross-check and runs inside `test-all` |
| R4 Every full `make test` rewrites the committed fixture | A writer regression silently replaces the ground truth | The anchor assertions on both sides are absolute, not derived from the fixture, so a corrupted fixture fails T5 and T8; the fixture's diff must be explained in the `/peer-review` of this increment, not skimmed |
| R5 The fixture-writing test depends on cwd = repo root | A directly invoked binary writes the fixture somewhere else, or nowhere | The test fails loudly on `fopen` failure, naming the path and the expected cwd; `make` always runs from the root |
| R6 `sips` is macOS-only and may vanish | T4 becomes a no-op | Accepted: the skip prints `SKIPPED` explicitly and is never reported as a pass; the constitution's honest-status rule is satisfied |
| R7 Inherited assumption A2 (no wire protocol) may not survive contact with USB-CDC | A future story finds the file format unsuitable as a payload | Contained by design: the version field exists and the header is 10 bytes; a framing layer can wrap this format without changing it |
| R8 The 38,410-byte fixture is a binary blob in git | Review diffs of it are unreadable | Accepted, mandated by AC-2.3; it is written once and only changes when the pattern or the format changes — which is exactly the signal R4 relies on |

---

## ✅ PLAN GATE

*All boxes checked → `/increment` may start. Any box open → back to `/sprint-plan`.*

- [x] Spec status is `approved` (never plan against a draft)
- [x] Architecture decision includes rejected alternatives (a decision without alternatives is a guess)
- [x] Architecture respects the constitution's technical constraints (or a conflict is recorded) — §3 no dynamic allocation (the writer allocates nothing), §3 no resolution literal (T10 extends lint to `dump_format.cpp`), §4 host-test gate, §5 English + snake_case, §8 this *is* the named substitute verification method
- [x] Every task maps to a user story — no orphan tasks, no story without tasks
- [x] Every Must AC and every applicable NFR is covered by at least one task (AC-1.1…1.8, AC-2.1…2.5, AC-3.1…3.6, AC-4.1/4.2, AC-5.1; NFR-1…NFR-7; NFR-8/NFR-9 N/A per spec)
- [x] Every task has a checkable definition of done
- [x] Task order respects dependencies (walking skeleton T1–T3 runs end-to-end before any validation or pattern work)
- [x] Test strategy covers every Must story
- [x] Line budget respected: Ist 231 / Soll ~300 (excluding HTML comments)
- [x] Status set to `approved` by the user — approved 2026-09-01
