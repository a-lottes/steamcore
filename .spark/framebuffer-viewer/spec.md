# Spec: framebuffer-viewer

| | |
|---|---|
| **Phase** | Specify |
| **Owner** | Product Owner (`/story-time`), Designer (`/look-and-feel`) |
| **Status** | `approved` |
| **Date** | 2026-09-01 |
| **Ticket** | `none` |

**Handoff**
- **Status:** `approved` — approved by the user on 2026-09-01, after /look-and-feel filled §8 Design Review.
- **Summary:** rendering-core proved the framebuffer's *logic* with byte-level unit tests, but nobody has ever looked at a frame it produced. Deliver a documented dump-file format plus a Python decoder that renders it as a PNG, proven end-to-end against a real, engine-produced fixture — no ESP-IDF, no board, no wired display required.
- **Open:** `none` — two rounds of Clarify pass done (C1–C6, then C7–C14); every ambiguity found was either resolved here or explicitly deferred to `/look-and-feel` (palette hex, §8) or `/sprint-plan` (literal file paths, CLI flag syntax) as that phase's decision to make, not the Product Owner's.
- **Binding ruling:** §4 User Stories US-1…US-5; §7 Clarifications for what changed each round and why.
- **On conflict:** the numbered body below wins for everything except `Status`; log the mismatch as a finding at the next `/peer-review` and proceed.

## 1. Problem & Goal

- **Problem:** rendering-core (e5d4be3, done) proved its framebuffer, sprite-blit and dirty-tile logic entirely through unit-test assertions the same developer wrote. Nobody has ever seen a pixel this engine produced. That's not a cosmetic gap: an assertion and the code it checks can share the same wrong mental model (a flipped sprite, a transposed axis, a mis-mapped colour), and no unit test catches its own author's blind spot. The ILI9488 panel is unwired and ESP-IDF is not installed, so a real display is weeks away — every rendering-touching story between now and then (sprites, animations, the boot screen) would otherwise ship blind.
- **Goal:** a documented, versioned full-frame dump format, a Python tool that decodes it into a PNG, and proof — via a fixture the real C++ rendering-core API produced, not hand-typed bytes — that the decoded image is pixel-correct.
- **Success signal:** one documented command draws a known pattern through the actual `Framebuffer`/`Sprite` API in a host test, dumps it, decodes it with the Python tool, and an automated test confirms every pixel of the resulting PNG matches what was drawn — runnable today, with zero ESP-IDF, no board, no display. Deferred signal: the future USB-CDC device story reuses this exact file format as its payload without changing a byte of what's defined here.
- **Why now:** explicitly deferred out of rendering-core's own scope (§6) and named there as its own story. Constitution §4/§8 already declare this the project's substitute verification method for a browser-less product — right now that method doesn't exist, and constitution §8 states QA cannot yet capture the framebuffer half of any story for that reason.

## 2. Target Users

- **Engine developer (primary, today):** the solo maintainer. Needs to see what rendering-core's API actually drew, with no display and no ESP-IDF on this machine.
- **Future device-side-dump story (consumer, later):** same person, later hat. Needs a battle-tested definition of "one frame's bytes" so that story only has to solve *how those bytes cross a wire* — not what they mean.
- *Not a user of this feature:* the console player. Nothing here reaches the cabinet; the output is a PNG file on the developer's Mac.

## 3. Assumptions & Open Questions

