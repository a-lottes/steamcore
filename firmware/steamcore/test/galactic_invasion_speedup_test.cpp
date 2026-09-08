#include "galactic_invasion/galactic_invasion.h"

#include "fb_compare.h"
#include "galactic_invasion_fixture.h"
#include "steamcore/color.h"
#include "steamcore/font.h"
#include "steamcore/game_loop.h"
#include "steamcore/sprite.h"
#include "test_harness.h"

using steamcore::Color;
using steamcore::Framebuffer;
using steamcore::GameInput;
using steamcore::GameLoop;
using steamcore::glyphFor;
using steamcore::kGlyphAdvance;
using steamcore::kGlyphHeight;
using steamcore::kGlyphWidth;
using steamcore::Sprite;
using steamcore::games::GalacticInvasion;
using steamcore::games::kEnemyCount;
using steamcore::games::kFormationStepTicks;
using steamcore::games::kPlayerHeight;
using steamcore::games::kPlayerWidth;
using steamcore::games::kPlayerY;
using steamcore::games::kScoreBounds;
using steamcore::games::kScorePerKill;
using steamcore::games::kStepTicksMin;
using steamcore::games::stepIntervalTicks;
using steamcore::test::findFormationBounds;
using steamcore::test::FormationBounds;
using steamcore::test::framebuffersEqual;

