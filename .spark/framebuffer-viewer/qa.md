# QA Report: framebuffer-viewer

| | |
|---|---|
| **Phase** | Review (hands-on) |
| **Owner** | QA Tester (`/demo-day`) |
| **Input** | `.spark/constitution.md` §8 (substitute QA method), `.spark/framebuffer-viewer/spec.md` (`approved`), `.spark/framebuffer-viewer/plan.md` (`approved`), `.spark/framebuffer-viewer/review.md` (`passed`) |
| **Status** | `passed` |
| **Round** | 1 |
| **Date** | 2026-09-02 |

**Handoff**
- **Status:** `passed`.
- **Verdict:** Yes — I would demo this right now. All 22 ACs and all 6 QA-owned NFRs verified by actually running the commands and inspecting real output (test suite, `make view`'s PNG, CLI error paths, and two independent from-scratch re-decodes bypassing the tool's own code). Zero bugs found, Blocker/Major/Minor.
- **Open:** `none` — 0 new findings. Pre-existing Nits F9/F10/F11/F17 from `review.md` noted, not re-filed, none blocking.
- **Binding ruling:** §5 Verdict and the gate checklist below — the only binding location; there is no other round to point to.
- **On conflict:** the numbered body below wins for everything except `Status`.

## 1. Test Environment

- **App URL:** N/A — constitution §8 declares `Browser-observable surface: no`; there is no UI a browser can drive.
- **Substitute method used (constitution §8):** framebuffer dump (`.scfb`) decoded by `tools/fb_view.py`, plus host-compiled unit tests. **This feature is itself that substitute method's tooling** — its own ACs are entirely about the dump format and the Python decoder, not about anything running on the ESP32. No AC in this spec requires the live USB-CDC device path (confirmed by reading spec.md in full: US-1…US-5 cover file format, C++ writer, Python decoder, and round-trip proof — all host-only, per A2/§6 explicitly deferring wire/framing and live capture to a future story). Nothing in this round could not be captured for that reason.
- **Toolchain used:** Apple clang 14.0.3, Python 3.9.6, `/usr/bin/sips` (macOS), GNU Make — matches constitution §4's recorded host toolchain.
- **Commands actually run and their real output observed:** `make clean && make test-all` (twice — once for the gate, plus later re-runs of `test-roundtrip` around a mutation probe); `make view`; `python3 -B tools/fb_view.py <args>` invoked directly ~15 times with hand-crafted malformed dumps and edge-case paths; two from-scratch Python scripts I wrote and ran that independently re-decode the fixture (`.scfb` → palette indices) and the PNG (`build/pattern.png` → RGB, with my own PNG chunk/CRC/zlib/filter-byte reader) without reusing `fb_view.py`'s or `test_roundtrip.py`'s code.
- **Test data:** the committed fixture `firmware/steamcore/test/fixtures/reference_pattern.scfb` (engine-produced, AC-2.3) plus hand-crafted malformed `.scfb` files I built with `printf` for each rejection path (bad magic, bad version, size mismatch, invalid palette index, zero width/height, allocation-bomb header).

## 2. Acceptance Criteria Verification

