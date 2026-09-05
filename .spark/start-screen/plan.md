# Plan: start-screen

| | |
|---|---|
| **Phase** | Plan |
| **Owner** | Engineering Manager (`/sprint-plan`) |
| **Input** | `.spark/start-screen/spec.md` (`approved`) |
| **Status** | `approved` |
| **Date** | 2026-09-04 |

**Handoff**
- **Status:** `approved` — user approved 2026-09-04, including the STEAMCORE wordmark override to §1 Decision 4.
- **Summary:** A3 resolved: the title screen is a **free function in the `drawText` mould** —
  `drawTitleScreen(Framebuffer&, GameState)` in a new `title_screen.{h,cpp}` — that *observes* a `GameState`
  value and draws only for `READY`. `GameState`, `GameSession`, `GameInput` and `GameLoop<Game>` stay
  byte-identical (asserted by `git diff --exit-code` in T5). Both elements — the `"STEAMCORE"` wordmark and the
  `"PRESS START"` prompt — are drawn with the shipped `drawText`; layout is fixed and derived from
  `Framebuffer::width()`/`kGlyphAdvance`/`kGlyphHeight`, with disjointness proven twice (a `static_assert` in the
  header and a runtime intersection test).
- **Open:** `0 tasks not done` — all 12 done, including T12 (hardware-gated, ran successfully — panel already wired since v0.2.0). See §3.
- **Flagged for approval (decided, not asked):** (a) the NFR-5 public-surface reading — now **four** symbols, one
  fewer than the previous draft because the emblem's `Sprite` accessor disappeared with the emblem, §1 Decision 6;
  (b) the logo element is the **`"STEAMCORE"` wordmark rendered with `drawText`** — the *wordmark* was the user's
  call in review (it replaces the wordless gear emblem this plan previously chose); `drawText` rather than
  hand-authored letterform art was mine, argued in §1 Decision 4; (c) US-3 carries one small new capability, an
  SCFB-over-serial dump path `docs/dump-format.md` had scoped out, §1 Decision 8.
- **Binding ruling:** §3 Task Breakdown for current task status; a plan revision after review/QA findings updates §1/§3 in place, never a new section
- **On conflict:** the numbered body below wins for everything except `Status`; log the mismatch as a finding at the next `/peer-review` and proceed — don't stop on it.

## 1. Architecture Decision

- **Context:** Every primitive exists; nothing composes them. Spec §6 forbids touching `GameState`'s enum,
  `GameSession`'s transition rules, `GameInput`'s fields or `GameLoop<Game>`'s call contract — a discipline two
  released features already held. `GameSession::state()` is a public, read-only accessor, so an *external*
  observer needs nothing added to `GameSession` at all. The open questions are therefore only: what shape the new
  type takes, where the two elements sit, and who clears the framebuffer.