| # | Assumption / Question | Resolution |
|---|---|---|
| A1 | The idea arrived partly as a solution ("USB-CDC dump, Python tool under `tools/`, PNG"). Constitution §4/§8 already name exactly this as the project's rendering-verification method — treated as a fixed constraint, not a spec-invented solution, same posture as rendering-core A1. | Accepted |
| A2 | **The wire/framing protocol (USB-CDC framing, sync bytes, checksum, chunking, baud rate) is out of this story's scope.** This story defines only a **file** format for one full-frame dump; deciding how those bytes travel live over a serial link is deferred to the future device-side story, once ESP-IDF and a board exist to build and validate a real sender against. | Accepted (C1) |
| A3 | **Full-frame dump only — no partial/dirty-tile dump.** A dev/debug dump is 38,400 bytes; there is no bandwidth motive to make it partial. Correctness of what's decoded is the only thing that matters here. | Accepted (C2) |
| A4 | **Exact RGB hex values for the 4 palette colours are a Designer decision (`/look-and-feel`, §8), not fixed by this spec.** This story only constrains them functionally: pairwise distinct, ordered by increasing brightness BLACK → DARK_ORANGE → ORANGE → BRIGHT_ORANGE. | **Confirmed** by the user — kept deferred to `/look-and-feel` (C7) |
| A5 | **The Python tool uses only the Python 3 standard library — no third-party package, no `pip install`, Pillow explicitly excluded.** PNG's compressed `IDAT` stream is produced with the stdlib `zlib` module alone (a "stored"/uncompressed DEFLATE block is sufficient — correctness, not file size, is required, C14). | **Confirmed** by the user (C8) |
| A6 | The C++ dump-writer is **host-only tooling code** (file I/O, string formatting) — it does not ship in the device firmware image, so it is exempt from the zero-dynamic-allocation rule that binds `firmware/steamcore/include`/`src` (constitution §3/§4; `tools/check_constraints.sh` already scopes only those two directories). It still compiles under the existing host toolchain with no ESP-IDF header, and is exercised by the existing `make test` gate. | Accepted |
| A7 | The decoder reads width/height **from the dump header**, never from a hard-coded 240×160 — so a future Phase-4 resolution change (constitution §3, "Open (Phase 4)") needs no change to this tool. | Accepted |
| A8 | **Multi-byte header fields (version, width, height) are little-endian, stated explicitly in the format doc.** Both this host and the ESP32-S3 target (Xtensa LX7) are little-endian today, but the format doesn't quietly rely on that coincidence — it's pinned so a future toolchain change can't silently break it. | Accepted (C9) |
| A9 | **The fixture's path (relative to the repo root) is documented in exactly one place**, referenced by both the C++ test that writes it and the Python test that reads it (US-4) — never two independently hard-coded path strings that can drift apart. The literal path itself is a `/sprint-plan` decision. | Accepted (C10) |
| A10 | **Decode failures are atomic and stderr-reported:** either a fully valid PNG is written or none is, the message goes to stderr, and the exit code is non-zero — the exact numeric code is left to `/sprint-plan`, same as rendering-core left AC-1.2's exit code undictated. | Accepted (C11) |
| A11 | The decoder checks a dump's **declared** width/height against the file's **actual** byte size (AC-1.4/AC-1.8) before allocating any width×height-sized buffer — so a corrupted header claiming an enormous frame costs a file-size check, never a large allocation attempt. | Accepted (C12) |
| A12 | **CLI flag syntax is a `/sprint-plan` decision.** This spec fixes only the two pieces of information a call must convey (dump path, output path) and the defined failure states (US-3) — not literal flag names. | Accepted (C13) |
| A13 | **Output-path and format conventions:** an existing file at the output path is silently overwritten (standard CLI convention, no prompt); PNG *correctness* is required, not minimal file size — an uncompressed/"stored" DEFLATE stream inside a spec-valid PNG satisfies this story (A5). | Accepted (C14) |

## 4. User Stories

### US-1 (Must): A documented, versioned full-frame dump format

> As the engine developer, I want a documented byte format for one full framebuffer dump, so that the Python decoder and the future device-side sender can be built independently against the same written contract instead of against each other's source code.

**Acceptance criteria:**

- [ ] AC-1.1: Given the documented format, when it is read, then it specifies: a marker identifying the file as a SteamCore framebuffer dump, a format version number, the frame's width and height in pixels, and every pixel's palette index in row-major order — sufficient to decode the image with no out-of-band knowledge of the current compile-time resolution (A7).
- [ ] AC-1.2: Given a file whose marker doesn't match, when a reader parses it, then the reader detects and rejects it as "not a SteamCore dump" before touching any pixel data.
- [ ] AC-1.3: Given a file whose version the reader doesn't support, when parsed, then the reader detects and rejects it as an unsupported version before touching any pixel data.
- [ ] AC-1.4: Given a file whose declared width × height doesn't match the number of pixel bytes present, when parsed, then the reader detects and rejects the mismatch rather than reading past the payload or silently truncating the image.
- [ ] AC-1.5: Given a pixel value outside 0–3, when parsed, then the reader detects and rejects it rather than mapping it to an arbitrary colour.
- [ ] AC-1.6: Given the format doc read side-by-side with rendering-core's `Framebuffer` in-memory layout (row-major, 1 byte/pixel, `Color` 0–3), then the pixel payload is bit-for-bit identical to that layout — no transform, packing or compression in this version.
- [ ] AC-1.7: Given a multi-byte header field (version, width, height), when it is read, then it is interpreted as little-endian per the format doc's explicit byte-order statement (A8) — never left to either toolchain's native default.
- [ ] AC-1.8: Given a dump whose header declares a width/height implying a payload far larger than the file's actual size, when parsed, then the mismatch (AC-1.4) is detected from the file's actual byte count **before** any width×height-sized buffer is allocated (A11).

