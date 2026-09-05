# Review Report: collision-system

| | |
|---|---|
| **Phase** | Review |
| **Owner** | Reviewer (`/peer-review`) |
| **Status** | `passed` |
| **Round** | 1 |
| **Date** | 2026-09-05 |

**Handoff**
- **Status:** `passed` — all 15 Must ACs (US-1, US-2) and all 3 Should ACs (US-3) trace to implementing code and to tests that actually discriminate; every gate re-run by the reviewer, not taken on report.
- **Verdict:** Ships. The overflow double-proof is real (both detectors re-derived from scratch and observed firing), the four new lint rules all fire, and no trace of `/increment`'s four mutation passes survives. Two low-risk fixes applied by the reviewer; four Minors/Nits left open, none gate-blocking.
- **Open:** `4 open` — Blockers: none; Majors: none (Minors F3, F4; Nits F5, F6, F7 — see §3)
- **Binding ruling:** §6 Verdict and the gate checklist below — the only binding location; there is no other round to point to
- **On conflict:** the numbered body below wins for everything except `Status`; log the mismatch as a finding at the next `/peer-review` and proceed — don't stop on it.

## 1. Scope

Reviewed the full working-tree state of `collision-system` (nothing committed yet; `git status --short`). Read in full as new: `firmware/steamcore/include/steamcore/collision.h`, `test/collision_test.cpp`, `test/collision_overflow_test.cpp`, `test/collision_dispatch_test.cpp`, `test/collision_sweep_test.cpp`, `test/bench_collision.cpp`. Read as `git diff` against HEAD: `Makefile`, `tools/check_constraints.sh`, `docs/host-tests.md`.

**Gates re-run by the reviewer, not taken on report:** `make test` (197/0), `make test-asan` (197/0), `make test-gcc` (197/0), `make lint` (OK), `make bench` (OK — `overlaps()` 5.06 ns/call, ratio 1.92x; `sweepCollisions()` 0.95 µs/sweep, ratio 1.86x, budget 1.6667 ms). All five re-run again after the reviewer's own edits; still green.

**Independently re-derived, not cited from `plan.md`** (trigger condition (b), Must-AC verification, plus (d), concrete doubt about the lint rules): T4's overflow mutation, both detectors, in a scratch copy — compile-time (`static_assert expression is not an integral constant expression`, `value 2147483648 is outside the range of representable values of type 'int'`) and runtime under UBSan (`signed integer overflow: 2147483646 + 2 cannot be represented in type 'int'`, exit 134). T4's degenerate-guard and strict-inequality mutations, by exhaustive replay of the shipped battery. All four T7 lint rules, by running the real `check_constraints.sh` against a mutated copy of the tree (rules (c)/(d) fail the gate on deletion; (a)/(b) on a planted violation). The shipped `collision.h`/`check_constraints.sh` carry no residue of any mutation.

**Not reviewed:** no device-side surface exists (spec A7/NFR-12 — deliberate, per plan §4, not a gap). No blast-radius tool was used: `aspark-graph` is unbuilt for this repo and indexes none of this project's languages; components were scoped by hand from plan §2. No active lens beyond the constitution's scoped `library` lens, which lands as NFR-6/NFR-7 below.

## 2. Plan Conformance

