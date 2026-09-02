# Constitution: BRASS-01 / SteamCore

| | |
|---|---|
| **Scope** | Project-wide — binds every SPARK phase and every feature |
| **Owner** | The user (amended via `/charter`) |
| **Status** | `active` |
| **Date** | 2026-09-01 |

<!-- Greenfield note: at the time of writing the repo contains only README.md and .gitignore.
     Every technical entry below is therefore a forward-binding commitment, not an observation of
     existing code. The first increments must match it, or this document gets amended — not ignored. -->

## 1. Product Principles

Priority order; the earlier principle wins a tie.

1. **Atmosphere beats capability.** The ESP32-S3 can do far more than BRASS-01
   asks of it. A feature that makes the console look or feel more modern is
   rejected even when the hardware could carry it easily.
2. **The limits are the product, not an obstacle.** The 4-colour palette, the
   240×160 virtual resolution and the handful of buttons are design decisions.
   No feature may raise a stated limit to make itself easier to build.
3. **Own platform, not an emulator.** Nothing is built for cycle-accuracy or
   for compatibility with a historical console. "Would a 1983 machine have done
   it this way" is a style question here, never a correctness one.
4. **Arcade immediacy.** A game is playable within seconds of START: no
   tutorial, no nested menus, no text the player must read to begin. Short
   sessions, visible score, restart always one button away.
5. **The console is offline.** No network, cloud or online feature is in scope.
   Everything works with the device on a table and nothing else attached.

## 2. Project Profile & Active Lenses

- **Project type(s):** `library` — SteamCore is an engine consumed by game
  modules through a fixed contract: `update(const GameInput&)`, `render(Framebuffer&)`.
  `render` takes the framebuffer as a parameter rather than the README's
  originally sketched `render()` — a deliberate, plan-approved deviation
  (`game-loop` plan §1 Decision 4): binding the `Framebuffer&` once at
  `GameLoop` construction and passing it into every `render` call makes "the
  same framebuffer instance every tick" a structural guarantee rather than
  caller discipline. `onCollision(Entity&, Entity&)`, also per README's
  original "Game Loop" sketch, is **not yet implemented** — no `Entity` type
  exists anywhere in the codebase, and "Collision System" remains its own
  separate, unbuilt item on README's Phase 2 checklist (`game-loop` spec §6
  Out of Scope). It is **not** `website`, `web-app`, `api` or `cli`: the repo
  has no `package.json`, no HTML, no route handlers and no terminal
  entrypoint, and the only product output surface is an SPI-attached TFT
  driven by firmware.
  *(Confirmed by the user. "Library" here means an internal, statically-linked
  API contract — not a published package.)*
- **Characteristics:** none active.
  - `handles-auth`, `handles-payments`, `handles-pii` — no accounts, no payment
    path, no personal data; a highscore holds three initials chosen by the
    player at the cabinet.
  - `is-public` — the device has no network stack in scope (README *Design
    Constraints*: "Online — nicht erforderlich").
  - `has-database` — persistence is highscores and settings in internal flash
    (README *Highscore-System*). No SQL engine, no ORM, no migration files.
  - `is-multilingual` — the on-device UI is single-locale English
    ("PRESS START", "SYSTEM READY"). The German README is documentation, not a
    product locale.
- **Active lenses:**

| Lens | Why it's active (or off) | Enforced in |
|---|---|---|
| `library` | Active — the engine↔game boundary is the project's central contract ("Die Engine soll möglichst wenig Abhängigkeiten zwischen den Spielen erzeugen"). Applies **scoped**: the *Public API surface* and *Contract clarity* checks bind; *semver* and *packaging/tree-shaking* are no-ops for a statically-linked firmware image. | `/story-time`, `/peer-review` |
| `ux` | Off — type-triggered by `web-app`/`website`; this is neither. Its checks are forms, mobile width, hover and `prefers-reduced-motion`, none of which exist here. The on-device UI is governed by §6 instead. | — |
| `seo`, `api`, `cli` | Off — nothing indexable, no HTTP surface, no shipped CLI. The Python viewer under `tools/` is internal dev tooling, not a product surface. | — |
| `security`, `data`, `i18n` | Off — no characteristic above is active. | — |

- **Active-lens load:** 1 lens active. Not elevated.

## 3. Technical Constraints

- **Stack / runtime:** ESP-IDF + C++ on ESP32-S3-N16R8 (16 MB flash, 8 MB
  PSRAM). The ESP32 is the **only** runtime target — there is deliberately no
  desktop or SDL simulator. Host-side test binaries for pure logic are not a
  simulator and are allowed (§4).
