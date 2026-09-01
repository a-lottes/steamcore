# Spec: rendering-core

| | |
|---|---|
| **Phase** | Specify |
| **Owner** | Product Owner (`/story-time`), Designer (`/look-and-feel`) |
| **Status** | `approved` |
| **Date** | 2026-09-01 |
| **Ticket** | `none` |

**Handoff**
- **Status:** `approved` — approved by the user on 2026-09-01 in the `/story-time` session.
- **Summary:** SteamCore has no code at all; nothing can be drawn, and no quality gate exists. Deliver the hardware-independent rendering substrate — host test gate, 240×160 4-colour framebuffer with clipped drawing, sprite blitting, and dirty-tile detection — all verifiable today with clang++ alone.
- **Open:** `none` — Q1–Q10 all answered; see §7 Clarifications (C6–C15).
- **Binding ruling:** §4 User Stories US-1…US-4; §7 Clarifications for what changed in this round and why.
- **On conflict:** the numbered body below wins for everything except `Status`; log the mismatch as a finding at the next `/peer-review` and proceed.

## 1. Problem & Goal

- **Problem:** The repository contains a README, a constitution and nothing else. The solo developer (who is also the author of every future game module) cannot draw a single pixel, and has no automated way to tell working code from broken code. Every later feature — boot screen, menu, all five games — sits behind this. Worse: without a shared framebuffer contract, each game would grow its own pixel loops, which is exactly what the README forbids ("möglichst wenig Abhängigkeiten zwischen den Spielen").
- **Goal:** A tested, hardware-independent rendering core that a game module can draw into, and that a future display driver can consume — with zero ESP-IDF headers, zero dynamic allocation, and a public surface small enough to defend.
- **Success signal:** One documented command compiles and runs the host test suite on a machine with **no ESP-IDF and no board attached**, reporting 0 failures and 0 sanitizer findings across all ACs of US-2…US-4. Second, deferred signal: when the display-driver story lands, it consumes the dirty-tile contract without editing a single file delivered here.
- **Why now:** Phase 2 is blocked at zero. The panel arrives 2026-09-02; if the format-independent half of the render path is already tested, the first hardware day is spent on hardware, not on inventing a framebuffer under time pressure. This is also the cheapest moment to fix the API contract — before five games depend on it.

## 2. Target Users

- **Engine developer (primary, today):** the solo maintainer. Needs a working quality gate before writing anything else.
- **Game-module author (primary, near future — same person, different hat):** writes `render()` against a framebuffer reference and must never touch display hardware, pixel formats or SPI. Draws only — never scans or commits tiles (A8).
- **Display driver (consumer, next story):** needs a precise, format-independent answer to "what changed since the last successfully transferred frame?".
- *Not a user of this feature:* the console player. Nothing in this increment is visible to a human. That is deliberate and is the main reason the story is cut this small.

## 3. Assumptions & Open Questions

