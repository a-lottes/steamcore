# Review Report: framebuffer-viewer

| | |
|---|---|
| **Phase** | Review |
| **Owner** | Reviewer (`/peer-review`) |
| **Input** | The diff of `/increment`, `.spark/framebuffer-viewer/plan.md` |
| **Status** | `passed` |
| **Round** | 3 |
| **Date** | 2026-09-01 |

**Handoff**
- **Status:** mirrors the header table above (authoritative for `Status`).
- **Verdict:** All four round-2 fixes (F12, F14, F15, F16) hold under fresh re-attack, each re-derived from source rather than read off the table. One new finding, a Nit (F17), unreachable through any documented `make` command. No open Blocker or Major.
- **Open:** `4 open` — Blockers: none; Majors: none; Nits only: F9, F10, F11, F17 (none blocks the gate; none is in the fix scope the user approved). `make clean && make test-all` re-run twice this round: exit 0 both times — 48 C++ tests in each of 4 configurations, 15 Python via `discover`, 2 via `test-roundtrip` standalone, sips 240x160 OK, lint OK. Fixture SHA `0b97094f…3eed5`, identical across all three rounds. Ready for `/demo-day`.
- **Binding ruling:** §6 Verdict and the gate checklist below — the only binding location.
- **On conflict:** the numbered body below wins for everything except `Status`; log the mismatch as a finding at the next `/peer-review` and proceed — don't stop on it.

## 1. Scope

Re-review of the working-tree diff against `e5d4be3` (`git status --short`): modified `Makefile`,
`docs/host-tests.md`, `tools/check_constraints.sh`; new `docs/dump-format.md`,
`firmware/steamcore/{include/steamcore/dump_format.h, src/dump_format.cpp,
test/dump_format_test.cpp, test/fixtures/reference_pattern.scfb}`, `tools/{fb_view.py,
test_fb_view.py, test_roundtrip.py}`. Also read `plan.md`'s post-review R1 amendment.

Round 3 re-verified F12, F14, F15 and F16 from source, not from their `fixed` cells —
re-verification of a claimed fix is condition (a) of the citation rule. Ran `make clean &&
make test-all` twice from scratch: **exit 0** both times, 48 C++ (×4 configs) / 15 Python
(discover) / 2 round-trip standalone / sips 240x160 / lint OK. Fixture SHA-256 `0b97094f…3eed5`
— identical to rounds 1 and 2, so NFR-3 holds across all three. Injected 5 fresh mutations and
one PATH-manipulation probe this round (§5). Rows not named above were left untouched.

**Not reviewed:** `assets/Buttons.png`, `assets/fonts/` — out of scope, unreferenced anywhere in
the repo. No tool file was passed, so scoping was done by hand from the diff and the repo tree.

## 2. Plan Conformance

| Task | Implemented as planned? | Note |
|---|---|---|
| T1 | ✅ r3 | The `≥ 1` rule is now in the byte-layout table rows for both `width` and `height` (`docs/dump-format.md:27-28`) as well as in *Reading this format* step 4. F15 closed. |
| T2 | ✅ | `#error` guard present; fixture path literal exists only in `Makefile:21` (verified by repo-wide grep). |
| T3 | ✅ | IHDR carries the dump's declared dimensions; imports stdlib-only. |
| T4 | ✅ r3 | DoD now holds fully: a failing produce-step aborts the target (F12 closed), and the `SKIPPED` branch still exits 0 — both re-proved this round (§5). |
| T5 | ✅ | All 12 anchors re-derived by hand from the `fillRect`/`blit`/`setPixel` calls in round 1; unchanged this round and re-confirmed green by `make test`. |
| T6 | ✅ r2 | Zero-dimension rejection added ahead of every other post-version check; all 7 rejection paths re-probed by hand (§5). |
| T7 | ✅ r2 | Both fragile assertions replaced; no OS `strerror` text remains in `tools/test_*.py` (grep-verified) and the suite still catches the regressions they exist for. |
| T8 | ✅ | Round 1's falsification test (swapped `PALETTE` entries / transposed row index) unchanged and still green. |
| T9 | ⚠️ | **Deviation, documented and correct.** `view: test` instead of `view: $(TEST_BIN)`. Re-verified this round: a C++ failure propagates (`make view` → rc=2). Recorded in `plan.md` Handoff line 14. Accepted. |
| T10 | ✅ r2 | The Python-import allowlist now catches all 10 evasion idioms probed, with no false positive on the real tree. |

## 3. Findings

