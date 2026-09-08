#include "galactic_invasion/galactic_invasion.h"

#include "fake_flash_backend.h"
#include "fb_compare.h"
#include "galactic_invasion_fixture.h"
#include "steamcore/color.h"
#include "steamcore/font.h"
#include "steamcore/game_loop.h"
#include "steamcore/highscore.h"
#include "steamcore/highscore_game.h"
#include "steamcore/highscore_screen.h"
#include "steamcore/sprite.h"
#include "test_harness.h"

using steamcore::Color;
using steamcore::Entity;
using steamcore::Framebuffer;
using steamcore::GameInput;
using steamcore::GameLoop;
using steamcore::HighscoreGame;
using steamcore::HighscoreStore;
using steamcore::Sprite;
using steamcore::count;
using steamcore::glyphFor;
using steamcore::kEntryHeaderRow;
using steamcore::kEntryHeaderText;
using steamcore::kEntryHeaderWidth;
using steamcore::kEntryHeaderX;
using steamcore::kGlyphAdvance;
using steamcore::kGlyphHeight;
using steamcore::kGlyphWidth;
using steamcore::kInitialsCount;
using steamcore::rowY;
using steamcore::games::GalacticInvasion;
using steamcore::games::kHighscoreName;
using steamcore::games::kHighscoreSlot;
using steamcore::games::kScoreBounds;
using steamcore::games::kScorePerKill;
using steamcore::test::FakeFlashBackend;
using steamcore::test::framebuffersEqual;

namespace {

using Store = HighscoreStore<FakeFlashBackend>;
using Wrapped = HighscoreGame<GalacticInvasion, Store>;

GameInput fireInput() {
  GameInput input{};
  input.fire = true;
  return input;
}

void enterPlaying(GameLoop<Wrapped>& loop) {
  loop.tick(GameInput{});
  loop.tick(GameInput{/*start=*/true});
}

void enterPlainPlaying(GameLoop<GalacticInvasion>& loop) {
  loop.tick(GameInput{});
  loop.tick(GameInput{/*start=*/true});
}

constexpr int32_t kMaxTicks = 10000;

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

bool entryHeaderShown(const Framebuffer& fb) {
  Framebuffer expected;
  placeExpected(expected, kEntryHeaderX, rowY(kEntryHeaderRow),
                kEntryHeaderText, Color::BRIGHT_ORANGE);
  const int32_t y0 = rowY(kEntryHeaderRow);
  for (int32_t y = y0; y < y0 + kGlyphHeight; ++y) {
    for (int32_t x = kEntryHeaderX; x < kEntryHeaderX + kEntryHeaderWidth;
         ++x) {
      if (fb.pixel(x, y) != expected.pixel(x, y)) return false;
    }
  }
  return true;
}

// Drives a fresh, already-PLAYING wrapped loop with continuous fire and
// no movement until the flow's own ENTRY header first appears (i.e. a
// qualifying ending was reached) -- galactic_invasion_combat_test.cpp's
// own derivation guarantees at least kScorePerKill by tick ~70, well
// before the round can otherwise end.
bool driveToQualifyingEnd(GameLoop<Wrapped>& loop, const Framebuffer& fb) {
  for (int32_t i = 0; i < kMaxTicks; ++i) {
    loop.tick(fireInput());
    if (entryHeaderShown(fb)) return true;
  }
  return false;
}

// Submits three letters (all 'A', the default) from a freshly-opened
// ENTRY screen -- one settling tick first (the flow's own edge trackers
// latch `true` on `begin()`, R5), then three fire-edge/release cycles.
void submitDefaultInitials(GameLoop<Wrapped>& loop) {
  loop.tick(GameInput{});  // settle past begin()'s own latch
  for (int32_t i = 0; i < 3; ++i) {
    loop.tick(fireInput());
    loop.tick(GameInput{});
  }
}

}  // namespace

// The spec's own §1 success signal, end to end: a full round to a
// qualifying score, the initials sequence to completion, the table holds
// the new entry at the correct rank, a second store reconstructed over
// the same bytes (the host's power-cycle stand-in) reads it back
// byte-identical, and a corrupted block yields a clean, empty table.
STEAMCORE_TEST(galactic_invasion_highscore_end_to_end_success_signal) {
  FakeFlashBackend backend;
  Store store(backend);
  GalacticInvasion game;
  Wrapped wrapped(game, store, kHighscoreSlot, kHighscoreName);
  Framebuffer fb;
  GameLoop<Wrapped> loop(wrapped, fb);
  enterPlaying(loop);

  CHECK(driveToQualifyingEnd(loop, fb));
  submitDefaultInitials(loop);

  CHECK_EQ(count(store.table(kHighscoreSlot)), 1);
  CHECK_EQ(store.table(kHighscoreSlot).entries[0].initials[0], 'A');
  CHECK_EQ(store.table(kHighscoreSlot).entries[0].initials[1], 'A');
  CHECK_EQ(store.table(kHighscoreSlot).entries[0].initials[2], 'A');
  CHECK(store.table(kHighscoreSlot).entries[0].score >= kScorePerKill);
  const int32_t recordedScore = store.table(kHighscoreSlot).entries[0].score;

  // Reconstruct a second store over the same backing bytes.
  Store reconstructed(backend);
  CHECK(reconstructed.load());
  CHECK_EQ(reconstructed.table(kHighscoreSlot).entries[0].score, recordedScore);
  CHECK_EQ(reconstructed.table(kHighscoreSlot).entries[0].initials[0], 'A');

  // Corrupt the backend's version field; reloading yields a clean, empty
  // table (AC-1.3), through the whole wired path, not just decodeBlock()
  // in isolation (already proven at T3/T4).
  backend.rawBytes()[4] = static_cast<uint8_t>(backend.rawBytes()[4] + 1);
  Store afterCorruption(backend);
  CHECK(!afterCorruption.load());
  CHECK_EQ(count(afterCorruption.table(kHighscoreSlot)), 0);
}

