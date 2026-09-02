# Plan: game-loop

| | |
|---|---|
| **Phase** | Plan |
| **Owner** | Engineering Manager (`/sprint-plan`) |
| **Input** | `.spark/game-loop/spec.md` (`approved`) |
| **Status** | `approved` |
| **Date** | 2026-09-02 |

**Handoff**
- **Status:** `approved` — user approved as proposed, including both flagged deviations (`render(Framebuffer&)`, the stepper reading of AC-1.1).
- **Summary:** One header-only class template `GameLoop<Game>` in `include/steamcore/game_loop.h`, constructed once with `Game&` + `Framebuffer&` and stepped by a single `tick(const GameInput&)`; the framebuffer identity of AC-2.2 is guaranteed structurally by that binding, and the per-tick determinism comparison of AC-4.1 falls out of stepping two independent instances in lockstep. Plus `GameInput{bool start, bool fire}`. No `.cpp`, no new engine constant, no new public symbol beyond those three.
- **Open:** `0 tasks not done` — all 9 tasks `done`, no deviations from the approved architecture. Each task's DoD was verified, not asserted: T1's wrong-signature compile error quoted directly; T2/T3/T4/T6 each mutation-tested against the exact defect class their AC exists to catch (call-order swap, field swap, implicit-clear, hidden-shared-state divergence, input-ignoring consumer) with the injected bug observed failing before being reverted; T7's two new lint rules demonstrated failing once each; T8's budget demonstrated failing once when lowered. `make clean && make test-all` green: 89 C++ tests across 4 configurations (clang, clang+ASan/UBSan, g++, plus 89 again for png-external), 3 bench binaries all `BENCH OK`, 15 Python + 2 round-trip tests, `sips` cross-check OK, `make lint` OK.
- **Binding ruling:** §3 Task Breakdown for current task status; a plan revision after review/QA findings updates §1/§3 in place, never a new section
- **On conflict:** the numbered body below wins for everything except `Status`; log the mismatch as a finding at the next `/peer-review` and proceed — don't stop on it.

## 1. Architecture Decision

- **Context:** The spec fixes *what* (fixed `update`→`render` order per tick, one `Framebuffer`, a two-bool
  `GameInput`, provable replay determinism) and deliberately leaves *how* open. The mechanism itself is
  genuinely tiny — two calls in a fixed order. The deliverable is therefore the **contract shape** plus
  the **proof**, not lines of logic, and the two places where a wrong shape would cost the most are
  (a) how a consumer plugs in without dynamic allocation or virtual dispatch (constitution §3/§6; this
  codebase has no virtual function anywhere yet) and (b) whether AC-4.1's "byte-identical after *every*
  tick" is expressible without storing 50 framebuffer snapshots. `Framebuffer` exposes no byte-compare
  and is not owned by this feature's task list, so the comparison oracle must live in `test/`.