- **Decision:**
  1. **A free function, not a type.** `void drawTitleScreen(Framebuffer& fb, GameState state)` in
     `include/steamcore/title_screen.h` + `src/title_screen.cpp`. It holds no state, so it is exactly the shape
     `drawText(Framebuffer&, int32_t, int32_t, const char*, Color)` already established for stateless drawing in
     this codebase. It takes the `GameState` **value**, never a `GameSession&`: the caller writes
     `drawTitleScreen(fb, session_.state())` — literally `game_state.h`'s own doc-comment example, packaged as a
     reusable call instead of inline in every future game. Reading a public accessor is not a change to the type
     read; `GameSession` gains nothing, loses nothing, and is not recompiled into a different shape.
  2. **The phase gate lives inside the function.** `READY` draws both elements; `PLAYING` and `GAME_OVER` draw
     nothing at all (a `switch` on the enum, so `-Wswitch` catches a future state). Encapsulating it here is what
     stops every future game from re-deciding "which phase shows the title" (spec §1 Goal), and makes AC-2.1/2.2
     testable on the function alone, with no `GameSession` in the test.
  3. **No implicit clear, ever.** The function only ever *adds* pixels, and only while `READY` — it never clears
     the framebuffer and never erases itself on exit. This matches `GameLoop`'s shipped contract ("the framebuffer
     is never implicitly cleared between ticks") and AC-1.1's own "Given a framebuffer cleared to BLACK". US-2's
     story text ("never obscured by leftover title pixels") is satisfied by the documented usage — the caller
     clears each frame, exactly as `GameLoop`'s contract already requires of any game that wants a clean frame.
     This is stated in the doc comment (T8) because a naive reading would expect erase-on-exit.
  4. **The logo element is the `"STEAMCORE"` wordmark, drawn with the shipped `drawText`.** One call,
     `drawText(fb, x, y, kWordmarkText, Color::BRIGHT_ORANGE)` with `constexpr char kWordmarkText[] =
     "STEAMCORE"`: nine characters, every one inside the font's 43-character set, `kGlyphAdvance` apart → a
     72 × 8 px element. There is **no bespoke pixel array, no art validator and no new `.rodata`** beyond the
     string itself. `title_screen.cpp` then composes *only* `drawText` — which makes AC-1.3 structural rather
     than reviewed (T7 rule d) and makes the whole screen one ink colour, so AC-1.2's "no `ORANGE`/`DARK_ORANGE`
     pixel anywhere" becomes a clean global assertion instead of one carved around an emblem's body tone (the
     previous draft's emblem quietly contradicted T3's own assertion). The logo therefore inherits exactly the
     legibility posture A6 already accepted for the prompt, rather than inventing a second one.

     This plan's earlier objection — "at 64 px wide, nine letters are ≤7 px each (illegible)" — was about
     squeezing hand-drawn letters into a 64 px-wide `Sprite`. It does not apply: the wordmark takes the width it
     needs (72 px, still 84 px of margin on the 240 px canvas) and every letter is a full 8 × 8 glyph, the same
     size the prompt already ships at. Hand-authored letterforms are rejected below on **cost**, not on width.
  5. **Layout — fixed, derived, disjoint by construction:**

     | Element | Rect `(x, y, w, h)` | Derivation |
     |---|---|---|
     | `"STEAMCORE"` wordmark (the logo element) | `(84, 48, 72, 8)` | `w = (sizeof(kWordmarkText) - 1) * kGlyphAdvance`, never a literal 9; `x = (Framebuffer::width() - w) / 2`; `y = 6 * kGlyphHeight`; `h = kGlyphHeight` |
     | `"PRESS START"` prompt | `(76, 112, 88, 8)` | `w = (sizeof(kPromptText) - 1) * kGlyphAdvance`, never a literal 11; `x = (Framebuffer::width() - w) / 2`; `y = 14 * kGlyphHeight`; `h = kGlyphHeight` |

     The canvas is 20 glyph rows tall (`Framebuffer::height() / kGlyphHeight`): the wordmark sits on row 6, the
     prompt on row 14, so rows 48–55 vs 112–119 leave a **56-row gap** — both centred, both fully on-screen, and
     every `y` is a glyph-grid multiple rather than a free-floating margin. **No resolution literal anywhere**
     (constitution §3). Note for the implementer: `title_screen.{h,cpp}` `#include`s `text.h`, so
     `check_constraints.sh`'s glyph-metric rule auto-extends to them — a bare `8` or `43` fails `make lint`; use
     `kGlyphWidth`/`kGlyphHeight`/`kGlyphAdvance`.
  6. **Recorded interpretation of NFR-5 (flagged for approval).** Four public symbols: `drawTitleScreen`, the POD
     `TitleBounds`, and `kTitleWordmarkBounds`/`kTitlePromptBounds`. It was five while a `titleLogo()` `Sprite`
     accessor existed; the wordmark removed the sprite, so the accessor went with it — the *reading* is unchanged,
     the surface simply got smaller. The two rects are exported rather than restated in the test on purpose: the
     test then guards the values the drawing actually uses, so a layout edit cannot silently break AC-1.6.
     `kWordmarkText` and `kPromptText` stay private — the tests restate both strings, giving an independent check
     of each element's width.
  7. **Disjointness is proven twice:** a `static_assert` in the header (the general four-way separating-axis
     expression, not "wordmark is above prompt"), plus T3's runtime intersection test with its own helper.
  8. **US-3 splits in two, and carries one small new capability (flagged).** `docs/dump-format.md` deliberately
     scoped out "how those bytes travel over a wire", so AC-3.1's "dumped over USB-CDC through the Python viewer"
     has no transport today. T11 adds the minimum one: the harness prints the SCFB bytes as **hex lines between
     two sentinel markers** on the existing USB-Serial/JTAG console, and `tools/scfb_capture.py` (stdlib only —
     `bytes.fromhex`, so no new import and no allowlist edit) turns a captured log into a `.scfb` that the shipped
     `fb_view.py` decodes. This finally makes constitution §8's substitute verification method enforceable on
     device, which §8 itself anticipated. T11 needs no hardware; T12 is the flash-and-look half. Unlike
     `input-driver` T10, **T12 is expected to run**: the panel is wired and proven since v0.2.0, and the harness
     needs no buttons (a synthetic START pulse drives READY → PLAYING).