| Task | Implemented as planned? | Note |
|---|---|---|
| T1 | ✅ | `collision.h:54-91` matches §1 Decisions 3/4/6 line for line; includes only `<cstdint>`/`<type_traits>`; no resolution or tile-size literal. |
| T2 | ✅ | Table-driven, both argument orders (`collision_test.cpp:27-30`), all four touching configs + the four one-past counterparts + corner-only + the degenerate rows + the `memcmp` purity test. See F1 for the one row class the battery could not discriminate. |
| T3 | ✅ | 5 `static_assert`s + the same 5 cases as `volatile`-laundered runtime `CHECK`s. Both halves verified to fail independently (§1). |
| T4 | ✅ | All three mutations reverted cleanly — re-derived, not trusted. |
| T5 | ✅ | All three callable kinds, order compared by address, the caller's own functor instance observed (no copy), idempotence. |
| T6 | ✅ | `sweepCollisions` delegates to `checkCollision` (`collision.h:104`); nullptr/0/1/negative-count no-ops; N(N−1)/2 against the computed formula at N=2/3/8/32. |
| T7 | ✅ | Two file lists as planned; `bench_collision.cpp` correctly excluded from `CLOCK_RNG_PATTERN` and correctly included in `SCOPED_ALLOC_PATTERN`; all four rules verified to fire. |
| T8 | ⚠️ | Doc comment covers every NFR-7 item, but R7's own stated mitigation ("T8's doc comment states the sweep reads each entity at the moment its pair is tested") was missing — F2, fixed. Public-surface audit re-confirmed: exactly `Entity`, `overlaps`, `checkCollision`, `sweepCollisions`. |
| T9 | ⚠️ | Bench wired and gating as planned, and the doubling check does prove the loop ran — but the generated workload contains zero collisions (F3). |
| — | ✅ | **Deviation (T9 before T7) holds up.** T9 depends only on T6, T8 on T5+T7; no declared dependency is violated, and the missing-file-is-a-failure branch (`check_constraints.sh:522-525, 537-540`) is real, so building T7 first would genuinely have left `make lint` red. Final script state verified correct independently of build order. |
| — | ✅ | Header-only with no `.cpp` deviates from constitution §5's "`.h`/`.cpp` pairs" — reasoned in plan §1 Decision 1 (two of three functions are templates, `constexpr` needs a visible definition) and consistent with 8 existing header-only engine headers. Accepted, not a finding. |

## 3. Findings

| # | Severity | Location | Finding | Status |
|---|---|---|---|---|
| F1 | Minor | `firmware/steamcore/test/collision_test.cpp:109-122` | AC-1.4's negative-size coverage passed for the wrong reason. Of the six rows, only `{5,5,0,0}` vs `{0,0,10,10}` could detect the degenerate guard's removal; the two `w<0`/`h<0` rows sit at the origin, where the naive math returns `false` anyway. Verified empirically: a guard weakened from `<= 0` to `== 0` survived the entire suite. AC-1.4's "width ≤ 0" half was therefore unverified. **Fix applied:** two bracketing rows added (`{10,0,-5,10}` vs `{0,0,20,10}` and the y-mirror), where the inverted extent satisfies `bx0 < ax1 < ax0 < bx1` and the weakened guard reports `true`. | fixed |
| F2 | Minor | `firmware/steamcore/include/steamcore/collision.h:93-98` | The `sweepCollisions` doc comment omitted the mutating-callback caveat that plan R7 names as its own mitigation ("documented rather than discovered") — an NFR-7 contract-clarity gap on the one real trap this API has. **Fix applied:** four-line note added stating each entity is read when its own pair is tested, so a mutating callback changes later pairs within the same sweep. | fixed |
| F3 | Minor | `firmware/steamcore/test/bench_collision.cpp:61-69`, `91-101` | The bench's LCG pool spreads 1024 entities of ~10–49 px over a ±1000 coordinate range, so **not one of the 8,000,000 measured pairs overlaps, and the 32-entity sweep's callback fires 0 of 496 possible times per pass** (reviewer-measured). NFR-1's headline numbers therefore measure only the early-exit path, and `measureSweep`'s `volatile` sink — the file's own stated R1 mitigation — is never written at all. Reviewer-measured worst case: 1.641 µs/sweep all-overlapping vs 1.118 µs zero-collision, still ~1000x inside the 1.6667 ms budget, so NFR-1's *conclusion* is unaffected — the reported figure is simply a best case. Fix: tighten the coordinate range (e.g. ±100) so a realistic hit rate appears, or measure an all-overlapping variant as a third line. Left open — it changes the number plan §3 T9 and QA will record. | open |
| F4 | Minor | `docs/host-tests.md:56`; `firmware/steamcore/test/bench_collision.cpp:2-5` | The doc says both collision figures run "against 10% of the 16.67 ms 60 Hz tick", but only the sweep is compared to a budget (`bench_collision.cpp:160`); `overlaps()`'s ns/call is printed and never gated. The file's own header comment still names the superseded `>= 1,000,000` / `>= 100,000` floors rather than the shipped 8,000,000 / 400,000. Fix: say "the sweep against 10% of the tick; `overlaps()` reported per call, not gated", and update the two floors. | open |
| F5 | Nit | `docs/host-tests.md:15-45` | The intro carries a per-feature paragraph for `display-driver`, `input-driver` and `start-screen` explaining each one's host/device split, but none for `collision-system` — the first feature with no device half at all, which is exactly this section's subject. Fix: one sentence recording that all of US-1/US-2/US-3 are host-CI-verified with no device-only AC. | open |
| F6 | Nit | `firmware/steamcore/include/steamcore/collision.h:43-50` | The usage example does not compile as written from outside the namespace: the lambda parameters are bare `Entity&` while everything else is `steamcore::`-qualified, and `input` is unused. Fix: qualify both parameters (or add a `using`), and drop or use `input`. | open |
| F7 | Nit | `firmware/steamcore/test/collision_sweep_test.cpp:49-55` | `PairRecorder` silently stops recording index pairs past `kMaxPairs` (32) while `count` keeps incrementing. Harmless at N=5 today, but a future case with more than 32 overlapping pairs would compare against truncated arrays with no signal. Fix: `CHECK(count < kMaxPairs)` before the store. | open |

