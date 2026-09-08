#pragma once

#include <cstdint>

#include "steamcore/collision.h"
#include "steamcore/font.h"
#include "steamcore/framebuffer.h"
#include "steamcore/highscore.h"
#include "steamcore/initials_entry.h"

// The two Highscore System screens (spec US-2/US-3) -- stateless free
// functions composing the shipped `drawText` only (`title_screen.h`'s own
// precedent), `BRIGHT_ORANGE` on `BLACK`, neither clearing the
// framebuffer (the caller does, exactly as `GalacticInvasion::render()`
// already does for its own screens).
//
// Example:
//   steamcore::InitialsEntry entry;
//   entry.begin();
//   steamcore::Framebuffer fb;
//   fb.clear(steamcore::Color::BLACK);
//   steamcore::drawInitialsEntryScreen(fb, 12500, entry);
//   // ... later, once a table exists:
//   fb.clear(steamcore::Color::BLACK);
//   steamcore::drawHighscoreTableScreen(fb, "GALACTIC INVASION", table);
//
// Contract:
//  - `drawInitialsEntryScreen()` draws the header, `score` (centred in
//    the worst-case-width field, see below), the three current letters
//    from `entry`, and an underline beneath whichever position
//    `entry.cursor()` names (NFR-7/C13) -- never any outcome text.
//  - `drawHighscoreTableScreen()` draws `displayName` as a header, then
//    exactly `count(table)` ranked rows -- no placeholder row for an
//    empty slot (AC-3.2).
//  - Neither function clears the framebuffer or reads/writes anything
//    beyond its own parameters; both are pure functions of their
//    arguments (deterministic, NFR-3).
//
// Layout convention (CLAUDE.md "derive on-screen layout from font
// metrics, never a literal", extended here for two situations this
// codebase has not hit before, per the highscore-system Design Review):
//  - Every element sits on its own row, `rowY(index) = index * kGlyphHeight`
//    -- disjointness between rows is proved *once*, generally, by three
//    static_asserts per screen (row indices strictly increasing; the
//    row pitch is at least one glyph tall; the last possible row is
//    fully on-screen), never by restating a pairwise `!overlaps()` per
//    row pair (Design Review R3) -- a scalable proof that stays correct
//    however many rows a screen ends up needing.
//  - The score line is the first *runtime-variable-width* text this
//    codebase has ever rendered (every prior screen's text was a
//    compile-time-fixed literal or a zero-padded fixed-width HUD value).
//    Its field rect is sized to the *worst case* (`kMaxScoreGlyphs`
//    digits, A14) and proven on-screen at compile time; the actual,
//    usually-shorter string is centred *within* that already-proven-safe
//    field at runtime (Design Review R2) -- never positioned from a
//    literal, and never assumed to be the worst-case width itself.
namespace steamcore {

inline constexpr int32_t rowY(int32_t rowIndex) { return rowIndex * kGlyphHeight; }

// A14: up to 5 digits (0-99999), unpadded -- the same bound `highscore.h`'s
// own `kMaxStoredScore` already enforces on every stored value.
inline constexpr int32_t kMaxScoreGlyphs = 5;

static_assert(kTableSize <= 9,
              "table rank digits are rendered as a single character 1-9");

// --- ENTRY screen (spec AC-2.1, US-2) ---
// Top to bottom: header, score, three letters, the active-letter
// underline (NFR-7/C13) beneath whichever position is active.
inline constexpr char kEntryHeaderText[] = "HIGH SCORE";

inline constexpr int32_t kEntryHeaderRow = 3;
inline constexpr int32_t kEntryScoreRow = kEntryHeaderRow + 1;
inline constexpr int32_t kEntryLettersRow = kEntryScoreRow + 1;
inline constexpr int32_t kEntryUnderlineRow = kEntryLettersRow + 1;

inline constexpr int32_t kEntryHeaderWidth =
    static_cast<int32_t>(sizeof(kEntryHeaderText) - 1) * kGlyphAdvance;
inline constexpr int32_t kEntryHeaderX =
    (Framebuffer::width() - kEntryHeaderWidth) / 2;

inline constexpr int32_t kScoreFieldWidth = kMaxScoreGlyphs * kGlyphAdvance;
inline constexpr int32_t kScoreFieldX =
    (Framebuffer::width() - kScoreFieldWidth) / 2;

inline constexpr int32_t kLettersWidth = kInitialsCount * kGlyphAdvance;
inline constexpr int32_t kLettersX = (Framebuffer::width() - kLettersWidth) / 2;

// Exported layout rects (galactic_invasion.h's own convention) -- a test
// (or a future caller) never has to restate a coordinate this file
// already computed. The underline row's rect is the full letters-row
// width, a conservative over-approximation for the compile-time proof;
// the actual underline is one glyph cell within it.
inline constexpr Entity kEntryHeaderBounds{kEntryHeaderX, rowY(kEntryHeaderRow),
                                            kEntryHeaderWidth, kGlyphHeight};
inline constexpr Entity kEntryScoreBounds{kScoreFieldX, rowY(kEntryScoreRow),
                                           kScoreFieldWidth, kGlyphHeight};
inline constexpr Entity kEntryLettersBounds{kLettersX, rowY(kEntryLettersRow),
                                             kLettersWidth, kGlyphHeight};
inline constexpr Entity kEntryUnderlineBounds{kLettersX,
                                               rowY(kEntryUnderlineRow),
                                               kLettersWidth, kGlyphHeight};

static_assert(kEntryHeaderRow < kEntryScoreRow &&
                  kEntryScoreRow < kEntryLettersRow &&
                  kEntryLettersRow < kEntryUnderlineRow,
              "ENTRY screen rows must be strictly increasing");
static_assert(rowY(kEntryUnderlineRow) + kGlyphHeight <= Framebuffer::height(),
              "ENTRY screen's last row must be fully on-screen");
static_assert(kEntryHeaderX >= 0 &&
                  kEntryHeaderX + kEntryHeaderWidth <= Framebuffer::width(),
              "kEntryHeaderBounds must be fully on-screen");
static_assert(kScoreFieldX >= 0 &&
                  kScoreFieldX + kScoreFieldWidth <= Framebuffer::width(),
              "kEntryScoreBounds (worst-case width) must be fully on-screen");
static_assert(kLettersX >= 0 &&
                  kLettersX + kLettersWidth <= Framebuffer::width(),
              "kEntryLettersBounds must be fully on-screen");

// Spec AC-2.1/C12: `score` is drawn centred within `kEntryScoreBounds`
// (the worst-case field, Design Review R2); the three letters and the
// active-position underline come from `entry` directly (`entry.letter(i)`/
// `entry.cursor()`) -- no outcome (WIN/LOSS) text is ever drawn here.
void drawInitialsEntryScreen(Framebuffer& fb, int32_t score,
                              const InitialsEntry& entry);

// --- TABLE screen (spec AC-3.1/AC-3.2, US-3) ---
// Top to bottom: the per-game display-name header, then one row per
// occupied rank (AC-3.2: no placeholder row for an empty slot).
inline constexpr char kNameGlyphsExample[] = "GALACTIC INVASION";  // longest known display name today
inline constexpr int32_t kMaxNameGlyphs =
    static_cast<int32_t>(sizeof(kNameGlyphsExample) - 1);

// "N. XXX 12345": a 1-digit rank, ". ", kInitialsCount letters, " ", up to
// kMaxScoreGlyphs unpadded score digits.
inline constexpr int32_t kMaxRowGlyphs =
    1 + 2 + kInitialsCount + 1 + kMaxScoreGlyphs;

inline constexpr int32_t kTableHeaderRow = 2;
inline constexpr int32_t kTableFirstEntryRow = kTableHeaderRow + 1;

inline constexpr int32_t kNameWidth = kMaxNameGlyphs * kGlyphAdvance;
inline constexpr int32_t kNameX = (Framebuffer::width() - kNameWidth) / 2;
inline constexpr int32_t kRowWidth = kMaxRowGlyphs * kGlyphAdvance;
inline constexpr int32_t kRowX = (Framebuffer::width() - kRowWidth) / 2;

inline constexpr Entity kTableHeaderBounds{kNameX, rowY(kTableHeaderRow),
                                            kNameWidth, kGlyphHeight};

// The bounding rect of table row `rankIndex` (0-based, [0, kTableSize)).
inline constexpr Entity tableRowBounds(int32_t rankIndex) {
  return Entity{kRowX, rowY(kTableFirstEntryRow + rankIndex), kRowWidth,
                kGlyphHeight};
}

static_assert(kTableHeaderRow < kTableFirstEntryRow,
              "TABLE screen's header must sit above its first entry row");
static_assert(rowY(kTableFirstEntryRow + kTableSize - 1) + kGlyphHeight <=
                  Framebuffer::height(),
              "TABLE screen's last possible row must be fully on-screen");
static_assert(kNameX >= 0 && kNameX + kNameWidth <= Framebuffer::width(),
              "kTableHeaderBounds must be fully on-screen");
static_assert(kRowX >= 0 && kRowX + kRowWidth <= Framebuffer::width(),
              "every table row (worst-case width) must be fully on-screen");

// Spec AC-3.1/AC-3.2: `displayName` (a caller-supplied, compile-time-fixed
// literal, at most `kMaxNameGlyphs` characters) as a header, then exactly
// `count(table)` rows -- no placeholder for an empty slot.
void drawHighscoreTableScreen(Framebuffer& fb, const char* displayName,
                               const HighscoreTable& table);

}  // namespace steamcore
