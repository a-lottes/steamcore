# Plan: collision-system

| | |
|---|---|
| **Phase** | Plan |
| **Owner** | Engineering Manager (`/sprint-plan`) |
| **Input** | `.spark/collision-system/spec.md` (`approved`) |
| **Status** | `approved` |
| **Date** | 2026-09-05 |

**Handoff**
- **Status:** `approved` — user approved 2026-09-05; all 9 tasks now `done` (increment complete, 2026-09-05).
- **Summary:** `firmware/steamcore/include/steamcore/collision.h` — the POD `Entity` (four `int32_t`s) plus
  `constexpr bool overlaps(const Entity&, const Entity&)`, `template <typename Callback> void
  checkCollision(Entity&, Entity&, Callback&&)`, and (Should) `template <typename Callback> void
  sweepCollisions(Entity*, int32_t, Callback&&)` — exactly as planned in §1, plus four test files and
  `bench_collision.cpp`. Same one-file-per-concern shape `game_loop.h`/`input.h` already use. Overflow safety is
  `clip.h`'s widen-before-summing idiom, made `constexpr` so the `int32_t`-extreme cases are additionally proven
  at compile time. `make test-all` green (197 passed / 0 failed, `make lint OK`, `make bench OK`); `make
  test-asan`/`make test-gcc` both green on every collision test. Ready for `/peer-review`.
- **Open:** none — all 9 tasks `done`, see §3.
- **Deliberate absence, not an oversight:** this is the first feature in this project's history with **no
  device-side task at all** — no hardware-gated row, no `firmware/system/` harness, no `CMakeLists.txt` entry.
  Spec A7/NFR-12: pure geometry, zero I/O, zero rendering surface. §4 records this explicitly.
- **Binding ruling:** §3 Task Breakdown for current task status; a plan revision after review/QA findings updates §1/§3 in place, never a new section
- **On conflict:** the numbered body below wins for everything except `Status`; log the mismatch as a finding at the next `/peer-review` and proceed — don't stop on it.

## 1. Architecture Decision

- **Context:** The spec has already settled every load-bearing product question (A1–A10, C1–C5): `Entity` is a
  four-field POD, dispatch is a compile-time template callable, touching is not colliding, zero/negative size
  never overlaps, `int32_t` extremes must be UB-free, and no shipped type is touched. What is left is genuinely
  technical: where the type lives, the exact function shapes, the specific overflow-safe arithmetic, and how the
  overflow claim is *proven* rather than asserted.