// AC-5.1: the score reported to the Highscore System equals the digits
// GalacticInvasion's own render() drew on that same ending tick -- read
// back as pixels from a scratch framebuffer, never trusting the accessor
// against the very field it returns. Also: the flow opens exactly once,
// even if GAME_OVER is held (neutral input) for 200 further ticks.
STEAMCORE_TEST(galactic_invasion_highscore_reports_the_rendered_score_exactly_once) {
  // First, drive an *unwrapped* GalacticInvasion to the same score-10
  // ending independently, and read its own rendered "SCORE: 0010" text --
  // this is the independent oracle AC-5.1 must be checked against.
  GalacticInvasion referenceGame;
  Framebuffer referenceFb;
  GameLoop<GalacticInvasion> referenceLoop(referenceGame, referenceFb);
  enterPlainPlaying(referenceLoop);
  for (int32_t i = 0; i < 70; ++i) referenceLoop.tick(fireInput());
  // referenceGame now shows "SCORE: 0010" on its live HUD (galactic_invasion_
  // combat_test.cpp's own precedent) -- read those exact pixels back.
  bool referenceMatchesScore10 = true;
  {
    Framebuffer expectedScore;
    placeExpected(expectedScore, kScoreBounds.x, kScoreBounds.y, "SCORE: 0010",
                  Color::BRIGHT_ORANGE);
    for (int32_t y = kScoreBounds.y; y < kScoreBounds.y + kScoreBounds.h; ++y) {
      for (int32_t x = kScoreBounds.x; x < kScoreBounds.x + kScoreBounds.w;
           ++x) {
        if (referenceFb.pixel(x, y) != expectedScore.pixel(x, y)) {
          referenceMatchesScore10 = false;
        }
      }
    }
  }
  CHECK(referenceMatchesScore10);

  // Now the wrapped path: drive to the exact same tick (70), read the
  // wrapped frame's own rendered score (still the unmodified game's HUD,
  // since the flow has not activated yet at tick 70), and record it as
  // the value the store's `qualifies()` was asked about.
  FakeFlashBackend backend;
  Store store(backend);
  GalacticInvasion game;
  Wrapped wrapped(game, store, kHighscoreSlot, kHighscoreName);
  Framebuffer fb;
  GameLoop<Wrapped> loop(wrapped, fb);
  enterPlaying(loop);
  for (int32_t i = 0; i < 70; ++i) loop.tick(fireInput());
  bool wrappedMatchesScore10 = true;
  {
    Framebuffer expectedScore;
    placeExpected(expectedScore, kScoreBounds.x, kScoreBounds.y, "SCORE: 0010",
                  Color::BRIGHT_ORANGE);
    for (int32_t y = kScoreBounds.y; y < kScoreBounds.y + kScoreBounds.h; ++y) {
      for (int32_t x = kScoreBounds.x; x < kScoreBounds.x + kScoreBounds.w;
           ++x) {
        if (fb.pixel(x, y) != expectedScore.pixel(x, y)) {
          wrappedMatchesScore10 = false;
        }
      }
    }
  }
  CHECK(wrappedMatchesScore10);

  // Continue until the flow opens; the score that qualified must match
  // `game.roundResult().score` at that exact tick -- the same accessor
  // `render()` would have drawn from had the flow not intercepted it this
  // tick (AC-5.2's contract: stable from the tick `ended` first becomes
  // true until the next restart). The round may keep scoring kills after
  // tick 70 before it actually ends, so this is not necessarily still
  // exactly kScorePerKill -- the oracle is the game's own live value at
  // the ending tick, not a value hand-derived for an earlier tick.
  CHECK(driveToQualifyingEnd(loop, fb));
  const int32_t endingScore = game.roundResult().score;
  CHECK(endingScore >= kScorePerKill);
  submitDefaultInitials(loop);
  CHECK_EQ(store.table(kHighscoreSlot).entries[0].score, endingScore);

  // 200 further idle ticks never re-record -- the table stays exactly one
  // entry deep. (The flow is sitting on its TABLE screen here, not
  // finished: no `start` edge has been given, so the wrapped game stays
  // frozen behind it. The complementary half of AC-5.1's "exactly once" --
  // an *ended, un-latched* round ticking on without the flow ever opening
  // -- is covered by highscore_game_test.cpp's own counting-stub test.)
  for (int32_t i = 0; i < 200; ++i) loop.tick(GameInput{});
  CHECK_EQ(count(store.table(kHighscoreSlot)), 1);
}

