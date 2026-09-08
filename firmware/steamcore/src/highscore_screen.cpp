#include "steamcore/highscore_screen.h"

#include "steamcore/color.h"
#include "steamcore/text.h"

namespace steamcore {

namespace {

// Unpadded digit formatter -- a `char` buffer and a digit loop, never
// `<cstdio>`/`snprintf` (galactic_invasion.cpp's own `formatScoreText`
// precedent), never dynamic allocation. Clamps to `kMaxStoredScore` and
// to non-negative, the same posture every other formatter in this
// codebase takes toward out-of-range input. Returns the number of digits
// written (always in [1, kMaxScoreGlyphs]).
int32_t formatScoreDigits(char (&buf)[kMaxScoreGlyphs + 1], int32_t score) {
  int32_t value = score;
  if (value < 0) value = 0;
  if (value > kMaxStoredScore) value = kMaxStoredScore;

  char digits[kMaxScoreGlyphs];
  int32_t count = 0;
  if (value == 0) {
    digits[count++] = '0';
  } else {
    while (value > 0 && count < kMaxScoreGlyphs) {
      digits[count++] = static_cast<char>('0' + (value % 10));
      value /= 10;
    }
  }
  for (int32_t i = 0; i < count; ++i) buf[i] = digits[count - 1 - i];
  buf[count] = '\0';
  return count;
}

}  // namespace

void drawInitialsEntryScreen(Framebuffer& fb, int32_t score,
                              const InitialsEntry& entry) {
  drawText(fb, kEntryHeaderX, rowY(kEntryHeaderRow), kEntryHeaderText,
           Color::BRIGHT_ORANGE);

  char scoreText[kMaxScoreGlyphs + 1];
  const int32_t glyphs = formatScoreDigits(scoreText, score);
  // Design Review R2: centre the actual (usually shorter) string within
  // the worst-case field already proven on-screen at compile time --
  // never assume the runtime string is the worst-case width itself.
  const int32_t scoreX =
      kScoreFieldX + (kScoreFieldWidth - glyphs * kGlyphAdvance) / 2;
  drawText(fb, scoreX, rowY(kEntryScoreRow), scoreText, Color::BRIGHT_ORANGE);

  char lettersText[kInitialsCount + 1];
  for (int32_t i = 0; i < kInitialsCount; ++i) {
    lettersText[i] = entry.letter(i);
  }
  lettersText[kInitialsCount] = '\0';
  drawText(fb, kLettersX, rowY(kEntryLettersRow), lettersText,
           Color::BRIGHT_ORANGE);

  const int32_t cursor = entry.cursor();
  if (cursor >= 0 && cursor < kInitialsCount) {
    const int32_t underlineX = kLettersX + cursor * kGlyphAdvance;
    drawText(fb, underlineX, rowY(kEntryUnderlineRow), "-",
             Color::BRIGHT_ORANGE);
  }
}

namespace {

// The number of characters in `name` up to but not including a
// terminating '\0', clamped to `kMaxNameGlyphs` -- `displayName` is
// documented as a caller-supplied literal of at most `kMaxNameGlyphs`
// characters (highscore_screen.h), so the clamp is defence against a
// caller violating that precondition, never an expected path. A nullptr
// `name` counts as zero glyphs, matching `drawText`'s own documented
// nullptr-is-a-no-op contract (text.h) -- before this centring existed,
// `drawHighscoreTableScreen` inherited that tolerance for free by
// forwarding straight to `drawText` (peer-review F11).
int32_t nameGlyphCount(const char* name) {
  if (name == nullptr) return 0;
  int32_t n = 0;
  while (name[n] != '\0' && n < kMaxNameGlyphs) ++n;
  return n;
}

// "N. XXX 12345" -- rank digit, ". ", initials, " ", unpadded score.
void formatRow(char (&buf)[kMaxRowGlyphs + 1], int32_t rankIndex,
               const HighscoreEntry& entry) {
  int32_t pos = 0;
  buf[pos++] = static_cast<char>('1' + rankIndex);
  buf[pos++] = '.';
  buf[pos++] = ' ';
  for (int32_t k = 0; k < kInitialsCount; ++k) buf[pos++] = entry.initials[k];
  buf[pos++] = ' ';
  char scoreDigits[kMaxScoreGlyphs + 1];
  const int32_t glyphs = formatScoreDigits(scoreDigits, entry.score);
  for (int32_t k = 0; k < glyphs; ++k) buf[pos++] = scoreDigits[k];
  buf[pos] = '\0';
}

}  // namespace

void drawHighscoreTableScreen(Framebuffer& fb, const char* displayName,
                               const HighscoreTable& table) {
  // Design Review R2/plan §1 Decision 8: centre the actual display name
  // within the worst-case field already proven on-screen at compile time
  // -- the same treatment drawInitialsEntryScreen already gives the score
  // line, never assuming the runtime string is the worst-case width
  // itself (peer-review F2).
  const int32_t nameGlyphs = nameGlyphCount(displayName);
  const int32_t nameX = kNameX + (kNameWidth - nameGlyphs * kGlyphAdvance) / 2;
  drawText(fb, nameX, rowY(kTableHeaderRow), displayName, Color::BRIGHT_ORANGE);

  const int32_t n = count(table);
  for (int32_t i = 0; i < n; ++i) {
    char row[kMaxRowGlyphs + 1];
    formatRow(row, i, table.entries[i]);
    const Entity bounds = tableRowBounds(i);
    drawText(fb, bounds.x, bounds.y, row, Color::BRIGHT_ORANGE);
  }
}

}  // namespace steamcore