- **Decision:**
  1. **One header, `include/steamcore/collision.h`, and no `.cpp`.** `Entity` has no meaning independent of
     collision detection inside this spec's scope, and this codebase already keeps a POD next to the mechanism
     that consumes it (`game_loop.h` = `GameInput` + `GameLoop<Game>`; `input.h` = `InputSignal` + `Debouncer` +
     `InputReader`). A separate `entity.h` would be a header that could never be usefully included alone.
     Header-only, deviating from `start-screen`'s "header-only is reserved for templates" note **for a stated
     reason**: two of the three functions genuinely *are* templates, and `overlaps` must be `constexpr`
     (Decision 3), which requires its definition to be visible. Consequence: no `ENGINE_SRCS` entry, no
     `CMakeLists.txt` entry, nothing to link.
  2. **The header includes `<cstdint>` and `<type_traits>` and no `steamcore/` header at all** — not even
     `config.h`: A6 decided entity coordinates are not bounded by or clipped to the screen. That zero-include
     line is the structural expression of A8 ("touches no existing type"); it cannot drift.
  3. **`overlaps` — `constexpr`, widen-then-compare, degenerate-guard first.**

         constexpr bool overlaps(const Entity& a, const Entity& b) {
           if (a.w <= 0 || a.h <= 0 || b.w <= 0 || b.h <= 0) return false;  // A5
           const int64_t ax0 = a.x, ay0 = a.y, bx0 = b.x, by0 = b.y;
           const int64_t ax1 = ax0 + a.w, ay1 = ay0 + a.h;   // exclusive
           const int64_t bx1 = bx0 + b.w, by1 = by0 + b.h;   // exclusive
           return ax0 < bx1 && bx0 < ax1 && ay0 < by1 && by0 < ay1;
         }

     **Why this is actually overflow-free, not merely believed to be:** every input is `int32_t`, so each value
     lies in `[-2^31, 2^31-1]`; the sum of any two therefore lies in `[-2^32, 2^32-2]`, which is exactly
     representable in `int64_t` (`[-2^63, 2^63-1]`) with 31 bits of headroom. The widening happens **before** the
     addition — `ax0` is already `int64_t`, so `ax0 + a.w` promotes `a.w` and adds in 64-bit; writing
     `int64_t ax1 = a.x + a.w;` would add in 32-bit and *then* widen, which is precisely the bug (T4 mutates
     exactly this line). Unlike `clip.h`, nothing is ever narrowed back — the result is a `bool`, so there is no
     second narrowing step to argue about. This is `clip.h`'s idiom minus its one hard part.

     The four strict `<` comparisons are A4 (touching is not colliding): `a`'s right edge `ax1` equal to `b`'s
     left edge `bx0` fails `bx0 < ax1`. The degenerate guard is **load-bearing, not decorative**: with `w < 0`,
     `ax1 < ax0`, and `bx0 < ax1 < ax0 < bx1` is satisfiable — a negative-width entity would report `true`
     without it. T4 mutates that too.

     `constexpr` is the second, independent proof of AC-1.5. Signed overflow is undefined behaviour, and UB is
     **not a constant expression**: a `static_assert(overlaps(...))` over `INT32_MAX`-class inputs fails to
     *compile* if the arithmetic ever narrows to 32-bit. So the claim is guarded by the compiler at build time
     *and* by UBSan at run time, on both toolchains, with no reviewer diligence in between.
  4. **`checkCollision` — forwarding reference, `std::is_invocable_v` static_assert.**

         template <typename Callback>
         void checkCollision(Entity& a, Entity& b, Callback&& callback) {
           static_assert(std::is_invocable_v<Callback&, Entity&, Entity&>,
                         "Callback must be callable as callback(steamcore::Entity&, steamcore::Entity&)");
           if (overlaps(a, b)) callback(a, b);
         }

     `Callback&&` rather than by value: by value silently copies the callable, so a stateful counting functor's
     mutations would land in a throwaway copy — the same footgun class `game_loop.h`'s review F10 documented for
     a by-value `Framebuffer`. `std::is_invocable_v` rather than `game_loop.h`'s `std::void_t` detection idiom:
     `void_t` exists there to detect a *member call expression* on a consumer type, which has no standard trait;
     "is this callable with these arguments" is exactly what C++17's `is_invocable` already spells, and inventing
     a bespoke trait for it would be the exciting choice, not the boring one. The named-error *outcome* — the
     thing the codebase actually cares about — is identical. `Entity&` (mutable) matches the constitution §2
     sketch `onCollision(Entity&, Entity&)` and lets a callback act on what it hit; `checkCollision` itself never
     writes either entity, and `overlaps`' `const&` makes AC-1.6 a compile-time fact.
  5. **`sweepCollisions(Entity* entities, int32_t count, Callback&& callback)` (Should).** Pointer + count, not a
     `Entity (&)[N]` template-size reference: a zero-length C array is ill-formed in C++, and AC-3.2 explicitly
     requires N = 0 to run cleanly. Guarded by `if (entities == nullptr || count <= 0) return;`, then the
     canonical `for i, for j = i+1` double loop **delegating to `checkCollision(entities[i], entities[j],
     callback)`** — so "the sweep and the pair test can never disagree" is structural, and AC-3.1's
     never-against-itself and AC-3.3's exactly `N(N-1)/2` are properties of the loop bounds, not of a second copy
     of the overlap logic.
  6. **Public surface: exactly four symbols** — `Entity`, `overlaps`, `checkCollision`, `sweepCollisions` —
     matching NFR-6's own count (one POD + two Musts + the optional sweep). Two `static_assert`s pin the type:
     `sizeof(Entity) == 4 * sizeof(int32_t)` (mirroring `game_loop.h`'s `GameInput` size guard — a fifth field or
     a `virtual` member both change `sizeof`) and `std::is_standard_layout_v<Entity>`.