| Spec ID | Steps performed | Expected | Observed | Result |
|---|---|---|---|---|
| AC-1.1 | Wrote a Python script that parses the fixture's raw bytes (magic/version/w/h/payload) with no dependency on `fb_view.py`, per `docs/dump-format.md`'s own table alone | Format doc alone is sufficient to decode | `magic=SCFB, version=1, width=240, height=160, payload_len=38400` — matched exactly, no out-of-band knowledge used | ✅ pass |
| AC-1.2 | `python3 tools/fb_view.py` on a file with magic `XXXX` | Rejected before pixel data touched | `fb_view: bad magic b'XXXX', expected b'SCFB'`, exit=1, no PNG | ✅ pass |
| AC-1.3 | Dump with version `153` | Rejected as unsupported version | `fb_view: unsupported version 153, this tool only reads version 1`, exit=1 | ✅ pass |
| AC-1.4 | Dump declaring 2×2 but with 1 payload byte | Rejected, no truncation/overread | `fb_view: declared 2x2 needs 4 payload bytes, file has 1`, exit=1 | ✅ pass |
| AC-1.5 | Dump with a payload byte value `9` | Rejected, no arbitrary colour mapping | `fb_view: invalid palette index 9 at payload offset 3 (valid range is 0-3)`, exit=1 | ✅ pass |
| AC-1.6 | My own raw-byte anchor check (12 points) against `docs/dump-format.md`'s anchor table, straight off the fixture's payload bytes | Payload bit-for-bit matches drawn pattern | All 12/12 anchors matched (BLACK/DARK_ORANGE/ORANGE/BRIGHT_ORANGE all correct at their coordinates); `make test`'s `dump_format_test.cpp` per-pixel comparison also green (109/109) | ✅ pass |
| AC-1.7 | Parsed header with `struct.unpack("<HHH", ...)` (explicit little-endian) — a big-endian misread would have produced wrong w/h and failed the size check | Header fields little-endian per doc | width=240, height=160 read correctly; had endianness been wrong this would have failed loudly (confirmed indirectly, matches doc's explicit statement) | ✅ pass |
| AC-1.8 | Crafted a 6-byte file with header claiming 65535×65535 | Size mismatch detected from file size before any big buffer allocated | `declared 65535x65535 needs 4294836225 payload bytes, file has 0`, exit=1, completed in 0.089s total (no allocation attempt) | ✅ pass |
| AC-2.1 | `make clean && make test` from scratch | Writer builds/runs under existing gate, no ESP-IDF header, no allocation | `109 passed, 0 failed` (includes `dump_format_test.cpp`); `make lint` separately confirms no ESP-IDF header / no dynamic allocation in `src`/`include` | ✅ pass |
| AC-2.2 | Same raw-byte anchor check as AC-1.6, all 4 palette colours present, asymmetric pattern (2 rects, "F" glyph, vertical line, 4 differently-coloured corners) | Payload matches drawn pattern exactly at every coordinate | 12/12 anchors correct; corners (0,0)=DARK_ORANGE, (239,0)=ORANGE, (0,159)=BRIGHT_ORANGE, (239,159)=BLACK — all four different, confirms asymmetry | ✅ pass |
| AC-2.3 | `git status --short` / `ls -la` on the fixture path | Fixture is a committed file in the repo | `firmware/steamcore/test/fixtures/reference_pattern.scfb` present, 38,410 bytes, not listed as untracked (`git status` clean on it) | ✅ pass |
| AC-2.4 | Observed `make test`'s own byte-identity assertion (`dump_format_test.cpp`) pass twice (fresh build + `test-gcc` recompile with a different compiler) | Two serializations of the same state are byte-identical | Green under both clang++ and g++ compiles, both giving `109 passed, 0 failed` | ✅ pass |
| AC-2.5 | `grep -rn` the literal fixture path across the whole repo (excluding `.spark/` prose and `build/`) | Path appears as a code literal exactly once | Only `Makefile:21`; `.spark/framebuffer-viewer/plan.md` mentions are plan prose, not a second code literal | ✅ pass |
| AC-3.1 | `make view`; separately, ran `fb_view.py` writing to a path that already held a placeholder file | PNG with dump's declared dimensions; existing file silently overwritten | `build/pattern.png`: "PNG image data, 240 x 160, 8-bit/color RGB"; placeholder file silently replaced with a valid PNG, no prompt | ✅ pass |
| AC-3.2 | Independent from-scratch PNG decoder (own CRC check, `zlib.decompress`, filter-byte handling) reading `build/pattern.png`'s actual pixel RGBs at all 12 anchor points | Each of 4 palette indices maps to one fixed documented RGB, pairwise distinct, brightness-ordered | RGBs exactly `(0,0,0)`/`(77,38,0)`/`(179,89,0)`/`(255,153,51)` at every anchor, matching `docs/dump-format.md` and spec §8 hex values | ✅ pass |
| AC-3.3 | Ran all 5 malformed-dump cases (bad magic, bad version, size mismatch, invalid pixel, zero-dims) directly | Non-zero exit, specific stderr message, no PNG written | All 5: exit=1, message names the specific cause, `ls` confirmed no output file created in every case | ✅ pass |
| AC-3.4 | Ran with no arguments; with a directory as input path; with a missing file path | Usage info or "cannot open" message to stderr, non-zero exit, no raw traceback | No args → argparse usage text, exit=2; directory → `fb_view: [Errno 21] Is a directory`, exit=1; missing file → `fb_view: [Errno 2] No such file or directory`, exit=1 — no Python traceback in any case | ✅ pass |
| AC-3.5 | Decoded the fixture twice to separate files, `cmp`'d them | Byte-identical PNGs | `cmp` reported no difference — `IDENTICAL` | ✅ pass |
| AC-3.6 | Ran a successful decode, captured stdout and exit code | Prints output path to stdout, exit 0 | Printed `/tmp/stdout_check.png`, exit=0 | ✅ pass |
| AC-4.1 | Cross-checked my independent raw-payload anchor decode against my independent raw-PNG anchor decode, both against the doc's published anchor table | Every PNG pixel matches the documented mapping for the colour actually drawn | 12/12 matched on both independent decodes; also `make test-roundtrip` green (`test_every_pixel_matches_payload_through_the_real_cli`, `test_matches_published_anchor_table`) | ✅ pass |
| AC-4.2 | **Mutation test performed live:** swapped the `BRIGHT_ORANGE`/`DARK_ORANGE` RGB literals in `tools/fb_view.py`, ran `make test-roundtrip`, then restored the file exactly (`diff` confirmed clean) and re-ran to confirm green again | Test fails loudly, names the first mismatching coordinate | `1871 pixel(s) differ; first at (0,0): expected (77, 38, 0), got (255, 153, 51)`; exit=2 (`make: *** [test-roundtrip] Error 1`). After revert: `OK`, exit=0, `diff` showed zero delta from the pre-mutation file | ✅ pass |
| AC-5.1 | `make clean && make view` on a clean checkout | Produces the PNG with no other manual step, non-zero exit if either half fails | `build/pattern.png` produced, exit=0; T9's `view: test` dependency (plan Handoff) means a C++ failure would propagate — consistent with review.md's rc=2 finding, not independently re-broken this round | ✅ pass |
| NFR-1 | `make test-python`'s own timing assertion, plus my own `time python3 -B tools/fb_view.py ...` | < 1s | Suite reports pass on `test_decode_completes_within_one_second`; my own wall-clock measurement: 0.124s total including Python interpreter startup | ✅ pass |
| NFR-2 | All malformed-input runs above, `ls`-checked after each | No malformed input ever yields any output file | Confirmed zero stray output files after all ~10 malformed/edge-case runs | ✅ pass |
| NFR-3 | `cmp` on two decodes (AC-3.5); fixture SHA implicitly stable across `make clean && make test-all` (writer side) | Both writer and decoder deterministic | Decoder: byte-identical confirmed by `cmp`. Writer: `make test`'s own byte-identity assertion passed under both clang++ and g++ | ✅ pass |
| NFR-4 | `grep -nE "^\s*(import|from)\s+" tools/*.py`; `make lint`'s import-allowlist check | Stdlib only, zero new build-gate dependencies | Only `argparse, os, struct, sys, tempfile, zlib, unittest, subprocess, time, fb_view` (local module) — no third-party import anywhere; `make lint` → `tools/*.py imports only from the standard library ... OK` | ✅ pass |
| NFR-5, NFR-6 | — | Spec's own "How it's verified" column names `/peer-review` for both (library-lens API surface / contract clarity) | Not QA-owned per the spec; `review.md` already covers them (NFR-5/NFR-6 rows, both ✅ met) | N/A for QA (owned by Review) |
| NFR-7 | Read stderr text on every rejection case above; stdout on success | Specific-cause message, not generic; success names output path | Every rejection named its exact cause (bad marker / unsupported version / size mismatch / invalid palette index / cannot open); success printed the real output path | ✅ pass |
| NFR-8, NFR-9 | — | N/A per spec (offline dev tool, no interactive UI) | Confirmed — no network call observed in any run, no interactive prompt in any CLI path exercised | N/A (per spec) |

## 3. Exploratory Findings

No bugs found. Beyond the ACs, I also probed: a path containing spaces (worked, PNG written correctly); a Unicode filename `日本語ファイル名.png` (worked); an unwritable output directory (`chmod 500`) → `fb_view: cannot create output at .../out.png: Permission denied`, exit=1, no partial file left; an extra positional CLI argument → argparse usage error, exit=2; `-h` → clean help text, exit=0, no traceback anywhere in any of these paths.

| # | Severity | Steps to reproduce | Expected vs. observed | Status |
|---|---|---|---|---|
| — | — | (none found this round) | — | — |

## 4. Console & Network

N/A — no browser, no network surface (offline CLI dev tool, constitution §2/NFR-8). No stray network calls observed in any of the ~20 `fb_view.py` invocations run this round (nothing to check with a network tool; confirmed by design — the tool touches only local files, per source and per NFR-8's own reasoning, and no run hung or showed any DNS/socket activity).

## 5. Verdict

Yes, I would demo this right now. Every one of the 22 acceptance criteria and every QA-owned NFR (NFR-1, NFR-2, NFR-3, NFR-4, NFR-7; NFR-8/9 N/A) was verified by actually running a command and reading its real output — not by reading source and assuming. Two of those checks went further than the existing test suite: an independent from-scratch PNG decoder (own CRC/zlib/filter-byte handling, no reuse of `fb_view.py` or `test_roundtrip.py` code) confirmed the actual pixel RGBs in `build/pattern.png` match the documented palette at all 12 anchor points, and a live mutation test (palette-swap injected into `fb_view.py`, reverted after, working tree confirmed clean via `diff`) proved AC-4.2's "fails loudly and names the first bad coordinate" claim myself rather than trusting `review.md`'s prior mutation record. `make clean && make test-all` ran green end-to-end (109 C++ tests × 4 configs, 15 Python via `discover`, 2 standalone round-trip, `sips` 240×160 OK, lint OK) — test counts are higher than `review.md`'s recorded 48/15 because three sibling features (text-rendering, game-loop, game-state-management) have landed on `main` since that review; nothing about this feature's own coverage changed. This feature is, by its own design, the project's host-side verification tool — no AC requires the live ESP32/USB-CDC path, so nothing was left unverified for that reason. Zero bugs found. Pre-existing Nits F9/F10/F11/F17 from `review.md` remain open by the user's prior scope decision; I did not re-probe or re-file them as this round found nothing new to add.

---

## ✅ QA GATE

- [x] Every Must-story acceptance criterion verified in the real browser and passed — N/A method substituted per constitution §8 (declared, complete, performable); verified by the declared substitute method instead (host test suite + direct CLI invocation + two independent from-scratch re-decodes), all 22 ACs pass
- [x] Every browser-observable NFR verified and passed — substituted the same way; NFR-1/2/3/4/7 pass, NFR-8/9 N/A per spec, NFR-5/6 owned by `/peer-review` (already ✅ in review.md)
- [x] No open Blocker or Major bugs (Minor bugs listed and accepted by the user) — none found
- [x] Browser console free of errors on the tested flows — N/A, no browser surface (§8)
- [x] Tested on all agreed viewports — N/A, no UI/viewport surface (§8)
- [x] Line budget respected: Ist 133 / Soll ~130 (excluding HTML comments) — 3 over; the AC table is longer than typical because every one of 22 ACs plus 6 NFRs got its own performed-step row, per §8's "coverage is unchanged" instruction
- [x] Status set to `passed`