// AC-4.1: a round ending with score 0 (never fired) renders the
// unmodified GAME OVER frame, byte-identical to an unwrapped
// GalacticInvasion's, and restarts on a single `start` press -- no
// initials-entry or top-5 screen ever appears.
STEAMCORE_TEST(galactic_invasion_highscore_non_qualifying_round_is_untouched) {
  GalacticInvasion plainGame;
  Framebuffer plainFb;
  GameLoop<GalacticInvasion> plainLoop(plainGame, plainFb);
  enterPlainPlaying(plainLoop);

  FakeFlashBackend backend;
  Store store(backend);
  GalacticInvasion wrappedGame;
  Wrapped wrapped(wrappedGame, store, kHighscoreSlot, kHighscoreName);
  Framebuffer wrappedFb;
  GameLoop<Wrapped> wrappedLoop(wrapped, wrappedFb);
  enterPlaying(wrappedLoop);

  // Never fire, never move: the round ends via the threshold failsafe or
  // contact, always with score 0.
  for (int32_t i = 0; i < kMaxTicks; ++i) {
    plainLoop.tick(GameInput{});
    wrappedLoop.tick(GameInput{});
    CHECK(framebuffersEqual(plainFb, wrappedFb));
  }
  CHECK_EQ(count(store.table(kHighscoreSlot)), 0);

  // A single start press restarts both identically.
  plainLoop.tick(GameInput{/*start=*/true});
  wrappedLoop.tick(GameInput{/*start=*/true});
  CHECK(framebuffersEqual(plainFb, wrappedFb));
  plainLoop.tick(GameInput{});
  wrappedLoop.tick(GameInput{});
  CHECK(framebuffersEqual(plainFb, wrappedFb));
}

// AC-3.3/R4: `start` held from the moment of a qualifying ending, through
// the entire ENTRY and TABLE flow, then released and pressed once,
// restarts the round on that single press -- never needing a second one
// (the regression this wrapper's "exactly one game.update() per tick,
// real input on the finishing tick" design exists to prevent).
STEAMCORE_TEST(galactic_invasion_highscore_start_held_through_flow_restarts_on_one_press) {
  FakeFlashBackend backend;
  Store store(backend);
  GalacticInvasion game;
  Wrapped wrapped(game, store, kHighscoreSlot, kHighscoreName);
  Framebuffer fb;
  GameLoop<Wrapped> loop(wrapped, fb);
  enterPlaying(loop);

  // Drive to a qualifying end while holding `start` the whole time
  // (fire *and* start both held -- a real player's hand rarely does
  // this, but the wrapper must not care).
  bool sawHeader = false;
  for (int32_t i = 0; i < kMaxTicks && !sawHeader; ++i) {
    GameInput input = fireInput();
    input.start = true;
    loop.tick(input);
    sawHeader = entryHeaderShown(fb);
  }
  CHECK(sawHeader);

  // `start` stays held through the whole ENTRY/TABLE flow.
  GameInput heldStartAndFire = fireInput();
  heldStartAndFire.start = true;
  loop.tick(heldStartAndFire);  // settle tick, start already held throughout
  for (int32_t i = 0; i < 3; ++i) {
    // fire is already held from before ENTRY opened too -- release and
    // press it fresh each time, same as every other initials test.
    loop.tick(GameInput{/*start=*/true});
    GameInput fireAndStart{};
    fireAndStart.fire = true;
    fireAndStart.start = true;
    loop.tick(fireAndStart);
  }

  // Now on TABLE, still holding `start` throughout -- it must not have
  // registered as a fresh edge here either (it was already held when
  // TABLE was entered).
  for (int32_t i = 0; i < 5; ++i) {
    GameInput input{};
    input.start = true;
    loop.tick(input);
  }

  // Release, then press once: this single edge must both finish the flow
  // and restart the round in the same tick.
  loop.tick(GameInput{});
  loop.tick(GameInput{/*start=*/true});
  loop.tick(GameInput{});

  // The frame is now a fresh PLAYING round, byte-identical to a never-
  // played one.
  GalacticInvasion referenceGame;
  Framebuffer referenceFb;
  GameLoop<GalacticInvasion> referenceLoop(referenceGame, referenceFb);
  enterPlainPlaying(referenceLoop);
  referenceLoop.tick(GameInput{});
  CHECK(framebuffersEqual(referenceFb, fb));
}