## 4. Requirements Traceability

| Spec ID | Implemented at | Verdict |
|---|---|---|
| AC-1.1 | `collision.h:75`; `collision_test.cpp:35, 73` | ✅ met |
| AC-1.2 | `collision.h:75`; `collision_test.cpp:42, 90` | ✅ met |
| AC-1.3 | `collision.h:75` (four strict `<`); `collision_test.cpp:73, 90, 102` | ✅ met — pinned from both sides; mutation to `<=` reproducibly fails the suite |
| AC-1.4 | `collision.h:71`; `collision_test.cpp:109` | ✅ met (after F1 — negative half was non-discriminating before) |
| AC-1.5 | `collision.h:72-74`; `collision_overflow_test.cpp:34-99` | ✅ met — both detectors independently re-derived firing; UBSan clean on the real code |
| AC-1.6 | `collision.h:70` (`const&`); `collision_test.cpp:126` | ✅ met |
| AC-1.7 | `collision.h:3-4`; `check_constraints.sh:527-547`; `make test-gcc` | ✅ met — no allocation, no ESP-IDF header, both compilers green. Two-compiler half stays *nominal*: `/usr/bin/g++` is Apple clang (docs/host-tests.md "Toolchain reality"), a project-wide documented condition, not this feature's defect |
| AC-2.1 | `collision.h:90`; `collision_dispatch_test.cpp:40, 62, 86` | ✅ met |
| AC-2.2 | `collision.h:90`; `collision_dispatch_test.cpp:51, 73, 98` | ✅ met |
| AC-2.3 | `collision.h:90`; `collision_dispatch_test.cpp:110, 123` | ✅ met — compared by address, so a copy is caught alongside a swap |
| AC-2.4 | `collision.h:84-89`; all three callable kinds tested; `check_constraints.sh:537-547` bans `std::function` | ✅ met |
| AC-2.5 | `collision.h:85-91` (no state); `collision_dispatch_test.cpp:137` | ✅ met |
| AC-3.1 | `collision.h:100-107`; `collision_sweep_test.cpp:30-36, 103` | ✅ met |
| AC-3.2 | `collision.h:101`; `collision_sweep_test.cpp:61, 71, 81, 90` | ✅ met |
| AC-3.3 | `collision.h:102-106`; `collision_sweep_test.cpp:126, 141` | ✅ met — against the computed formula, not hardcoded counts |
| NFR-1 | `bench_collision.cpp`; `Makefile:154-157` | ⚠️ partial — measured and inside budget, but on a zero-collision workload (F3); reviewer-measured worst case still ~1000x inside budget |
| NFR-2 | `check_constraints.sh:537-547` | ✅ — rule verified to fire on a planted `std::vector` |
| NFR-3 | `make test-asan` re-run: 197/0 | ✅ |
| NFR-4 | `make test-gcc` re-run: 197/0; `-Wall -Wextra -Werror` clean; no resolution literal | ✅ (same nominal-GCC caveat as AC-1.7) |
| NFR-5 | `check_constraints.sh:519-535` | ✅ — rule verified to fire on a planted `<chrono>`; `bench_collision.cpp`'s exemption matches the `bench_game_loop.cpp` precedent and is documented in both the script and `docs/host-tests.md` |
| NFR-6 | `collision.h:54, 70, 85, 100` | ✅ — public surface is exactly the four planned symbols, re-audited by grep; no shipped type's header touched |
| NFR-7 | `collision.h:6-50, 67-98` | ✅ (after F2) — every listed item present, plus one usage example (see F6) |
| NFR-12 | Whole feature | ✅ — 100% host-CI-verifiable; nothing claimed as hardware-verified |