| # | Severity | Location | Finding | Status |
|---|---|---|---|---|
| F1 | Major | `tools/check_constraints.sh:81-83` (pre-fix) | The stdlib-import allowlist matched only column-0, single-module `import` lines. Verified by injection: an indented `import requests`, `import os, requests`, `try: import PIL / except ImportError` and an indented `from PIL import Image` all left `make lint` reporting **OK**. This is the sole automated guard for spec A5/C8/§6 + NFR-4. Fix: match at any indentation, split comma lists. Re-verified r2 against 10 evasion probes (incl. tab indentation, `import os,requests`, `from requests.adapters import`, `import numpy as np`) — all caught; a clean stdlib-only file still passes. | fixed r2 |
| F2 | Major | `tools/fb_view.py:57-63` | A well-formed dump declaring `width=0`/`height=0` passed every check and produced a PNG with an IHDR of 0x0 — invalid per the PNG spec — then exited 0, against NFR-2/A10 and AC-3.1. Fix: reject `width == 0 or height == 0` in `read_dump()`. Re-verified r2 by hand on four dumps (0x0, 0x5, 5x0, and 0x0 *with* stray payload bytes): each gives rc=1, a specific message, and no output file. Mutating the check to `if False:` makes `test_zero_width_is_rejected` and `test_zero_height_is_rejected` fail on `returncode == 0`; both green again after revert. | fixed r2 |
| F3 | Minor | `Makefile:127-137`, `docs/host-tests.md:28`, `plan.md` R1 | All three described `sips` as a PNG-*validity* oracle when it only parses the IHDR. Fix: reword to "reads IHDR only, does not decode IDAT". Re-verified r2 by re-deriving the underlying fact (condition (a)): `sips -g pixelWidth` returns rc=0 and correct dimensions for a raw-DEFLATE IDAT, a missing filter byte, a corrupt IDAT CRC **and** a `b"NOTZLIBATALL"` payload — on both a 4x4 synthetic and the real 240x160 PNG. The new wording is accurate and, if anything, conservative. | fixed r2 |
| F4 | Minor | `tools/fb_view.py:126-146` | An `OSError` while writing reported the internal temp path, not the path the user typed (NFR-7). Fix: wrap `mkstemp`/write/`replace` and re-raise naming `output_path`. Re-verified r2 on three failure modes: nonexistent output dir → `cannot create output at /nonexistent_dir_xyz/out.png: No such file or directory`; read-only dir → `Permission denied`; output path is itself a directory (`os.replace` fails) → `cannot write output to …: Is a directory`. All rc=1, no `.fb_view_*` temp file left behind. | fixed r2 |
| F5 | Minor | `tools/test_fb_view.py:85-97` | `test_directory_as_input` asserted no stderr content, and `test_missing_file` asserted the OS's English `strerror` wording. Fix: assert the tool's own `fb_view: ` prefix plus the path. Re-verified r2: grep confirms no `strerror` text remains in either test file, and three injected regressions (main no longer catching `OSError`; a generic "invalid file" message; the message naming `args.output` instead of `args.dump`) fail 2, 8 and 7 tests respectively. See F16 for the one residual weakness. | fixed r2 |
| F6 | Minor | `Makefile:139` | `test-png-external: $(TEST_BIN)` linked the C++ binary without running it, so it neither refreshed the fixture nor used the prerequisite — the "link without run" pattern already removed from `view`. Fix: depend on `test`. Re-verified r2 the same way `view` was: corrupted the committed fixture's anchor `(5,5)` byte, ran `make clean && make test-png-external`, and the fixture came back at SHA `0b97094f…3eed5` — regenerated from current source, not stale. A C++ failure also propagates (rc=2). | fixed r2 |
| F7 | Minor | `docs/host-tests.md:26` | Said `make test-python` "Runs `tools/test_fb_view.py`"; it discovers **both** suites. Fix applied r1: row names both suites. Re-verified r2 — still names both. (Its test *count* went stale afterwards; that is F13, not a regression of this.) | fixed r2 |
| F8 | Minor | `firmware/steamcore/test/dump_format_test.cpp:160-186` | `…_writes_nothing_and_returns_zero` asserted only the return value, though `dump_format.h:24-25` documents `out` as left untouched. Fix applied r1: `0xA5` sentinel + zero-bytes-touched assertion. Re-verified r2 by re-injecting the mutation (capacity check moved after the four magic writes): fails at `dump_format_test.cpp:180` with `touched == 0`, 47/48; green again after revert. | fixed r2 |
| F12 | **Major** | `Makefile:138-154` | The recipe was one `;`-joined shell list with no `set -e`, so a failing `python3 -B tools/fb_view.py …` did **not** stop it and `sips` then validated whatever `build/sips_check.png` a *previous* run left behind — printing `test-png-external OK` and exiting 0. Fix applied: `set -e` after the `sips`-availability check plus `rm -f $(BUILD_DIR)/sips_check.png` before regenerating. Re-verified r3 by re-running the exact round-2 repro (decoder patched to `return 1` with a stale 452-byte `sips_check.png` in place): `make test-png-external` now **exits 2**, prints the decoder's error, and leaves no stale PNG. Two further mutations: a decoder that exits 0 writing nothing, and one writing `NOTAPNGATALL` — both give rc=2 with `test-png-external FAILED: sips reports …`, never OK. `SKIPPED` path re-proved unaffected by `set -e` (PATH farm of 1,196 symlinks with `sips` removed): prints `SKIPPED (sips not found)`, **exit 0**. | fixed r3 |
| F13 | Minor | `docs/host-tests.md:26` | The row claimed `test-python` runs "(13 tests)"; F2's fix added two, so `discover` now runs 15 — the doc understated the gate, the same honest-status class as F7 one round later. Fix applied: count corrected to 15 and `zero width/height` added to the listed rejection paths. Re-ran `make clean && make test-all` after the edit — still exit 0. | fixed r2 |
| F14 | Minor | `.spark/framebuffer-viewer/plan.md:14` | The plan's Handoff reported "13 Python tests"; the real count is 15. Fix applied by the EM. Re-verified r3 by counting from source, not from the sentence: `grep -c "def test_"` gives **13** in `test_fb_view.py` and **2** in `test_roundtrip.py`, and `discover` runs exactly 15 — the line's parenthetical now matches both halves. It additionally records the `test-png-external` `set -e`/staleness deviation, which round 2 asked for. Artifact wording, capped at Minor. | fixed r3 |
| F9 | Nit | `tools/fb_view.py:128` | `tempfile.mkstemp` creates the file mode 0600 and `os.replace` carries that mode across, so every PNG is 0600 rather than umask-default (re-confirmed r2: `build/pattern.png` is `-rw-------`), and an existing file's permissions are silently replaced on overwrite. Fix: `os.chmod(tmp_path, 0o666 & ~umask)` before the replace. | open |
| F10 | Nit | `tools/check_constraints.sh:77` | The allowlist carries `re` and `__future__`, which no `tools/*.py` imports (re-confirmed r2 by grep). An allowlist should enumerate what is actually needed. Fix: drop both; re-add when a file imports them. | open |
| F11 | Nit | `firmware/steamcore/include/steamcore/dump_format.h:26` | `serializeDump` dereferences `out` unconditionally once `capacity >= total`, but no non-null precondition is documented (re-confirmed r2) — every other public contract here states its preconditions (`framebuffer.h:26-44`, `:84-90`). Library lens §4. Fix: add "Precondition: `out` points to at least `capacity` writable bytes". | open |
| F15 | Nit | `docs/dump-format.md:27-28` | The `≥ 1` rule appeared only in *Reading this format* step 4, so a reader implementing a *writer* from the byte-layout table alone could still emit a 0x0 dump. Fix applied: both rows now read `` `uint16`, pixels, must be `>= 1` ``. Re-verified r3 by reading the table itself — the constraint is in the `width` **and** `height` rows, not just the prose. | fixed r3 |
| F16 | Nit | `tools/test_fb_view.py:99-108` | The test asserted `assertIn(self.tmpdir.name, stderr)`, which `out_path` (inside `tmpdir.name`) also satisfies; the `args.dump`/`args.output` swap escaped it. Fix applied: the input is now a distinct `…/a_directory_not_a_file` subdirectory and the assertion names it. Re-verified r3 by re-running that exact swap: the suite goes from 7 failures to **11**, and `test_directory_as_input` is among them, failing at `:108` with `'…/a_directory_not_a_file' not found in "fb_view: … '…/out.png'"`. | fixed r3 |
| F17 | Nit | `tools/test_fb_view.py:123-125`, `tools/test_roundtrip.py:122-124` | Both suites `skipTest` when `STEAMCORE_FIXTURE_DUMP` is unset. Probed r3: `python3 -B -m unittest discover -s tools` without the variable prints `OK (skipped=6)` and exits 0 — 6 of 15 tests, including both round-trip proofs, silently not run. Not reachable through any documented command: `make test-python`/`test-roundtrip`/`test-all` always export it, and the fixture is committed, so the skip never fires in the gate (all 15 ran `ok` in both full runs this round). Still the project's recurring "green run that checked less than it claims" shape. Fix: `self.fail()` on an unset variable (the Makefile is the only supported caller) and keep `skipTest` only for a genuinely missing fixture file. | open |

