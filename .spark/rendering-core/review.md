# Review Report: rendering-core

| | |
|---|---|
| **Phase** | Review |
| **Owner** | Reviewer (`/peer-review`) |
| **Input** | The `/increment` diff (whole untracked tree), `.spark/rendering-core/plan.md` |
| **Status** | `passed` |
| **Round** | 3 |
| **Date** | 2026-09-01 |

**Handoff**
- **Status:** mirrors the header table above (authoritative for `Status`).
- **Verdict:** All 14 findings from rounds 1–2 hold up under re-attack — every one was re-probed from scratch, none was taken on the `fixed` cell's word — and the two Majors that were still open (F3, F11) are genuinely dead: the redesigned diagonal fixture kills round 1's exact mutation plus nine more of my own, and the `.PHONY` binaries give 0/10 false greens where the old rule gave 4/10. Two new low-severity findings, neither blocking.
- **Open:** `0 open` — all 16 findings across three rounds now show `fixed` in §3. F15 fixed by resolving to the repo root via `BASH_SOURCE` and failing loudly (exit 1) if `INCLUDE_DIR`/`SRC_DIR` don't exist, rather than silently reporting OK — verified: temporarily removing `firmware/steamcore/include` now produces `check_constraints: expected directory … does not exist`, exit 1, instead of a false `make lint OK`. F16 fixed by updating the plan Handoff's stale counts (41→43 tests, 0.0016→0.0014 ms bench). `make clean && make test-all`: 43 passed / 0 failed, exit 0.
- **Binding ruling:** §6 Verdict and the gate checklist below — the only binding location; there is no other round to point to
- **On conflict:** the numbered body below wins for everything except `Status`; log the mismatch as a finding at the next `/peer-review` and proceed — don't stop on it.

## 1. Scope

Still a brand-new repository, nothing committed, so there is no git diff: the round-3 scope is the whole untracked tree again (except `README.md`, `.gitignore`, `.spark/`, and the pre-existing `assets/` art — old mtimes, no task claims them, no gate touches them), read in full, with attention on the six files the round-2 fixes touched: `Makefile`, `tools/check_constraints.sh`, `firmware/steamcore/test/sprite_test.cpp`, `firmware/steamcore/include/steamcore/framebuffer.h`, plus plan §1 Decision 2's second amendment.

Nothing was taken on trust from a `fixed` cell. Every round-1 and round-2 attack was re-run from scratch: the `clip.h` sabotage without `make clean` (F1), a zero-match `FILTER` (F2), **ten** blit mutations incl. round 1's exact one (F3), nine lint injections (F4), a 10-trial same-second edit-then-test stress loop (F11), a from-scratch build of every target with the two dropped `-I` paths absent (F12), both comment-heuristic edge cases plus two more (F13), and an A/B UBSan repro against a locally re-narrowed `int32_t` copy (F6). AC-1.3 was re-derived from scratch again — trigger (b), a Must AC's verification — by deleting `setPixel`'s bounds check: `make test-asan` reported `index -1205 out of bounds` at `framebuffer.cpp:16` and exited 2. Baseline and final `make clean && make test-all`: **43 passed / 0 failed, exit 0** (clang; 43/43 under ASan+UBSan; 43/43 under `test-gcc`; `test-negative` OK on both its assertions; `dirty scan: 0.0014 ms`; `make lint OK`). Every injected change was reverted and byte-compared against its backup; the tree was re-verified green afterwards.

No reviewer edits this round. Not reviewed: `README.md`, device behaviour (no toolchain, no board), the `library` lens's *semver* and *packaging* halves (no-ops per constitution §2). No tool file was passed; scoping was done by hand.

## 2. Plan Conformance