namespace {

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

bool scoreTextIs(const Framebuffer& fb, const char* text) {
  Framebuffer expected;
  placeExpected(expected, kScoreBounds.x, kScoreBounds.y, text,
                Color::BRIGHT_ORANGE);
  for (int32_t y = kScoreBounds.y; y < kScoreBounds.y + kScoreBounds.h; ++y) {
    for (int32_t x = kScoreBounds.x; x < kScoreBounds.x + kScoreBounds.w;
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

bool enemyShotThreatensColumn(const Framebuffer& fb, int32_t x0, int32_t x1) {
  for (int32_t y = 0; y < Framebuffer::height(); ++y) {
    for (int32_t x = x0; x < x1; ++x) {
      if (fb.pixel(x, y) == Color::DARK_ORANGE) return true;
    }
  }
  return false;
}

// Color-specific (BRIGHT_ORANGE, not the fixture's plain != BLACK) so an
// enemy sprite (Color::ORANGE) sharing the player's row band is never
// mistaken for it -- galactic_invasion_round_test.cpp's own precedent.
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

// Sweeps and fires (galactic_invasion_round_test.cpp's own shot-aware
// look-ahead dodge, restated here) until the score shows exactly
// `kills * kScorePerKill`, then stops firing -- but keeps running the
// same sweep movement so the survivor count itself doesn't change any
// further while a caller measures the resulting step cadence.
void driveUntilNKills(GameLoop<GalacticInvasion>& loop, const Framebuffer& fb,
                       int32_t kills, int32_t maxTicks) {
  char target[16];
  int32_t v = kills * kScorePerKill;
  target[0] = 'S'; target[1] = 'C'; target[2] = 'O'; target[3] = 'R';
  target[4] = 'E'; target[5] = ':'; target[6] = ' ';
  target[7] = static_cast<char>('0' + (v / 1000) % 10);
  target[8] = static_cast<char>('0' + (v / 100) % 10);
  target[9] = static_cast<char>('0' + (v / 10) % 10);
  target[10] = static_cast<char>('0' + v % 10);
  target[11] = '\0';

  constexpr int32_t kSweepHalfPeriod = 150;
  int32_t phase = 0;
  for (int32_t i = 0; i < maxTicks && anyEnemyPixelOnScreen(fb) &&
                      !scoreTextIs(fb, target);
       ++i) {
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
        const int32_t rawCandidateX =
            px + (wantRight ? 2 : -2);  // kPlayerSpeedX, restated
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

// AC-9.1: stepIntervalTicks is a pure, monotone-non-increasing function of
// the survivor count, evaluating to exactly kFormationStepTicks (the
// pre-US-9 constant, T5's own interval) at full health and kStepTicksMin
// at the last survivor.
STEAMCORE_TEST(galactic_invasion_step_interval_is_monotone_and_bounded) {
  CHECK_EQ(stepIntervalTicks(kEnemyCount), kFormationStepTicks);
  CHECK_EQ(stepIntervalTicks(1), kStepTicksMin);

  int32_t previous = stepIntervalTicks(kEnemyCount);
  for (int32_t alive = kEnemyCount - 1; alive >= 1; --alive) {
    const int32_t current = stepIntervalTicks(alive);
    CHECK(current <= previous);
    CHECK(current >= kStepTicksMin);
    previous = current;
  }
}

// AC-9.1: with all kEnemyCount enemies alive, the observed cadence is
// unchanged from T5's own fixed-interval behaviour -- no step before tick
// kFormationStepTicks, exactly one on it.
STEAMCORE_TEST(galactic_invasion_full_health_cadence_matches_pre_us9_behavior) {
  GalacticInvasion game;
  Framebuffer fb;
  GameLoop<GalacticInvasion> loop(game, fb);
  enterPlaying(loop);

  const FormationBounds start = findFormationBounds(fb);
  for (int32_t i = 1; i < kFormationStepTicks; ++i) {
    loop.tick(GameInput{});
    const FormationBounds b = findFormationBounds(fb);
    CHECK_EQ(b.minX, start.minX);
  }
  loop.tick(GameInput{});
  const FormationBounds afterStep = findFormationBounds(fb);
  CHECK(afterStep.minX != start.minX);
}

// AC-9.1: after 9 scripted kills, the measured step cadence matches
// stepIntervalTicks(9) exactly -- counted in rendered frames (no
// accessor, NFR-5), not read from any field.
STEAMCORE_TEST(galactic_invasion_cadence_matches_formula_after_nine_kills) {
  GalacticInvasion game;
  Framebuffer fb;
  GameLoop<GalacticInvasion> loop(game, fb);
  enterPlaying(loop);

  driveUntilNKills(loop, fb, 9, 20000);
  CHECK(scoreTextIs(fb, "SCORE: 0090"));

  const int32_t expectedInterval = stepIntervalTicks(9);

  // Stop firing, keep the survivor count fixed at 9, and measure the next
  // step boundary via the formation's rendered bounding box.
  const FormationBounds start = findFormationBounds(fb);
  int32_t ticksToStep = -1;
  for (int32_t k = 1; k <= expectedInterval + 5 && ticksToStep == -1; ++k) {
    loop.tick(GameInput{});
    const FormationBounds b = findFormationBounds(fb);
    if (b.minX != start.minX || b.minY != start.minY) ticksToStep = k;
  }
  CHECK_EQ(ticksToStep, expectedInterval);
}

// AC-9.2: the same fixed kill/input script replayed on two fresh
// instances produces an identical step-timing sequence -- checked via
// full-frame byte-identity every tick throughout a run that includes
// several real kills (and therefore several speed changes), not just the
// enemy-fire LCG this general check already covers elsewhere.
STEAMCORE_TEST(galactic_invasion_speedup_sequence_is_deterministic) {
  GalacticInvasion gameA;
  Framebuffer fbA;
  GameLoop<GalacticInvasion> loopA(gameA, fbA);
  enterPlaying(loopA);

  GalacticInvasion gameB;
  Framebuffer fbB;
  GameLoop<GalacticInvasion> loopB(gameB, fbB);
  enterPlaying(loopB);

  constexpr int32_t kSweepHalfPeriod = 150;
  int32_t phase = 0;
  int32_t firstMismatch = -1;
  for (int32_t i = 0; i < 3000 && firstMismatch == -1; ++i) {
    GameInput input{};
    input.fire = true;
    if ((phase / kSweepHalfPeriod) % 2 == 0) {
      input.right = true;
    } else {
      input.left = true;
    }
    ++phase;
    loopA.tick(input);
    loopB.tick(input);
    if (!framebuffersEqual(fbA, fbB)) firstMismatch = i;
  }
  CHECK_EQ(firstMismatch, -1);
}
