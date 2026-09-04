# Spec: display-driver

| | |
|---|---|
| **Phase** | Specify |
| **Owner** | Product Owner (`/story-time`), Designer (`/look-and-feel`) |
| **Status** | `approved` |
| **Date** | 2026-09-03 |
| **Ticket** | `none` |

**Handoff**
- **Status:** `approved` — user approved as proposed on 2026-09-03, including the GameLoop-integration scope override (C8), both Designer `/look-and-feel` findings as resolved (F1/F2, C12), and the ~250-line Soll overage (276 actual) as an accepted, explicitly-reasoned exception rather than a further cut into load-bearing content.
- **Summary:** rendering-core proved the `Framebuffer`/`DirtyTracker`'s *logic*; game-loop separately proved a deterministic tick mechanism, entirely off-device; today's bring-up spike separately proved the real ILI9488 panel accepts 18bpp writes. Nothing joins all three — no pixel any `GameLoop`-driven consumer has ever drawn has reached the physical panel, tick after tick. Deliver a `steamcore`-namespaced driver that converts and scales `Framebuffer`'s dirty tiles into the panel's wire format and pushes only those tiles over SPI DMA, proven twice: host-side (pure conversion/mapping math) and on the real board — via a minimal, throwaway `GameLoop`-driven consumer, ticked live, not a one-shot static push (serial log + human visual check).
- **Open:** `none` — the five judgment calls flagged in the prior draft (A2, A3, A5, A6, A11) are now confirmed or, for A3 (GameLoop integration), explicitly overridden by the user; two follow-on judgment calls that override required (A12 push call-site/API-surface boundary, A11's revised split-fixture approach) are made and logged below (§7 C7–C11), same "flagged, not blocking" posture as before; the Designer's two Major `/look-and-feel` findings (F1, F2, §8) are now resolved via this round's A11/AC-3.1/AC-3.2 revisions (C12).
- **Binding ruling:** §4 User Stories for the current stories; §7 Clarifications for what was decided and why.
- **On conflict:** the numbered body below wins for everything except `Status`; log the mismatch as a finding at the next `/peer-review` and proceed.

## 1. Problem & Goal

- **Problem:** rendering-core's `Framebuffer`/`DirtyTracker` are proven correct — byte-level unit tests, plus framebuffer-viewer's PNG dumps. game-loop separately proved a deterministic tick mechanism, entirely off-device. Today's bring-up spike separately proved the real KMRTM35018-SPI panel accepts 18bpp pixel data. But nothing connects all three: every rendering-touching story to date (sprites, animations, the tick mechanism itself) has been verified only against a PNG on a Mac, and `GameLoop::tick()` has never once driven a real pixel onto the physical panel it will ship on — "the console" is currently an engine plus a disconnected hardware spike, not a working cabinet.
- **Goal:** a `steamcore`-namespaced display driver that takes exactly what `DirtyTracker` reports changed on a `Framebuffer`, converts each pixel through the engine's fixed 4-colour palette into the panel's 18bpp wire format, maps it ×2 into the correct region of the real 480×320 landscape panel, and pushes only those tiles over SPI DMA — never a full frame — proven on the real board by a minimal, throwaway `GameLoop<Consumer>` instance ticked live, so it is `GameLoop::tick()`'s output, not just a bare test-harness push, that reaches the physical panel.
- **Success signal:** (1) a host-run unit suite proves the palette→18bpp conversion and the tile→panel-coordinate mapping correct for all 4 colours and all 150 tile positions, zero ESP-IDF headers touched; (2) on the real board, a minimal `GameLoop<Consumer>` instance is constructed and ticked repeatedly (at least 20 ticks): its first tick renders a known multi-colour pattern (all 150 tiles dirty, proving pixel-region correctness), and every following tick moves a single deterministic marker by one tile — a human, reading the serial log's per-tick tile count and watching the physical panel live, confirms the first frame is correct and that the marker visibly, continuously updates tick after tick, never a static two-frame comparison (AC-3.1/AC-3.2).
- **Why now:** the direct, explicitly-named next increment after today's spike — constitution §3/§4 both call the spike "not the production display driver... remains future planned work." It is the last missing link before any game can be seen running on real hardware, and constitution §8 explicitly anticipates this exact moment: the framebuffer-dump/serial verification method "becomes enforceable once the ESP-IDF toolchain and the board exist" — as of yesterday, they do. game-loop now also gives this feature a real, proven tick mechanism to prove itself against, not just a bare push.

## 2. Target Users

- **Engine developer (primary, today):** needs to see the engine's actual `GameLoop`-driven output on the physical cabinet screen, not just a PNG or a one-shot static push — every future rendering-touching story depends on this pipeline existing and being trustworthy, tick after tick.
- **Future game-module author (indirect beneficiary):** this cycle's synthetic consumer proves the exact contract they'll rely on — `GameLoop<Game>::tick()` driving their `render(Framebuffer&)` output onto the real screen with no extra work on their part. Composing a real game with `GameSession`, real input and a menu is still a later, separate story (A3, §6).
- *Not a user of this feature:* the console player. No game, menu or registry exists yet to display — this delivers the pipe (plus one synthetic, non-gameplay proof consumer), not game content, matching every prior engine story's posture.

## 3. Assumptions & Open Questions

| # | Assumption / Question | Resolution |
|---|---|---|
| A1 | The idea arrived pre-scoped ("Framebuffer/dirty-tile output... SPI DMA... ILI9488"). Constitution §3 already fixes dirty-tile-over-SPI-DMA as a non-negotiable — treated as an inherited constraint, not a spec-invented solution (rendering-core A1 / framebuffer-viewer A1 posture). | Accepted |
| A2 | **Host-testability boundary.** Constitution §4: "the moment a logic module includes an ESP-IDF header it drops out of the only gate this project has." A display driver structurally needs `spi_master.h`. Resolution: the pixel-conversion and tile-mapping *math* (US-1/US-2) stay pure, zero-ESP-IDF-header, host-tested; only the actual SPI transmission call requires ESP-IDF and is therefore, structurally, the **first** `steamcore` production code to drop out of the host gate. The exact file/module boundary that keeps these separated is a `/sprint-plan` decision — but that the boundary exists, and where ESP-IDF headers start, is fixed here. | **Resolved (PO judgment)** |
| A3 | **GameLoop integration is in scope this cycle — overridden by the user.** The prior draft excluded wiring into `GameLoop<Game>::tick()`; the user explicitly rejected that cut and required this feature to prove itself against a real, ticking `GameLoop<Consumer>` instance, not a bare test-harness push (C8). Still excluded: `GameSession`/READY-PLAYING-GAME_OVER session-phase handling, a restart button, real button-driven input, a system menu, a game registry, and any actual playable game (win/lose rules, scoring) — pulling those in would be a much larger scope explosion than proving `GameLoop`-driven rendering reaches the real panel. | **Overridden by the user** (C8) |
| A4 | Panel landscape resolution (480×320) exactly matches 240×160 ×2 with zero letterboxing — already confirmed fact (constitution §3). The future ~800×480 Phase-4 panel's scale factor is explicitly out of scope (constitution §3 "Open (Phase 4)"). | Accepted |
| A5 | **On-device verification method.** Serial log transcript + a human's direct visual inspection of the physical panel (no camera/automated pixel capture) is accepted as sufficient for this feature's on-device Must ACs — the first `steamcore` feature for which constitution §8's declared substitute method is actually enforceable now that the toolchain and board both exist. | **Resolved (PO judgment), confirmed by the user** (C7) |
| A6 | **Failure-handling asymmetry.** A steady-state (post-init) SPI transfer failure must never abort/crash the firmware — `DirtyTracker::commit`'s own documented contract already expects a driver to report back only actually-transferred tiles, leaving failed ones dirty for retry (its own header comment sketches exactly this). An *init*-time failure (panel not responding at boot) may still abort/log-and-halt, mirroring the bring-up spike's own `ESP_ERROR_CHECK` precedent — nothing useful can proceed without a working display anyway. | **Resolved (PO judgment), confirmed by the user** (C9) |
| A7 | The panel's landscape resolution and the ×2 scale factor become their own compile-time constants (extending `config.h`'s existing pattern); no file outside that definition spells out `480`, `320`, `16`, or a bare `2` scale factor — extends constitution §3's existing resolution-literal rule to the panel side. | Accepted |
| A8 | Reuses the bring-up spike's already-hardware-verified init sequence (reset pulse, COLMOD=0x66, MADCTL landscape+BGR, sleep-out, display-on) and pin wiring as a **starting point** — ported into reviewed, `steamcore`-namespaced code with a defined public contract, not a byte-for-byte copy (the spike is explicitly "not the real driver"). | Accepted |
| A9 | No dynamic allocation: a single fixed-size, tile-sized conversion buffer (≤ 32×32×3 = 3,072 bytes) only — same posture as every prior increment. | Accepted |
| A10 | Single-threaded contract, inherited unchanged from `Framebuffer`/`DirtyTracker`: the same task draws into the `Framebuffer` and calls the push function; nothing here synchronizes concurrent access. | Accepted |
| A11 | **On-device visual proof needs two fixtures, not one, each verified by the channel suited to it.** AC-3.1's first-frame, all-150-tiles-dirty pixel-region-correctness check reuses framebuffer-viewer's existing checked-in fixture pattern — but per the Designer's F1 finding (§8), that fixture's three single-pixel corner markers and its 1px diagnostic line were designed for programmatic, bit-exact diffing, never for a human eyeball, and the `(0,0)` corner marker sits at the panel's most bezel-vulnerable position in the palette's lowest-contrast colour (`DARK_ORANGE`); AC-3.1's human check is therefore scoped to the pattern's large, legible regions only, with the small elements verified via the serial log's per-tile record instead. A static pattern alone also cannot demonstrate "`GameLoop::tick()` drives a continuously-updating real-panel display" (the point of the user's override, A3) — AC-3.2 adds one new, small, deterministic per-tick animation: a single tile-aligned marker moving by a fixed, non-random step each tick, rendered as a solid full-tile fill in `BRIGHT_ORANGE` (the palette's highest-contrast step, per the Designer's F2 finding, §8) so it stays trivially trackable live, driven by the synthetic consumer's `update`, reusing `game-loop`'s already host-proven replay-determinism guarantee rather than inventing a new determinism story. | **Resolved (PO judgment), revised this round** (C11, C12) |
| A12 | **Push call-site is outside `tick()`/`render()`; `GameLoop`'s public API is unmodified.** The on-device harness's own loop calls `GameLoop<Consumer>::tick(input)` and then, immediately after, calls the driver's push function on the same `Framebuffer`/`DirtyTracker` pair — never from inside `Consumer::render()`, and never by adding a parameter or hook to `game_loop.h`. This keeps §6's "no change to `GameLoop`'s already-shipped public API" true while still making "`GameLoop::tick()` genuinely drives the real panel" structurally true, not just claimed. | **Resolved (PO judgment), new this round** (C8) |
| A13 | **The synthetic consumer is throwaway test-only, not new public API.** It lives beside the on-device harness (mirrors `game-loop`'s own in-repo test-double posture, its A10), carries no win/lose/score logic, and composes neither `GameSession` nor real button decoding nor a menu. NFR-7's public-surface list (conversion entry point, mapping entry point, panel-driver push entry point) is therefore unchanged by this revision — the consumer type itself is not counted. | **Resolved (PO judgment), new this round** (C8) |

## 4. User Stories

### US-1 (Must): Palette-to-panel pixel conversion is correct and host-testable

> As the engine developer, I want a pure, host-testable function converting each engine palette colour into the exact 3-byte 18bpp value the ILI9488 accepts, so the pixel-format half of this feature is proven correct on my Mac before it ever touches hardware or ESP-IDF.

**Acceptance criteria:**

- [ ] AC-1.1: Given each of the four `Color` values, when converted, then each produces a distinct, documented 3-byte 18bpp value matching framebuffer-viewer's already-designer-approved RGB hex palette (§8 there) — no new, independently invented colour mapping.
- [ ] AC-1.2: Given the conversion function's source, when compiled and run under the existing host `make test` gate (clang++/g++, C++17), then it builds and passes with zero ESP-IDF header included.
- [ ] AC-1.3: Given the conversion function called twice with the same `Color`, then it returns byte-identical output both times.

### US-2 (Must): Engine tile grid maps to the correct physical panel region

> As the engine developer, I want a pure, host-testable function mapping any of `DirtyTracker`'s 150 tile positions to its exact 32×32-pixel destination window on the real 480×320 landscape panel, so the ×2 scale-and-place math is proven before it reaches hardware.

**Acceptance criteria:**

- [ ] AC-2.1: Given tile `(col, row)` for every value in `[0, kTileCols) x [0, kTileRows)`, when mapped, then the resulting panel-pixel window is exactly `[col*32, col*32+31] x [row*32, row*32+31]` — covering the full 480×320 area with no gap and no overlap between adjacent tiles.
- [ ] AC-2.2: Given the mapping and conversion functions' source, when grepped, then no literal `240`, `160`, `480`, `320`, `16`, or a bare `2` scale factor appears outside their compile-time constant definitions (A7).
- [ ] AC-2.3: Given the mapping function, when compiled and tested under the host `make test` gate, then it passes with zero ESP-IDF header (mirrors AC-1.2).

### US-3 (Must): A live, GameLoop-driven consumer's dirty tiles reach the real panel continuously, over SPI DMA, confirmed on-device

> As the engine developer, I want `DirtyTracker`'s reported dirty tiles — converted and mapped per US-1/US-2 — pushed to the real ILI9488 panel over the SPI bus's DMA-capable transfer path, tick after tick as a minimal `GameLoop`-driven consumer runs, so the cabinet's screen is provably driven by `GameLoop::tick()` output live, over many ticks, not a disguised one-shot test-harness push or a two-frame static comparison.

**Acceptance criteria:**

- [ ] AC-3.1: Given a minimal, throwaway `GameLoop<Consumer>` on-device test harness (A12, A13) whose first `tick()` call renders framebuffer-viewer's checked-in fixture pattern (A11) into a fresh `Framebuffer` — `DirtyTracker`'s first scan reports all 150 tiles dirty — when that first tick's output is pushed to the real, wired panel, then a human visually confirms the pattern's large, legible regions (the two overlap rects and the 6×8 glyph block) show their correct colour on the physical panel, and the serial log's per-tile report confirms the three single-pixel corner markers and the 1px diagnostic line each landed at their exact expected coordinate and colour — those small elements are verified via the log, never asked of the human eye (A11, F1) — and the log records exactly 150 tiles sent.
- [ ] AC-3.2: Given the same harness continuing to run for at least 20 further consecutive ticks, each tick's `Consumer::update` moving one tile-aligned marker — rendered as a solid, single-colour `BRIGHT_ORANGE` fill of its entire tile (A11, F2) — by one tile along a fixed, deterministic path, and the driver pushing that tick's dirty tiles immediately after `GameLoop::tick()` returns (A12), when a human watches the physical panel live while the harness runs, then the marker is observed occupying at least 5 different tile positions over the run — continuous motion, not a static image — and for every one of those ticks the serial log records a tile count strictly less than 150, matching the number of tiles the marker actually left and entered that tick, with every tile the marker never visits still showing its AC-3.1 colour throughout.
- [ ] AC-3.3: Given the driver's source, when grepped, then no call issues a single SPI transaction covering the full 480×320 frame — every transaction's transfer size corresponds to at most one tile's worth of panel pixels (constitution §3/§6 non-negotiable).
- [ ] AC-3.4: Given the driver's source, when grepped, then it performs no dynamic allocation — a single fixed-size, tile-sized conversion buffer only (A9).
- [ ] AC-3.5: Given the on-device harness's source, when read, then `GameLoop<Consumer>`'s construction and `tick()` call use exactly `game_loop.h`'s already-shipped, unmodified public signature (A12) — no push call is made from inside `Consumer::render()` or from inside `GameLoop` itself, and `Consumer` is a throwaway type local to the harness, not a new `steamcore` public header (A13, NFR-7).

### US-4 (Must): A failed tile transfer is retried, never silently lost, never crashes the device

> As the engine developer, I want a tile that fails to transfer mid-operation to stay dirty for the next push and never abort the running firmware, so a transient SPI glitch during actual gameplay degrades to "that tile updates one frame late," never a crash or a permanently stale region.

**Acceptance criteria:**

- [ ] AC-4.1: Given a push call where one tile's transfer is made to fail (test harness / fault injection), when the call returns, then only the tiles that actually transferred are committed to `DirtyTracker`'s comparison buffer — the failed tile is reported back as not-transferred, per `DirtyTracker::commit`'s documented contract, and stays dirty on the next scan.
- [ ] AC-4.2: Given the same failing condition persists, when the next push call runs, then the previously-failed tile is attempted again — never permanently skipped.
- [ ] AC-4.3: Given a steady-state (post-init) transfer failure of any kind, when it occurs, then the firmware does not abort, crash or halt — it returns from the push call having committed only the successful tiles (AC-4.1).
- [ ] AC-4.4: Given a failure during the panel's *initialization* sequence, when it occurs, then the failure is logged; an abort at this specific stage is acceptable (A6) but must never be silent.

### US-5 (Should): SPI clock tuned toward the constitution's 40MHz budget

> As the engine developer, I want the real driver's SPI clock pushed toward the constitution's 40MHz per-tile budget — not left at the bring-up spike's conservative, untuned 10MHz — so the dirty-tile mechanism's actual performance benefit is realized, not just theoretically possible.

**Acceptance criteria:**

- [ ] AC-5.1: Given the driver running at its chosen clock speed, when AC-3.1/AC-3.2's pattern is pushed repeatedly, then the panel shows no visual corruption, streaking or tearing at that speed (same visual-correctness bar the spike proved at 10MHz), and the chosen clock speed plus why it was chosen is recorded in `qa.md`.

*Priority note: US-5 is the first thing dropped if this story must shrink — the dirty-tile mechanism (US-1…US-4) delivers its core value at any stable clock speed; raw throughput is a tuning concern, not a correctness one.*

## 5. Non-Functional Requirements

| # | Category | Requirement (measurable) | How it's verified |
|---|---|---|---|
| NFR-1 | Performance | A single dirty tile (32×32 panel pixels, 3,072 bytes) transfers within a documented, measured bound at the driver's chosen SPI clock, recorded in `qa.md`. A frame with all 150 tiles dirty is **not** required to beat the constitution's documented ~10.9fps full-frame floor — that case is mathematically identical to a full-frame push at the same clock; the improvement is over typical partial-churn frames, proven by AC-3.2's strictly-fewer-than-150 per-tick count. | qa.md on-device measurement |
| NFR-2 | Reliability / memory | Zero dynamic allocation anywhere in the driver: no `new`/`malloc`/`std::vector`/`std::string` (A9). | `/peer-review` (grep), AC-3.4 |
| NFR-3 | Reliability / failure handling | Steady-state transfer failures never abort; failed tiles are retried next push (US-4); init failure is the sole case that may abort, and never silently. | AC-4.1–4.4 |
| NFR-4 | Portability / toolchain | US-1/US-2's pure conversion/mapping logic compiles and passes under the host `make test` gate (clang++/g++, C++17, zero ESP-IDF header). Only the SPI transmission call requires ESP-IDF — the first `steamcore` production code exempt from the host gate, verified instead on-device (A2). | AC-1.2, AC-2.3, `/peer-review` |
| NFR-5 | Determinism | The conversion/mapping logic never reads wall-clock time or unseeded RNG. Waiting for DMA completion is *display-output timing*, decoupled from and never feeding back into the 60Hz game-logic determinism guarantee (constitution §3/§4) — no dropped-frame catch-up, no time-based interpolation. The on-device harness's per-tick marker motion (US-3, A11) is likewise a pure function of tick index, no randomness — reusing, not re-proving, game-loop's already host-tested replay-determinism guarantee, so this feature makes no new determinism claim requiring its own host test. | `/peer-review` (grep), AC-1.3, AC-2.1, AC-3.2 |
| NFR-6 | Constitution literals | No new GPIO literal outside `board_config.h` (reuses existing CS/RESET/DC/MOSI/SCK only); no literal `240`/`160`/`480`/`320`/`16`/bare-`2`-scale-factor outside their compile-time constant definitions (A7). | `/peer-review` (grep), AC-2.2 |
| NFR-7 | **Library lens — public API surface** | Exactly the new public surface named here: a palette→18bpp conversion entry point, a tile→panel-window mapping entry point, and one panel-driver entry point that inits the panel and pushes a `Framebuffer`'s `DirtyTracker`-reported dirty tiles, reporting back which tiles actually transferred. No other new public symbol; internal SPI/GPIO plumbing stays private. The on-device harness's synthetic `GameLoop`-compatible consumer (US-3, A13) is explicitly throwaway/test-only and is **not** counted against this list — it is not a new `steamcore` public header. | `/peer-review` |
| NFR-8 | **Library lens — contract clarity** | Doc comment on the driver entry point states: inherited single-threaded/no-throw contract; that only actually-transferred tiles are ever committed (US-4); that init failure is the one case that may abort while steady-state failure never does (A6); that display-output timing never feeds game-logic determinism (NFR-5); one usage example, mirroring `DirtyTracker`'s own header-comment style. | `/peer-review` |
| NFR-9 | Observability / ops | Every push logs at minimum the tile count sent and the tile count that failed/retried; every init logs success or failure explicitly — diagnosable without a debugger (constitution §4 "honest status reporting"). | AC-3.1, AC-3.2, `/peer-review` |
| NFR-10 | Security & privacy | N/A — offline device, no personal data, no network path touched by a display driver (constitution §2). | — |
| NFR-11 | Accessibility | N/A for new design — the palette's legibility (brightness-ordered, distinct luminance) was already decided and design-reviewed in framebuffer-viewer (§8 there); this feature faithfully reproduces those already-approved colours on real hardware and introduces no new visual design surface. | — |

*Lens note: `library` active **scoped** (constitution §2) — semver/packaging are no-ops for this statically-linked firmware image; Public API surface and Contract clarity land as NFR-7/NFR-8.*

## 6. Out of Scope

- **Full-frame display push of any kind** — constitution §3/§6 non-negotiable; AC-3.3 actively guards against it.
- **The XPT2046 touch controller** — off-limits per constitution; this exact panel module doesn't even have one populated.
- **Backlight PWM/dimming control** — hardwired to 3V3, no GPIO; re-wiring is a future hardware decision.
- **The future ~800×480 Phase-4 panel** and its scale factor — constitution §3 explicitly defers that decision; this feature implements only the confirmed ×2/480×320 mapping for the current prototype panel.
- **A system menu, a game registry, `GameSession`/READY-PLAYING-GAME_OVER session-phase handling, a restart button, real button-driven input, or any actual playable game** (win/lose rules, scoring) — `GameLoop`-driven live rendering to the real panel is in scope this cycle via one minimal, throwaway synthetic consumer (US-3, A3/A12/A13); composing that consumer with `GameSession`, a real game, or a menu is future system-integration work.
- **Camera-based or other automated pixel capture from the physical panel** — verification is serial log + direct human visual inspection only, this cycle (A5).
- **Automatic retry backoff/limit strategy** beyond "stays dirty, retried next push" (US-4) — no retry-count cap, no exponential backoff.
- **Double buffering or vsync/tear synchronization** beyond the dirty-tile mechanism itself.
- **Any change to `Framebuffer`, `DirtyTracker`, `GameLoop`, `GameInput`, or `GameState`'s already-shipped public API** (v0.0.1–v0.1.0) — this feature only adds a new, separate output path, plus one throwaway on-device test consumer external to that API (A12/A13).
- **SPI clock-speed tuning beyond a documented, stable working value** (Should, US-5) — a rigorous perf-optimization pass is future work if the achieved rate proves insufficient once real games exist.
- **Reading from the panel (MISO)** — write-only driver, matching the bring-up spike; MISO stays wired but unused.
- **Enlarging or redesigning framebuffer-viewer's fixture** for the sake of on-device visual coverage — F1's gap is closed by routing small elements through the serial log instead (A11), not by inventing a second, larger fixture; the existing fixture stays the single source of truth for AC-3.1's pattern.

## 7. Clarifications

| # | Date | Question | Resolution |
|---|---|---|---|
| C1 | 2026-09-03 | Can a display driver honour constitution §4's "no ESP-IDF header in logic code" rule at all? | **No, not entirely** — the SPI transmission call structurally cannot. Resolution: the pure conversion/mapping math (US-1/US-2) stays host-testable and ESP-IDF-free; only the transmission call is exempt, verified on-device instead (A2, NFR-4). |
| C3 | 2026-09-03 | Is a full-frame-equivalent "worst case" (all 150 tiles dirty) required to beat the constitution's documented ~10.9fps floor? | **No** — that case is mathematically a full-frame push at the same clock; the dirty-tile benefit is proven over typical partial-churn frames instead (NFR-1, AC-3.2). |
| C4 | 2026-09-03 | Is camera-based/automated on-device pixel verification required, given no host-testable path exists for real hardware? | **No** — serial log + direct human visual inspection is accepted, being the first feature where constitution §8's declared substitute method is actually enforceable (A5). |
| C5 | 2026-09-03 | Does a mid-operation SPI failure crash the firmware? | **No, never after init succeeds** — `DirtyTracker::commit`'s own documented contract already expects partial success; only an *init*-time failure may abort (A6, US-4). |
| C7 | 2026-09-02 | Is serial log + human visual inspection (no camera/automated capture) acceptable AC verification for this feature's on-device Must stories? | **Yes, accepted as proposed** — no spec change; A5 stands as originally drafted. |
| C8 | 2026-09-02 | Does this feature stay driver-only, with no `GameLoop` wiring, as the prior draft's A3 proposed? | **No — overridden by the user.** `GameLoop`-driven live rendering to the real panel is explicitly in scope this cycle: US-3 revised to require a minimal, throwaway `GameLoop<Consumer>` harness ticked at least 20 times, pushed live, not a two-frame static push. Boundary drawn by the PO: `GameSession`, a real game, a menu and a registry stay out — only a synthetic, non-gameplay consumer is needed to prove the point, called from the harness's own loop after `tick()` returns, never from inside `render()` or `GameLoop` itself, so `GameLoop`'s shipped public API stays unmodified (A3, A12, A13, §6). *(Supersedes C2, which asked the same question against the pre-override draft.)* |
| C9 | 2026-09-02 | Does a mid-operation SPI failure crash the firmware, given the steady-state-vs-init asymmetry? | **No, accepted as proposed** — no spec change; A6/US-4 stand as originally drafted. |
| C10 | 2026-09-02 | Is the 40MHz SPI clock target a Must or a Should? | **Should, accepted as proposed** — no spec change; US-5 stands as originally drafted (first thing dropped if the story must shrink). |
| C11 | 2026-09-02 | Does the GameLoop-integration override (C8) change what on-device fixture is needed for the visual proof? | **Yes.** A static single-pattern push (framebuffer-viewer's fixture, reused) still proves pixel/colour/region correctness on the first tick (AC-3.1), but cannot show a live, continuously-updating display. AC-3.2 now requires one added, new, small, deterministic per-tick marker animation, reusing game-loop's already-proven replay-determinism guarantee rather than inventing a new one (A11 revised). *(Supersedes C6, which asked the same question before this resolution existed.)* |
| C12 | 2026-09-03 | Does the Designer's `/look-and-feel` review (§8) require changing AC-3.1/AC-3.2, given F1 (the fixture's smallest, lowest-contrast, worst-positioned elements were never visually verified) and F2 (the moving marker has no required visual form)? | **Yes, both Major findings applied.** AC-3.1's human visual check is now scoped to the fixture's large, legible regions (the two overlap rects + the 6×8 glyph block); the three corner markers and the 1px diagnostic line are verified via the serial log's per-tile coordinate/colour report instead (F1). AC-3.2 now requires the moving marker to render as a solid, single-colour `BRIGHT_ORANGE` full-tile fill (F2). A11 revised accordingly. |

## 8. Design Review

- **Overall impression:** The spec's own posture ("proof device, not a designed
  game element") is accepted as a *scope* argument — this harness rightly
  needs no game-quality art, no menu, no session. But "throwaway test-only"
  is not a §6 exemption: constitution §6 binds *any* pattern that reaches a
  real `Framebuffer`/panel, and this is the first one that will. The palette
  itself cannot drift (structurally 4 colours, inherited at the engine
  level, per the task brief) so no finding is made there. Two concrete gaps
  remain, both Major, both small and additive to fix, neither requiring new
  product scope: AC-3.1 quietly asks a *human eyeball* to verify a fixture
  that was purpose-built and previously verified *programmatically* (below),
  and AC-3.2 never pins down what the marker actually looks like, leaving
  "large pixels, clear silhouette" to an unstated implementer choice on the
  very first live pixels a human will watch on the real cabinet. Tile-
  granular, one-step-per-tick marker motion (no easing, no sub-pixel slide)
  is confirmed as the right call against Product Principle 1 — see below.

- **F1 (Major) — AC-3.1 repurposes a programmatic fixture for a visual check
  it was never designed to survive.** `framebuffer-viewer`'s own spec built
  this exact fixture (`firmware/steamcore/test/dump_format_test.cpp`) to
  catch flip/transpose/stride bugs *bit-exactly*, and its own AC-4.1 states
  the pattern is "checked programmatically, pixel by pixel, **never
  visually**." A11/AC-3.1 now reuses that same fixture and asks a human to
  visually confirm "every region shows its correct colour" on the real
  panel. Three of its elements are close to un-eyeballable at that job: the
  three single-pixel corner markers (`setPixel` calls, lines 84–86) become a
  single 2×2-physical-pixel dot after ×2 scaling, sitting in the extreme
  corner of a 480×320 panel where a prototype 3.5" module's bezel/mount is
  most likely to crop a few pixels; the `(0,0)` corner is drawn in
  `DARK_ORANGE`, the palette's lowest-contrast step against `BLACK` (44/255
  luminance gap, the tightest of the four-colour ramp per
  `framebuffer-viewer` §8's own table) — so the hardest-to-see colour step
  is placed at the hardest-to-see location, at the smallest possible size.
  The 1px-wide diagnostic line (`fillRect(200,0,1,53)`, 2 physical px wide)
  is less at risk since its elongated shape is easier for an eye to catch
  than an isolated dot, but is still thin relative to the panel.
  **Violates:** Heuristic "visibility of status" (the acceptance check
  itself must be reliably observable) and this project's own legibility bar
  extended to real viewing conditions. **Location:** spec A11/AC-3.1; source
  `firmware/steamcore/test/dump_format_test.cpp:82-87`. **Fix:** scope
  AC-3.1's human check explicitly to the pattern's large, high/mid-contrast,
  multi-pixel regions (the two overlap rects + the 6×8 glyph block — plenty
  to confirm pixel-region correctness), and have the corner markers / thin
  line verified by the serial log's tile-count/coordinate record instead of
  by eye — or, if full-pattern visual coverage is actually wanted, ask the
  PO whether the corner markers should be drawn larger (e.g. a small filled
  block) specifically for this on-device check, since the original fixture
  optimizes for a different problem (bit-exact programmatic diffing) than
  this feature's (a human confirming what they see).

- **F2 (Major) — AC-3.2's marker has no required visual form.** Nothing in
  US-3/A11/AC-3.2 states the marker renders as a large, solid, single-colour
  fill of its tile. As written, an implementation could satisfy every literal
  clause — tile occupancy, per-tick count strictly < 150, untouched tiles
  keep their AC-3.1 colour — while drawing the marker as something small or
  low-contrast within its 16×16-engine/32×32-panel-pixel tile, which would
  technically move but be genuinely hard for a human to track live, quietly
  undermining the very check AC-3.2 exists to provide. This is exactly the
  spec-level gap through which a future `/increment` could plausibly drift
  toward "flashy tech-demo" motion rather than "arcade cabinet" — not
  because anyone intends to violate §6, but because the spec doesn't say.
  **Violates:** constitution §6 non-negotiable ("large pixels... clear
  silhouettes") — left unstated on the first pixels a human watches live on
  real hardware. **Location:** spec A11, AC-3.2. **Fix:** add one clause
  requiring the marker to render as a solid, single-palette-colour fill of
  its *entire* tile (suggest `BRIGHT_ORANGE`, the ramp's highest-contrast
  step, matching `framebuffer-viewer` §8's own luminance ordering) — this is
  additive spec precision, not new scope; it's the natural reading of "large
  pixels, clear silhouette" at this resolution, just made explicit instead
  of assumed.

- **Confirmed, no finding — motion granularity honors Principle 1:** the
  spec locks the marker to exactly one tile per tick with no easing,
  interpolation or sub-pixel motion (A11/NFR-5), even though the hardware
  and dirty-tile pipeline could technically support smoother motion. That is
  the correct call against Principle 1 ("atmosphere beats capability... a
  feature that makes the console look or feel more modern is rejected even
  when the hardware could carry it easily") — chunky, stepped motion is
  period-correct, not a missed opportunity. No change requested.

- **Accessibility notes:** Largely N/A, consistent with `framebuffer-viewer`
  §8's reasoning — no interactive control, no keyboard path, no focus order
  exists yet for this throwaway harness. One wording nit, Minor: NFR-11
  states this feature "introduces no new visual design surface" and treats
  the palette's legibility as fully settled by `framebuffer-viewer`'s
  review. That review's luminance table was computed against nominal sRGB
  hex values rendered in a PNG viewer — AC-3.1/AC-3.2 are actually the
  *first* time that decision gets checked against real emissive TFT
  hardware (18bpp quantization, backlight brightness, off-axis viewing
  angle), which is exactly what F1 above is about. Recommend rewording
  NFR-11 to say the palette *decision* is unchanged but AC-3.1's human check
  is its first real-hardware confirmation, not an assumption — a one-clause
  edit, non-blocking.

- **Design risks & required changes:** Two Major findings (F1, F2) route
  back to the PO/spec as small, additive wording changes to A11 and
  AC-3.1/AC-3.2 — neither invents new product scope, both are gaps in
  *specification precision* on the exact pixels a human will judge live on
  the real cabinet for the first time. Recommend resolving both before
  `Status` is set to `approved`: F1 by scoping AC-3.1's visual check to the
  legible regions (or enlarging the corner markers, PO's call), F2 by
  requiring a full-tile solid-fill marker. Neither is a Blocker — the
  pipeline this story proves is sound regardless, and both are fixable by
  spec-text edits, not rework — but leaving them unresolved risks an
  on-device QA pass/fail that doesn't actually reflect whether the pipeline
  works. One Minor, non-blocking wording note (NFR-11) recorded above.

---

## ✅ SPEC GATE

*All boxes checked → `/sprint-plan` may start. Any box open → back to `/story-time` or `/look-and-feel`.*

- [x] Problem, goal and success signal are concrete (no buzzwords, no "everyone")
- [x] Every story has testable Given/When/Then acceptance criteria
- [x] Stories are prioritized (MoSCoW) and at least one is a Must
- [x] Non-functional requirements are stated and measurable (or marked N/A with reason)
- [x] Clarify pass done: functional scope (C1/C8 — GameLoop-integration boundary), data (A7/A9), roles/permissions (N/A, no UI; `GameSession`/menu/registry explicitly excluded, C8/A3), error/edge cases (US-4, AC-4.1–4.4), NFRs (NFR-1…11, NFR-5/NFR-7 extended for the GameLoop-driven consumer), integrations (NFR-4/A2, the SPI bus; GameLoop push-call-site boundary, A12), UX flows (AC-3.1/3.2's on-device states, revised for live tick-over-tick motion and for the Designer's F1/F2 findings, C12), out-of-scope (§6, narrowed for GameLoop and for F1's fixture-reuse decision) — all swept, including the revised US-3
- [x] Open questions are resolved or explicitly accepted as risk — the five PO judgment calls flagged in the prior draft (A2, A3, A5, A6, A11) are now confirmed or, for A3, explicitly overridden by the user (C7–C12); two follow-on judgment calls the override required (A12, A13) are made and logged, not blocking; the Designer's two Major findings are resolved (C12)
- [x] Out-of-scope section is filled (something was consciously cut)
- [x] Constitution (`.spark/constitution.md`) respected — dirty-tile-only push (§3/§6), no dynamic allocation, no GPIO literal, determinism boundary named explicitly (NFR-5); `GameLoop`'s already-shipped public API left unmodified (A12); no unresolved conflict found
- [x] Design review done for UI-facing features (or marked N/A with reason) — completed via `/look-and-feel` (§8); two Major findings (F1, F2) resolved this round via A11/AC-3.1/AC-3.2 revisions (C12); one Minor NFR-11 wording note left open, non-blocking
- [x] Line budget respected: Ist 276 / Soll ~250 (excluding HTML comments) — 26 over; reason: §8's Design Review addition (~107 lines, Designer-owned, out of PO edit scope) accounts for the overage on its own; §§3/4/7 were trimmed this round (two superseded Clarification rows folded into their superseding entries) but the remaining AC/NFR/Assumption content in §§1–7 is load-bearing and wasn't cut further — see Handoff
- [x] Status set to `approved` by the user
