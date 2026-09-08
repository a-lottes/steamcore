#include <cstdio>

#include "galactic_invasion/galactic_invasion.h"

#include "steamcore/color.h"
#include "steamcore/dump_format.h"
#include "steamcore/font.h"
#include "steamcore/framebuffer.h"
#include "steamcore/game_loop.h"
#include "steamcore/sprite.h"
#include "test_harness.h"

#ifndef STEAMCORE_GAME_DUMP
#error "STEAMCORE_GAME_DUMP must be defined by the build (see Makefile)"
#endif
#ifndef STEAMCORE_GAME_WIN_DUMP
#error "STEAMCORE_GAME_WIN_DUMP must be defined by the build (see Makefile)"
#endif

using steamcore::Color;
using steamcore::Framebuffer;
using steamcore::GameInput;
using steamcore::GameLoop;
using steamcore::glyphFor;
using steamcore::kDumpHeaderSize;
using steamcore::kGlyphAdvance;
using steamcore::kGlyphHeight;
using steamcore::kGlyphWidth;
using steamcore::serializeDump;
using steamcore::Sprite;
using steamcore::games::GalacticInvasion;
using steamcore::games::kEnemyCount;
using steamcore::games::kPlayerHeight;
using steamcore::games::kPlayerWidth;
using steamcore::games::kPlayerY;
using steamcore::games::kScoreGlyphCount;
using steamcore::games::kScorePerKill;

namespace {

constexpr size_t kBufferCapacity =
    kDumpHeaderSize +
    static_cast<size_t>(Framebuffer::width()) * Framebuffer::height();

void writeDump(const Framebuffer& fb, const char* path) {
  static uint8_t buffer[kBufferCapacity];
  const size_t written = serializeDump(fb, buffer, sizeof(buffer));
  CHECK_EQ(written, kBufferCapacity);

  FILE* out = std::fopen(path, "wb");
  if (out == nullptr) {
    CHECK(false && "could not open dump path for writing -- run make from the repo root");
    return;
  }
  const size_t fwritten = std::fwrite(buffer, 1, written, out);
  std::fclose(out);
  CHECK_EQ(fwritten, written);
}

void enterPlaying(GameLoop<GalacticInvasion>& loop) {
  loop.tick(GameInput{});
  loop.tick(GameInput{/*start=*/true});
}

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

// The end-screen's final-score line is centred, unlike the HUD's top-left
// kScoreBounds (which never renders during GAME_OVER at all) -- its
// position is restated independently here, mirroring
// galactic_invasion_round_test.cpp's own kOutcomeTextY/kFinalScoreTextX.
bool finalScoreTextIs(const Framebuffer& fb, const char* text) {
  constexpr int32_t kOutcomeTextY = 9 * kGlyphHeight;
  constexpr int32_t kFinalScoreTextY = kOutcomeTextY + kGlyphHeight;
  constexpr int32_t kFinalScoreTextWidth = kScoreGlyphCount * kGlyphAdvance;
  constexpr int32_t kFinalScoreTextX =
      (Framebuffer::width() - kFinalScoreTextWidth) / 2;

  Framebuffer expected;
  placeExpected(expected, kFinalScoreTextX, kFinalScoreTextY, text,
                Color::BRIGHT_ORANGE);
  for (int32_t y = kFinalScoreTextY; y < kFinalScoreTextY + kGlyphHeight; ++y) {
    for (int32_t x = kFinalScoreTextX; x < kFinalScoreTextX + kFinalScoreTextWidth;
         ++x) {
      if (fb.pixel(x, y) != expected.pixel(x, y)) return false;
    }
  }
  return true;
}

bool anyEnemyPixelOnScreen(const Framebuffer& fb) {
  for (int32_t y = 0; y < Framebuffer::height(); ++y) {
    for (int32_t x = 0; x < Framebuffer::width(); ++x) {
      if (fb.pixel(x, y) == Color::ORANGE) return true;
    }
  }
  return false;
}

// Scoped to the player's own row band, not the whole screen (the HUD's
// SCORE/LIVES text is also BRIGHT_ORANGE and renders at y=0, so scanning
// from the top would find that text instead), and scanning every row in
// the band for the true minimum x, not just the first row with any lit
// pixel: the player's wedge shape is narrower at its nose (top) than its
// base, so stopping at the first match would return the nose's offset,
// not the sprite's actual left edge (the same fix the shared fixture's
// own findPlayerSpriteX needed, T3).
int32_t findPlayerXStrict(const Framebuffer& fb) {
  int32_t minX = -1;
  for (int32_t y = kPlayerY; y < kPlayerY + kPlayerHeight; ++y) {
    for (int32_t x = 0; x < Framebuffer::width(); ++x) {
      if (fb.pixel(x, y) == Color::BRIGHT_ORANGE) {
        if (minX == -1 || x < minX) minX = x;
        break;
      }
    }
  }
  return minX;
}

bool enemyShotThreatensColumn(const Framebuffer& fb, int32_t x0, int32_t x1) {
  for (int32_t y = 0; y < Framebuffer::height(); ++y) {
    for (int32_t x = x0; x < x1; ++x) {
      if (fb.pixel(x, y) == Color::DARK_ORANGE) return true;
    }
  }
  return false;
}

// Drives a fresh, already-PLAYING round to a genuine win, via
// galactic_invasion_round_test.cpp's own shot-aware look-ahead sweep --
// restated here since a dump test is meant to stand alone.
void driveToWin(GameLoop<GalacticInvasion>& loop, const Framebuffer& fb) {
  constexpr int32_t kSweepHalfPeriod = 150;
  int32_t phase = 0;
  for (int32_t i = 0; i < 10000 && anyEnemyPixelOnScreen(fb); ++i) {
    GameInput input{};
    input.fire = true;
    const int32_t px = findPlayerXStrict(fb);
    if (px != -1) {
      if (enemyShotThreatensColumn(fb, px, px + kPlayerWidth)) {
        if (px < Framebuffer::width() / 2) {
          if (px < Framebuffer::width() - kPlayerWidth) input.right = true;
        } else {
          if (px > 0) input.left = true;
        }
      } else {
        const bool wantRight = (phase / kSweepHalfPeriod) % 2 == 0;
        const int32_t rawCandidateX = px + (wantRight ? 2 : -2);
        const int32_t maxX = Framebuffer::width() - kPlayerWidth;
        const int32_t candidateX =
            rawCandidateX < 0 ? 0 : (rawCandidateX > maxX ? maxX : rawCandidateX);
        if (!enemyShotThreatensColumn(fb, candidateX, candidateX + kPlayerWidth)) {
          if (wantRight) {
            input.right = true;
          } else {
            input.left = true;
          }
        }
      }
    }
    ++phase;
    loop.tick(input);
  }
}

}  // namespace