| Task | Implemented as planned? | Note |
|---|---|---|
| T1 | ✅ | Layout, Makefile, harness as specified. The stale-binary class is closed at the root: all four binaries are unconditionally `.PHONY`, so mtime never decides anything (F1, F11). |
| T2 | ✅ | Failure path, filter, zero-match and `kMaxCases`-overflow all fail loudly; re-verified. |
| T3 | ✅ | Sanitizer target re-derived against a real injected fault (§1). `test-gcc` honestly recorded as nominal. |
| T4 | ⚠️ | The `16` and resolution checks fire correctly on every injection I could think of, but the script that runs them can report OK on nothing → F15. |
| T5…T9, T11…T13 | ✅ | Unchanged; re-confirmed by mutation (§5). |
| T10 | ✅ | The `MarkedAtlas` window now carries a position-dependent diagonal marker and `onlyWindowSliceEquals` is an independent oracle (it recomputes the expected colour, never reads `sprite.pixels`). All three AC-3.7 cases now bite (F3). |
| T14 | ⚠️ | Patterns cover NFR-2/NFR-4; nine injections all caught. Same F15 caveat as T4. |
| T15 | ✅ | Overflow guarantee scoped to a stated precondition; scan/commit pairing rule documented. |
| §1 Dec. 2 | ✅ | The Makefile passes exactly one `-I` and the plan says exactly that. Verified by building every target clean without the two dropped paths (F12). |
| Plan Handoff | ⚠️ | Its summary line still reports the pre-review numbers (41 tests, 0.0016 ms) → F16. |

## 3. Findings

