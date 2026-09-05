# QA Report: collision-system

| | |
|---|---|
| **Phase** | Review (hands-on) |
| **Owner** | QA Tester (`/demo-day`) |
| **Input** | `.spark/collision-system/spec.md`, `plan.md`, `review.md` (`passed`, round 1) |
| **Status** | `passed` |
| **Round** | 1 |
| **Date** | 2026-09-05 |

**Handoff**
- **Status:** `passed`.
- **Verdict:** Yes — I'd demo this. All 15 Must ACs (US-1, US-2) and all 3 Should ACs (US-3) independently re-run by me, not taken on `/increment`'s or `/peer-review`'s word, all green. `make test`/`test-asan`/`test-gcc` 197/0 each, `make lint OK`, `make bench OK`. No framebuffer/device QA applies (spec A7/NFR-12) — this is a pure host-CI feature by design, not a gap.
- **Open:** `none` — 0 Blockers, 0 Majors. review.md's F3/F4 (Minor, benchmark evidence quality / doc wording) remain open and accepted by the user as non-blocking; nothing new found.
- **Binding ruling:** §5 Verdict and the gate checklist below.
- **On conflict:** the numbered body below wins for everything except `Status`.

## 1. Test Environment

- **App URL:** N/A — constitution §8: `Browser-observable surface: no`.
- **Method used:** host-compiled unit-test half of the declared substitute (§8). The framebuffer-dump half does not apply to this feature at all: `collision-system` is pure geometry with zero rendering surface and zero device-side component, by design (spec A7/NFR-12, plan §4 "no device task at all" — confirmed by reading `plan.md` §2/§4 and `check_constraints.sh`, not asserted). This is the correct outcome for this feature, not an uncaptured gap.
- **Host / toolchain:** macOS, Apple clang 14 (`clang++`), `/usr/bin/g++` (itself Apple clang — documented pre-existing project condition, `docs/host-tests.md` "Toolchain reality"), `make`.
- **Test data:** none required — pure in-memory `Entity` values constructed inline by each test.

## 2. Acceptance Criteria Verification

All commands run by me from repo root; commands and outputs below are as observed, not cited from `review.md`.