| # | Assumption / Question | Resolution |
|---|---|---|
| A1 | The idea arrived partly as a solution ("Framebuffer 240×160, 1 Byte/Pixel, 4-Farben-Palette, 16×16 Dirty Tiles"). These are **not** spec-invented solutions — they are fixed by constitution §3 and are treated here as constraints. | Accepted |
| A2 | Everything in this spec is verifiable with the host toolchain recorded in constitution §4. **No AC requires the board, ESP-IDF, cmake or the ILI9488 panel.** | Accepted |
| A3 | Per constitution §4 *Rendering verification*: this increment **cannot** be shown via the USB-CDC framebuffer dump + Python viewer, because neither the driver nor the viewer exists. QA therefore records the framebuffer half of §8 as **"not capturable yet"** with that reason, per criterion, and does not substitute source reading for it. Host unit tests are the evidence that counts here. | Accepted (C10) |
| A4 | The unverified 18-bpp claim (constitution §3) does **not** put this story at risk: no pixel-format conversion, scaling or push is in scope. Only tile *detection* is, and that is format-independent. | Accepted |
| A5 | Tile size (16) and virtual resolution (240×160) are compile-time constants in one header. | Accepted |
| A6 | **Tile-grid invariant:** 240/16 = 15 and 160/16 = 10 divide evenly, so the grid is exactly 15 × 10 = **150 tiles with no partial tile at any edge**, carried as a **150-bit field (20 bytes)**. Any future resolution/tile-size pair must preserve even division; a build that violates it fails at compile time (`static_assert`). | Accepted (C8) |
| A7 | `Framebuffer` is an ordinary, freely instantiable class — required so host tests never test against global state (constitution §4). The **engine** owns exactly one static instance plus its dirty tracker; games receive a reference and never own one. | Accepted (C9) |
| A8 | **Surface partition:** games see drawing operations only. Dirty scan and commit live on a separate driver-facing type and are not reachable from a game module's `render()`. | Accepted (C13) |
| A9 | **Sprite descriptor:** width, height, a pointer to pixel data in framebuffer format (1 byte per pixel), and a **stride/pitch** — so a sprite sheet or the 8×8 font atlas can point into a larger array without copying. | Accepted (C11) |
| A10 | **Threading contract: single-threaded.** The game draws and the driver scans/commits from the same task; no internal locking. Re-evaluated in the driver story, when a FreeRTOS task layout actually exists. Documented on the public API (NFR-7), not left implicit. | Accepted (C15) |
| A11 | **Coordinates are signed 32-bit** on the public API, and clipping is computed overflow-safe for every value of that type. | Accepted (C14) |

## 4. User Stories

### US-1 (Must): The host test gate exists

> As the engine developer, I want one documented command that compiles and runs all SteamCore host tests with the system C++ toolchain, so that every later increment has a real quality gate that needs neither ESP-IDF nor a board.

**Acceptance criteria:**

- [ ] AC-1.1: Given a clean checkout on a machine with `clang++` and `make` but **without** ESP-IDF, cmake or ninja, when I run the documented test command from the repository root, then the suite compiles with `-std=c++17 -Wall -Wextra -Werror`, prints a pass/fail count, and exits 0.
- [ ] AC-1.2: Given a deliberately broken assertion, when the test command runs, then it exits non-zero and the output names the failing test and its `file:line`.
- [ ] AC-1.3: Given the sanitizer test target, when it runs the full suite under `-fsanitize=address,undefined`, then it completes with zero sanitizer findings.
- [ ] AC-1.4: Given a test name pattern, when I run the command with that pattern, then only matching tests execute — so a single failing AC can be re-run in isolation.

### US-2 (Must): Framebuffer with palette and clipped drawing

> As a game-module author, I want a 240×160 framebuffer over the 4-colour palette with pixel, clear and filled-rectangle operations that clip safely, so that I can produce an exact image without knowing anything about the display.

*Colour is `enum class Color : uint8_t` with exactly `BLACK`, `DARK_ORANGE`, `ORANGE`, `BRIGHT_ORANGE` (C6) — an out-of-range colour cannot be constructed, so no AC covers one. Coordinates are signed 32-bit (A11).*

**Acceptance criteria:**

- [ ] AC-2.1: Given a freshly initialised framebuffer, when every pixel is read back, then all of them are BLACK.
- [ ] AC-2.2: Given a framebuffer, when it is cleared to ORANGE, then all width × height pixels read back ORANGE.
- [ ] AC-2.3: Given a framebuffer, when a pixel is written at (0,0) and at (width−1, height−1) and read back, then both return the colour written and no other pixel changed.
- [ ] AC-2.4: Given a framebuffer, when a pixel or filled rect is drawn at coordinates entirely outside it (negative, or ≥ width/height), then the buffer is unchanged and the call returns normally — no crash, no error code, no exception.
- [ ] AC-2.5: Given a framebuffer, when a filled rect is drawn crossing the left, top, right and bottom edge (four separate cases), then exactly the on-screen part is filled and every off-screen row/column is untouched.
- [ ] AC-2.6: Given a filled rect with zero or negative width or height, when drawn, then the framebuffer is unchanged.
- [ ] AC-2.7: Given any drawing call in AC-2.3…AC-2.6, when the suite runs under ASan (AC-1.3), then no read or write occurs outside the framebuffer's storage.
- [ ] AC-2.8: Given two independently constructed `Framebuffer` instances, when one is cleared to ORANGE, then the other is unchanged — the type carries no global or shared state (A7).
- [ ] AC-2.9: Given the extreme coordinate cases — a pixel at `INT32_MIN` and at `INT32_MAX`, a rect at `INT32_MIN`, and a rect whose x + width or y + height would overflow signed 32-bit — when each is drawn, then the framebuffer is unchanged or correctly clipped, the suite reports no UBSan signed-overflow finding, and no out-of-bounds access occurs (A11).