### US-2 (Must): C++ dump writer, proven against a real, engine-produced fixture

> As the engine developer, I want a host-testable function that serializes an actual `Framebuffer` into the documented format, exercised by a test that draws a known pattern through the real rendering-core API, so that the Python decoder is proven against bytes the engine itself produced — not bytes I typed by hand and might have gotten subtly wrong on one side.

**Acceptance criteria:**

- [ ] AC-2.1: Given a `Framebuffer` cleared to a known colour, when serialized, then the output satisfies AC-1.1 and AC-1.6, built and run by the existing `make test` gate, with no ESP-IDF header in the writer's source (A6).
- [ ] AC-2.2: Given a `Framebuffer` drawn — through the existing `clear`/`fillRect`/`setPixel`/`blit` API — into a pattern using all four palette colours that is **not symmetric** under any horizontal flip, vertical flip or transpose, when serialized, then the dump's payload matches that pattern exactly at every coordinate. The asymmetry is deliberate: a row/column swap, an axis flip or an off-by-one in either the writer or a later reader must produce a detectably wrong image, never a coincidentally-correct one.
- [ ] AC-2.3: Given the test in AC-2.2, when it runs, then it writes its output to a fixture file checked into the repository, so the same real, engine-produced bytes are available to the Python test suite (US-4) without needing the C++ toolchain at Python-test time.
- [ ] AC-2.4: Given the writer called twice on the identical `Framebuffer` state, when the two outputs are compared, then they are byte-identical (NFR-3).
- [ ] AC-2.5: Given the fixture file from AC-2.3, when its path is referenced by the C++ test that writes it and the Python test that reads it (US-4), then both reference the **same single documented path** — never two independently hard-coded strings that can drift apart (A9).

### US-3 (Must): Python decoder renders a dump as a PNG

> As the engine developer, I want a documented command that reads a dump file and writes a PNG, using a fixed, documented palette-to-colour mapping, so that I can look at what a rendering change actually produced without a display, ESP-IDF or the board.

**Acceptance criteria:**

- [ ] AC-3.1: Given a valid dump file (US-1/US-2), when I run the documented command with its path and an output path, then a PNG is written whose dimensions equal the dump's declared width and height, overwriting silently if a file already exists at that path (A13).
- [ ] AC-3.2: Given the decoded image, when its pixels are inspected, then each of the four palette indices maps to one fixed, documented RGB colour; the four are pairwise distinct and ordered by increasing brightness BLACK → DARK_ORANGE → ORANGE → BRIGHT_ORANGE (A4 — exact hex is a Designer decision).
- [ ] AC-3.3: Given a file that fails AC-1.2, AC-1.3, AC-1.4, AC-1.5 or AC-1.8, when the command runs against it, then it exits non-zero, writes to **stderr** a message naming the specific problem (bad marker / unsupported version / size mismatch / invalid palette index), and writes no PNG — the failure is atomic (A10).
- [ ] AC-3.4: Given the command run with no arguments, or with a path that cannot be opened as a regular file (missing, a directory, unreadable), then it prints usage information (no-arguments case) or a clear "cannot open input" message (bad-path case) to stderr and exits non-zero in both cases, without a raw stack trace.
- [ ] AC-3.5: Given the same valid dump decoded twice, when the two PNGs are compared, then they are byte-identical (NFR-3).
- [ ] AC-3.6: Given a successful decode, when the command finishes, then it prints the output PNG's path to stdout and exits 0 — so a human or a script can locate the result without guessing.