- **Language standard: C++17**, one dialect for both the host tests and the
  target build. Supported by Apple clang 14 and by the ESP-IDF GCC alike, so
  code that compiles for the test gate compiles for the device.
- **Virtual resolution:** 240×160, integer ×2 to the 480×320 landscape area of
  the prototype panel. Held as a **compile-time constant**; no literal `240`,
  `160`, `480` or `320` appears outside that definition, so the final 7–8"
  panel can take a different one.
- **Framebuffer:** 1 byte per pixel over a 4-colour palette (`BLACK`,
  `DARK_ORANGE`, `ORANGE`, `BRIGHT_ORANGE`) = 38,400 bytes, plus an equally
  sized comparison buffer for dirty tracking.
- **Display path:** ILI9488 3.5" SPI TFT. Over SPI this controller cannot take
  RGB565 — only 18 bpp / 3 bytes per pixel. A full frame is 480×320×3 =
  460,800 bytes; at 40 MHz that is ~92 ms ≈ 10.9 fps before overhead.
  Therefore: **dirty 16×16 tiles pushed by SPI DMA; never a full-frame push.**
  - **Load-bearing assumption, not yet verified.** The whole tile pipeline rests
    on the 18-bpp-only claim, which cannot be checked from this repo. The panel
    arrives 2026-09-02; confirming the pixel format against the real hardware is
    the **first** thing done with it, before anything is built on top.
- **Open (Phase 4):** the final 7–8" panel is ~800×480. 240×160 scales integer
  to 480×320 (×2) or 720×480 (×3, letterboxed) — neither fills 800×480. Whether
  that panel gets ×3 with letterboxing or its own virtual resolution is
  **deliberately not decided now**; that is exactly why the resolution is a
  compile-time constant.
- **Timing:** game logic runs a fixed, deterministic 60 Hz step. Display output
  is decoupled from it. Logic never reads wall-clock time or frame duration.
- **Memory:** **no dynamic allocation at runtime.** No `new`, `malloc`,
  `std::vector`, `std::string` or equivalent after boot — static storage or
  fixed-size pools only.
- **Games:** statically compiled into the image; each **self-registers into a
  central registry**, and the system menu lists exactly what the registry holds.
  No runtime module loading. The registration *mechanism* is an architecture
  decision for `/sprint-plan`, not a constitutional one.
- **Pin assignment:** every GPIO number lives in one central `board_config.h`.
  No GPIO literal anywhere else. The current values are **placeholders** — the
  display hardware arrives 2026-09-02 and nothing is wired yet.
- **Off-limits:** the XPT2046 touch controller on the panel is deliberately
  unused; no network/online code; no RGB or full-colour rendering path; no
  desktop simulator.

## 4. Quality Bars (Definition of Done defaults)

- **Toolchain reality (as of 2026-09-01):** a **host C++ toolchain is present**
  — Apple clang 14.0.3 (`/usr/bin/clang++`), `/usr/bin/g++`, `/usr/bin/make`,
  Xcode Command Line Tools. **Absent:** ESP-IDF, cmake, ninja; no display is
  wired. So: host-compiled code runs today; the firmware cannot be built,
  flashed or run on hardware. Amend this section the day that changes.
- **Host unit tests are a real gate, today.** Hardware-independent logic
  (timing, collision, sprite blitting into a framebuffer, the registry,
  scoring) is covered by unit tests compiled and run with `clang++`/`make` — no
  cmake, no ESP-IDF, no board required. A Must story whose logic could be tested
  this way and wasn't is not done.
- **Therefore "no ESP-IDF header in logic code" is load-bearing, not tidiness.**
  The moment a logic module includes an ESP-IDF header it drops out of the only
  gate this project currently has. Hardware access stays behind a thin driver
  layer for that reason.
- **Honest status reporting:** no increment is ever reported as built, flashed
  or hardware-verified when it was not. Unverified work says "unverified" and
  names what blocked it.
- **Statically checkable, therefore enforced now:** no dynamic allocation after
  boot; no GPIO literal outside `board_config.h`; no full-frame display push;
  no resolution literal outside the compile-time constant. These are grep-level
  checks and are part of every review regardless of toolchain state.
- **Determinism:** the same input sequence and the same seed must produce the
  same framebuffer sequence. Randomness is seeded explicitly and never drawn
  from time.
- **Rendering verification:** a rendering change is verifiable without a panel
  by dumping the framebuffer over USB-CDC to the Python viewer under `tools/`.
  A change that cannot be shown this way states so.