// US-12/AC-12.4/NFR-7: dumps a mid-round PLAYING frame (full 18-enemy
// formation, the player, a shot in flight, both HUD strings) so `make
// view` can render it and the sprites' silhouettes can actually be looked
// at -- mirroring title_screen_dump_test.cpp's own precedent.
STEAMCORE_TEST(galactic_invasion_playing_frame_dumps_to_disk_for_visual_check) {
  GalacticInvasion game;
  Framebuffer fb;
  GameLoop<GalacticInvasion> loop(game, fb);
  enterPlaying(loop);

  GameInput fireInput{};
  fireInput.fire = true;
  loop.tick(fireInput);  // formation intact, shot just spawned, HUD visible

  writeDump(fb, STEAMCORE_GAME_DUMP);
}

// US-11/AC-11.2: dumps the win screen too, so the two end screens'
// visible difference (not just their measured widths) can be eyeballed.
STEAMCORE_TEST(galactic_invasion_win_frame_dumps_to_disk_for_visual_check) {
  GalacticInvasion game;
  Framebuffer fb;
  GameLoop<GalacticInvasion> loop(game, fb);
  enterPlaying(loop);
  driveToWin(loop, fb);
  CHECK(!anyEnemyPixelOnScreen(fb));
  // anyEnemyPixelOnScreen alone can't distinguish a genuine win from a
  // loss (GAME_OVER renders no enemies either way) -- the full score is
  // the actual proof every one of the kEnemyCount enemies was destroyed.
  char expectedScore[kScoreGlyphCount + 1];
  const int32_t fullClearScore = kEnemyCount * kScorePerKill;
  std::snprintf(expectedScore, sizeof(expectedScore), "SCORE: %04d", fullClearScore);
  CHECK(finalScoreTextIs(fb, expectedScore));

  writeDump(fb, STEAMCORE_GAME_WIN_DUMP);
}