- **Decision:**
  1. **`include/steamcore/game_loop.h`, header-only, no `.cpp`** — the same deliberate deviation from the
     `.h`/`.cpp` convention `sprite.h` already carries (§5), for the same reason: a POD struct and a
     class template have no out-of-line code. `ENGINE_SRCS`/`TEST_SRCS` are wildcards, so `test`,
     `test-asan`, `test-gcc` and `lint` pick this up with no Makefile edit.
  2. **The consumer plugs in as a template parameter, not a base class.** `template <typename Game>
     class GameLoop` — compile-time binding, zero overhead, no vtable, no allocation, and it forecloses
     nothing: a future abstract game interface (constitution §3's registry story) still satisfies this
     template when instantiated with `GameInterface&`. A `static_assert` in `steamcore::detail`, built on
     `std::is_invocable_v`, turns a wrong `update`/`render` signature into a named error instead of a
     template-expansion wall (NFR-7 contract clarity; `detail` follows the same not-public-surface convention
     `clip.h` uses for `Framebuffer` -- though `clip.h` lives in `src/`, unreachable from the single `-I include`
     path, while `game_loop.h`'s `detail` symbols live in `include/` and are technically reachable; the
     convention matches, the reachability does not -- review F7).
  3. **The loop is a stepper, not a batch runner:** it is constructed with `Game& game, Framebuffer& fb`
     and exposes exactly one method, `void tick(const GameInput& input)`, which calls
     `game.update(input)` then `game.render(fb)`. "N ticks" is N calls by the caller (spec A8: the tick is
     a *caller-driven discrete step*). This is what makes AC-4.1 cheap: two loops over two fresh
     consumers and two framebuffers are stepped in lockstep and compared after each `tick`, with no
     snapshot storage and no hashing. **Recorded interpretation:** AC-1.1's "requested tick count N" is
     the caller's loop count, asserted via a `driveTicks(loop, inputs, n)` helper in test code; AC-1.2's
     N = 0 is "constructed, never ticked" (T2 asserts both counters are 0).
  4. **Binding `Framebuffer&` at construction is what proves AC-2.2.** Because the reference is captured
     once and `tick` takes no buffer, "the same instance every tick" is structural, not a caller
     discipline. `render` therefore takes `Framebuffer&`: `render(Framebuffer&)`, not README's sketched
     `render()`. **Deliberate, recorded deviation** from the README/constitution §2 contract sketch — with
     `render()` the game would hold its own buffer reference and AC-2.2 would be untestable at the
     mechanism level. Constitution §3 assigns the mechanism to `/sprint-plan`; README stays untouched
     (German documentation, §5). The loop owns no framebuffer and creates no global: rendering-core's
     deferred `Framebuffer& engineFramebuffer()` stays deferred to the bootstrap story (NFR-6).
  5. **`GameInput` is a plain aggregate with exactly `bool start = false; bool fire = false;`** in the
     same header — default member initializers in `sprite.h`'s style, so `GameInput{}` is a defined
     all-released value and an uninitialized read is designed out. That is caller-side default
     construction, never mechanism substitution (AC-3.2: `tick` copies nothing and decides nothing).
     `static_assert(sizeof(GameInput) == 2 * sizeof(bool))` pins "exactly two fields" (AC-3.1) portably —
     an added third field breaks the build, without asserting a `sizeof(bool)` the xtensa GCC must honour.
  6. **The determinism oracle is test-only.** `test/fb_compare.h` (header-only, quoted include, no
     Makefile change) compares two framebuffers through the public `pixel(x, y)` over all 38,400
     positions and reports the first mismatching coordinate. `test/replay_fixture.h` holds the
     deterministic consumer, the input sequence generator and `kReplayTicks`, shared by the AC-4.1 test
     and the NFR-1 bench so the bench derives its tick count instead of hand-typing it (text-rendering
     review F7). No new public symbol; `test/` is not API surface.
  7. **No new engine constant, therefore no literal-scoping lint rule.** This feature adds no screen-size
     logic (A12) and — per A8/NFR-6 — deliberately no `kTickHz`/60 Hz constant, because no scheduler is
     built. The lint work is the opposite kind of rule: a **negative** grep for wall-clock and RNG use
     over a named game-loop file set (AC-1.3, AC-4.4), plus NFR-2's allocation grep extended to that same
     set, since `check_constraints.sh` currently scans `include/` and `src/` only.

- **Alternatives considered:**

  | Alternative | Why rejected |
  |---|---|
  | Abstract base class with virtual `update`/`render` | Runtime polymorphism nobody needs yet: one consumer at a time (§6), vtable per instance, and the first virtual dispatch in the codebase. Buys only what the registry story might want, and the template does not foreclose that story — a `GameLoop<GameInterface&>`-style instantiation still works |
  | Struct of two function pointers, or two free function pointers | Consumer state then travels as an untyped `void*` context, which no AC needs and every future game pays for; loses the compile-time signature check of decision 2 |
  | Free function `tick(Game&, Framebuffer&, const GameInput&)`, no object at all | Smallest possible surface, but the framebuffer is re-supplied every tick, so AC-2.2's "same instance every time" degrades from a structural guarantee to caller discipline — exactly what US-2 exists to remove |
  | Batch `run(game, fb, const GameInput* seq, int32_t n)` | Reads AC-1.1 most literally, but AC-4.1 must compare after *every individual* tick: a batch call forces either 50 stored framebuffer snapshots (1.9 MB of static storage) or a hash (probabilistic, not "byte-identical"), or degenerates into 1-tick calls — i.e. a stepper with worse ergonomics |
  | Both `run(...)` and `tick(...)` | Two tick-driving entry points; NFR-6 makes the second one a review finding |
  | A tick counter / `ticksElapsed()` on the loop | Speculative state and a public accessor no AC asks for (NFR-6, YAGNI). The caller already knows how many times it called `tick` |
  | New public `Framebuffer::operator==` / `data()` for the comparison | A new public symbol on a type this feature does not own, added for a test's convenience — NFR-6 finding. `pixel(x, y)` already reads every byte |
  | Compare via the existing `serializeDump` byte payload | Makes the constitution's determinism proof depend on an unrelated serializer; a bug there could mask or fabricate equality, and it needs two 38 KB static buffers for nothing |
  | Put `GameInput` in its own `game_input.h` | NFR-7 wants one place stating the whole per-tick contract; splitting it scatters the doc comment for zero benefit today |

- **Consequences:** *Easier* — the whole feature is one header plus tests; the future Input Abstraction
  story extends `GameInput` in one file, and Collision/Audio/Game-State stories get their call slots by
  composing a consumer, not by editing this header. Zero overhead on device, nothing to allocate.
  *Harder* — the contract is duck-typed, so a mis-typed `update` is a template error (mitigated by the
  `static_assert`); every consumer instantiates its own `GameLoop`, so `tick` is not a runtime-swappable
  call; and `render(Framebuffer&)` now differs from the README sketch, which the game-state story inherits.
  *Deliberately not decided here* — the game registry mechanism (unchanged from rendering-core: this
  feature adds no namespace-scope object).

## 2. Affected Components

Scoped by hand — no tool file was passed with this task, so no blast-radius query was run and none is
cited here.

- **New:** `firmware/steamcore/include/steamcore/game_loop.h`; tests
  `firmware/steamcore/test/game_loop_test.cpp`, `game_loop_determinism_test.cpp`, `fb_compare_test.cpp`,
  test-only headers `fb_compare.h`, `replay_fixture.h`, and `bench_game_loop.cpp`.
- **Modified:** `Makefile` (a third bench binary, one recipe line + one `.PHONY` entry — same shape as
  `TEXT_BENCH_BIN`), `tools/check_constraints.sh` (the AC-1.3/AC-4.4 time-and-RNG rule and NFR-2's
  allocation grep extended to the game-loop file set), `docs/host-tests.md`.
- **Untouched, by design:** `framebuffer.{h,cpp}`, `sprite.h`, `color.h`, `config.h`, `font.*`, `text.*`,
  `dirty_tracker.*`, `dump_format.*`, `tools/fb_view.py`, `README.md`, `games/` (§6, A10).
- **New dependencies: none.** Standard library only (`<type_traits>` for `std::void_t`, `<utility>` for
  `std::declval` -- both only in the signature-detection `static_assert`s; review F2/F8 replaced the
  original `decltype(&Game::update)` approach and dropped the unused `<cstdint>` this line used to name);
  NFR-11 stays N/A.
- **Public API surface added (NFR-6), with its named consumer:** `GameLoop` (the one tick-driving type,
  US-1), its `tick` (the one entry point), `GameInput` with exactly `start`/`fire` (US-3). The `update`
  and `render` slots are the consumer's own members, not new engine symbols. Nothing else.

## 3. Task Breakdown

| # | Task | Story | Covers (AC / NFR) | Depends on | Status | Definition of Done |
|---|---|---|---|---|---|---|
| T1 | Walking skeleton: `game_loop.h` with `GameInput`, `GameLoop<Game>`, one counting test consumer, one tick end to end | US-1, US-2, US-3 | AC-1.1, NFR-2, NFR-4, NFR-6 | – | `done` | `game_loop.h` declares exactly `GameInput{start, fire}` and `GameLoop<Game>` (constructor taking `Game&` and `Framebuffer&`, one public method `tick(const GameInput&)`) plus a `steamcore::detail` `static_assert` on the `update`/`render` signatures, demonstrated once with a deliberately wrong consumer signature whose named error is quoted in the task note; `game_loop_test.cpp` defines a counting consumer and proves one `tick` calls `update` once then `render` once; `make test`, `make test-gcc`, `make test-asan` and `make lint` are green and no new `-I` path, Makefile target or engine `.cpp` was added — files: firmware/steamcore/include/steamcore/game_loop.h, firmware/steamcore/test/game_loop_test.cpp |
| T2 | Tick count and strict call interleaving for N = 0, 1, 100 | US-1 | AC-1.1, AC-1.2, AC-1.4, NFR-3 | T1 | `done` | The consumer appends one entry per call to a fixed-size in-object event log; separate named tests for N = 0, 1 and 100 assert `update` and `render` counts equal N exactly and that the log is strictly alternating `update(i), render(i), update(i+1)` with no adjacent same-kind pair; the N = 0 case constructs a `GameLoop`, never ticks, and asserts both counters are 0 with no crash; `make test-asan` reports zero findings for the file — files: firmware/steamcore/test/game_loop_test.cpp |
| T3 | Per-tick input delivery and the exact `GameInput` shape | US-3, US-2 | AC-2.1, AC-3.1, AC-3.2, AC-3.3 | T2 | `done` | A test drives a sequence whose every index carries a distinct `start`/`fire` pair and asserts the consumer recorded, per tick *i*, exactly the values supplied at index *i* (never *i−1* or *i+1*); a second test drives all four combinations (false/false, start only, fire only, both) in separate ticks and asserts each is observed unmodified; `static_assert(sizeof(GameInput) == 2 * sizeof(bool))` lives in the header and a test asserts `GameInput{}` reads back both fields false — files: firmware/steamcore/test/game_loop_test.cpp, firmware/steamcore/include/steamcore/game_loop.h |
| T4 | Framebuffer contract: one instance, no implicit clear, update-before-render state visibility | US-2 | AC-2.2, AC-2.3, AC-2.4 | T3 | `done` | Three named tests prove: the consumer records `&fb` on every one of 100 ticks and every recorded address equals the address of the single caller-owned `Framebuffer`; a consumer whose `render` never calls `clear()` draws one distinct pixel on tick 1 and another on tick 2, and after tick 2 **both** pixels read back with their colours (nothing was cleared between ticks); a consumer whose `update` increments its own counter and whose `render` writes that counter's value as a pixel colour yields, after each tick, exactly the value `update` left for that same tick — files: firmware/steamcore/test/game_loop_test.cpp |
| T5 | Test-only framebuffer comparison oracle, proven able to fail | US-4 | AC-4.1 | T1 | `done` | `fb_compare.h` provides one header-only `framebuffersEqual(a, b, &x, &y)` comparing all `Framebuffer::width() * height()` positions through the public `pixel()` and reporting the first mismatch; `fb_compare_test.cpp` proves it returns true for two identically-drawn buffers and false with the exact coordinate for a single-pixel difference placed at (0,0), at the last pixel and at one interior position — the oracle is only accepted once it has been observed failing on a real difference; no Makefile change is needed for either file — files: firmware/steamcore/test/fb_compare.h, firmware/steamcore/test/fb_compare_test.cpp |
| T6 | 50-tick replay determinism, compared after every tick, plus a sensitivity check | US-4 | AC-4.1, AC-4.2, AC-4.3, NFR-3, NFR-5 | T4, T5 | `done` | `replay_fixture.h` holds `kReplayTicks` (≥ 50), a deterministic consumer whose `update`/`render` are a pure function of its own prior state and that tick's input (no time, no RNG — AC-4.2 is satisfied by containing no randomness at all, which the test asserts by construction and T7 by grep), and a sequence generator cycling all four `start`/`fire` combinations; `game_loop_determinism_test.cpp` steps two freshly constructed consumers with two separate framebuffers in lockstep and asserts `framebuffersEqual` after **every individual tick**, naming the tick index and coordinate on failure; a second test proves the fixture is *sensitive* — replaying a sequence that differs at one tick produces a difference the oracle detects, so a consumer that ignored its input could not pass; `make test-asan` reports zero findings; `make test` and `make test-gcc` both green — files: firmware/steamcore/test/replay_fixture.h, firmware/steamcore/test/game_loop_determinism_test.cpp |
| T7 | Grep gates: no wall-clock, no unseeded RNG, no allocation in the game-loop file set | US-1, US-4 | AC-1.3, AC-4.4, NFR-2, NFR-5 | T6 | `done` | `check_constraints.sh` gains a rule over an explicitly named game-loop file set (`game_loop.h` and every game-loop test/fixture file, deliberately excluding the bench) rejecting `<chrono>`, `<ctime>`, `<time.h>`, `<sys/time.h>`, `std::chrono`, `steady_clock`, `system_clock`, `high_resolution_clock`, `clock(`, `clock_gettime(`, `time(`, `gettimeofday`, `rand(`, `srand(`, `random_device` and `<random>` (review F11: the first token list missed `clock_gettime`/`<time.h>`/`<sys/time.h>`, demonstrated bypassable, then closed), with `//` comment lines stripped as the existing rules do; the file set is spelled out in the script with its reason, because the two existing `bench_*.cpp` files legitimately use `<chrono>` and must stay outside this rule; NFR-2's allocation grep is extended to the same set (which `include/`+`src/` scanning does not reach); both rules are demonstrated failing once against a temporarily inserted violation and the observation recorded; `make lint` green on the real tree — files: tools/check_constraints.sh, docs/host-tests.md |
| T8 | NFR-1 benchmark for the 50-tick replay | US-4 | NFR-1 | T6 | `done` | `bench_game_loop.cpp` runs both replay runs (2 × `kReplayTicks` ticks, the count taken from `replay_fixture.h`, never hand-typed) after one warm-up pass, prints the measured milliseconds against the 5 ms budget and exits non-zero above it, in `bench_text.cpp`'s style; it measures the `tick` calls only and prints the per-tick comparison cost as a **separate, non-gating** line, since NFR-1's budget names the replay and the comparison is verification scaffolding; the existing `bench` target builds and runs it as a third binary on its own recipe line, listed in the binary `.PHONY` block; a deliberately lowered budget is shown once to make `make bench` fail — files: firmware/steamcore/test/bench_game_loop.cpp, Makefile, docs/host-tests.md |
| T9 | Contract documentation and the NFR-6 surface audit | US-1, US-2, US-3, US-4 | NFR-6, NFR-7 | T7, T8 | `done` | `game_loop.h`'s doc comment states, each explicitly: the fixed `update`-then-`render` order once per tick; that the framebuffer is bound once at construction and is never implicitly cleared, so clearing is `render`'s own explicit call; that a tick is a caller-driven discrete step with no wall-clock pacing, measured interval or real-time claim; the inherited single-threaded, nothing-throws, no-error-code contract; `GameInput`'s exactly two fields as raw pass-through with no decoding, debouncing or edge detection; and one compilable usage example showing a minimal consumer with both members and a `GameLoop` stepped in a caller loop. The header is audited symbol by symbol against NFR-6: `GameInput{start, fire}`, `GameLoop<Game>` and its one entry point `tick` are the public surface; `detail::kGameHasUpdate`/`kGameHasRender` are conventionally private (same non-public convention as `clip.h`, not the same reachability -- review F7) and are named explicitly here rather than folded into "everything else"; `docs/host-tests.md` describes what `bench` and `lint` now cover; `make test-all` green from a clean checkout — files: firmware/steamcore/include/steamcore/game_loop.h, docs/host-tests.md |

## 4. Test Strategy

- **Everything here is host unit tests** (`STEAMCORE_TEST`, `make test`), as with both prior increments:
  all 15 ACs are provable with `clang++` alone (A2), no board, no ESP-IDF, no cmake. One named test per
  AC (or per AC case group) so `make test FILTER=ac_4_1`-style filtering lets QA record criteria
  individually (constitution §8).
- **US-1** — `game_loop_test.cpp`, T2: counts *and* an ordered event log. Counting alone cannot catch a
  loop that calls both callbacks in the wrong order or batches all updates before all renders; the
  strictly-alternating log assertion can, and that is the actual content of AC-1.1.
- **US-2** — T4, three separate tests, one per AC. Instance identity is asserted on the recorded
  *address*, not on drawn content, because two distinct buffers can hold identical pixels. The
  no-implicit-clear case reads both pixels back after the second tick, which fails if the mechanism ever
  gains a hidden `clear()`.
- **US-3** — T3. The distinct-value-per-index test is the one that fails if `tick` ever caches, delays or
  defaults an input; the four-combination test is AC-3.3's explicit matrix. AC-3.1 is a `static_assert`,
  so a third field is a build break rather than a test that someone might not run.
- **US-4** — T5 + T6, and this is where the false-green risk lives, so the proof is built in two
  independent layers: the comparator is proven to *detect* a one-pixel difference (T5) before it is
  trusted, and the fixture is proven *sensitive* to a changed input sequence (T6). Without both, a
  consumer that ignored its input and a comparator that always returned true would produce a green
  determinism suite that verifies nothing.
- **Sanitizers** cover what assertions cannot: `make test-asan` (`-fsanitize=address,undefined
  -fno-sanitize-recover=all`) over the 100-tick and 50-tick replay cases is where AC-1.4, AC-4.3 and
  NFR-3 are actually proven.
- **Grep gates (T7)** carry AC-1.3, AC-4.4 and NFR-2/NFR-5's static half. They are automated in
  `make lint` rather than left to reviewer diligence, and both are demonstrated failing once — an
  unexercised gate is an untested one.
- **NFR-1** is a separate `-O2` binary outside `make test`; a timing assertion inside the correctness
  gate is a flake generator. Its measured value and host are recorded in `docs/host-tests.md`.
- **Deliberately not covered, and why:** device behaviour and any real 60 Hz pacing — no ESP-IDF, no
  board, and A8 puts a scheduler out of scope; nothing here may be reported as hardware-verified.
  `/demo-day` in a browser — no browser-observable surface (constitution §8, `no`). No `make view` PNG is
  produced: this feature draws nothing of its own, the fixture's pixels are verification data, and a
  fourth dump would be surface without a consumer (NFR-10 N/A). Real GCC — `/usr/bin/g++` is clang on
  this host, so NFR-4's two-compiler claim stays honestly reported as nominal.

## 5. Risks & Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| A determinism suite that is green without verifying anything — a fixture that ignores its input, or a comparator that never reports a difference | Highest risk here: constitution §4's own guarantee would be certified by a test that cannot fail. This exact false-green class dominated both prior review rounds | Two independent negative proofs are task DoD, not nice-to-have: T5 requires the comparator to be observed failing on a one-pixel difference at three positions; T6 requires the fixture to be observed producing a *detected* difference for a changed input sequence |
| The stepper reading of AC-1.1 ("requested tick count N" as the caller's loop count) is not the reviewer's reading | Medium — a review finding on shape, after the code exists; adding a batch `run()` later would breach NFR-6's single entry point | Interpretation recorded explicitly in §1 Decision 3 with AC-4.1 as the reason, and made visible in the tests via a `driveTicks(loop, inputs, n)` helper so N is literal at each call site. Flagged for the approval conversation rather than buried |
| `render(Framebuffer&)` deviates from README/constitution §2's sketched `render()`, and the game-state and registry stories inherit it | Medium — a later contract change touches every future game | Recorded as a decision with its reason (AC-2.2 is untestable at mechanism level otherwise); constitution §3 assigns the mechanism to `/sprint-plan`; today there is no game to break, so the cost of reversing it is one signature in one header |
| Duck-typed template contract accepts or rejects a consumer for reasons the author cannot read | Low-Medium — the first real game author pays it | `detail` `static_assert` on `is_invocable` (T1), demonstrated once against a wrong signature; NFR-7's doc comment carries a compilable minimal consumer |
| NFR-1's 5 ms budget is ambiguous about whether the 38,400-pixel-per-tick comparison counts | Low-Medium — a measurement that flatters or fails the feature for the wrong reason | T8 measures exactly what NFR-1 names (the 2 × 50 `tick` calls) and prints the comparison cost as a separate non-gating line, so the reviewer sees both numbers instead of one blended one |
| The new lint rule is scoped so narrowly it never fires, or so broadly it flags the two legitimate `<chrono>` bench files | Low-Medium — a tuned-to-pass gate is worse than no gate | The file set is named explicitly in the script with its reason (T7), the bench exclusion is stated, and the rule must be observed failing on an inserted violation |
| Inherited A8: nothing here says anything about real 60 Hz behaviour on device | Low, accepted by the spec | No scheduler, no `kTickHz` constant and no timing claim is created; the honest-status rule (constitution §4) applies to every report about this feature |
| Inherited: `/usr/bin/g++` is clang, so NFR-4/NFR-5's two-compiler determinism claim is nominal | Low | `CXX` stays overridable, C++17 with no extensions keeps exposure small, and `docs/host-tests.md` already records this as unverified rather than passed |

---

## ✅ PLAN GATE

*All boxes checked → `/increment` may start. Any box open → back to `/sprint-plan`.*

- [x] Spec status is `approved` (never plan against a draft)
- [x] Architecture decision includes rejected alternatives (9 recorded, §1)
- [x] Architecture respects the constitution's technical constraints (§3 no dynamic allocation — header-only template, references and stack objects only; no wall-clock read, enforced by `make lint`; C++17; `steamcore` namespace, `snake_case` file name; no ESP-IDF header; one `-I` path unchanged; no new resolution/tile literal; English throughout) — no conflict found
- [x] Every task maps to a user story — no orphan tasks, no story without tasks
- [x] Every Must AC and every applicable NFR is covered by at least one task (AC-1.1…1.4, AC-2.1…2.4, AC-3.1…3.3, AC-4.1…4.4, NFR-1…NFR-7; NFR-8/9/10/11 are N/A per the spec)
- [x] Every task has a checkable definition of done
- [x] Task order respects dependencies (walking skeleton T1 first: header, consumer and one end-to-end tick before any AC battery)
- [x] Test strategy covers every Must story
- [x] Line budget respected: Ist 194 / Soll ~300 (excluding HTML comments)
- [x] Status set to `approved` by the user