| Spec ID | Steps performed | Expected | Observed | Result |
|---|---|---|---|---|
| AC-1.1 | Ran `build/steamcore_tests collision_clearly_separated` (isolated) | 1 passed, 0 failed | `1 passed, 0 failed` | ✅ pass |
| AC-1.2 | Ran `build/steamcore_tests collision_entities_sharing` | 1 passed, 0 failed | `1 passed, 0 failed` | ✅ pass |
| AC-1.3 | Ran `collision_touching_edges`, `collision_one_pixel_past`, `collision_corner_only` individually | all pass | `1 passed, 0 failed` x3 | ✅ pass |
| AC-1.4 | Ran `collision_zero_or_negative` | 1 passed, 0 failed (covers both `w<0`/`h<0` bracketing rows per review F1 fix) | `1 passed, 0 failed`; confirmed the F1 fix rows (`{10,0,-5,10}` etc.) are present in `collision_test.cpp:109-122` | ✅ pass |
| AC-1.5 | Ran `build/steamcore_tests collision_overflow` (release) and `build/steamcore_tests_asan collision_overflow` (UBSan); read `collision_overflow_test.cpp` for the 5 namespace-scope `static_assert`s | Both binaries pass; static_asserts present; UBSan exit 0 | Release: `1 passed, 0 failed`. ASan/UBSan: `1 passed, 0 failed`, exit 0. Confirmed 5 `static_assert(overlaps(...))`/`static_assert(!overlaps(...))` at lines 34, 39, 44, 50, 56, each paired with a matching runtime `CHECK` at lines 79-99. Did not re-run the mutation testing myself (already done twice per task instructions — `/increment` T4 and independently re-derived by the reviewer); confirmed instead that the tests these mutations were verified against still exist and still pass under `test-asan` specifically | ✅ pass |
| AC-1.6 | Ran `collision_overlaps_does_not_mutate` | 1 passed, 0 failed | `1 passed, 0 failed` | ✅ pass |
| AC-1.7 | Read `collision.h:1-4` (includes only `<cstdint>`, `<type_traits>`); ran `make test-gcc` | No ESP-IDF header, no allocation, identical results on both compilers | Includes confirmed clean; `make test-gcc`: `197 passed, 0 failed` — identical to `make test`'s clang result. Two-compiler claim stays *nominal* per `docs/host-tests.md`: `/usr/bin/g++` is Apple clang on this host, a project-wide documented condition predating this feature, not a collision-system defect | ✅ pass |
| AC-2.1 | Ran `collision_dispatch_capturing_lambda_fires_once_on_overlap`, `..._free_function_fires_once_on_overlap`, `..._stateful_functor_fires_once...` (all 3 callable kinds) | each fires exactly once | Full `collision_dispatch` filter: `9 passed, 0 failed` | ✅ pass |
| AC-2.2 | Same filter, `..._never_fires_without_overlap` variants (all 3 kinds) | callback never invoked | included in the 9/9 pass above | ✅ pass |
| AC-2.3 | Ran `collision_dispatch_callback_receives_a_first_b_second`, `..._order_holds_when_b_is_left_of_and_above_a` | order never swapped, including geometrically-swapped case | both pass (within the 9/9) | ✅ pass |
| AC-2.4 | Read `collision.h:84-89` (`template <typename Callback>`, `static_assert(std::is_invocable_v<...>)`); read `check_constraints.sh:537-549` (SCOPED_ALLOC_PATTERN over the collision file set bans `std::function`) | template-only dispatch, no `std::function`, enforced by lint | Confirmed in source and confirmed the lint rule is live (see NFR-2 below) | ✅ pass |
| AC-2.5 | Ran `collision_dispatch_is_idempotent` | same pair/callable run twice detects identically both times | `1 passed` (within the 9/9) | ✅ pass |
| AC-3.1 | Ran `collision_sweep_fires_for_exactly_the_known_overlapping_pairs` | fires exactly once per overlapping pair, never `i==j` | pass (within `collision_sweep` filter: `7 passed, 0 failed`) | ✅ pass |
| AC-3.2 | Ran `collision_sweep_count_zero_never_fires`, `..._count_one_never_fires`, `..._nullptr_is_noop`, `..._negative_count_is_noop` | no crash, callback never fires | all 4 pass (within the 7/7) | ✅ pass |
| AC-3.3 | Ran `collision_sweep_all_overlapping_fires_exactly_n_choose_2`, `..._all_disjoint_fires_zero_times` | exactly N(N-1)/2 at N=2,3,8,32; zero when disjoint | both pass (within the 7/7) | ✅ pass |
| NFR-1 | Ran `make bench` myself | overlaps()/sweepCollisions() comfortably inside the 16.667 ms tick / 1.6667 ms (10%) sweep budget | `overlaps(): 5.43 ns/call over 8,000,000 pairs (doubled ratio 1.86x)`; `sweepCollisions(): 0.96 us/sweep (32 entities) over 400,000 passes (doubled ratio 1.95x)` — both `<< ` budget (sweep ~1735x under 1.6667 ms). Doubling ratios in the 1.86-1.95x range confirm the loop isn't optimized away. Known, accepted gap (review F3, non-blocking): this run's random LCG workload produced the same zero-collision early-exit path the reviewer already flagged — not a fresh finding, and the reviewer separately worst-case-measured 1.641 us/sweep all-overlapping, still ~1000x inside budget | ✅ pass |
| NFR-2 | Ran `make lint`; read `check_constraints.sh:537-551` | zero dynamic allocation, no `std::vector`/`std::function` anywhere in the collision file set (header + all 4 test files + bench) | Rule present and firing clean: `--- no dynamic allocation in the collision-system file set (NFR-2...) ---` in `make lint OK` output | ✅ pass |
| NFR-3 | Ran `make test-asan` (full suite, includes AC-1.5's extreme cases) | zero UBSan/ASan findings | `197 passed, 0 failed`, clean exit | ✅ pass |
| NFR-4 | Ran `make test-gcc`; read `collision.h` for resolution/tile-size literals | clean on both compilers, `-Wall -Wextra -Werror`, no resolution literal outside `config.h` | `197 passed, 0 failed`; no bare `16`/`240`/`160`/`480`/`320` found in `collision.h`; existing include/-wide lint rules confirmed still passing over it | ✅ pass |
| NFR-5 | Ran `make lint`; read `check_constraints.sh:513-535` | no wall-clock read or unseeded RNG in the collision mechanism | Rule present and firing clean: `--- no wall-clock read or unseeded RNG in the collision mechanism (collision-system NFR-5) ---`; confirmed file set is `collision.h` + all 4 test files (bench correctly excluded, matching `bench_game_loop.cpp` precedent) | ✅ pass |
| NFR-6 | Read `collision.h` in full; grepped for top-level declarations | exactly `Entity`, `overlaps`, `checkCollision`, `sweepCollisions` — no other public symbol, no shipped type touched | Confirmed exactly those 4 symbols at lines 54, 70, 85, 104; `git status` confirms `game_loop.h`, `game_state.*`, `input.h`, `framebuffer.*`, `sprite.h` are not modified by this feature | ✅ pass |
| NFR-7 | Read `collision.h:6-50` doc comment | states field set/units, touching/zero-size semantics, overflow guarantee, callback order/statelessness, inherited contract, one usage example | All present, including the mutating-callback caveat (review F2 fix, lines 100-103) | ✅ pass |
| NFR-12 | Whole feature | 100% host-CI-verifiable, no hardware claim | Confirmed — every AC above was verified by a host command I ran myself; no framebuffer dump exists or is claimed | ✅ pass |

NFR-8/9/10/11 are N/A per spec (no observability/security/accessibility/integration surface) — confirmed no such surface exists in `collision.h`.

## 3. Exploratory Findings

Beyond the AC battery, I read the full test files for `collision_test.cpp`, `collision_overflow_test.cpp`, `collision_dispatch_test.cpp`, `collision_sweep_test.cpp` to look for gaps the mapped tests above might have missed (double-checking degenerate combinations, argument-order symmetry, N=0/1 edges — the "double-click / empty-input / boundary" equivalents for a pure-logic feature). No new bug found. Two already-known, already-open Minors from `review.md` remain and are not re-litigated here:

| # | Severity | Steps to reproduce | Expected vs. observed | Status |
|---|---|---|---|---|
| B1 (= review F3) | Minor | Read `bench_collision.cpp`'s LCG workload; re-ran `make bench` | Expected a representative hit-rate benchmark; observed the same zero-actual-collision early-exit path the reviewer already measured — confirms the gap is real and current, not fixed, but it was never claimed fixed | open (accepted, non-blocking per review.md) |
| B2 (= review F4) | Minor | Read `docs/host-tests.md:56` | Doc still says both figures run "against 10% of the tick," but only the sweep is gated; confirmed still present, unchanged | open (accepted, non-blocking) |

No genuinely new bug was found during this pass.

## 4. Console & Network

N/A — no browser, no network surface for this feature (constitution §8, spec A7). Checked instead: compiler warnings (`-Wall -Wextra -Werror` clean on both toolchains, 0 warnings across `make test`/`test-gcc`) and sanitizer output (`make test-asan`: 0 ASan/UBSan findings). Both clean.

## 5. Verdict

Yes, I would demo this right now. I ran every gate myself rather than trusting `/increment`'s or `/peer-review`'s report: `make test` (197/0), `make test-asan` (197/0, UBSan clean including the AC-1.5 extreme-value cases), `make test-gcc` (197/0), `make lint` (OK, all 4 collision-system rules present and confirmed in `check_constraints.sh`), `make bench` (OK, both figures three orders of magnitude inside budget). I then narrowed to individual tests per AC (26 collision-specific tests total, matching the sum of the four test files' `STEAMCORE_TEST` counts exactly) and confirmed each Must and Should AC has at least one test that discriminates it, not merely a suite that happens to be green. The two open Minors (benchmark workload realism, a doc-wording overstatement) are known, already accepted by the user in `review.md`, and don't change any Must AC's verified status. No framebuffer/device-side QA applies to this feature — that is the correct, spec-anticipated outcome for a pure-geometry primitive with zero rendering surface (A7/NFR-12), not an uncaptured gap.

---

## ✅ QA GATE

- [x] Every Must-story acceptance criterion verified (by me, running the actual test) and passed — AC-1.1 through AC-2.5 (12 ACs)
- [x] Every applicable NFR verified and passed (browser-observable substitute: host-CI observable) — NFR-1 through NFR-7, NFR-12 (NFR-8/9/10/11 N/A per spec)
- [x] No open Blocker or Major bugs — 0 found; 2 pre-existing accepted Minors (B1/B2 = review F3/F4) carried forward, not blocking
- [x] Host toolchain output free of warnings/errors on the tested flows (substitute for "console free of errors") — `-Wall -Wextra -Werror` clean on both compilers, 0 UBSan/ASan findings
- [x] Tested on both agreed toolchains (clang++, g++-as-Apple-clang) — substitute for "agreed viewports," per constitution §8 N/A for viewports
- [x] Line budget respected: Ist 121 / Soll ~130 (excluding HTML comments)
- [x] Status set to `passed`
