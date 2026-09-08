#include "galactic_invasion/galactic_invasion.h"

#include "fb_compare.h"
#include "galactic_invasion_fixture.h"
#include "steamcore/color.h"
#include "steamcore/font.h"
#include "steamcore/game_loop.h"
#include "steamcore/highscore.h"
#include "steamcore/highscore_game.h"
#include "steamcore/highscore_screen.h"
#include "steamcore/round_result.h"
#include "steamcore/sprite.h"
#include "test_harness.h"

using steamcore::Color;
using steamcore::Framebuffer;
using steamcore::GameInput;
using steamcore::GameLoop;
using steamcore::HighscoreGame;
using steamcore::HighscoreTable;
using steamcore::Sprite;
using steamcore::kEntryHeaderRow;
using steamcore::kEntryHeaderText;
using steamcore::kEntryHeaderWidth;
using steamcore::kEntryHeaderX;
using steamcore::kInitialsCount;
using steamcore::glyphFor;
using steamcore::kGlyphAdvance;
using steamcore::kGlyphHeight;
using steamcore::kGlyphWidth;
using steamcore::rowY;
using steamcore::games::GalacticInvasion;
using steamcore::games::kScorePerKill;
using steamcore::test::framebuffersEqual;

namespace {

void enterPlaying(GameLoop<GalacticInvasion>& loop) {
  loop.tick(GameInput{});
  loop.tick(GameInput{/*start=*/true});
}

GameInput fireInput() {
  GameInput input{};
  input.fire = true;
  return input;
}

// Restated independently of galactic_invasion.cpp's own anonymous
// namespace (galactic_invasion_round_test.cpp's own precedent) -- the row
// GalacticInvasion's own GAME_OVER outcome text (GAME OVER / YOU WIN)
// renders on. Used only to prove that row never lights up once the
// wrapper starts intercepting the ending -- never re-derived from
// production code, since NFR-5 exposes no accessor to check it against.
constexpr int32_t kOutcomeTextY = 9 * kGlyphHeight;

// A contiguous run of at least two glyph-advances of BRIGHT_ORANGE in the
// outcome row -- text-shaped, unlike the 2px-wide player projectile that
// legitimately transits this same row mid-flight during ordinary PLAYING
// gameplay (an earlier, simpler "any BRIGHT_ORANGE in the row" version of
// this check false-positived on exactly that).
constexpr int32_t kOutcomeTextRunThreshold = 2 * kGlyphAdvance;

bool outcomeTextRowLit(const Framebuffer& fb) {
  for (int32_t y = kOutcomeTextY; y < kOutcomeTextY + kGlyphHeight; ++y) {
    int32_t run = 0;
    for (int32_t x = 0; x < Framebuffer::width(); ++x) {
      if (fb.pixel(x, y) == Color::BRIGHT_ORANGE) {
        if (++run >= kOutcomeTextRunThreshold) return true;
      } else {
        run = 0;
      }
    }
  }
  return false;
}

// Restated independently of highscore_flow.cpp's own T1 stub layout,
// exact-pixel-match style (title_screen_test.cpp's kExpectedWordmarkText
// precedent) -- row 2*kGlyphHeight coincides with the formation's own
// starting row (kFormationStartY), and any run-length or any-lit-pixel
// heuristic here is unreliable in both directions: the player's own
// BRIGHT_ORANGE projectile transits this row during ordinary flight (a
// false positive for a loose check), while individual glyphs have
// internal gaps that never produce a long contiguous run on a given
// scanline (a false negative for a strict one). An exact match against
// an independently rendered reference is the only check that is neither.
void placeExpected(Framebuffer& fb, int32_t x0, int32_t y0, const char* text,
                    Color ink) {
  int32_t gx = x0;
  for (const char* p = text; *p != '\0'; ++p, gx += kGlyphAdvance) {
    const Sprite glyph = glyphFor(*p);
    for (int32_t row = 0; row < kGlyphHeight; ++row) {
      for (int32_t col = 0; col < kGlyphWidth; ++col) {
        if (glyph.pixels[row * glyph.stride + col] != Color::BLACK) {
          fb.setPixel(gx + col, y0 + row, ink);
        }
      }
    }
  }
}

// T7 replaced T1's own throwaway stub header position with the real
// screen's (highscore_screen.h) -- referencing the real, now-independently
// -tested (highscore_screen_test.cpp) exported constants directly here,
// rather than re-duplicating them a second time, is what this task needs:
// this file only cares whether the flow's real screen appeared at all,
// which screen it is is highscore_screen_test.cpp's own job to verify.
bool entryHeaderShown(const Framebuffer& fb) {
  Framebuffer expected;
  placeExpected(expected, kEntryHeaderX, rowY(kEntryHeaderRow),
                kEntryHeaderText, Color::BRIGHT_ORANGE);
  const int32_t headerY = rowY(kEntryHeaderRow);
  for (int32_t y = headerY; y < headerY + kGlyphHeight; ++y) {
    for (int32_t x = kEntryHeaderX; x < kEntryHeaderX + kEntryHeaderWidth;
         ++x) {
      if (fb.pixel(x, y) != expected.pixel(x, y)) return false;
    }
  }
  return true;
}

// T1's throwaway store double: T4 replaces it with the real
// HighscoreStore<Backend>, which satisfies this same shape (`qualifies`/
// `table`/`record`).
struct StubStore {
  bool alwaysQualifies = true;
  int32_t lastQualifiesScore = -1;
  int32_t qualifiesCalls = 0;
  HighscoreTable fakeTable{};