## 4. Requirements Traceability

| Spec ID | Implemented at | Verdict |
|---|---|---|
| AC-1.1 | `docs/dump-format.md:23-53` | ✅ met |
| AC-1.2 | `tools/fb_view.py:43-45` | ✅ met — probed, rc=1, `bad magic`, no PNG |
| AC-1.3 | `tools/fb_view.py:47-52` | ✅ met — probed, version check precedes w/h read |
| AC-1.4 | `tools/fb_view.py:65-70` | ✅ met — probed truncated file |
| AC-1.5 | `tools/fb_view.py:72-77` | ✅ met — probed byte `4`, names offset 7 |
| AC-1.6 | `src/dump_format.cpp:28-35`; test `:186-204` | ✅ met — per-pixel, mutation-verified |
| AC-1.7 | `src/dump_format.cpp:7-10`; `fb_view.py:47,54-55` | ✅ met — big-endian mutation fails `make test` |
| AC-1.8 | `tools/fb_view.py:65-70` | ✅ met — 65535x65535 header on a 10-byte file rejected instantly, no allocation |
| AC-2.1 | `src/dump_format.cpp`; `dump_format_test.cpp` | ✅ met — no ESP-IDF header, no allocation, runs under `make test` |
| AC-2.2 | `dump_format_test.cpp:64-88, 196-217` | ✅ met — 12 anchors re-derived by hand, all correct |
| AC-2.3 | `dump_format_test.cpp:123-131` | ✅ met |
| AC-2.4 | `dump_format_test.cpp:221-238` | ✅ met — also byte-identical across a clean rebuild |
| AC-2.5 | `Makefile:21-22, 158, 163` | ✅ met — one literal repo-wide |
| AC-3.1 | `tools/fb_view.py:57-63, 121-146` | ✅ met r2 — F2 closed; 0x0/0xN/Nx0 all rejected, no PNG written |
| AC-3.2 | `tools/fb_view.py:22-27` | ✅ met — matches spec §8 hex and `docs/dump-format.md:45-50` exactly |
| AC-3.3 | `tools/fb_view.py:36-79`; tests `:57-83` | ✅ met — all 7 rejections probed: rc=1, stderr names the cause, no file |
| AC-3.4 | `tools/fb_view.py:149-162`; tests `:85-107` | ✅ met r2 — F5 closed; directory and missing-file cases now assert the tool's own contract |
| AC-3.5 | `tools/fb_view.py:103`; test `:132-138` | ✅ met |
| AC-3.6 | `tools/fb_view.py:161-162` | ✅ met |
| AC-4.1 | `tools/test_roundtrip.py:129-158` | ✅ met — every pixel, via the real CLI |
| AC-4.2 | `tools/test_roundtrip.py:153-158, 160-174` | ✅ met — 4 mutations each fail and name the first bad coordinate |
| AC-5.1 | `Makefile:122-125` | ✅ met — `make clean && make view` produced `build/pattern.png`; C++ failure propagates (rc=2) |
| NFR-1 | `tools/fb_view.py`; test `:140-145` | ✅ met — 0.045 s re-measured, budget 1 s |
| NFR-2 | `tools/fb_view.py:36-79` | ✅ met r2 — F2 closed; no malformed input yields any output file |
| NFR-3 | `src/dump_format.cpp`; `fb_view.py:103` | ✅ met — fixture SHA identical across a clean rebuild *and* across rounds |
| NFR-4 | `Makefile`; `check_constraints.sh:72-106` | ✅ met — no third-party import exists; the guard survives 10 evasion probes |
| NFR-5 | `dump_format.h:13-26` | ✅ met — 3 new public symbols, all consumed by the test; no new `Framebuffer` method, no `friend`, no `reinterpret_cast`/`const_cast` |
| NFR-6 | `docs/dump-format.md:27-28, 63-65` | ✅ met r3 — the `≥ 1` range is in the byte-layout table *and* the reader steps; F15 closed |
| NFR-7 | `tools/fb_view.py:132, 142, 158` | ✅ met r2 — F4 closed; every rejection and the write failure name the user's own path |
| NFR-8, NFR-9 | — | ✅ N/A per spec, agreed: local files only, no network, no interactive UI |