| # | Severity | Location | Finding | Status |
|---|---|---|---|---|
| F1 | Blocker | `Makefile:31,37,65,79,97` | Binary targets had no header prerequisites, so a header-only edit was never rebuilt and `make test` reported the previous binary's result. **Re-attacked r3:** appending garbage to `src/clip.h` without `make clean` now forces a recompile that fails (`clip.h:63:1: error: expected unqualified-id`, exit 2). The fix was redesigned from header prerequisites to unconditional `.PHONY` binaries, which subsumes it — see F11. | fixed r2 |
| F2 | Major | `test_harness.cpp:61-107`, `Makefile:58-63` | A zero-match filter exited 0. **Re-attacked r3:** `make test FILTER=this_is_nonsense_qqq` prints `ERROR: no test matched filter "…"` and exits 2; `make test-negative` asserts it automatically. `kMaxCases` overflow is covered by `g_overflowed` (`:38,47,100-105`). | fixed r2 |
| F3 | Major | `sprite_test.cpp:200-312` | AC-3.7's clipped cases existed but `MarkedAtlas`'s window was uniformly ORANGE, so a source-row error landing inside the window was invisible — round 1's mutation passed 43/43. **Confirmed fixed r3 by 10 mutations, not by reading:** the window now carries a diagonal marker and round 1's exact mutation (`(srcOffsetY+row)*stride` → `srcOffsetY*sprite.width + row*stride`) fails `ac_3_7_stride_sub_rectangle_clipped_top`. Also caught: `stride` initialised from `sprite.width` (3 AC-3.7 tests fail), `srcOffsetY` dropped, `srcOffsetX` dropped, offsets swapped, ±1 row shift, column shear, `srcOffsetY/2`, an equal row+column shift (the one class a diagonal is theoretically blind to — the window's 8×8 edge catches it as BRIGHT_ORANGE leakage), and a transposed read. Residual, priced as no finding: the marker `r==c` is transposition-symmetric, so the AC-3.7 tests alone cannot see a transposed read — the suite still fails 8 tests on it via the asymmetric `kOpaque8x8` fixture, so the gate holds. | fixed r3 |
| F4 | Major | `tools/check_constraints.sh:19,25,40-44,48-52` | Lint patterns were narrower than NFR-2/NFR-4. **Re-attacked r3:** the same nine injections into `sprite.h` — `std::unique_ptr`, `std::function`, `calloc`, `<hal/gpio_types.h>`, `"soc/soc.h"`, `<sdkconfig.h>`, `static const int kTile = 16;`, `static int arr[16];`, `*p = 16;` — all nine caught, all restored, lint green on the shipped tree. | fixed r2 |
| F5 | Minor | `dirty_tracker.h`, `dirty_tracker.cpp` | Consumer-less public `TileMask::clearAll()`. **Re-confirmed r3:** `grep -rn clearAll firmware/ tools/ docs/` returns nothing. Every remaining public symbol has a consumer (`library` lens §1). | fixed r2 |
| F6 | Minor | `framebuffer.cpp:54-59`, `framebuffer.h:35-44,85-91` | `blit`'s source index overflowed `int32_t`. **Re-confirmed r3 by A/B:** a locally re-narrowed copy still reproduces `runtime error: signed integer overflow: 2147483646 * 8`; against the shipped `int64_t` code that diagnostic is gone and only a plain OOB read at `framebuffer.cpp:62` remains — exactly the caller-precondition violation the doc now names. Documentation-only half accepted (a safe regression test needs a ~3 GB source array). | fixed r2 |
| F7 | Minor | `dirty_tracker.h:66-73,81-82,91-93` | The same-`Framebuffer` precondition for `scan()`/`commit()` was unstated. **Re-confirmed r3:** stated once at class level, referenced from both methods, and it names what goes wrong (poisoned comparison buffer, stale tiles, no diagnostic). | fixed r2 |
| F8 | Minor | `plan.md:24` | Plan claimed "exactly one include path" while the Makefile passed three. **Re-confirmed r3:** the Makefile passes exactly one `-I` (`Makefile:16`), so the plan's original claim is now literally true. | fixed r2 |
| F9 | Nit | `src/clip.h:5-8` | Stale "(later) blit" comment. Corrected by the reviewer in round 1; still correct. | fixed r1 |
| F10 | Nit | `framebuffer.h:49-54` | `width()/height()` duplicated `kScreenWidth/kScreenHeight` — two public spellings of one fact (NFR-6). **Confirmed fixed r3:** the doc comment now names `width()`/`height()` canonical for game/driver code and explains that the constants exist for the engine's own storage sizing, not as a second public API. Both remain correct, so documenting the canonical one is the right resolution for a Nit. | fixed r3 |
| F11 | Major | `Makefile:26-31` | GNU Make 3.81 compares mtimes at 1-second granularity and treats equal as up-to-date; the suite links in ~1.3 s, so a same-second edit was invisible and `make test` printed `43 passed, 0 failed` for code it never compiled — **4 false greens in 10 trials** in round 2. **Confirmed fixed r3 by re-running my own 10-trial stress loop** (build, immediately append a compile-breaking line to `framebuffer.h`, `make test`, restore, ×10): **0/10 false greens**. All four binaries are now unconditionally `.PHONY`, which removes the mtime comparison entirely rather than narrowing its trigger — the stronger fix, and the rationale is written into the Makefile. Cost is a ~1.3 s full relink per invocation and 11 s for `make test-all`, correctly judged worth it. | fixed r3 |
| F12 | Minor | `plan.md:24`, `Makefile:16` | The F8 amendment justified two extra include paths with claims false of the shipped tree. **Confirmed fixed r3 in the code, not just the text:** `CXXFLAGS` now carries only `-I$(INC_DIR)`; no test file includes `clip.h` (`clipping_test.cpp:1-5`; the one grep hit is prose in a comment at `:123`), `test_harness.h` resolves as a same-directory quoted include, and `make clean && make test-all` builds test, selfcheck, asan, gcc and bench binaries clean without the dropped paths. | fixed r3 |
| F13 | Nit | `check_constraints.sh:30-37,43,51` | Two edges of the comment heuristic: a leading-`*` exclusion hid a real `*p = 16;` deref, and the resolution check had no comment exclusion at all so a doc comment saying "240 pixels wide" failed lint. **Confirmed fixed r3 by injection:** both checks now share one rule matching only a leading `//`; `*p = 16;` alone on a line is caught, and prose comments containing `240` or `16` pass. Accepted residual, documented in the script: a `/* */` block-comment continuation line mentioning `240` still fails lint — a loud false positive, deliberately preferred over a silent false negative, and this codebase uses `//` exclusively. | fixed r3 |
| F14 | Nit | `test_harness.h:19-26`, `docs/host-tests.md:15,16,20` | F2's and F4's new gate behaviours were undocumented. Fixed by the reviewer in round 2; **re-confirmed r3** — `runAll`'s contract states the two non-zero paths and the command table describes the zero-match failure and the widened lint patterns. | fixed r2 |
| F15 | Minor | `tools/check_constraints.sh:8-9,19-20,25-26` | `INCLUDE_DIR`/`SRC_DIR` are **relative** paths and each `grep` runs as an `if` condition, where `set -e` does not apply. If a scanned directory is not there, `grep` exits 2, the `if` is false, no violation is reported — and the script prints `make lint OK` and **exits 0 having checked nothing**. Verified three ways: with `INCLUDE_DIR` renamed, and by running `bash tools/check_constraints.sh` from `tools/` and from `$HOME` — every time, four `grep: … No such file or directory` lines on stderr, `make lint OK`, exit 0. This matters because lint is the *only* enforcement of three constitution §6 non-negotiables and is NFR-2's and NFR-4's named verification method; a green gate that scanned nothing is the same class as F1/F11. It is Minor, not Major, because the only wired invocation (`make lint`, from the repo root via `make test-all`) is correct — I re-verified all nine F4 injections through it — so nothing shipping today is unverified; the exposure is an alternate invocation or a future directory move. Fix: `cd "$(dirname "$0")/.."` at the top, plus `for d in "$INCLUDE_DIR" "$SRC_DIR"; do [ -d "$d" ] \|\| { echo "check_constraints: $d missing"; exit 1; }; done`. Left open rather than reviewer-fixed: which of the two the gate should do is the script owner's call. | fixed |
| F16 | Nit | `plan.md:14` | The plan's Handoff summary still says `make test-all` green with "41 tests … bench 0.0016 ms". The tree has **43** tests and measures **0.0014 ms** (§1). Harmless today — the Handoff is explicitly non-authoritative and §3's task table, which is binding, carries no count — but a stale number in the block a future reader reads *first* is how a plan starts rotting. Artifact wording only, changes no gate answer, so capped at Nit. Fix: update the two numbers, or drop them and point at `docs/host-tests.md`. Not reviewer-fixed: `plan.md` is EM-owned and `approved`. | fixed |

## 4. Requirements Traceability

| Spec ID | Implemented at | Verdict |
|---|---|---|
| AC-1.1 | `Makefile:21-37`, `docs/host-tests.md:14` | ✅ met r3 — 43 passed / exit 0 on a clean checkout; header edits rebuild (F1) and a same-second edit no longer yields a stale binary (F11, 0/10) |
| AC-1.2 | `Makefile:39-63`, `test_harness.cpp:81`, `harness_selfcheck.cpp:8` | ✅ met — observed `FAIL selfcheck_deliberate_failure (…/harness_selfcheck.cpp:8: 1 == 2)` + non-zero exit |
| AC-1.3 | `Makefile:69-81` | ✅ met — re-derived again by deleting `setPixel`'s bounds check; target aborted with exit 2 |
| AC-1.4 | `Makefile:29`, `test_main.cpp:7`, `test_harness.cpp:63,95-98` | ✅ met r2 — filtering works and a zero-match filter now exits 2 with a named error |
| AC-2.1…2.3 | `framebuffer.cpp:9,11,15`; `framebuffer_test.cpp:30,35,41` | ✅ met — full read-back, not spot checks |
| AC-2.4…2.6 | `clip.h:28,56`, `framebuffer.cpp:16,29`; `clipping_test.cpp:36-92` | ✅ met — all four edges as separate cases |
| AC-2.7 | ASan/UBSan run over the whole suite | ✅ met |
| AC-2.8 | `framebuffer.h:97` (no statics); `framebuffer_test.cpp:63` | ✅ met |
| AC-2.9 | `clip.h:33-49`; `clipping_test.cpp:96-149` | ✅ met — mutation-verified (int32 clip sum ⇒ test fails) |
| AC-3.1…3.6 | `framebuffer.cpp:41-67`; `sprite_test.cpp:75-196,314` | ✅ met — mutation-verified (a `+1` source-offset shift fails 12 tests) |
| AC-3.7 | `framebuffer.cpp:56-59`; `sprite_test.cpp:200-312` | ✅ met r3 — position-dependent fixture with an independent oracle; 10 addressing mutations tried, all caught (F3) |
| AC-4.1…4.5, 4.7, 4.8 | `dirty_tracker.cpp:28-64`; `dirty_tracker_test.cpp` | ✅ met — sentinel design makes AC-4.8 fall out of the ordinary rule, as planned |
| AC-4.6 | `dirty_tracker.h:44,47`; `config.h:19-24` | ✅ met — `uint32_t[5]`, `static_assert(sizeof == 20)`, even-division asserts |
| NFR-1 | `bench_dirty_scan.cpp`; `docs/host-tests.md:24-36` | ✅ 0.0014 ms vs 5 ms budget, re-measured; device timing correctly not claimed |
| NFR-2 | `framebuffer.h:97`, `dirty_tracker.h:97`; `check_constraints.sh:19` | ✅ met r2 — fixed arrays only, and the grep gate now catches smart pointers, `*alloc`, `std::function` and the rest (7 injections) |
| NFR-3 | `Makefile:70` | ✅ met |
| NFR-4 | `Makefile:16`; `check_constraints.sh:25` | ✅ met r2 — no ESP-IDF include path (structural), and `hal/`/`soc/`/`sdkconfig.h` now caught by lint; the two-compiler claim stays honestly unverified (`/usr/bin/g++` is clang) |
| NFR-5 | `sprite_test.cpp:314`; grepped `src/`+`include/` for `time`/`rand`/`chrono`/`clock` — one hit, the words "compile-time" in a comment | ✅ met |
| NFR-6 | `dirty_tracker.h` / `framebuffer.h` split; `clip.h` in `src/` under `detail` | ✅ met r3 — clean game/driver split, one entry point per operation, no consumer-less export (F5), and the `width()`/`kScreenWidth` duplication now has a documented canonical spelling (F10) |
| NFR-7 | doc comments in all five public headers | ✅ met r2 — all eight required points present, one example per module, the overflow guarantee scoped to a stated precondition (F6) and the scan/commit pairing rule documented (F7) |
| NFR-8 | `test_harness.cpp:81` | ✅ met |
| NFR-9/10 | — | ✅ N/A per spec |
| NFR-11 | `Makefile`, sources — stdlib only | ✅ met — zero third-party code |
| Constitution §6 | palette enum, no allocation, no GPIO, no persistence, tile pipeline | ✅ no non-negotiable violated. "No full-frame push" holds by construction; "persisted data carries a format version" and the GPIO rule remain not applicable. Naming (§5) still conforms. |

## 5. What Was Checked

- [x] Correctness: every Must AC re-traced; 10 blit mutations, 1 clip mutation and 1 bounds-check deletion injected and reverted this round — every one caught, none silently passing
- [x] Non-functional: NFR-1…NFR-8 and NFR-11 re-judged; `library` lens §1 (surface: 14 public symbols, each with a named consumer) and §4 (contract clarity: doc comments + one example per module) walked over all five public headers — surface unchanged this round
- [x] Error handling: unchanged — the API is no-throw/no-error-code by design; the harness's two "ran fewer tests than asked" paths return non-zero. One error path *is* swallowed, in the lint script → F15
- [x] Security: N/A per spec NFR-9 (offline device, no input, no persistence); no secrets, no user-supplied data path in the diff
- [x] Tests: 43 green under clang, g++ and ASan/UBSan; the three AC-3.7 cases were re-read line by line and their oracle confirmed independent of the code under test, not just re-run
- [x] Readability: the Makefile's and lint script's new comments explain *why* (the 3.81 mtime trade, the leading-`//`-only rule) rather than restating the code

## 6. Verdict

This one holds. I re-ran every attack from rounds 1 and 2 from scratch rather than reading the `fixed` cells, and all fourteen findings survive the re-attack as genuinely closed. The two that were still open are the ones I care about, and both were fixed properly rather than narrowly: F11 was answered by making every binary unconditionally `.PHONY` — killing the mtime comparison outright instead of patching its trigger — and my own 10-trial same-second stress loop that produced 4 false greens in round 2 now produces 0, at a cost of ~1.3 s per relink that the Makefile comment defends honestly. F3 was answered by redesigning the fixture, which is what I asked for, and I pushed harder on it than the finding required: round 1's exact mutation now fails `ac_3_7_stride_sub_rectangle_clipped_top`, and nine further mutations of my own — stride from `width`, either offset dropped, offsets swapped, ±1 row, column shear, halved offset, an equal row-and-column shift, a transposed read — are all caught too. I checked specifically whether the diagonal design carries the same class of blind spot it replaced, since `r==c` is symmetric under transposition and invariant under an equal row+column shift; both escape hatches are shut, the first by the asymmetric `kOpaque8x8` fixture elsewhere in the file (8 tests fail) and the second by the window's own 8×8 boundary leaking BRIGHT_ORANGE. The oracle is independent — `onlyWindowSliceEquals` recomputes the expected colour and never reads `sprite.pixels` — so it is not the tautology the old uniform check effectively was. F12 was fixed in the code rather than by rewording the plan, which is the better of the two answers I offered, and every target still builds clean without the dropped paths. What I did find is new and small: the lint script uses relative paths inside `if grep …` conditions where `set -e` does not fire, so a missing directory makes it print `make lint OK` and exit 0 having scanned nothing — I reproduced it three ways, and it is the same silent-green class as F1 and F11, but it is Minor rather than Major because the only wired invocation, `make lint` from the repo root, is correct and I verified all nine injections through it. Plus a stale test count in the plan's handoff block. Neither blocks: no Blocker and no Major is open, all 28 Must ACs trace to code that a mutation can break, `make clean && make test-all` is 43/43 and exit 0 across clang, g++ and ASan/UBSan, and no constitution non-negotiable is violated. This increment is the solid foundation the next story needs — the font-atlas glyph it exists to protect will now turn a test red if its top-clipped rows come from the wrong place. Ship it, and fold F15 and F16 into the next increment.

---

## ✅ REVIEW GATE

*All boxes checked → `/demo-day` may start. Any box open → back to `/increment`. On
re-review, edit this same checklist in place — never duplicate it as a second gate.*

- [x] No open Blocker findings — F1 re-confirmed fixed under re-attack in round 3
- [x] No open Major findings — F2, F3, F4 and F11 all re-confirmed fixed under re-attack in round 3; nothing was waived, none was needed
- [x] Every Must AC traces to implementing code; no constitution non-negotiable violated — all 28 ACs trace and none is ⚠️ any more (§4)
- [x] All plan deviations documented and accepted — T4, T14 and the plan Handoff recorded in §2, each carrying a finding (F15, F16), both non-blocking
- [x] Test suite runs green — `make clean && make test-all`: 43 passed / 0 failed, exit 0 (clang, g++, ASan+UBSan), `test-negative` OK on both assertions, bench 0.0014 ms, lint OK
- [x] Line budget respected: Ist 118 / Soll ~150 (excluding HTML comments)
- [x] Status set to `passed` — two open findings remain (F15 Minor, F16 Nit); neither is a Blocker or a Major, so neither holds the gate