- **Alternatives considered:**

  | Alternative | Why rejected |
  |---|---|
  | Add a `TITLE`/`ATTRACT` state to `GameState`, or a title flag to `GameSession` | Spec §6 forbids it outright; no consumer game exists to justify the first break of a discipline two released features held. An external reader needs nothing added |
  | `class TitleScreen` with a `render(Framebuffer&, GameState)` member | It would hold zero state — a namespace with extra steps. `drawText` is the codebase's shape for stateless drawing; deviating would itself be an architecture decision |
  | `template <typename Session> class TitleScreen` observing a session | This codebase uses templates only where the seam genuinely varies (`Transmitter`, `Source`, `Game`). One instantiation, no policy to vary, and it would couple the screen to `GameSession` where a plain enum value suffices — and make every test build a session |
  | A `TitleScreenGame` that owns a `GameSession` and *is* the `GameLoop<Game>` consumer | A real game must own its own session; this would force two sessions or an awkward wrapper, and changes what a game composes rather than what it draws |
  | Draw the title from inside `GameLoop` when the session is READY | `GameLoop` knows nothing about `GameSession` and §6 forbids changing its call contract |
  | Erase the two rects on the first non-READY tick (erase-on-exit) | Needs remembered state, and would blank two rectangles of a *playing* game's screen. The clear is the caller's, per `GameLoop`'s shipped contract (Decision 3) |
  | A wordless 64 × 64 hand-authored gear emblem (this plan's previous choice) | Overruled by the user in review, and rightly: a title screen's job is to name the console, and no AC asks for a gear. AC-1.1 names "a title/logo element", singular — the wordmark simply *is* that element, at no reinterpretation cost |
  | Hand-authored letterform art for "STEAMCORE" (e.g. 16 px-tall letters, ~144 × 16) | It *would* be legible, so the objection is cost, not size: ~2,300 `Color` entries of bespoke art re-drawing letters the shipped font already draws, plus an art validator, plus a Mode-B round it is far likelier to fail than glyphs this project has already accepted twice. If a genuinely bigger title is ever wanted, the honest fix is a second, larger font — its own story, reusable by every screen |
  | A small icon *plus* the wordmark as one combined logo element | Reintroduces every cost of bespoke art for pure decoration, and forces reinterpreting AC-1.1's single element and AC-1.6's rect count. The wordmark alone is the logo element |
  | Nine `drawText` calls with manual letter tracking, to make the wordmark look wider | Fake typography: still 8 px tall, so no legibility is gained, and it breaks the one-call-per-element shape that makes T3's pixel-exact proof trivial |
  | Header-only `inline` implementation instead of `.h`/`.cpp` | Header-only here is reserved for templates; non-template drawing functions live in `.cpp` (`text.cpp`, `font.cpp`). Deviating buys nothing |
  | Add the logo to the font atlas as extra (larger) glyphs | Changes `kGlyphCount`, breaks `text-rendering`'s own tests, and abuses a 43-character charset for something that is not part of that charset |
  | Render the prompt from a hand-authored sprite (bigger, more legible than 8×8) | AC-1.2 locks the prompt to `drawText`/the existing font; A6 accepts the legibility risk explicitly |
  | Import `assets/*.png`/`*.ttf` for the logo | §6/A2: no decode pipeline exists; building one is its own story. T7 adds a lint rule so this cannot creep in |
  | Log-only device harness (no panel push), as `input-driver` chose | `input-driver`'s ACs were worded against the serial log; AC-3.1 is worded against a decoded image, and US-3's whole point is the first screen a human actually sees |

- **Consequences:** *Easier* — every Must is proven on `make test`/`test-asan`/`test-gcc` with zero ESP-IDF; the
  screen carries **no bespoke art at all**, so there is no pixel array to validate, nothing that can drift from a
  `Sprite` descriptor, and one ink colour end to end; a future game composes the screen with one call and no new
  type; the dump path T11 adds makes every future rendering feature's on-device QA cheap. *Harder* — the logo is
  the same 8 px type size and the same ink as the prompt, so visual hierarchy rests entirely on position and
  whitespace (T10 judges it, and a bigger title becomes a font story, not a patch here); the caller must clear the
  frame (documented, not enforced); the layout constants are compile-time, so a future per-game title needs a new
  decision (YAGNI now); `app_main.cpp` is overwritten again, so `input-driver`'s still-blocked T10 harness moves
  back into git history (its header stays on disk, so restoring it is a two-line edit).

## 2. Affected Components

Scoped by hand — no tool file was passed with this task, so no blast-radius query was run and none is cited here.

- **New (pure, host-gated):** `firmware/steamcore/include/steamcore/title_screen.h`,
  `firmware/steamcore/src/title_screen.cpp`; tests `title_screen_test.cpp`, `title_screen_session_test.cpp`,
  `title_screen_determinism_test.cpp`, `title_screen_dump_test.cpp`, fixture `title_screen_game.h`, bench
  `bench_title_screen.cpp` (all under `firmware/steamcore/test/`).
- **New (tooling/device):** `tools/scfb_capture.py` + `tools/test_scfb_capture.py`;
  `firmware/system/main/title_screen_harness_game.h`.
- **Modified:** `Makefile` (title dump define, `view`, bench target — `src/*.cpp` and `test/*_test.cpp` are
  globbed, so the engine source and the four test files need no entry); `tools/check_constraints.sh` (T7);
  `docs/host-tests.md`, `docs/device-build.md`, `docs/dump-format.md` (T11 records the wire framing that document
  currently declares out of scope); `firmware/system/main/app_main.cpp` and `CMakeLists.txt` (the explicit `SRCS`
  list needs `title_screen.cpp`, `text.cpp`, `font.cpp`, `dump_format.cpp`).
- **Untouched, by design:** `game_state.{h,cpp}`, `game_loop.h`, `input.h`, `framebuffer.*`, `sprite.h`, `text.*`,
  `font.*`, `color.h`, `config.h`, `board_config.h`, `dirty_tracker.*`, `port/esp32/*` — T5's `git diff
  --exit-code` makes the first three a checked claim, not an intention.
- **New dependencies: none.** No package, no service; the one new *pattern* (an SCFB-over-serial hex dump) is
  justified in §1 Decision 8 and is ~40 lines on each side.

## 3. Task Breakdown

| # | Task | Story | Covers (AC / NFR) | Depends on | Status | Definition of Done |
|---|---|---|---|---|---|---|
| T1 | Walking skeleton: `title_screen.{h,cpp}` end to end, both elements real | US-1, US-2 | AC-1.1, AC-1.2, AC-1.3, AC-2.1 | – | `done` | `title_screen.{h,cpp}` built exactly to spec: `TitleBounds`, `kTitleWordmarkBounds`/`kTitlePromptBounds` derived from `sizeof(...)-1 * kGlyphAdvance`, on-screen + disjointness `static_assert`s, `drawTitleScreen` as a `switch` on `GameState` calling `drawText` twice for READY only. 3 tests (READY draws inside both rects; PLAYING/GAME_OVER draw nothing, `framebuffersEqual` against an untouched buffer). `make test`/`test-asan`/`test-gcc`/`lint` all green (157/157), no Makefile change. | The header declares `TitleBounds`, `kTitleWordmarkBounds`/`kTitlePromptBounds` (derived per §1 Decision 5 — no resolution literal, no bare `8`/`43`, and both widths from `sizeof(...) - 1` rather than a literal character count) and `void drawTitleScreen(Framebuffer&, GameState)`; `static_assert`s pin both rects fully on-screen and mutually disjoint via the four-way separating-axis expression; the `.cpp` draws with exactly two `drawText(..., Color::BRIGHT_ORANGE)` calls — `"STEAMCORE"` then `"PRESS START"` — under a `switch` on `GameState`, and composes nothing else (with no bespoke art there is no placeholder step: the skeleton already is the finished drawing); three tests: READY leaves non-BLACK pixels inside both rects, PLAYING and GAME_OVER each leave a cleared framebuffer byte-identical (`framebuffersEqual`); `make test`, `test-asan`, `test-gcc` and `lint` green with no `Makefile` change — files: firmware/steamcore/include/steamcore/title_screen.h, firmware/steamcore/src/title_screen.cpp, firmware/steamcore/test/title_screen_test.cpp |
| T2 | Wordmark string: font coverage, derived width, single-ink proof | US-1 | AC-1.1, AC-1.4, NFR-2 | T1 | `done` | Added two explicit `static_assert`s pinning both rects' `.w` to their string's derived width (redundant by construction today, guards a future hardcode). Two new tests: every "STEAMCORE" character is a defined glyph via `glyphCharAt`; the READY-rendered framebuffer contains only BLACK/BRIGHT_ORANGE pixels. `make test`/`test-asan`/`test-gcc`/`lint` green (159/159). | `kWordmarkText` is `"STEAMCORE"` and `kTitleWordmarkBounds.width` is `(sizeof(kWordmarkText) - 1) * kGlyphAdvance`, pinned by a `static_assert` so the rect and the string can never disagree; a test walks the string and asserts via `glyphCharAt` that every character is one of the font's `kGlyphCount` defined characters — so no glyph can render as the tofu placeholder, and a future string edit that introduces one fails a test instead of shipping a box on the title screen; a second test asserts the READY-rendered framebuffer contains no pixel that is neither `BLACK` nor `BRIGHT_ORANGE`, which is AC-1.4's "no decoded PNG/TTF byte, no art asset" stated as the positive fact that every lit pixel came from the shipped font atlas; `make test-all` green — files: firmware/steamcore/include/steamcore/title_screen.h, firmware/steamcore/src/title_screen.cpp, firmware/steamcore/test/title_screen_test.cpp |
| T3 | Pixel-exact layout proof, ink colour and rect disjointness | US-1 | AC-1.1, AC-1.2, AC-1.6 | T2 | `done` | 4 new tests: full-buffer pixel-exact match against the test's own `placeExpected` (glyphFor-based, never calling `drawTitleScreen`); every pixel outside both rects is BLACK; both rects' width/x independently recomputed and matched; a test-local `rectsIntersect` helper confirms the two exported rects don't overlap. The "no ORANGE/DARK_ORANGE anywhere" assertion was already covered by T2's single-ink test — noted, not duplicated. `make test`/`test-asan`/`test-gcc`/`lint` green (163/163). | A full-buffer comparison built by the test's **own** placement loop (mirroring `text_fixture_test.cpp`): the test places each glyph of its own restated `"STEAMCORE"` from `(84, 48)` and each glyph of its own restated `"PRESS START"` from `(76, 112)` with its own `kGlyphAdvance` arithmetic in `BRIGHT_ORANGE`, then asserts zero differing pixels against `drawTitleScreen`'s output, reporting the first mismatch coordinate; separate named tests assert every pixel outside both rects is BLACK, that no pixel anywhere is `ORANGE` or `DARK_ORANGE` (AC-1.2's contrast ruling made a failing assertion, not a comment), that the test's independently computed widths and centred `x` equal `kTitleWordmarkBounds` (72, 84) and `kTitlePromptBounds` (88, 76), and that a rect-intersection helper written in the test returns an empty intersection for the two exported rects; `make test-all` green — files: firmware/steamcore/test/title_screen_test.cpp |
| T4 | ASan edge battery on the composed screen and both strings | US-1 | AC-1.5, NFR-2 | T2 | `done` | Both title strings drawn via `drawText` at every extreme x/y combination (±width/height, ±1, 0, INT32_MIN/MAX) on a 7×7 grid — no crash under ASan/UBSan. Second test re-confirms the fixed-position `drawTitleScreen` stays in-bounds and draws nothing for PLAYING, under the sanitizer build specifically. Comment honestly notes this is defense-in-depth now (no bespoke Sprite means the original overstated-stride failure mode is gone). `make test`/`test-asan`/`test-gcc`/`lint` green (165/165). | A test draws both title strings at every extreme — `(-w, -h)`, `(-1, -1)`, `(0, 0)`, `(width()-1, height()-1)`, `(width(), height())`, all four corners, and `INT32_MIN`/`INT32_MAX` on each axis — asserting no out-of-bounds access under `make test-asan` (`-fsanitize=address,undefined -fno-sanitize-recover=all`), and that a fixed-position `drawTitleScreen` writes nothing outside the framebuffer in `READY` and nothing at all in `PLAYING`/`GAME_OVER`; the test comment records honestly that this is now defence in depth rather than the sole guard — with no hand-authored `Sprite` the overstated-`stride` failure mode is gone and `drawText`'s per-glyph clipping is already proven by `text-rendering`, but this battery is the only check that the *composed* call stays in bounds when the layout constants change; `make test-asan` and `make test-all` green — files: firmware/steamcore/test/title_screen_test.cpp |
| T5 | Session composition through `GameLoop`, and the byte-identical proof | US-2 | AC-2.1, AC-2.2, NFR-5 | T1 | `done` | New `title_screen_game.h` (`TitleScreenGame`, owns a real `GameSession`, `update`/`render` matching `game_state.h`'s own doc example) driven through `GameLoop<TitleScreenGame>`. 3 tests: title present every READY tick; wholly absent (byte-identical to an untouched buffer) the tick after a `start` rising edge; stays absent across 20 further ticks with `start` held. `git diff --exit-code HEAD` over `game_state.{h,cpp}`, `game_loop.h`, `input.h`: empty — confirmed byte-identical. `make test`/`test-asan`/`test-gcc`/`lint` green (168/168). | `title_screen_game.h` defines a test-only consumer that owns a real `GameSession`, calls `session_.advance(input, false)` in `update` and `fb.clear(Color::BLACK); drawTitleScreen(fb, session_.state());` in `render`, driven through an unmodified `GameLoop<Game>`; tests assert the title is present on every READY tick, wholly absent (framebuffer all BLACK) on the very next tick after a `start` rising edge, and still absent across ≥20 further ticks with `start` held true (no flicker back); `git diff --exit-code` over `game_state.h`, `game_state.cpp`, `game_loop.h` and `input.h` is recorded showing all four byte-identical to `HEAD`; `make test-all` green — files: firmware/steamcore/test/title_screen_game.h, firmware/steamcore/test/title_screen_session_test.cpp |
| T6 | Determinism: two runs, byte-identical framebuffer sequences | US-2 | AC-2.3, NFR-3 | T5 | `done` | New `title_screen_determinism_test.cpp`: an 8-step fixed sequence (READY, rising edge, held, release, second press) run twice through independent `TitleScreenGame`+`GameLoop` pairs, `framebuffersEqual` checked after every tick, first mismatch reported. Negative control: one run's rising-edge step is flipped, checked immediately after that tick (not at sequence end, since PLAYING/PLAYING re-converges one tick later) — confirms the comparison can actually detect a divergence. `make test`/`test-asan`/`test-gcc`/`lint` green (170/170). | Following `session_replay_fixture.h`'s shape, one fixed `GameInput` sequence (covering READY, the rising edge, held `start`, release and a second press) is run twice against two freshly constructed consumers and framebuffers; `framebuffersEqual` asserts the two buffers agree at **every** step, not only the last, and the test fails loudly with the first mismatching coordinate and step index; a negative control confirms the comparison can fail (one deliberately perturbed step); `make test`, `test-asan` and `test-gcc` green — files: firmware/steamcore/test/title_screen_determinism_test.cpp |
| T7 | Lint rules for this feature's own concerns | US-1, US-2 | AC-1.3, AC-1.4, NFR-2, NFR-3 | T3, T6 | `done` | 5 new rules in `check_constraints.sh`: shared clock/RNG + allocation patterns over the title-screen file set; no `assets/`/`.png`/`.ttf` reference in `title_screen.{h,cpp}`; no `setPixel`/`fillRect`/`blit` in `title_screen.cpp` (drawText-only, structural AC-1.3); presence check for the disjointness `static_assert`, anchored on its message text (not a bare "static_assert exists" check, since the header has several). All 5 demonstrated failing individually against a planted violation, each reverted. `docs/host-tests.md` updated. `make lint`/`make test`/`test-asan`/`test-gcc` green (170/170). | `check_constraints.sh` gains one named start-screen block over the file set `title_screen.h`, `title_screen.cpp`, `title_screen_test.cpp`, `title_screen_session_test.cpp`, `title_screen_determinism_test.cpp`, `title_screen_game.h`, using the existing missing-file-is-a-failure posture: (a) the shared `CLOCK_RNG_PATTERN` (a clock or RNG read here would break AC-2.3 exactly as it would for game-loop/game-state); (b) the shared `SCOPED_ALLOC_PATTERN`, extending the allocation ban into `test/`; (c) no `assets/`, `.png` or `.ttf` reference in the two source files, automating AC-1.4's "no decoded PNG/TTF byte"; (d) no `setPixel(`, `fillRect(` or `blit(` in `title_screen.cpp`, so both elements can only reach the screen through `drawText` and AC-1.3 becomes structural rather than reviewed (tests are excluded — they legitimately build expected buffers with `setPixel` and `blit`); (e) a presence check that the header's disjointness `static_assert` has not been deleted; each of the five is demonstrated failing once against a temporarily planted violation, then reverted with `make lint` and `make test-all` green; `docs/host-tests.md` describes the new block — files: tools/check_constraints.sh, docs/host-tests.md |
| T8 | Contract documentation and the NFR-5 surface audit | US-1, US-2 | NFR-5, NFR-6 | T5, T7 | `done` | Most contract statements were already present from T1; added the one gap (exact strings/bounds named explicitly) and clarified "composes only drawText" (was ambiguously worded around blit). Surface audit: exactly 4 public symbols (`TitleBounds`, `kTitleWordmarkBounds`, `kTitlePromptBounds`, `drawTitleScreen`) match §1 Decision 6's list; `detail::kWordmarkText`/`kPromptText` correctly excluded. §2 Affected Components checked against `git status` — no drift. `docs/host-tests.md` already updated in T7. `make test`/`test-asan`/`test-gcc`/`lint` green (170/170). | The header's doc comment states, each explicitly: that `READY` alone draws the screen and `PLAYING`/`GAME_OVER` draw nothing (A3's resolution); that it **never clears and never erases** — the caller owns the frame, same as `GameLoop`'s shipped contract — and what happens if a caller forgets; that it composes **only `drawText`** (no `blit`, no `setPixel`, no sprite or art of its own); the exact strings `"STEAMCORE"` and `"PRESS START"`, the single ink colour `BRIGHT_ORANGE`, and both bounding rects `(84, 48, 72, 8)` and `(76, 112, 88, 8)` with their glyph-row derivation; and the inherited single-threaded / nothing-throws / no-error-code / no-allocation contract; one usage example in `game_loop.h`/`input.h` style showing `drawTitleScreen(fb, session_.state())` inside a game's `render`; the public surface is audited symbol by symbol against §1 Decision 6's list of four and §2 corrected in place if it drifted; `docs/host-tests.md` states which ACs the host gate covers and which need the board; `make test-all` re-verified green after the doc-only edits — files: firmware/steamcore/include/steamcore/title_screen.h, docs/host-tests.md |
| T9 | Viewable dump fixture + `bench_title_screen` | US-1 | NFR-1 | T3 | `done` | New `TITLE_DUMP`/`clean-title-dump` Makefile wiring (exact `TEXT_DUMP` pattern), `title_screen_dump_test.cpp` writes the READY screen to it, `make view` now also produces `build/title_screen.png` — visually confirmed: STEAMCORE centered above, PRESS START centered below, black elsewhere. `bench_title_screen.cpp` measures 10,000 iterations (single-call timing would be noise-dominated for a 2-string draw) — measured 3.26 µs/call, far under the 16.67ms 60Hz budget. `make view`/`make bench`/`make test-all` green (171/171). | A dump test serializes a READY-rendered framebuffer with `serializeDump` to the path a new `TITLE_DUMP` Makefile variable names (`-DSTEAMCORE_TITLE_DUMP`, exactly the `TEXT_DUMP` pattern, including the `clean-title-dump` prerequisite so a filtered run cannot decode a stale file), and `make view` renders it to `build/title_screen.png`; `bench_title_screen.cpp` measures `drawTitleScreen` in `READY` over ≥10,000 iterations at `-O2` and prints µs/call, and the number is recorded here against the 16.67 ms 60 Hz tick budget (NFR-1's measured half — no new cost model, just the measurement the spec says already exists for the parts); `make view`, `make bench` and `make test-all` green — files: Makefile, firmware/steamcore/test/title_screen_dump_test.cpp, firmware/steamcore/test/bench_title_screen.cpp |
| T10 | Scheduled Mode-B design check of the composed screen (spec C7) | US-1 | AC-1.4, NFR-7 | T9 | `done` | **Run, not just named**, against `build/title_screen.png`. Verdict: **ships as-is, no layout fix required.** No Blocker/Major. One Minor (accepted, not fixed): wordmark and prompt share identical size/weight/ink, so hierarchy rests entirely on reading order + the 56-row gap + "PRESS START"'s idiom-recognition, not visual weight — exactly the risk the plan's own Risk table named and accepted; fixing it would need a second/larger font or bespoke letterforms, explicitly out of scope per §1 Decision 4, so no new story opened either (reviewer's own recommendation). One Nit (not actioned): the prompt is visually wider than the wordmark (11 vs 9 chars in a fixed-advance font) — imperceptible at 16px difference on a 240px canvas. AC-1.6 and constitution §6 both independently confirmed compliant from the image. | The follow-up spec C7 requires is **run, not just named**: a `/look-and-feel` Mode-B pass against `build/title_screen.png` (the artifact T9 produces), judging the composition against constitution §6's "large pixels, simple sprites, clear silhouettes, orange on black, 4 colours" and the layout against AC-1.6's disjointness. With no bespoke art the question is no longer "is this silhouette any good" but "does a 72 × 8 wordmark on glyph row 6 read as a *title* above a same-size prompt on row 14, or does the screen read as two equal lines of text"; every finding is recorded in this row and any Major one is fixed by adjusting the layout constants (row placement, spacing) before this task is `done`, while a finding that could only be fixed by bespoke letterform art or a larger font is recorded as a **new story**, never absorbed here (§1 Decision 4); if the ceremony cannot be run inside this increment, this row is set `blocked` with that reason and named as an explicit **`/go-live` blocker**, never silently dropped — files: firmware/steamcore/include/steamcore/title_screen.h, firmware/steamcore/src/title_screen.cpp |
| T11 | Device harness + SCFB-over-serial capture path (no hardware needed) | US-3 | AC-3.1 | T8 | `done` | New `title_screen_harness_game.h` (real `GameSession`, synthetic `start` pulse after 150 ticks, labeled synthetic) drives `drawTitleScreen` to the real panel via the released `Ili9488Display`/`DirtyTracker`, and dumps SCFB bytes hex-encoded between `SCFB-DUMP-BEGIN`/`SCFB-DUMP-END` markers once per state change. `tools/scfb_capture.py` (stdlib-only, added to the lint allowlist) extracts/validates blocks from a transcript; `tools/test_scfb_capture.py` proves it against noise, a truncated block, a byte-count mismatch, and multiple valid blocks (10 tests, all pass). `CMakeLists.txt` gained `title_screen.cpp`/`text.cpp`/`font.cpp`/`dump_format.cpp`; `gpio_input_source.cpp` dropped (unused by this harness). `idf.py build` green after fixing two latent font.cpp device-build bugs (§ Deviations). `docs/device-build.md` and `docs/dump-format.md` updated with the capture walkthrough and wire framing. `make test`/`test-asan`/`test-gcc`/`test-python`/`lint` all green (171/171 + 10 new Python tests). | `title_screen_harness_game.h` renders the title screen into a real `Framebuffer` (clear + `drawTitleScreen(fb, session_.state())`), pushes dirty tiles with the released `Ili9488Display`/`DirtyTracker` exactly as `ili9488_display.h`'s example shows, injects a documented **synthetic** `start` pulse after N ticks so the READY → PLAYING disappearance is observable without buttons (labelled synthetic in the log, mirroring `input_harness_game.h`), and emits the framebuffer once per state change as `serializeDump` bytes printed in fixed-width hex lines between `SCFB-DUMP-BEGIN`/`SCFB-DUMP-END` markers; `tools/scfb_capture.py` (stdlib only, no new import) extracts the markers from a captured log and writes a `.scfb` that `fb_view.py` decodes unchanged, and `tools/test_scfb_capture.py` proves it on the host against a synthetic log containing log noise, a truncated block and a valid block; `CMakeLists.txt` gains `title_screen.cpp`, `text.cpp`, `font.cpp`, `dump_format.cpp`; **`idf.py build` green**; `docs/device-build.md` and `docs/dump-format.md` record the wire framing and the capture command — files: firmware/system/main/title_screen_harness_game.h, firmware/system/main/app_main.cpp, firmware/system/main/CMakeLists.txt, tools/scfb_capture.py, tools/test_scfb_capture.py, docs/device-build.md, docs/dump-format.md |
| T12 | **Hardware-gated:** flash, capture, decode, human confirmation | US-3 | AC-3.1, AC-3.2 | T11 | `done` | **Ran, not blocked** — panel already wired since v0.2.0, harness needs no buttons. Flashed via `idf.py -p /dev/cu.usbmodem14101 flash`. First capture attempt hit a real bug (recorded below as a Deviation): the ~1,200-line dump loop starved the idle task long enough to trip the task watchdog mid-dump, corrupting the capture — correctly rejected by `scfb_capture.py` rather than mis-decoded (the tool did its job). Fixed with a periodic `vTaskDelay` in `dumpFramebufferOverSerial`, reflashed, recaptured clean. Both dumps decoded via `tools/scfb_capture.py` + `tools/fb_view.py`: READY shows STEAMCORE at (84,48) and PRESS START at (76,112), rest black, byte-exact match to the host-rendered PNG; PLAYING is fully black. **Human confirmed the physical panel independently** (2026-09-04, verbatim: "funktioniert" — matches both decoded dumps and shows the same READY→PLAYING blank-out on the synthetic START pulse). AC-3.1/AC-3.2 verified, not parked. | Expected to be runnable — the panel is wired and proven since v0.2.0 and this harness needs no buttons. When run: flashed per `docs/device-build.md` (manual RESET after flash), the `idf.py monitor` output is captured to a file, `tools/scfb_capture.py` + `fb_view.py` decode it to a PNG, and the human confirms the decoded image shows `STEAMCORE` at `(84, 48)` and `PRESS START` at `(76, 112)`, that the rest is black, and that the panel itself shows the same screen and blanks on the synthetic START; the transcript and the human's verbatim confirmation are recorded in this row. If the board is physically unavailable at increment time, this row is reported `blocked` with that reason and AC-3.1 recorded as **not capturable yet** per AC-3.2 and constitution §8 — never as passed, and never substituted by source reading — files: docs/device-build.md |

## 4. Test Strategy

- **Host-CI-verifiable at `/increment` time (`make test`, `test-asan`, `test-gcc`, `lint`, `bench` — zero
  ESP-IDF):** AC-1.1–AC-1.6 (T1–T4), AC-2.1–AC-2.3 (T5, T6), NFR-1 (T9, measured), NFR-2/NFR-3 (T7),
  NFR-5/NFR-6 (T8), plus `tools/test_scfb_capture.py` for T11's Python half. This is the same class of feature as
  `rendering-core`/`text-rendering`/`game-loop`: all of the Musts are pure rendering logic.
- **US-1** — T3 is the primary proof and it is built the way `text_fixture_test.cpp` was: the expected
  framebuffer comes from the test's **own** placement arithmetic and its own restatement of both strings, never
  from calling the code under test, so a wrong advance, a wrong string or a wrong ink colour cannot agree with
  itself. T4 carries the memory-safety half; T2 carries the wordmark's font coverage and the single-ink
  invariant. AC-1.2's colour is a failing assertion (no `ORANGE`/`DARK_ORANGE` pixel anywhere), not a comment —
  and with both elements drawn in one ink it now holds for the whole framebuffer without exception.
- **US-2** — T5 proves the behaviour where it actually matters, through an unmodified `GameLoop` and a real
  `GameSession`, and the `git diff --exit-code` in the same task is what makes "nothing else changed" checkable
  rather than asserted. T6 is the determinism proof in the shape `game-loop`/`game-state`/`input-driver` all
  used: two runs, byte-identical at every step, with a negative control so the comparison is known to be able to
  fail.
- **US-3** — split (§1 Decision 8): T11 is fully executable with a compiler alone and its Python half is
  host-tested; only T12 needs the board. Nothing here is reported as hardware-verified without a transcript
  (constitution §4/§6).
- **Sanitizers, honestly scoped:** the earlier draft needed ASan to catch an overstated hand-authored `Sprite`
  descriptor. With the wordmark there is no authored sprite, and `drawText`'s per-glyph clipping is already
  covered by `text-rendering`'s suite — so T4 is kept as the guard on the *composed* call and its layout
  constants, not as the feature's main safety argument, and its comment says so rather than overclaiming.
- **Deliberately not automated, with reasons:** the visual hierarchy of the composed screen (T10's human
  ceremony — no test can judge whether a wordmark reads as a title); the font's physical legibility at cabinet
  distance (spec A6, PO-accepted, and AC-3.1's dump cannot simulate viewing distance); `/demo-day` in a browser
  (no browser-observable surface, constitution §8); real GCC (`/usr/bin/g++` is Apple clang — `test-gcc` stays
  honestly reported as nominal).

## 5. Risks & Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| The wordmark is the same 8 px type size and the same ink as the prompt, so the screen may read as two equal lines of text rather than a title above a call to action | Medium: it is the first thing a human ever sees on this console | Hierarchy is carried by position and whitespace: wordmark on glyph row 6, prompt on row 14, a 56-row gap between them, both centred on an otherwise black screen. T10's Mode-B pass judges exactly this with a Major finding blocking `done`, and T12 puts it on the real panel; if it fails, the remedy (letterform art or a second, larger font) is a new story, per §1 Decision 4 — not a mid-increment scope grab |
| The logo now inherits A6's legibility risk: the console's own name is drawn in the same 8 px font whose cabinet-distance legibility the PO accepted only for the prompt | Medium: an unreadable title screen is a bad first impression, and no host test can detect it | This is knowingly the *same* accepted risk, not a new one — same font, same size, same ink as the prompt A6 covers, and 9 characters against that prompt's 11. T12 is a human looking at the real panel at real distance, the only check that can settle it; a failure there is recorded as a finding and a follow-up story |
| A caller forgets to clear the frame, so title pixels survive into PLAYING — the exact failure US-2's story describes | Medium: looks like a bug in this feature, is a bug in the caller | Decision 3 is stated explicitly in the doc comment (T8) with the consequence named, and T5's fixture consumer models the documented usage. Erase-on-exit was considered and rejected with a reason |
| `title_screen.{h,cpp}` include `text.h`, so the glyph-metric lint rule silently extends to them and a bare `8` fails `make lint` late in the task | Low, but confusing when it hits | Called out in §1 Decision 5 and again in T1's DoD: every metric comes from `kGlyph*`, every dimension from `Framebuffer::width()`/`height()`, and both element widths from `sizeof(...) - 1` |
| T11 adds a wire framing `docs/dump-format.md` explicitly declared out of scope — scope creep on a `Should` | Medium | Deliberate and flagged (§1 Decision 8): ~40 lines per side, no new dependency, host-tested, and it makes constitution §8's own substitute method enforceable. US-3 is the first thing cut (spec C6); if cut, T11/T12 go together and the Musts are untouched |
| T11 overwrites `app_main.cpp`, so `input-driver`'s still-blocked T10 harness is no longer one command away | Low-Medium | Same accepted posture as `input-driver` took toward `display-driver`'s harness: harnesses are throwaway, `input_harness_game.h` stays on disk, and restoring it is a two-line `app_main` edit |
| 76,800 hex characters over the serial console is slow or gets truncated by the monitor | Medium: T12 fails for a transport reason, not a rendering one | The dump is emitted once per state change, not per tick; `tools/scfb_capture.py` validates the byte count against the SCFB header and **rejects** a short block rather than decoding a partial frame (proven by T11's host test); the panel push is an independent second confirmation path |
| A future second game wants a different title screen, and the layout is compile-time constant | Low, and deliberate | YAGNI: no second game and no registry exist (spec A1, user-confirmed). Parameterizing now would be the speculative surface NFR-5 forbids |

## Deviations (T11)

- **`font.cpp` had two latent, previously-undiscovered device-build bugs, fixed as part of T11's own "idf.py build green" DoD.** `text.cpp`/`font.cpp` had never been compiled for the ESP32 target before this task (no prior harness composed `drawText`) — `idf.py build` surfaced both immediately:
  1. `glyphPixel`'s compile-time art validator used `throw` to turn a malformed glyph row into a build failure. Host clang/g++ never disable exceptions, so this worked there; ESP-IDF's device build passes `-fno-exceptions`, so the same `throw` is a hard compiler error regardless of whether it is ever actually reached. Fixed by replacing it with a call to a declared, non-`constexpr` function (`reportInvalidGlyphArt`) — calling any non-`constexpr` function during the constant evaluation of `kAtlas` (a `constexpr` global) is *itself* not a constant expression, so the same "bad art fails the build" contract holds without touching exceptions at all. Given a real (never-executed) body rather than left declared-only, since host clang's `-Wundefined-internal` under `-Werror` otherwise flags an internal-linkage function that's declared but never defined.
  2. Several `static_cast<size_t>(...)` call sites relied on `<array>` transitively providing `size_t` — true of the host libc++/libstdc++ this project builds against, not guaranteed of every implementation. Fixed with an explicit `#include <cstddef>`.
  - Both are small, obvious, behavior-preserving corrections (same glyphs, same validation, same compile-time-only failure mode) to already-shipped, released code (`text-rendering`) — not a start-screen scope or architecture change — so fixed directly per the `/increment` "small, obvious correction" rule rather than routed back to `/sprint-plan`. Re-verified: `make test`/`test-asan`/`test-gcc`/`lint` green on host before and after, `idf.py build` green on device after.

- **T12's first real capture attempt was corrupted by a task-watchdog trip, only observable by actually running on hardware.** `dumpFramebufferOverSerial` (`app_main.cpp`) printed all ~1,200 hex lines of one dump back-to-back with no yield, starving `IDLE0` past the default 5 s task-watchdog timeout; the watchdog's own backtrace log landed mid-dump, and `tools/scfb_capture.py` correctly rejected the resulting block as containing non-hex noise rather than mis-decoding it (proof that the "reject a corrupted block" behavior T11's own host tests exercise with synthetic noise also holds against a genuine, unplanned real-world corruption — the tool did its job; the harness had the bug). Fixed with a periodic `vTaskDelay(1)` every 20 lines; reflashed and recaptured clean (both dumps byte-exact against the host-rendered PNGs). Small, obvious, behavior-preserving correction to this task's own new code — not a scope or architecture change — so fixed directly rather than routed back to `/sprint-plan`.

---

## ✅ PLAN GATE

*All boxes checked → `/increment` may start. Any box open → back to `/sprint-plan`.*

- [x] Spec status is `approved` (never plan against a draft)
- [x] Architecture decision includes rejected alternatives (14 recorded, §1)
- [x] Architecture respects the constitution's technical constraints (§3 no dynamic allocation — no container and no runtime buffer anywhere, the only new `.rodata` is two string literals; §3 resolution as a compile-time constant — every coordinate derived from `Framebuffer::width()`/`height()` and `kGlyph*`, zero literals, enforced by the existing lint rules; §3/§4 determinism — no clock or RNG on the render path, newly enforced by T7; §4 no ESP-IDF header in `include/`/`src/` — the device half stays in `firmware/system/main/`; §6 graphics philosophy — 4-colour palette in practice reduced to BLACK plus BRIGHT_ORANGE, two centred text elements, no bespoke art, no animation, no gradient; C++17, `steamcore` namespace, `snake_case`, English) — no conflict found
- [x] Every task maps to a user story — no orphan tasks, no story without tasks
- [x] Every Must AC and every applicable NFR is covered by at least one task (AC-1.1–1.6, AC-2.1–2.3, AC-3.1–3.2; NFR-1–NFR-7; NFR-8/9/10 are N/A per the spec)
- [x] Every task has a checkable definition of done
- [x] Task order respects dependencies (walking skeleton first: T1 puts both real elements, the real layout constants and the phase gate through the whole `Framebuffer` → `drawText` path before any battery, proof or lint rule exists; the hardware-gated work is last and split so its buildable half is not blocked by its unbuildable one)
- [x] Test strategy covers every Must story, and states per AC whether it is host-CI-verifiable or needs the board
- [x] Line budget respected: Ist 228 / Soll ~300 (excluding HTML comments) — 72 under
- [x] Status set to `approved` by the user — 2026-09-04