### US-3 (Must): Sprite blitting with transparency

> As a game-module author, I want to blit a sprite of arbitrary fixed size with a transparent colour into the framebuffer at any position, so that I can draw ships, gears and obstacles without writing per-game pixel loops.

*A sprite is width, height, a pointer to 1-byte-per-pixel data in framebuffer format, and a stride (A9).*

**Acceptance criteria:**

- [ ] AC-3.1: Given an 8×8 sprite with known pixel data blitted at (10,10) into a cleared framebuffer, when the framebuffer is read back, then exactly those 64 pixels match the sprite data and every other pixel is BLACK.
- [ ] AC-3.2: Given a blit call with no transparency argument, when it runs, then `Color::BLACK` is treated as transparent: black source pixels leave the destination unchanged and every other source pixel overwrites it (C7).
- [ ] AC-3.3: Given the same sprite blitted at five positions — crossing each of the four edges, and one fully off-screen — when the framebuffer is read back, then only the overlapping region is written, the fully off-screen case changes nothing, and ASan reports no out-of-bounds access.
- [ ] AC-3.4: Given the same sprite blitted twice at the same position into the same starting state, when the two resulting framebuffers are compared, then they are byte-identical.
- [ ] AC-3.5: Given sprites of other sizes (at least 16×12 and 1×1), when blitted, then AC-3.1 through AC-3.3 hold unchanged.
- [ ] AC-3.6: Given a blit call passing `Color::ORANGE` as the transparent colour, when it runs over a BRIGHT_ORANGE background, then orange source pixels leave the destination unchanged and black source pixels **are** written — proving a black silhouette can be drawn over a lit area (C7).
- [ ] AC-3.7: Given a 32×32 source array and a sprite descriptor selecting the 8×8 sub-rectangle at offset (8,8) with stride 32, when it is blitted, then exactly that sub-rectangle is drawn, no neighbouring pixel of the source array leaks in, and the same holds when the sub-rectangle is clipped at a framebuffer edge (A9).

### US-4 (Should): Dirty 16×16 tile detection

> As the display driver (next story), I want to be told exactly which 16×16 tiles changed since the last successfully transferred frame, so that the console can honour "never a full-frame push" without scanning the screen itself.

*Dirty tracking is a separate type that pairs with a `Framebuffer` and owns the comparison buffer (C13), so a plain framebuffer costs 38,400 bytes and the game-facing API stays free of driver concerns.*

*Priority note: `Should`, not `Must` — it has no consumer until the driver exists. If this story has to shrink, this is the first thing dropped. It is kept because constitution §6 makes the tile pipeline non-negotiable, and because detection is the one half of it that is provably independent of the unverified 18-bpp question (A4).*

**Acceptance criteria:**