- **Alternatives considered:**

  | Alternative | Why rejected |
  |---|---|
  | A separate `entity.h` + `collision.h` | `Entity` has no standalone meaning in this scope; a header nobody can usefully include alone. Both existing multi-symbol headers (`game_loop.h`, `input.h`) already colocate the POD with its mechanism |
  | `.h`/`.cpp` pair for `overlaps`, per `start-screen`'s "header-only is for templates" note | Kills `constexpr`, and with it the compile-time half of the AC-1.5 proof — the single strongest guarantee this feature has. Two of three functions are templates anyway, so the `.cpp` would hold four comparison operators and nothing else |
  | `std::function<void(Entity&, Entity&)>` for dispatch | A2/AC-2.4 forbid it; `std::function` is already on `check_constraints.sh`'s forbidden list and may allocate — it would fail this project's own gate |
  | Raw function pointer `void (*)(Entity&, Entity&)` | Explicitly offered to the user and declined (A2). Cannot take a capturing lambda, and forces an indirect call the template seam avoids |
  | A `CollisionDetector` class holding entity references | Zero state to hold — a namespace with extra steps. The codebase's shape for stateless work is a free function (`drawText`, `drawTitleScreen`, `clipRect`) |
  | `void_t`-based `kCallbackIsInvocable` detection trait, mirroring `kGameHasUpdate` | Re-implements `std::is_invocable_v`. `void_t` earns its place in `game_loop.h` because member-call detection has no standard trait; here one exists |
  | `Callback callback` by value (the `std::for_each` convention) | Silently discards a stateful functor's mutations into a copy — the by-value footgun review F10 already recorded against `render(Framebuffer)` |
  | `template <int32_t N> sweepCollisions(Entity (&)[N], Callback&&)` | A zero-length C array is ill-formed, so AC-3.2's N = 0 case would be unwritable rather than proven |
  | Adding a convenience array-reference *overload* alongside the pointer form | It would genuinely remove a count/size-drift bug class for ~3 lines — but NFR-6 says any further public symbol is a review finding. YAGNI wins; if a real game hits the drift, it gets its own story |
  | Naming the pair/sweep functions `checkCollision`/`checkCollisions` | One letter apart, at a call site where the difference is the whole meaning. `sweepCollisions` is the story's own word (US-3) and is unmistakable |
  | `entitiesOverlap(a, b)` | `overlaps(bullet, enemy)` reads better and works unqualified via ADL; the argument types already name themselves |
  | `int32_t` half-open bounds clamped to the 240×160 screen, reusing `clip.h` | A6: this is a pure geometric primitive, deliberately unbounded — an off-screen bullet is a valid entity. `clip.h` is a private `src/` header about *drawing* |
  | Broad-phase partitioning / a spatial grid for the sweep | Spec §6 excludes it outright; AC-3.3's own bound is `N(N-1)/2` |

- **Consequences:** *Easier* — every AC is provable with `make test`/`test-asan`/`test-gcc`/`lint` and nothing
  else; the header has no engine dependency, so it cannot be broken by a change to any other file; `constexpr`
  makes the overflow claim fail loudly at build time on both compilers; the sweep cannot disagree with the pair
  test. *Harder* — `overlaps`' purity means `-O2` will delete a naive benchmark loop entirely (R1); a mutation of
  the widening now fails at *compile* time, which could mask the runtime UBSan half of AC-1.5 unless T4 runs both
  halves deliberately (R2); `Entity`'s deliberate minimality will feel restrictive to the first real game, and the
  lint block plus `sizeof` guard exist precisely to make adding a fifth field a conscious act (R4).

## 2. Affected Components

Scoped by hand — no tool file was passed with this task, so no blast-radius query was run and none is cited here.

- **New (pure, host-gated):** `firmware/steamcore/include/steamcore/collision.h`; tests
  `firmware/steamcore/test/collision_test.cpp`, `collision_overflow_test.cpp`, `collision_dispatch_test.cpp`,
  `collision_sweep_test.cpp`; bench `firmware/steamcore/test/bench_collision.cpp`.
- **Modified:** `Makefile` (one new bench binary + target only — `test/*_test.cpp` is globbed, and there is no
  `src/*.cpp` to add); `tools/check_constraints.sh` (T7); `docs/host-tests.md` (T7/T8).
- **Untouched, by design (A8/C4):** `game_loop.h`, `game_state.{h,cpp}`, `input.h`, `framebuffer.{h,cpp}`,
  `sprite.h`, `text.*`, `font.*`, `clip.h`, `config.h`, `board_config.h`, `dirty_tracker.*`, `port/esp32/*`,
  `firmware/system/**`. T8 makes this a checked claim (`git diff --exit-code`), not an intention.
- **No device-side component at all** — no `firmware/system/main/*` harness, no `CMakeLists.txt` edit, no
  `idf.py build` step, no hardware-gated task. First feature in this project for which that is true; see §4.
- **New dependencies: none.** No package, no service, no new pattern beyond `std::is_invocable_v`, which is a
  C++17 standard-library trait, not a dependency.

## 3. Task Breakdown