### US-4 (Must): Pixel-correct round-trip proof

> As the engine developer, I want an automated test that proves the whole chain — a known pattern drawn through the real API, dumped, decoded, rendered as a PNG — so that "the viewer works" is a passing test, not something I eyeballed once and hoped stayed true.

**Acceptance criteria:**

- [ ] AC-4.1: Given the fixture from AC-2.3 (real, engine-produced, all-four-colour, asymmetric pattern), when decoded, then every pixel of the resulting PNG matches the documented palette mapping (AC-3.2) for the colour actually drawn at that coordinate in the C++ test — checked programmatically, pixel by pixel, never visually.
- [ ] AC-4.2: Given this test, when run via its own documented command, then it fails loudly (non-zero exit, names the first mismatching coordinate) if a future change to either the writer or the decoder breaks the contract.

### US-5 (Should): One command, drawn pattern to PNG

> As the engine developer, I want one documented command that builds and runs the fixture-producing host test and then decodes its output, so that checking a rendering change doesn't require remembering two separate invocations across two languages.

**Acceptance criteria:**

- [ ] AC-5.1: Given a clean checkout, when I run the single documented command, then it produces the PNG from AC-4.1 with no other manual step, and exits non-zero if either the C++ or the Python half fails.

*Priority note: US-5 is pure convenience over US-1…US-4 and is the first thing dropped if this story has to shrink further — it adds no new capability, only fewer keystrokes.*

## 5. Non-Functional Requirements