- [ ] AC-4.1: Given a framebuffer identical to the comparison buffer, when the dirty scan runs, then zero tiles are reported dirty.
- [ ] AC-4.2: Given exactly one changed pixel, when the scan runs, then exactly one tile is dirty and it is the tile containing that pixel (checked for a pixel at a tile's first and last pixel).
- [ ] AC-4.3: Given changed pixels in the four corner tiles only, when the scan runs, then exactly those four tiles are reported, each identified by its tile column and row.
- [ ] AC-4.4: Given every pixel changed, when the scan runs, then all 15 × 10 tiles are reported dirty.
- [ ] AC-4.5: Given a scan that reported dirty tiles, when the caller **does not** commit and scans again with no further drawing, then the same dirty set is reported; when the caller commits **all** reported tiles and scans again, then zero tiles are dirty.
- [ ] AC-4.6: Given a scan result, when it is inspected, then it is a fixed-size 150-bit field (20 bytes, A6) that the caller reads directly — no allocation, no owning container, no callback required — and a `static_assert` fails the build if the resolution and tile size ever stop dividing evenly.
- [ ] AC-4.7: Given a scan reporting 10 dirty tiles, when the caller commits only 4 of them (the failed-SPI-transfer case), then the next scan still reports exactly the other 6 as dirty and the 4 committed ones as clean — a partial transfer never silently drops a frame.
- [ ] AC-4.8: Given a freshly constructed dirty tracker on which nothing has been drawn and nothing committed, when the **first** scan runs, then all 150 tiles are reported dirty — so the panel is painted once in full, tile by tile, and never starts from an unknown screen state (C12).

## 5. Non-Functional Requirements

| # | Category | Requirement (measurable) | How it's verified |
|---|---|---|---|
| NFR-1 | Performance | A full dirty scan of a completely changed framebuffer (38,400 bytes, 150 tiles) completes in < 5 ms with `-O2` on the reference host (Apple clang 14). Target-device timing is explicitly **not** measured this cycle and is not claimed. | host benchmark test + `/peer-review` |
| NFR-2 | Reliability / memory | Zero dynamic allocation: no `new`, `malloc`, `std::vector`, `std::string` or equivalent anywhere in the delivered engine code (grep gate). All buffers are static or fixed-size. A plain `Framebuffer` is 38,400 bytes; the comparison buffer is only paid for where dirty tracking is used (C13). | `/peer-review` (grep) |
| NFR-3 | Reliability / bounds | The full suite runs under `-fsanitize=address,undefined` with zero findings, including every off-screen, edge-crossing and extreme-coordinate case in AC-2.4, AC-2.5, AC-2.9 and AC-3.3. | AC-1.3 |
| NFR-4 | Portability / toolchain | Engine code compiles clean under both `clang++` and `g++` with `-std=c++17 -Wall -Wextra -Werror`; **no ESP-IDF header** (`esp_*`, `freertos/*`, `driver/*`) appears in any delivered file; no literal `240`, `160`, `480`, `320` or `16` (as tile size) outside the single constants header. | `/peer-review` (grep + both compilers) |
| NFR-5 | Determinism | The same sequence of drawing calls from the same initial state produces a byte-identical framebuffer, under both compilers; no engine code reads wall-clock time or an unseeded random source (grep for `time`, `rand`, `chrono`). | AC-3.4 + `/peer-review` |
| NFR-6 | **Library lens — public API surface** | The public surface is exactly the names a game module or the driver needs; everything else is `static` or in a `detail` namespace. **One** entry point per operation — one blit, one scan, one commit. The game-facing and driver-facing halves are separate types so a game module cannot reach scan/commit (A8). Any public symbol without a consumer named in §4 is a review finding. | `/peer-review` |
| NFR-7 | **Library lens — contract clarity** | Every public function carries a doc comment stating: coordinate preconditions and the signed-32-bit/overflow-safe guarantee (A11), clipping behaviour, transparency behaviour including the `Color::BLACK` default, the sprite stride semantics (A9), the commit contract of AC-4.5/4.7/4.8, the **single-threaded** contract (A10), and error behaviour — this API throws nothing and returns no error codes; out-of-range input is clipped or ignored as specified per function. One usage example per module shows the intended call sequence. | `/peer-review` |
| NFR-8 | Observability / ops | N/A for this increment — pure in-memory logic with no runtime failure mode to report and no logging surface yet. The only diagnostic output is the test runner, which must name the failing test and `file:line` (AC-1.2). | AC-1.2 |
| NFR-9 | Security & privacy | N/A — offline device, no input, no persistence, no personal data (constitution §2). | — |
| NFR-10 | Accessibility | N/A — nothing in this increment is visible to a human. Contrast and legibility of the orange palette become live at the first story that puts pixels on a panel. | — |
| NFR-11 | Integrations & dependencies | N/A — this increment calls no external system, library or service; it adds no third-party dependency beyond the C++17 standard library. There is nothing that can be slow or down. The only integration is the *future* driver, and it is a contract (US-4), not a call. | `/peer-review` |

*Lens note: `library` is active **scoped** (constitution §2). Its semver and packaging/tree-shaking halves are no-ops for a statically-linked firmware image and are not raised here. Its Public API surface and Contract clarity halves land as NFR-6 and NFR-7.*

## 6. Out of Scope

Consciously cut. Each of these is a candidate for its own story, not a forgotten requirement.

- **Text rendering and the 8×8 font — the recommended NEXT story.** Cut from here on purpose. It needs decisions this story does not: font asset format and location under `assets/fonts/`, glyph coverage (the README boot screen needs uppercase, digits, `.` and `>` — is that the whole set?), how the asset becomes C++ data, and whether 8×8 (= 30×20 characters) reads as "große Pixel" under constitution §6. With the stride-carrying sprite descriptor (A9) now decided, the font atlas needs no new blitting code at all — the next story is font data plus a glyph-lookup layer.
- **ILI9488 driver, SPI, DMA, tile push, 18-bpp conversion, ×2 scaling, backlight.** Requires the panel (arrives 2026-09-02) and ESP-IDF (not installed).
- **`board_config.h` and any GPIO assignment.** No hardware is wired.
- **USB-CDC framebuffer dump and the Python viewer under `tools/` — its own story.** Constitution §8 explicitly allows the framebuffer half of QA to be deferred until the toolchain and board exist; QA records it as "not capturable yet" with the reason (A3). No image or PGM dump is produced by this story's tests.
- **Games constructing their own framebuffers.** The type is instantiable (A7) so tests are not forced onto global state, but a game module receives a reference to the engine's single instance and nothing else. Off-screen composition is not a supported use yet.
- **Multi-task safety.** The contract is single-threaded (A10); locking, task hand-off and any FreeRTOS layout are the driver story's problem.
- **Line, circle, polygon, triangle and gradient primitives.** No consumer. Added when the first game needs one.
- **Sprite animation, frames, sprite sheets/atlases, flipping, rotation, scaling.** The *stride* makes an atlas possible later; no atlas handling is built here.
- **Double buffering / page flipping, palette remapping, fades, particles, CRT effects** (scanlines, glow, flicker).
- **Game loop and the 60 Hz fixed step, input, audio, collision, score, highscore, persistence, game registry, system menu, boot screen.** All of Phase 2 except this substrate.
- **Any decision about the final 800×480 panel's virtual resolution** (constitution §3 leaves this open by design).

## 7. Clarifications

| # | Date | Question | Resolution |
|---|---|---|---|
| C1 | 2026-09-01 | Does 8×8 text rendering belong in this first story? | **No.** Split out; recorded in §6 as the recommended next story. |
| C2 | 2026-09-01 | Is dirty-tile computation meaningful before any consumer exists? | **Kept, demoted to `Should`, scoped to detection only** (US-4). Justified by constitution §6 and by A4 — detection cannot be invalidated by whatever the panel's pixel format turns out to be. |
| C3 | 2026-09-01 | Must the USB-CDC dump + viewer be part of this story so constitution §8 can apply? | **No.** §8 defers the framebuffer half; the increment must only *state* it — done in A3. |
| C4 | 2026-09-01 | Should general draw primitives (lines, circles) be included? | **No.** Only clear, pixel and filled rect. |
| C5 | 2026-09-01 | Half-size version, if the story must shrink? | US-1 + US-2 alone still deliver the core value. Drop US-4 first, then US-3. |
| C6 | 2026-09-01 | Palette representation and handling of an out-of-range colour value. | `enum class Color : uint8_t` with exactly four values. An invalid colour cannot be constructed, so the clamp/mask/assert question disappears and no AC covers it. → US-2. |
| C7 | 2026-09-01 | Is the transparent index fixed to BLACK or chosen per blit? | **Per-blit parameter, defaulting to `Color::BLACK`** — the common case stays a one-liner, a black silhouette over a lit area stays possible. → AC-3.2, AC-3.6. |
| C8 | 2026-09-01 | What does the driver receive, and who clears the dirt? | **150-bit field (20 bytes) + explicit commit.** The driver reads the field, pushes, then reports back what it *actually* transferred; only that clears those tiles. → A6, AC-4.5, AC-4.6, AC-4.7. |
| C9 | 2026-09-01 | One framebuffer instance or many? | `Framebuffer` is an ordinary, freely instantiable class — host tests must not be forced onto global state. The engine owns one static instance; games get a reference. → A7, AC-2.8, §6. |
| C10 | 2026-09-01 | Is a host-side image dump needed as a QA bridge? | **No.** Host unit tests alone are sufficient proof here; the viewer becomes its own story and QA records the framebuffer half as "not capturable yet" per §8. → A3. |
| C11 | 2026-09-01 | Does the sprite descriptor carry a stride? | **Yes, from the start:** width, height, pointer to 1-byte-per-pixel data, stride. One field now versus every call site later; it also makes the font atlas free. → A9, AC-3.7. |
| C12 | 2026-09-01 | What is the comparison buffer's initial state? | **First scan reports all 150 tiles dirty** — the panel is painted once in full, tile by tile (150 tile pushes, never a full-frame push, so §6 holds). Made an explicit AC, not an implementation detail. → AC-4.8. |
| C13 | 2026-09-01 | Who owns the comparison buffer? | **A separate type that pairs with a `Framebuffer`.** A throwaway test instance costs 38,400 bytes instead of 76,800, and the game-facing API stays free of driver concerns. → A8, US-4 note, NFR-2, NFR-6. |
| C14 | 2026-09-01 | Coordinate type and overflow behaviour? | **Signed 32-bit**, clipping correct and overflow-safe for every value of the type. Pinned by an AC rather than left to ASan, which would not reliably catch signed overflow. → A11, AC-2.9. |
| C15 | 2026-09-01 | Threading model? | **Single-threaded, declared explicitly:** game draws, driver scans and commits, same task, no internal locking. Re-evaluated in the driver story when a task layout exists. → A10, NFR-7, §6. |

## 8. Design Review

- **Overall impression:** **N/A — no design review required for this increment.** It produces no human-visible output: no display driver, no text, no pixels reaching a panel. The first UI-facing story is the display driver, and the graphics philosophy of constitution §6 becomes reviewable there.
- **Heuristics findings:** N/A — no user-facing interaction surface exists in this increment.
- **Accessibility notes:** N/A — see NFR-10. Contrast and legibility of the orange palette are assessed at the first story that puts pixels on a panel.
- **Design risks & required changes:** none for the visual surface. The equivalent risk here is the *API* contract, which the constitution's active `library` lens covers instead — captured as NFR-6 (public API surface) and NFR-7 (contract clarity), both verified at `/peer-review`.

---

## ✅ SPEC GATE

*All boxes checked → `/sprint-plan` may start. Any box open → back to `/story-time` or `/look-and-feel`.*

- [x] Problem, goal and success signal are concrete (no buzzwords, no "everyone")
- [x] Every story has testable Given/When/Then acceptance criteria
- [x] Stories are prioritized (MoSCoW) and at least one is a Must
- [x] Non-functional requirements are stated and measurable (or marked N/A with reason)
- [x] Clarify pass done: full taxonomy swept; Q1–Q10 raised and all resolved (C6–C15)
- [x] Open questions are resolved or explicitly accepted as risk — 0 open
- [x] Out-of-scope section is filled (something was consciously cut)
- [x] Constitution (`.spark/constitution.md`) respected — no conflict found; §4 *Rendering verification* satisfied by the explicit statement in A3
- [x] Design review N/A with reason recorded in §8 — no human-visible output; the `library` lens covers the API-contract concerns instead
- [x] Line budget respected: Ist 191 / Soll ~250 (excluding HTML comments)
- [x] Status set to `approved` by the user — approved 2026-09-01