| # | Task | Story | Covers (AC / NFR) | Depends on | Status | Definition of Done |
|---|---|---|---|---|---|---|
| T1 | Walking skeleton: `Entity` → `overlaps` → `checkCollision` → callback, end to end | US-1, US-2 | AC-1.1, AC-1.2, AC-2.1, AC-2.2 | – | `done` | `collision.h` exists with `Entity` (x/y/w/h `int32_t`, each `= 0` defaulted so `Entity{}` is an empty region that collides with nothing), the two `static_assert`s of §1 Decision 6, `constexpr overlaps` exactly as §1 Decision 3 spells it, and `checkCollision` exactly as Decision 4 spells it; the header includes only `<cstdint>` and `<type_traits>` and contains no bare `16`, `240`, `160`, `480` or `320` on any non-comment line (the existing include/-wide resolution and tile-size lint rules already reach it — check this before declaring done, not after); four tests: two clearly-separated entities return `false`, two entities sharing a 1×1 region return `true`, an overlapping pair invokes a reference-capturing lambda exactly once, a non-overlapping pair never invokes it; `make test`, `test-asan`, `test-gcc` and `lint` all green with no `Makefile` change — files: firmware/steamcore/include/steamcore/collision.h, firmware/steamcore/test/collision_test.cpp |
| T2 | Boundary and degenerate battery: touching edges, zero/negative size, purity, symmetry | US-1 | AC-1.3, AC-1.4, AC-1.6 | T1 | `done` | A table-driven battery over hand-listed cases, each asserted in **both** argument orders (`overlaps(a,b) == overlaps(b,a)`, an invariant no single-order test can catch): all four touching-edge configurations (a's right == b's left, left == right, bottom == top, top == bottom) return `false`, and the four one-pixel-past-touching counterparts return `true`, so the `<` vs `<=` boundary is pinned from both sides; corner-only contact (two rects meeting at a single point) returns `false`; `w == 0`, `h == 0`, `w < 0`, `h < 0` and both-degenerate pairs each return `false` — including two identical zero-size entities at the same coordinate, and a zero-size entity strictly *inside* a large one; a purity test `memcmp`s byte-copies of both operands taken before and after an `overlaps` call and asserts zero difference; `make test`, `test-asan`, `test-gcc` green — files: firmware/steamcore/test/collision_test.cpp |
| T3 | Overflow safety at `int32_t` extremes, proven twice: at compile time and under UBSan | US-1 | AC-1.5, AC-1.7, NFR-3 | T1 | `done` | A dedicated test file whose cases are chosen so a naive 32-bit `x + w` genuinely overflows — at minimum `{INT32_MAX - 1, 0, 2, 2}` vs `{INT32_MAX - 1, 0, 2, 2}` (overlapping), `{INT32_MAX - 1, 0, 2, 2}` vs `{INT32_MIN, 0, 2, 2}` (disjoint), `{INT32_MIN, INT32_MIN, INT32_MAX, INT32_MAX}` against itself and against a far-away neighbour, and a pair whose *correct* answer differs from the answer naive wraparound would give (so the test can fail, not merely not-crash); every one of these is asserted **twice** — once as a namespace-scope `static_assert` (which cannot compile if the arithmetic is ever UB, since UB is not a constant expression) and once as a runtime `CHECK` on `volatile`-laundered inputs the optimizer cannot constant-fold; negative-coordinate cases well away from the extremes are included as ordinary AC-1.5 coverage; `make test-asan` (`-fsanitize=address,undefined -fno-sanitize-recover=all`) reports zero findings and `make test-gcc` produces identical results (both green, 10 passed / 0 failed, identical to clang) — files: firmware/steamcore/test/collision_overflow_test.cpp |
| T4 | Mutation-verify the overflow, degenerate and strictness claims — each detector demonstrated firing | US-1 | AC-1.3, AC-1.4, AC-1.5, NFR-3 | T2, T3 | `done` | Three mutations planted one at a time, each reverted before the next: **(a)** `ax1 = ax0 + a.w` → `ax1 = a.x + a.w`. Detector 1 (compile-time): `make test` failed with `static_assert expression is not an integral constant expression`, `note: value 2147483648 is outside the range of representable values of type 'int'` at `collision.h:62:27`, naming both extreme-case static_asserts (Case A, Case B). Detector 2 (runtime, with those 5 static_asserts temporarily commented so the suite links): `make test-asan` printed `collision.h:62:27: runtime error: signed integer overflow: 2147483646 + 2 cannot be represented in type 'int'`, `SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior ...`, exited non-zero. Both static_asserts and the line were reverted; verified byte-identical to the original (`grep` re-check). **(b)** deleted `if (a.w <= 0 || a.h <= 0 || b.w <= 0 || b.h <= 0) return false;`. `make test` failed: `FAIL collision_zero_or_negative_size_never_overlaps_either_order (collision_test.cpp:28: overlaps(c.a, c.b) == c.expected)`, 9 passed / 1 failed — guard confirmed load-bearing. Reverted. **(c)** changed `ax0 < bx1` to `ax0 <= bx1`. `make test` failed: `FAIL collision_touching_edges_do_not_overlap_either_order (collision_test.cpp:29: overlaps(c.b, c.a) == c.expected)`, 9 passed / 1 failed, while `collision_entities_sharing_a_1x1_region_overlap` stayed green in the same run — the battery pins A4 from both sides as designed. Reverted. After all three reverts: `make test-all` green, **181 passed, 0 failed**, `make lint OK` — no mutation survived — files: firmware/steamcore/include/steamcore/collision.h, firmware/steamcore/test/collision_overflow_test.cpp |
| T5 | Dispatch contract: argument order, callable kinds, the named compile error, statelessness | US-2 | AC-2.1, AC-2.3, AC-2.4, AC-2.5 | T1 | `done` | Tests instantiate `checkCollision` with **all three** callable kinds AC-2.4 names — a capturing lambda, a free function, and a stateful function object — and assert for each that it fires exactly once on overlap and never otherwise (6 tests); an order test gives the two entities distinguishable field values and asserts the callback received `&a` first and `&b` second (compared by address, not by value), including the case where `b` sits left of and above `a`, so a swap cannot hide behind symmetric geometry (2 tests); the stateful-functor case asserts the **caller's own instance** observed the invocation, proving `Callback&&` did not copy it away (§1 Decision 4); an idempotence test runs the same pair through the same callable twice with no mutation between and asserts identical detection both times (`calls == 2`), plus a `memcmp` that neither entity changed. 19 tests total, `make test`/`test-asan`/`test-gcc` all green (19 passed, 0 failed on every toolchain). The `static_assert`'s named error was demonstrated once, standalone (not in the committed suite — it doesn't compile): `steamcore::checkCollision(a, b, [](int, int) {})` against clang produced `error: static_assert failed due to requirement 'std::is_invocable_v<(lambda) &, steamcore::Entity &, steamcore::Entity &>' "Callback must be callable as callback(steamcore::Entity&, steamcore::Entity&)"` at `collision.h:75`, plus a secondary `no matching function for call to object of type '(lambda)'` at the `callback(a, b)` call site — both named exactly what was wrong. The scratch file was deleted after observing this, nothing reverted in the repo — files: firmware/steamcore/test/collision_dispatch_test.cpp |
| T6 | The N-entity sweep (Should) | US-3 | AC-3.1, AC-3.2, AC-3.3 | T5 | `done` | `sweepCollisions(Entity*, int32_t, Callback&&)` added to `collision.h` exactly as §1 Decision 5 spells it, delegating to `checkCollision` rather than repeating the overlap logic; tests over caller-owned plain `Entity` arrays with automatic storage duration and no allocation anywhere: `count == 0` (passing a valid pointer) and `count == 1` never fire and do not crash; a nullptr and a negative count are likewise no-ops; a hand-built 5-entity arrangement with a known set of overlapping pairs fires the callback for exactly that set, each pair exactly once and never for `i == j`, verified by recording every `(i, j)` the callback saw into a fixed-size array and comparing against the expected set; an all-overlapping arrangement at N = 2, 3, 8 and 32 fires exactly `N(N-1)/2` times at each N (asserted against the computed formula, not against four hardcoded numbers); a fully-disjoint arrangement at the same N values fires zero times; `make test`, `test-asan`, `test-gcc`, `lint` green — files: firmware/steamcore/include/steamcore/collision.h, firmware/steamcore/test/collision_sweep_test.cpp |
| T7 | Lint block for this feature's own concerns | US-1, US-2 | AC-1.7, AC-2.4, NFR-2, NFR-4, NFR-5 | T4, T6 | `done` | `check_constraints.sh` gained one named collision-system block, matching game-loop's precedent of two file lists (a determinism list and a determinism+bench alloc list) rather than one: **(a)** `SCOPED_ALLOC_PATTERN` over `collision.h` + all four test files + `bench_collision.cpp` — extends the allocation ban into `test/` and, because the pattern already matches `std::function`, doubles as the automated half of AC-2.4. **(b)** `CLOCK_RNG_PATTERN` over `collision.h` + the four test files only — **`bench_collision.cpp` deliberately excluded**, the same exemption `bench_game_loop.cpp` already has (measuring elapsed time with `<chrono>` is a bench file's entire legitimate purpose; T9 already relies on this). **(c)** presence check for `static_assert(sizeof(Entity) == 4 * sizeof(int32_t))`. **(d)** presence check for `int64_t` in `collision.h`. No `virtual`-keyword rule, same reasoning as planned (rule (c) already catches a vptr). All four rules demonstrated failing once against a temporarily planted violation and reverted: (b) `#include <chrono>` appended to `collision_test.cpp` → `make lint` failed naming exactly the collision determinism rule and the file:line; reverted, byte-identical (`diff` confirmed). (a) `std::vector<int> g_leak;` appended to `bench_collision.cpp` → failed naming the collision alloc rule; reverted (last line stripped, confirmed with `tail`). (c) the `sizeof(Entity)` static_assert deleted → failed naming exactly that guard; reverted from backup, `diff` confirmed clean. (d) every `int64_t` replaced with `int32_t` → failed naming the widening guard; reverted from backup, `diff` confirmed clean. `docs/host-tests.md`'s `make lint` and `make bench` rows updated to describe the new block and the fifth benchmark binary. `make lint` and `make test-all` green throughout — files: tools/check_constraints.sh, docs/host-tests.md |
| T8 | Contract documentation, usage example, and the public-surface audit | US-1, US-2 | NFR-6, NFR-7 | T5, T7 | `done` | `collision.h`'s header doc comment now states each item explicitly: `Entity`'s four fields as screen-space `int32_t` pixels that **may be negative and may lie entirely off-screen** (A6, unlike `Framebuffer`'s clipped drawing calls); the half-open region `[x, x+w) × [y, y+h)`; touching-is-not-colliding (A4) with a one-line worked example (`a{x:0,w:10}` vs `b{x:10,w:10}` → false); the `w <= 0`/`h <= 0` no-op mirroring `fillRect` (A5); the overflow guarantee and widen-before-summing reason (A6); `checkCollision`'s fixed `(a, b)` order, its own statelessness, and the by-value-copy footgun (review F10 parallel); and the inherited single-threaded/nothing-throws/no-error-code/no-allocation contract — plus the pre-existing bullet-vs-enemy usage example in `game_loop.h`/`input.h` style. Public-surface audit: `grep` of every top-level declaration in `collision.h` found exactly `Entity`, `overlaps`, `checkCollision`, `sweepCollisions` — matching §1 Decision 6 and §2 with no drift, no correction needed. `git diff --exit-code` over `game_loop.h`, `game_state.h`, `game_state.cpp`, `input.h`, `framebuffer.h`, `framebuffer.cpp`, `sprite.h` (all seven files behind the six named components) exited 0 — byte-identical to `HEAD` (A8/C4 verified, not assumed). `make test` re-verified green after the doc-only edit (197 passed, 0 failed) and `make test-all` green end to end — files: firmware/steamcore/include/steamcore/collision.h, docs/host-tests.md |
| T9 | Benchmark: pairwise call and 32-entity sweep against the 60 Hz tick budget | US-1, US-3 | NFR-1, NFR-12 | T6 | `done` | `bench_collision.cpp` plus `$(COLLISION_BENCH_BIN)` wired into `make bench` exactly as `bench_title_screen` is (`.PHONY` entry included). Inputs come from a fixed-seed explicit LCG recurrence seeded through a `volatile` read (never `<random>`/`rand()`/a clock — R1's mitigation plus this file's own future lint scope), and every accumulation goes through a `volatile` sink, so `-O2` cannot constant-fold or delete the loop. Measured at N and 2N and checked for a roughly-linear (1.3x-3.5x) doubling ratio as the "the loop actually ran" proof; the DoD floors of ≥1,000,000 pairs / ≥100,000 sweep passes were raised to 8,000,000 / 400,000 after the floor values showed 1.5x-3.0x ratio noise across repeated runs on this shared host — stable at the larger N (5 consecutive runs: 1.64x-2.14x). Recorded result (one representative `make bench` run): **overlaps(): 5.66 ns/call** over 8,000,000 pairs (doubled to 16,000,000: 44.82 ms → 90.57 ms, ratio 2.02x); **sweepCollisions(): 0.94 µs/sweep** (32 entities) over 400,000 passes (doubled to 800,000: 393.31 ms → 755.42 ms, ratio 1.92x) — well inside the 1.6667 ms (10% of the 16.667 ms 60Hz tick) budget. `make bench` and `make test-all` both green (197 passed, 0 failed; `make lint OK`). No device build, no flash step, no hardware-gated AC exists for this feature — files: Makefile, firmware/steamcore/test/bench_collision.cpp |

## 4. Test Strategy

- **Everything is host-CI-verifiable at `/increment` time** (`make test`, `test-asan`, `test-gcc`, `lint`,
  `bench`), and unusually for this project that is the *whole* strategy, not the host half of one. Spec A7/NFR-12:
  no rendering, no I/O, no hardware. **The absence of a device task is deliberate and recorded here so a reader
  does not mistake it for an oversight:** every prior feature (`display-driver`, `input-driver`, `start-screen`)
  carried at least a Should-scoped device-facing story, and CLAUDE.md's buildable/blocked split exists for exactly
  those. This feature has nothing to split — there is no framebuffer to dump, no pin to read, no panel to look at.
  Adding a device harness anyway would prove nothing the host gate does not already prove.
- **US-1 (Must)** — three layers, each catching what the others cannot. T1/T2 are the correctness battery
  (ordinary, boundary, degenerate), written table-driven and asserted in both argument orders so a
  symmetry-breaking bug cannot hide. T3 is the extreme-value proof, and it is deliberately doubled: a
  `static_assert` (UB is not a constant expression, so a 32-bit narrowing is a *build* failure) plus a
  `volatile`-laundered runtime case under `-fsanitize=address,undefined -fno-sanitize-recover=all` — the two
  detectors fail in different ways and neither can mask the other. T4 is the mutation pass that proves those
  detectors actually fire; without it, "no UBSan findings" is indistinguishable from "UBSan never looked".
- **US-2 (Must)** — T5 exercises all three callable kinds AC-2.4 names rather than one representative, compares
  callback arguments **by address** so an accidental copy is caught alongside a swap, and demonstrates the
  `static_assert`'s named error the way T7 demonstrates each lint rule: planted, observed, quoted, reverted.
  AC-2.5's statelessness is the mechanism's own idempotence, tested as such — what a caller's callback body does
  is explicitly its own business, per the AC's own wording.
- **US-3 (Should)** — T6. Cut first if scope shrinks (spec C5); it depends only on T5, and T7's file set and T9's
  sweep measurement are the only two things that then need adjusting (both flagged in their own rows).
- **NFR-1** is measured and printed, never asserted as a wall-clock `CHECK` inside `make test` — the same
  bench-outside-the-correctness-gate posture `bench_game_loop.cpp` records. Its one real trap is the optimizer
  eliding a pure loop, which T9's DoD makes a checkable step rather than a hope.
- **Deliberately not automated, with reasons:** `/demo-day` in a browser (no browser-observable surface,
  constitution §8 — QA's substitute method here is the host suite alone, and its framebuffer-dump half is simply
  not applicable to a feature that produces no pixels); real GCC coverage (`/usr/bin/g++` is Apple clang, so
  `make test-gcc` stays honestly reported as nominal, unchanged from every prior feature); and the "a real game
  composes this without editing a file delivered here" deferred signal, which the spec itself scopes to a future
  feature, not this one.

## 5. Risks & Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| R1 — `overlaps` is pure and `constexpr`, so `-O2` deletes the benchmark loop and NFR-1 records a fictitious ~0 ns | Medium: a measured number that measured nothing is worse than no number | T9's DoD requires a `volatile` sink, non-`constexpr` inputs, **and** a doubling check that the reported time scales with the iteration count before the task may be marked done |
| R2 — the `constexpr` proof is so strong it hides the runtime one: mutating the widening breaks the *build*, so UBSan is never observed firing and AC-1.5's stated verification method goes unproven | Medium | T4(a) explicitly runs both halves in order — build failure first, then with the `static_assert`s temporarily commented out so the suite links and UBSan can report the finding at that line |
| R3 — T7's missing-file-is-a-failure lint loop breaks `make lint` for the whole repo if US-3 is cut and `collision_sweep_test.cpp` never exists | Low, but it would block every later feature's gate | Called out in T7's DoD: the file set names exactly the files that exist; the sweep test is omitted from it if T6 is cut |
| R4 — `Entity`'s minimality is real discipline, and the first roadmap game will want an ID, a velocity or a sprite reference and will be tempted to add a field here | Medium: A1 was explicitly confirmed by the user; quietly widening it would undo that decision | Three overlapping guards: the `sizeof` `static_assert` (T1), T7's lint presence check on it, and NFR-6's "any further public symbol is a review finding". The intended answer — a game composes `Entity` as a member of its own richer struct — is stated in T8's doc comment |
| R5 — a bare `16`, `240` or `160` in `collision.h` (an obvious size for a doc example or a test constant) trips the existing include/-wide resolution and tile-size lint rules late in the task | Low, but confusing when it lands | Named in T1's DoD as a before-done check; test files live in `test/`, which those rules do not scan, so only the header is exposed |
| R6 — `constexpr` has only ever been decorative in this codebase (`Framebuffer::width()`/`height()` return a constant); here it is load-bearing for a correctness proof, and a future edit could drop the keyword with no test noticing | Low-Medium: dropping it silently removes the compile-time half of AC-1.5 | T3's `static_assert`s **are** the guard — they fail to compile the moment `overlaps` stops being usable in a constant expression, so the keyword cannot be dropped silently |
| R7 — `checkCollision`'s mutable `Entity&` invites a callback that mutates an entity mid-sweep, changing later pair results within the same `sweepCollisions` call | Low now, real once a game uses it | Out of scope to prevent (the spec puts collision *rules* entirely in the caller's hands), but T8's doc comment states the sweep reads each entity at the moment its pair is tested, so a mutating callback affects subsequent pairs — documented rather than discovered |

## Deviations

- **T9 executed before T7, in the task table order swap.** T7's lint block (per its own DoD) names `bench_collision.cpp` in its file set, using the same missing-file-is-a-failure loop posture every other lint block in `check_constraints.sh` already uses — but T9, which creates that file, was ordered *after* T7 in §3. Writing T7 first would have made `make lint` fail from the moment T7 landed until T9 caught up (the exact situation input-driver's T7 hit with `gpio_input_source.{h,cpp}`, recorded there as a deviation). Here the dependency graph itself permits avoiding it: T9 depends only on T6 (done), not on T7, and T8 depends on T5+T7 (not T9), so running T9 before T7 satisfies every declared dependency in §3 while never leaving `make lint` red at any commit boundary. No architecture, scope or story changed — only the execution order of two independency-compatible tasks.

---

## ✅ PLAN GATE

*All boxes checked → `/increment` may start. Any box open → back to `/sprint-plan`.*

- [x] Spec status is `approved` (never plan against a draft) — `approved` 2026-09-05, 0 open questions
- [x] Architecture decision includes rejected alternatives (12 recorded, §1)
- [x] Architecture respects the constitution's technical constraints (§3 no dynamic allocation — no container, no
  buffer, no allocation anywhere in the header or its tests, newly enforced by T7(a); §3 C++17 — `is_invocable_v`
  and `constexpr` are both C++17, supported by Apple clang 14 and ESP-IDF GCC alike; §3 resolution as a
  compile-time constant — this primitive is deliberately unbounded (A6) and spells no resolution literal at all,
  verified against the existing include/-wide rules; §3/§4 determinism — no clock, no RNG, newly enforced by
  T7(b); §4 no ESP-IDF header in `include/`/`src/` — the header includes `<cstdint>` and `<type_traits>` and
  nothing else; §4 host unit tests as the real gate — 100% of ACs land there; §5 `steamcore` namespace,
  `snake_case` filename, English, `.h` with no `.cpp` justified in §1 Decision 1; §6 zero vtables preserved) —
  no conflict found
- [x] Every task maps to a user story — no orphan tasks, no story without tasks
- [x] Every Must AC and every applicable NFR is covered by at least one task (AC-1.1–1.7, AC-2.1–2.5, AC-3.1–3.3;
  NFR-1–NFR-7 and NFR-12; NFR-8/9/10/11 are N/A per the spec)
- [x] Every task has a checkable definition of done
- [x] Task order respects dependencies (walking skeleton first: T1 puts `Entity`, the real overflow-safe
  `overlaps` and the real template dispatch through the whole path to an invoked callback before any battery,
  mutation pass, sweep or lint rule exists — the template seam, this feature's only integration risk, dies in
  the first task; no hardware-gated row exists at all, deliberately, per §4)
- [x] Test strategy covers every Must story, and states per story how it is verified — all host-CI, no board
- [x] Line budget respected: Ist 234 / Soll ~300 (excluding HTML comments) — 66 under
- [x] Status set to `approved` by the user — 2026-09-05