## 5. What Was Checked

- [x] Correctness: logic does what the acceptance criteria demand
- [x] Non-functional: applicable NFRs and constitution quality bars hold (§3 no dynamic allocation, §5 `snake_case` + `steamcore` namespace + English, §6 4-colour palette, §4 honest status — round 2's one breach, F12, is closed)
- [x] Error handling: all 7 decoder rejection paths plus 3 output-write failure modes re-probed by hand — exit codes, stderr text, no traceback, no output file, no leftover `.fb_view_*` temp file
- [x] Security: offline dev tool, local files only; hostile header cannot force a large allocation (AC-1.8 re-probed); no secrets
- [x] Tests: exist, are meaningful, and pass — **5 mutations and one PATH probe injected and reverted this round**: decoder `return 1` with a stale PNG present (rc=2, F12's repro), decoder exits 0 writing nothing (rc=2, `FAILED: sips reports x`), decoder writes `NOTAPNGATALL` (rc=2, `FAILED: sips reports <nil>x<nil>`), `args.dump`/`args.output` swap (11 failures, now incl. `test_directory_as_input`), env var unset (surfaced F17), and a 1,196-symlink `sips`-free PATH farm (`SKIPPED`, exit 0). `tools/fb_view.py` byte-compared against its pre-mutation copy afterwards: identical. Fixture SHA unchanged by all of it
- [x] Readability: the next developer will understand this — the new comments cite the finding they close, which is the right level of "why"
- [x] Library lens (scoped: Public API surface, Contract clarity) — §1.1/§1.2 unchanged and still minimal; §4.1/§4.2 re-checked, error behaviour documented (`0` return, `out` untouched) with F11/F15 as residual nits; §2 semver and §3 packaging are no-ops for a statically-linked firmware image per constitution §2

## 6. Verdict

**Passed.** The four fixes round 2 asked for all hold, and I confirmed each from source rather
than from its `fixed` cell. F12 was the one that mattered, so I rebuilt its repro exactly: with the
decoder patched to fail and a stale 452-byte `build/sips_check.png` sitting there from a successful
run, `make test-png-external` now exits 2 and deletes the stale file instead of printing
"OK: sips independently confirms 240x160". I then attacked it two further ways the developer did not
claim — a decoder that exits 0 having written nothing, and one that writes twelve bytes of garbage —
and both fail honestly with `test-png-external FAILED`, never a false OK. The `SKIPPED` branch was my
main worry about adding `set -e`, since a shell option added mid-recipe is exactly the kind of change
that quietly breaks the other path; it does not, because the `exit 0` fires before `set -e` is
reached, and I proved that empirically with a 1,196-symlink PATH farm with `sips` removed rather than
reasoning about it. F16's fix is real too: the same `args.dump`/`args.output` swap that escaped it
last round now takes the suite from 7 failures to 11, with `test_directory_as_input` failing on the
distinct subdirectory path. F14 and F15 are artifact edits and both say what they now claim — I
counted the tests from `def test_` rather than believing the sentence. Two clean `make clean &&
make test-all` runs, exit 0, with the fixture landing on the same SHA-256 it has had since round 1.
The one new thing I found is small and I want to be plain that it is small: both Python suites skip
rather than fail when `STEAMCORE_FIXTURE_DUMP` is unset, so a bare `unittest discover` prints
`OK (skipped=6)`. No documented command can reach that — the Makefile always exports the variable
and the fixture is committed — so it blocks nothing, but given how many times this project has ruled
against a green run that checked less than it claimed, it belongs on the record as F17. Beyond that,
a fourth pass over the decoder, the serializer, the lint script and the Makefile turned up nothing
new, which after three rounds of real findings is itself the useful result. F9, F10, F11 and F17 are
Nits and stay open by the user's own scope decision. This is ready for `/demo-day`.

---

## ✅ REVIEW GATE

*All boxes checked → `/demo-day` may start. Any box open → back to `/increment`. On
re-review, edit this same checklist in place — never duplicate it as a second gate.*

- [x] No open Blocker findings
- [x] No open Major findings (or explicitly waived by the user, with reason recorded here) — F12 closed and re-verified r3
- [x] Every Must AC traces to implementing code; no constitution non-negotiable violated
- [x] All plan deviations documented and accepted — `plan.md` Handoff line 14 now records both: T9's `view: test` change (rc=2 propagation re-verified r2) and `test-png-external`'s dependency + `set -e` fix (re-verified r3)
- [x] Test suite runs green — `make clean && make test-all` run twice this round: **exit 0** both times, 48 C++ (test / asan / gcc / png-external prereq) / 15 Python (discover) / 2 round-trip standalone / sips 240x160 / lint OK
- [x] Line budget respected: Ist 158 / Soll ~150 (excluding HTML comments) — 8 over, unchanged from round 2: F12/F14/F15/F16 were rewritten in place, not appended, and F17 cost one row offset by trimming F12's now-settled prose. The re-verification evidence stays inline because `fixed` is the claim under audit
- [x] Status set to `passed`