  bool qualifies(int32_t /*slot*/, int32_t score) {
    lastQualifiesScore = score;
    ++qualifiesCalls;
    return alwaysQualifies && score > 0;
  }
  const HighscoreTable& table(int32_t /*slot*/) const { return fakeTable; }
  bool record(int32_t /*slot*/, const char (&)[kInitialsCount],
              int32_t /*score*/) {
    return true;
  }
};

constexpr int32_t kMaxTicks = 10000;

}  // namespace

// AC-5.2/NFR-4: HighscoreGame<Game, Store> satisfies GameLoop<Game>'s own
// Game concept -- a named compile-time proof, exactly game_loop.h's own
// static_assert precedent for GalacticInvasion itself.
static_assert(
    sizeof(GameLoop<HighscoreGame<GalacticInvasion, StubStore>>) > 0,
    "HighscoreGame<Game, Store> must satisfy GameLoop<Game>'s Game concept");

// AC-2.1/AC-4.1: on a qualifying round, the ending tick renders the
// flow's own header instead of GalacticInvasion's own GAME_OVER screen --
// never a GAME_OVER frame shown first, on this or any later tick.
STEAMCORE_TEST(highscore_game_qualifying_round_shows_the_flow_not_game_over) {
  GalacticInvasion game;
  StubStore store;
  HighscoreGame<GalacticInvasion, StubStore> wrapped(game, store, /*slot=*/0,
                                                      "GALACTIC INVASION");
  Framebuffer fb;
  GameLoop<HighscoreGame<GalacticInvasion, StubStore>> loop(wrapped, fb);

  loop.tick(GameInput{});
  loop.tick(GameInput{/*start=*/true});

  bool everShowedOutcomeText = false;
  bool sawHeader = false;
  for (int32_t i = 0; i < kMaxTicks && !sawHeader; ++i) {
    loop.tick(fireInput());
    if (outcomeTextRowLit(fb)) everShowedOutcomeText = true;
    if (entryHeaderShown(fb)) sawHeader = true;
  }

  CHECK(sawHeader);
  CHECK(!everShowedOutcomeText);
  CHECK(store.lastQualifiesScore > 0);
}