*NFR-8/9/10/11 are N/A per the spec; nothing in the diff changes that.*

## 5. What Was Checked

- [x] Correctness: logic does what the acceptance criteria demand
- [x] Non-functional: applicable NFRs and constitution quality bars hold (§3 no dynamic allocation, C++17, no ESP-IDF header in `include/`; §6 non-negotiables — none reachable by this diff except no-dynamic-allocation, which holds)
- [x] Error handling: no failure mode exists to swallow — the only defensive paths (`nullptr`, `count <= 0`, degenerate size) are explicit early returns, each tested
- [x] Security: N/A per constitution §2 — no input crosses a trust boundary, no secret, no I/O
- [x] Tests: exist, discriminate (verified by re-running the mutations myself), and pass on all three toolchain configurations
- [x] Readability: the next developer will understand this

## 6. Verdict

This passes. The feature does the one thing it promised and proves it rather than asserting it: `overlaps` is 6 lines, the widening genuinely happens before every addition, and I confirmed from scratch — not from the increment's own account — that both halves of the AC-1.5 double proof actually fire, that all four new lint rules fail the gate when their guard is removed, and that none of `/increment`'s four mutation passes left residue behind. The public surface is exactly the four planned symbols, no shipped type was touched, and the T9-before-T7 deviation is sound reasoning that produced a correct final script either way. I found one real gap worth the review: AC-1.4's negative-size rows were positioned so that they returned `false` whether or not the guard existed, meaning the `w < 0` half of a Must acceptance criterion was passing for the wrong reason — a guard weakened from `<= 0` to `== 0` survived the whole suite. I fixed that with two bracketing rows and re-ran every gate. I also added the mutating-callback caveat plan R7 had asked T8 to document. What remains open is honest but not blocking: the benchmark's generated workload contains literally zero collisions, so NFR-1's headline number measures the early-exit path only — I measured the all-overlapping worst case at 1.641 µs against a 1.6667 ms budget, so the conclusion stands and the finding is about the evidence's quality, not the answer. Left open for the developer rather than fixed here, because changing it changes a number `plan.md` §3 T9 records and QA will re-run.

---

## ✅ REVIEW GATE

*All boxes checked → `/demo-day` may start. Any box open → back to `/increment`. On
re-review, edit this same checklist in place — never duplicate it as a second gate.*

- [x] No open Blocker findings
- [x] No open Major findings (or explicitly waived by the user, with reason recorded here) — none found
- [x] Every Must AC traces to implementing code; no constitution non-negotiable violated
- [x] All plan deviations documented and accepted — the T9/T7 order swap (plan §Deviations) and the header-only shape (plan §1 Decision 1) are both reasoned and hold up; T8/T9's ⚠️ rows are recorded as F2 (fixed) and F3 (open Minor)
- [x] Test suite runs green — `test`/`test-asan`/`test-gcc` 197/0 each, `lint` OK, `bench` OK; re-run after the reviewer's edits
- [x] Line budget respected: Ist 112 / Soll ~150 (excluding HTML comments) — 38 under
- [x] Status set to `passed`