## 5. Conventions

- **Language:** code, comments, `docs/` and commit messages in **English**.
  `README.md` stays **German**. Chat with the user is German.
- **Structure:** the repository layout follows the README's *Repository-Struktur*
  (`firmware/`, `games/`, `tools/`, `docs/`, `hardware/`, `assets/`). Each game
  is one self-contained directory under `games/` that registers itself.
- **Naming:** `snake_case` file names, `.h`/`.cpp` pairs, engine code under a
  `steamcore` namespace.
- **Commits:** Conventional Commits, one feature branch per `.spark/<feature>`.
- **Audio (convention, not a hard rule):** sound is normally synthesised from
  parameters — square-wave tones, short effects — rather than sampled, per the
  README's *Audio* section. Deviating is allowed; doing it by accident is not.
- **Generated artifacts:** `.aspark-graph/` and `graphify-out/` are disposable,
  rebuildable and never committed (already in `.gitignore`).

## 6. Non-Negotiables

- **The graphics philosophy is binding.** Large pixels, simple sprites, few
  animation frames, clear silhouettes, orange on black, 4 colours. Never RGB
  rainbows, photorealism, 3D, large textures or modern UI animation.
- **No dynamic allocation at runtime.**
- **No full-frame display push.**
- **No GPIO number outside `board_config.h`.**
- **Persisted data carries a format version.** A format change either migrates
  the old data or resets it cleanly — never reads a corrupt highscore or
  settings block as if it were valid.
- **Never claim hardware verification that did not happen.**

## 7. Delivery & Handoff

- **Release mode:** `direct` — solo project, local repository, no remote
  configured.
- **Approver:** n/a (mode is `direct`).
- **Target branch:** `main`.
- **Ticket-reference format:** `none`.
- **Terminal status:** `released`.

## 8. QA Method

- **Browser-observable surface:** `no`. There is no UI a browser can drive: no
  `package.json`, no HTML, no route handlers, no web framework, no terminal
  entrypoint. The product's only output surface is an SPI-attached TFT driven by
  firmware.
- **Substitute verification method:** **framebuffer dump over USB-CDC, decoded
  by the Python viewer under `tools/` into one image per acceptance criterion,
  plus the device's serial log transcript; hardware-independent logic
  additionally covered by host-compiled unit tests.**
  - The **unit-test half is enforceable today** — the host toolchain exists
    (§4), so QA runs and records it now.
  - The **framebuffer half becomes enforceable once the ESP-IDF toolchain and
    the board exist.** Until then QA states, per affected criterion, that it
    could not be captured and why — it does not substitute source reading for it.
- **Coverage is unchanged.** `qa.md` is still written, and every acceptance
  criterion and every NFR that QA owns is still verified and recorded
  individually under its own `AC-`/`NFR-` ID. This declaration changes the
  *method* only. It switches no ceremony off.

---

## Amendments

| Date | Change | Why |
|---|---|---|
| 2026-09-01 | Initial constitution | Project kickoff; hardware and engine decisions taken in the first `/charter` session |
| 2026-09-01 | §8 QA Method declared (surface `no`, framebuffer dump + serial transcript + host unit tests) | User confirmed the method explicitly; the project has no browser-drivable surface |
| 2026-09-01 | §3/§4 sharpened: C++17 fixed; host toolchain (Apple clang 14, `g++`, `make`) recorded as present | Verified this session — host unit tests are a real gate today, not an intention |
| 2026-09-01 | §3: ILI9488 18-bpp claim marked as an unverified load-bearing assumption; 800×480 panel scaling recorded as an open Phase-4 decision | Both underpin the render architecture but cannot be settled from the repo |
| 2026-09-01 | §6: persisted data must carry a format version | User accepted; a corrupt-read highscore block is a silent-failure class worth blocking |
| 2026-09-02 | §2: engine↔game contract corrected — `render()` → `render(Framebuffer&)`; `update(GameInput)` → `update(const GameInput&)`; `onCollision(Entity&, Entity&)` marked not yet implemented | `game-loop` (plan §1 Decision 4, user-approved) shipped `render(Framebuffer&)` so the framebuffer-identity guarantee (AC-2.2) is structural, not caller discipline; the actual `tick()` call is `update(const GameInput&)`, matching the `render` fix's own reasoning rather than leaving the same drift class half-corrected; `game-loop` review F9 flagged §2 as stale against its own "central contract" claim, since `Entity`/`onCollision` remain unbuilt and the constitution implied otherwise |