// AC-4.1/US-4: a round that never qualifies (the store never accepts any
// score) renders, tick for tick, exactly what an unwrapped GalacticInvasion
// would have rendered on its own -- the wrapper is completely invisible.
STEAMCORE_TEST(highscore_game_non_qualifying_round_matches_unwrapped_frame) {
  GalacticInvasion plainGame;
  Framebuffer plainFb;
  GameLoop<GalacticInvasion> plainLoop(plainGame, plainFb);
  enterPlaying(plainLoop);

  GalacticInvasion wrappedGame;
  StubStore store;
  store.alwaysQualifies = false;
  HighscoreGame<GalacticInvasion, StubStore> wrapped(wrappedGame, store,
                                                      /*slot=*/0,
                                                      "GALACTIC INVASION");
  Framebuffer wrappedFb;
  GameLoop<HighscoreGame<GalacticInvasion, StubStore>> wrappedLoop(wrapped,
                                                                    wrappedFb);
  wrappedLoop.tick(GameInput{});
  wrappedLoop.tick(GameInput{/*start=*/true});

  for (int32_t i = 0; i < kMaxTicks; ++i) {
    plainLoop.tick(fireInput());
    wrappedLoop.tick(fireInput());
    CHECK(framebuffersEqual(plainFb, wrappedFb));
  }
}

// AC-5.1 ("reported exactly once"): the wrapper's `reported_` latch means
// the store is consulted once per ending, not once per GAME_OVER tick --
// checked here against a counting stub while the round ends without
// qualifying, so the ending stays on the un-latched path for hundreds of
// further ticks. Peer-review F7: without this, deleting the latch broke
// no test (once the flow opens it owns every tick, so the qualifying
// path could never show the difference).
STEAMCORE_TEST(highscore_game_asks_the_store_once_per_ending_not_once_per_tick) {
  GalacticInvasion game;
  StubStore store;
  store.alwaysQualifies = false;
  HighscoreGame<GalacticInvasion, StubStore> wrapped(game, store, /*slot=*/0,
                                                      "GALACTIC INVASION");
  Framebuffer fb;
  GameLoop<HighscoreGame<GalacticInvasion, StubStore>> loop(wrapped, fb);

  loop.tick(GameInput{});
  loop.tick(GameInput{/*start=*/true});

  // Never fire, never move: the round ends with score 0 and stays ended.
  bool ended = false;
  for (int32_t i = 0; i < kMaxTicks && !ended; ++i) {
    loop.tick(GameInput{});
    ended = game.roundResult().ended;
  }
  CHECK(ended);
  CHECK_EQ(store.qualifiesCalls, 1);

  for (int32_t i = 0; i < 200; ++i) loop.tick(GameInput{});
  CHECK(game.roundResult().ended);
  CHECK_EQ(store.qualifiesCalls, 1);
}

// AC-5.1: roundResult() reflects GalacticInvasion's own GameState directly
// -- false while READY/PLAYING, true from the exact tick GAME_OVER is
// reached, stable while GAME_OVER holds, and false again (with the score
// reset) the instant a restart lands back in PLAYING.
STEAMCORE_TEST(galactic_invasion_round_result_reflects_ended_and_score) {
  GalacticInvasion game;
  Framebuffer fb;
  GameLoop<GalacticInvasion> loop(game, fb);

  CHECK(!game.roundResult().ended);

  enterPlaying(loop);
  CHECK(!game.roundResult().ended);

  // Hold fire, never move: reaches column 2's front-row enemy at ~tick 70
  // (galactic_invasion_combat_test.cpp's own derivation), scoring exactly
  // kScorePerKill well before the round later ends.
  for (int32_t i = 0; i < 70; ++i) loop.tick(fireInput());
  CHECK(!game.roundResult().ended);
  CHECK_EQ(game.roundResult().score, kScorePerKill);

  bool ended = false;
  for (int32_t i = 0; i < kMaxTicks && !ended; ++i) {
    loop.tick(fireInput());
    ended = game.roundResult().ended;
  }
  CHECK(ended);
  const int32_t finalScore = game.roundResult().score;
  CHECK(finalScore >= kScorePerKill);

  // A further tick while still GAME_OVER must not change the reported
  // score (the "stable thereafter" contract, round_result.h).
  loop.tick(GameInput{});
  CHECK(game.roundResult().ended);
  CHECK_EQ(game.roundResult().score, finalScore);

  // A start rising edge restarts into a fresh round.
  loop.tick(GameInput{/*start=*/true});
  CHECK(!game.roundResult().ended);
  CHECK_EQ(game.roundResult().score, 0);
}