| # | Category | Requirement (measurable) | How it's verified |
|---|---|---|---|
| NFR-1 | Performance | Decoding one 240×160 (38,400-byte) dump and writing its PNG completes in < 1s on the reference host (constitution §4's Apple clang 14 host). Trivial at this size; stated so a regression is still falsifiable. | `/peer-review` timing check |
| NFR-2 | Reliability | Every malformed-input case in AC-3.3/AC-3.4 is caught before any pixel is decoded and produces zero output files — a rejected dump never yields a silently-wrong or partial PNG (A10). | AC-3.3, AC-3.4, AC-1.2–1.5, AC-1.8 |
| NFR-3 | Determinism | The writer (AC-2.4) and the decoder (AC-3.5) are both deterministic: identical input produces byte-identical output, every time. | AC-2.4, AC-3.5 |
| NFR-4 | Portability / toolchain | The C++ half adds zero new build-gate dependencies: writer and its test compile and run under the existing `make test` (clang++, C++17, no ESP-IDF header, A6). The Python half runs under the Python 3 already on this Mac; per A5 (confirmed), **no third-party package** — PNG's `IDAT` stream is produced with the stdlib `zlib` module alone. | `/peer-review` (grep for `import` of anything beyond the stdlib) + documented commands actually run |
| NFR-5 | **Library lens — public API surface** | The writer's public surface is exactly one function (serialize a `Framebuffer` to the documented format), built on the *existing* `Framebuffer`/`Color` public API. No new public method is added to `Framebuffer`, `Sprite` or `DirtyTracker`. Any new public C++ symbol without a consumer named in §4 is a review finding. | `/peer-review` |
| NFR-6 | **Library lens — contract clarity** | The dump format is documented in one authoritative place, precise enough that the C++ writer and the Python decoder were built independently against that doc rather than against each other's source — including byte order (AC-1.7/A8), the fixture's single-source-of-truth path (A9), and the explicit non-goal that no wire/framing protocol is defined here (A2). | `/peer-review` |
| NFR-7 | Observability | A rejected file's error message, on stderr, names the specific failure (bad marker / unsupported version / size mismatch / invalid palette index) rather than a generic "invalid file"; a successful run names its output path on stdout (AC-3.6). | AC-3.3, AC-3.6 |
| NFR-8 | Security & privacy | N/A — offline dev tool, local files only, no network call, no personal data (constitution §2). | — |
| NFR-9 | Accessibility | N/A for the CLI tool itself — no interactive UI. Palette legibility (are the 4 colours tellable apart) is a functional requirement (AC-3.2) and a Design Review concern (§8), not a WCAG axis. | — |

*Lens note: `library` is active **scoped** (constitution §2) — semver/packaging are no-ops here; Public API surface and Contract clarity land as NFR-5/NFR-6.*

## 6. Out of Scope

- **USB-CDC wire/framing protocol** — sync bytes, checksum, chunking, baud rate. Deferred to the future device-side story, once ESP-IDF and a board exist to validate a real implementation against (A2).
- **ESP32-side C++ code that sends bytes over USB-CDC**, and any live/streaming capture from an actual serial port. This story only decodes files already on disk.
- **Partial / dirty-tile dumps.** Full frame only (A3) — no bandwidth motive for a 38,400-byte debug dump.
- **Multi-frame sequences, animation playback, or diffing two dumps against each other.** No consumer yet; single static frame is the whole ask.
- **Palette customization** (CLI flag, config file, alternate palettes). Fixed default only this cycle (A4).
- **Interactive/GUI viewer** (a live-refreshing window). File-in, PNG-out only.
- **Any change to the rendering-core public API** (`Framebuffer`, `Sprite`, `DirtyTracker`). This story only reads an existing instance; it adds no drawing primitive.
- **Third-party Python packages of any kind** (confirmed — A5/C8), including image libraries (Pillow), CLI-argument-parsing helpers, and test frameworks beyond what's already used in the repo.
- **A "golden image" / regression-comparison workflow** (saving a reference PNG and flagging future diffs). This story produces one PNG per run; comparing two runs is a future story if ever needed.
- **ILI9488 driver, `board_config.h`, GPIO assignment.** Unchanged from rendering-core's cut — display still unwired, ESP-IDF still absent.

## 7. Clarifications

| # | Date | Question | Resolution |
|---|---|---|---|
| C1 | 2026-09-01 | Does the wire/framing protocol (sync bytes, checksum, baud rate) belong in this story? | **No.** Only a file format is defined; framing is deferred to the device-side story, which will have an actual sender and receiver to validate against. → A2, §6. |
| C2 | 2026-09-01 | Does the dump format need to support partial/dirty-tile dumps, matching the eventual device driver? | **No.** Full frame only — a dev tool has no bandwidth constraint; only decode correctness matters. → A3, §6. |
| C3 | 2026-09-01 | Is a hand-built synthetic Python-side fixture enough, or does the decoder need proof against real engine output? | **Real engine output, via a host test that draws through the actual API and checks in its dump as a fixture** (US-2). A prose-only cross-language contract is exactly how a subtle mismatch survives both sides' own tests. → US-2, US-4. |
| C4 | 2026-09-01 | Who decides the exact RGB hex values for the four palette colours? | **The Designer, at `/look-and-feel`** (this is the first story with human-visible output) — this spec only pins the functional constraint (distinct, brightness-ordered). → A4. |
| C5 | 2026-09-01 | Does the dump-writer need to honour the zero-dynamic-allocation rule that binds engine code? | **No — it's host-only tooling, not shipped firmware,** exempt the same way existing `test`/`bench` files already are. → A6. |
| C6 | 2026-09-01 | Should the decoder hard-code 240×160? | **No — it reads width/height from the dump header**, so a future Phase-4 resolution change needs no decoder change. → A7, AC-1.1. |
| C7 | 2026-09-01 | Confirm: palette hex values deferred to `/look-and-feel`, not fixed in this spec? | **Confirmed by the user.** §8 stays open for the Designer; no hex values added here. → A4. |
| C8 | 2026-09-01 | Confirm: Python tool is standard-library only — is a third-party package (e.g. Pillow) permitted? | **Confirmed: standard library only, no `pip install`, Pillow explicitly excluded.** PNG's compressed stream is hand-built with stdlib `zlib` (an uncompressed/"stored" DEFLATE block is sufficient). → A5, NFR-4, §6. |
| C9 | 2026-09-01 | Should the format state byte order (endianness) explicitly, given it's meant to outlive this host? | **Yes — little-endian, stated explicitly in the format doc**, even though this host and the ESP32-S3 target both already are, so the contract doesn't quietly depend on that coincidence. → A8, AC-1.7. |
| C10 | 2026-09-01 | Where does the C++ fixture live, and how does the Python suite find it — a hard-coded relative path risks "works on my machine" drift between two toolchains. | **A single documented path, referenced by both the writer test and the reader test — never duplicated.** The literal path is a `/sprint-plan` decision; this spec only mandates the single-source-of-truth contract. → A9, AC-2.5. |
| C11 | 2026-09-01 | What exactly happens on a decode error — stream, exit-code convention, partial vs. no output? | **Atomic and stderr-reported:** a rejected file never produces a PNG (valid or partial), the message goes to stderr, exit is non-zero; the exact numeric code is a `/sprint-plan` detail, same posture as rendering-core's AC-1.2. → A10, AC-3.3/3.4. |
| C12 | 2026-09-01 | Can a corrupted header claiming an enormous frame force a large allocation from a tiny file? | **No — the file's actual byte size is checked against the declared dimensions before any width×height buffer is allocated.** → A11, AC-1.8. |
| C13 | 2026-09-01 | Is the CLI's argument/flag shape specified now, or left for `/sprint-plan`? | **Left to `/sprint-plan.`** This spec fixes only the two required inputs (dump path, output path) and the defined failure states — not flag syntax. → A12. |
| C14 | 2026-09-01 | Does an existing file at the output path get overwritten, and does the PNG need to be size-optimized? | **Silently overwritten (standard CLI convention); correctness required, not compactness** — an uncompressed/"stored" DEFLATE stream inside a spec-valid PNG is sufficient, consistent with C8's stdlib-only constraint. → A13, AC-3.1. |

## 8. Design Review

- **Overall impression:** Good. This is a headless developer tool, not a product screen — its only human-visible artifact is a PNG a single developer opens locally. Every heuristic concern a design review would normally raise here (status reporting, error clarity, safe-by-default overwrite behaviour) is already resolved by the spec's own ACs/NFRs, not left to this review. The one item this story deliberately deferred — the 4-colour palette's RGB values (C7/A4) — is resolved below, together with one adjacent decision (BLACK's exact value) the palette choice forces, and one out-of-scope observation flagged back to the PO as a question, not a required change. No blocking design risk found.

- **Palette decision (resolves C7/A4, binds AC-3.2):**

  | Colour | Hex | RGB | HSL |
  |---|---|---|---|
  | `BLACK` | `#000000` | (0, 0, 0) | H — S 0% L 0% |
  | `DARK_ORANGE` | `#4D2600` | (77, 38, 0) | H 30° S 100% L 15% |
  | `ORANGE` | `#B35900` | (179, 89, 0) | H 30° S 100% L 35% |
  | `BRIGHT_ORANGE` | `#FF9933` | (255, 153, 51) | H 30° S 100% L 60% |

  **Reasoning:** all three orange steps share the *same* hue (30°, standard
  orange) and the *same* saturation (100%); only lightness moves (15% → 35% →
  60%). That is a direct, literal reading of the constitution's "orange on
  black, 4 colours" (§6) and the README's "monochrome orange/schwarze Grafik"
  — one hue family, not three unrelated oranges, so the palette reads as a
  single coherent "steampunk amber" ramp rather than an arbitrary set of
  swatches. `BRIGHT_ORANGE` is pushed to L 60% (past the L 50%/pure-hue point)
  rather than stopping at maximum saturation, so it reads as a warm glow
  toward white — consistent with the README's planned CRT phosphor/bloom
  language — rather than just "a more saturated orange."

  Checked against the practical requirement this review was asked to verify
  (AC-3.2's "pairwise distinct," and the task's "tellable apart at a glance in
  a debugging PNG," not just "looks orangey"): relative luminance
  (`0.2126R + 0.7152G + 0.0722B`, 0–255 scale) is 0 / 43.6 / 101.7 / 167.3 for
  the four colours in order — monotonic, with the closest adjacent gap (BLACK
  → DARK_ORANGE) still ~44 points wide on a 255-point scale, several times any
  reasonable just-noticeable-difference threshold. A developer scanning a
  debug PNG for "which of the 4 shades is this pixel" can rely on brightness
  alone; hue does not have to be disambiguated because it never changes.

  **BLACK = pure `#000000`, not a lifted near-black:** considered and
  rejected the "off-black so I can tell 'touched' from 'unset'" alternative
  named in the task brief. It doesn't apply to this data model: per AC-1.5,
  the decoder rejects any pixel byte outside 0–3 before it reaches the
  palette map, and per A3/§6 this is always a *full*-frame dump — every byte
  in a valid file is a deliberate palette index, written by an explicit
  `clear`/`fillRect`/`setPixel`/`blit` call (AC-2.2). There is no "uninitialized
  memory rendered as a colour" case for this format to distinguish; index 0
  *is* the intentional background value. Pure black also matches the literal
  "Orange auf Schwarz" / "schwarzer Hintergrund" language in the README's
  *Farbmodell* section and gives DARK_ORANGE the largest possible headroom
  above the floor (44 luminance points) rather than starting the ramp already
  compressed against a lifted floor.

- **Heuristics findings:** None new — checked and already resolved elsewhere,
  not restated as findings: visibility of status (success path prints the
  output path to stdout, AC-3.6/NFR-7), error recovery (every rejection names
  the specific problem in plain language on stderr, AC-3.3/AC-3.4/NFR-7),
  consistency & standards (silent overwrite of an existing output file,
  non-zero exit + usage text on bad/missing args — both standard CLI
  convention, A13/AC-3.4), error prevention (malformed input is rejected
  before any pixel is decoded and before any width×height buffer is
  allocated, AC-1.8/A11/NFR-2).

- **Accessibility notes:** N/A, confirmed (agrees with NFR-9's reasoning, not
  restated as a new finding) — the output is a static PNG opened locally by
  its one author in their own image viewer; there is no interactive control,
  keyboard path, or focus order to audit, so WCAG contrast/keyboard/focus
  checks don't apply to this artifact. The one accessibility-*adjacent*
  property that does genuinely apply — can the four shades be told apart at a
  glance — is covered under the palette decision above as a visual-craft
  requirement (AC-3.2), not a WCAG obligation.

- **Design risks & required changes:** None required for this spec to
  proceed to `/sprint-plan`. One observation raised as a question back to the
  PO, not a required change (per the hard rule against inventing scope): the
  spec fixes PNG output dimensions to exactly the dump's declared width ×
  height (AC-3.1) — i.e., 240×160 pixels, unscaled. At that size most image
  viewers (Preview, VS Code) render the file physically tiny at 100% zoom,
  and a developer will need to zoom in manually to inspect individual pixels;
  most viewers' zoom is not guaranteed nearest-neighbour, so zoomed inspection
  may show interpolated edges between palette steps rather than crisp pixel
  boundaries. This is not a defect in what AC-3.1 specifies (unscaled output
  is the correct contract for a file format whose future consumer, per US-1,
  is code — not a human eyeball, by default) and is explicitly out of this
  story's asked-for scope. If tighter pixel-level legibility is wanted, it
  would be a *new*, separate capability (e.g. an optional integer-upscale
  flag on the CLI) for the PO to scope into a future story — not something
  this review adds unilaterally.

---

## ✅ SPEC GATE

*All boxes checked → `/sprint-plan` may start. Any box open → back to `/story-time` or `/look-and-feel`.*

- [x] Problem, goal and success signal are concrete (no buzzwords, no "everyone")
- [x] Every story has testable Given/When/Then acceptance criteria
- [x] Stories are prioritized (MoSCoW) and at least one is a Must
- [x] Non-functional requirements are stated and measurable (or marked N/A with reason)
- [x] Clarify pass done: full taxonomy swept across two rounds; C1–C6 then C7–C14 resolved (endianness, decode-error conventions, fixture-path contract, allocation-bound guard, CLI-syntax deferral, overwrite/compactness — all folded in)
- [x] Open questions are resolved or explicitly accepted as risk — 0 open
- [x] Out-of-scope section is filled (something was consciously cut)
- [x] Constitution (`.spark/constitution.md`) respected — advances §4/§8's named verification method; no conflict found
- [x] Design review done for UI-facing features (or marked N/A with reason) — completed via /look-and-feel: palette hex values fixed (§8), optional --scale flag surfaced and declined by the user, not added to scope
- [x] Line budget respected: Ist 182 / Soll ~250 (excluding HTML comments)
- [x] Status set to `approved` by the user — approved 2026-09-01
