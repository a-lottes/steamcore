#include "galactic_invasion/galactic_invasion.h"

#include "steamcore/color.h"
#include "steamcore/font.h"
#include "steamcore/text.h"
#include "steamcore/title_screen.h"

namespace steamcore::games {

namespace {

// US-11/AC-11.2: the two end screens differ not just in words but in
// rendered silhouette -- "YOU WIN" (56px) is deliberately shorter than
// "GAME OVER" (72px) by more than one full glyph-advance, proven at
// compile time below, not just asserted by eye. Both share one outcome
// row; the final score (AC-5.3/AC-11.3) is a second, disjoint row beneath
// it, proven disjoint the same way.
constexpr char kLossText[] = "GAME OVER";
constexpr char kWinText[] = "YOU WIN";
constexpr int32_t kLossTextWidth =
    static_cast<int32_t>(sizeof(kLossText) - 1) * kGlyphAdvance;
constexpr int32_t kWinTextWidth =
    static_cast<int32_t>(sizeof(kWinText) - 1) * kGlyphAdvance;
constexpr int32_t kLossTextX = (Framebuffer::width() - kLossTextWidth) / 2;
constexpr int32_t kWinTextX = (Framebuffer::width() - kWinTextWidth) / 2;
constexpr int32_t kOutcomeTextY = 9 * kGlyphHeight;

constexpr int32_t kFinalScoreTextWidth = kScoreGlyphCount * kGlyphAdvance;
constexpr int32_t kFinalScoreTextX =
    (Framebuffer::width() - kFinalScoreTextWidth) / 2;
constexpr int32_t kFinalScoreTextY = kOutcomeTextY + kGlyphHeight;

constexpr Entity kLossTextBounds{kLossTextX, kOutcomeTextY, kLossTextWidth,
                                  kGlyphHeight};
constexpr Entity kWinTextBounds{kWinTextX, kOutcomeTextY, kWinTextWidth,
                                 kGlyphHeight};
constexpr Entity kFinalScoreBounds{kFinalScoreTextX, kFinalScoreTextY,
                                    kFinalScoreTextWidth, kGlyphHeight};

static_assert(kLossTextX >= 0 && kLossTextX + kLossTextWidth <= Framebuffer::width(),
              "the loss text must be fully on-screen");
static_assert(kWinTextX >= 0 && kWinTextX + kWinTextWidth <= Framebuffer::width(),
              "the win text must be fully on-screen");
static_assert(kFinalScoreTextX >= 0 &&
                  kFinalScoreTextX + kFinalScoreTextWidth <= Framebuffer::width() &&
                  kFinalScoreTextY + kGlyphHeight <= Framebuffer::height(),
              "the final score line must be fully on-screen");
static_assert(kLossTextBounds.w - kWinTextBounds.w >= 2 * kGlyphAdvance,
              "AC-11.2: the win/loss text widths must differ by a visually "
              "obvious margin");
static_assert(!overlaps(kLossTextBounds, kFinalScoreBounds),
              "the loss text and the final score line must not overlap");
static_assert(!overlaps(kWinTextBounds, kFinalScoreBounds),
              "the win text and the final score line must not overlap");

// Fixed-width, zero-padded integer formatters -- a `char` buffer and a
// digit loop, never `<cstdio>`/`snprintf` (which would pull a formatting
// dependency into game logic for four digits) and never dynamic
// allocation (NFR-2). `score`/`lives` outside the representable range
// clamp rather than overflow the fixed field width.
void formatScoreText(char (&buf)[kScoreGlyphCount + 1], int32_t score) {
  constexpr char kPrefix[] = "SCORE: ";
  constexpr int32_t kPrefixLen = static_cast<int32_t>(sizeof(kPrefix) - 1);
  for (int32_t i = 0; i < kPrefixLen; ++i) buf[i] = kPrefix[i];

  int32_t value = score;
  if (value < 0) value = 0;
  if (value > 9999) value = 9999;
  buf[kPrefixLen + 0] = static_cast<char>('0' + (value / 1000) % 10);
  buf[kPrefixLen + 1] = static_cast<char>('0' + (value / 100) % 10);
  buf[kPrefixLen + 2] = static_cast<char>('0' + (value / 10) % 10);
  buf[kPrefixLen + 3] = static_cast<char>('0' + value % 10);
  buf[kScoreGlyphCount] = '\0';
}

void formatLivesText(char (&buf)[kLivesGlyphCount + 1], int32_t lives) {
  constexpr char kPrefix[] = "LIVES: ";
  constexpr int32_t kPrefixLen = static_cast<int32_t>(sizeof(kPrefix) - 1);
  for (int32_t i = 0; i < kPrefixLen; ++i) buf[i] = kPrefix[i];

  int32_t value = lives;
  if (value < 0) value = 0;
  if (value > 9) value = 9;
  buf[kPrefixLen] = static_cast<char>('0' + value);
  buf[kLivesGlyphCount] = '\0';
}

}  // namespace

void GalacticInvasion::update(const GameInput& input) {
  const GameState before = session_.state();

  if (before == GameState::PLAYING) {
    // US-1/AC-1.1-1.3: horizontal-only movement, clamped fully on-screen.
    // up/down are read by GameInput but never consulted here -- the
    // player's y never changes.
    if (input.left) player_.x -= kPlayerSpeedX;
    if (input.right) player_.x += kPlayerSpeedX;
    if (player_.x < 0) player_.x = 0;
    const int32_t maxX = Framebuffer::width() - kPlayerWidth;
    if (player_.x > maxX) player_.x = maxX;

    stepFormation();
    stepPlayerShot(input.fire);
    resolvePlayerShotHits();
    stepEnemyShots();

    // US-10/A12 (plan §1 Decision 9): read at the top, before contact is
    // resolved, and decremented at the bottom using this same snapshot --
    // never a fresh read -- so the tick a hit sets invulnTicks_ to
    // kInvulnerabilityTicks does not itself decrement it.
    const bool wasInvulnerable = invulnTicks_ > 0;
    // resolvePlayerContact() runs first: if it resolves a hit, it sets
    // invulnTicks_ > 0 immediately, so resolveEnemyShotHits()'s own
    // invulnerability guard (checked at its own top) already sees the new
    // value and correctly never resolves a second hit the same tick.
    const bool bodyContactResolved = resolvePlayerContact();
    const bool shotContactResolved = resolveEnemyShotHits();
    if (wasInvulnerable) --invulnTicks_;

    // AC-5.2's own wording gates this on contact having been resolved
    // (by either source): checked after both, not before, so a genuine
    // hit this same tick (lives lost and continuing, or the last life
    // ending the round) is never also overridden by the failsafe.
    checkFormationThreshold(bodyContactResolved || shotContactResolved);
  }

  // AC-4.3/AC-11.1 (formation-clear WIN), AC-10.5 (last-life contact LOSS)
  // and AC-5.2/AC-10.6 (threshold failsafe LOSS) all set outcome_ within
  // this same tick's simulation, strictly before this line runs.
  const bool sessionEnded = outcome_ != Outcome::NONE;

  session_.advance(input, sessionEnded);

  if (before != GameState::PLAYING && session_.state() == GameState::PLAYING) {
    resetRound();
  }
}

void GalacticInvasion::render(Framebuffer& fb) {
  fb.clear(Color::BLACK);
  switch (session_.state()) {
    case GameState::READY:
      drawTitleScreen(fb, session_.state());
      break;
    case GameState::PLAYING: {
      for (const Enemy& e : enemies_) {
        if (!isAlive(e)) continue;
        fb.blit(kEnemySprite, e.body.x, e.body.y);
      }

      if (playerShot_.w > 0) {
        fb.blit(kProjectileSprite, playerShot_.x, playerShot_.y);
      }

      for (const Entity& shot : enemyShots_) {
        if (shot.w > 0) fb.blit(kEnemyShotSprite, shot.x, shot.y);
      }

      // AC-10.8: flickers 4-on/4-off during invulnerability -- the same
      // expression covers the ordinary vulnerable case too, since
      // invulnTicks_ == 0 falls in the visible half (plan §1 Decision 9).
      if (invulnTicks_ % kFlickerPeriodTicks < kFlickerHalfPeriodTicks) {
        fb.blit(kPlayerSprite, player_.x, kPlayerY);
      }

      // HUD drawn last so nothing to come (formation, shots) overdraws
      // it -- AC-7.1/AC-10.1.
      char scoreText[kScoreGlyphCount + 1];
      formatScoreText(scoreText, score_);
      drawText(fb, kScoreBounds.x, kScoreBounds.y, scoreText,
               Color::BRIGHT_ORANGE);

      char livesText[kLivesGlyphCount + 1];
      formatLivesText(livesText, lives_);
      drawText(fb, kLivesBounds.x, kLivesBounds.y, livesText,
               Color::BRIGHT_ORANGE);
      break;
    }
    case GameState::GAME_OVER: {
      // AC-11.1: which outcome occurred is this game's own private state --
      // GameState itself never distinguishes the two (A3).
      const bool won = outcome_ == Outcome::WIN;
      drawText(fb, won ? kWinTextX : kLossTextX, kOutcomeTextY,
               won ? kWinText : kLossText, Color::BRIGHT_ORANGE);

      // AC-5.3/AC-11.3: both endings show the final score, same contract.
      char finalScoreText[kScoreGlyphCount + 1];
      formatScoreText(finalScoreText, score_);
      drawText(fb, kFinalScoreTextX, kFinalScoreTextY, finalScoreText,
               Color::BRIGHT_ORANGE);
      break;
    }
  }
}

void GalacticInvasion::resetRound() {
  player_.x = kPlayerStartX;
  score_ = 0;
  lives_ = kStartingLives;
  invulnTicks_ = 0;

  offsetX_ = 0;
  offsetY_ = 0;
  dir_ = 1;
  stepTicks_ = 0;
  for (Enemy& e : enemies_) {
    e.body.w = kEnemyWidth;
    e.body.h = kEnemyHeight;
  }
  updateEnemyPositions();

  playerShot_ = Entity{0, 0, 0, 0};
  for (Entity& shot : enemyShots_) shot = Entity{0, 0, 0, 0};
  enemyFireTicks_ = 0;
  rngState_ = kRngSeed;

  outcome_ = Outcome::NONE;
}

void GalacticInvasion::updateEnemyPositions() {
  for (int32_t i = 0; i < kEnemyCount; ++i) {
    const int32_t row = i / kEnemyCols;
    const int32_t col = i % kEnemyCols;
    enemies_[i].body.x = kFormationStartX + col * kEnemyPitchX + offsetX_;
    enemies_[i].body.y = kFormationStartY + row * kEnemyPitchY + offsetY_;
  }
}

void GalacticInvasion::stepFormation() {
  // US-9/AC-9.1: the step interval shortens as survivors are destroyed --
  // recomputed fresh every tick from the current count, never cached,
  // since a kill can change it at any time (this runs before
  // resolvePlayerShotHits(), so the count here is always >= 1: the last
  // kill hasn't happened yet this tick).
  int32_t aliveCount = 0;
  for (const Enemy& e : enemies_) {
    if (isAlive(e)) ++aliveCount;
  }
  if (++stepTicks_ < stepIntervalTicks(aliveCount)) return;
  stepTicks_ = 0;

  // AC-3.3: a step that would push any SURVIVING enemy's rect outside
  // [0, width()) performs no horizontal move at all -- instead the
  // formation reverses and descends once. A destroyed enemy's stale
  // position never blocks this check.
  bool wouldLeaveScreen = false;
  for (const Enemy& e : enemies_) {
    if (!isAlive(e)) continue;
    const int32_t candidateX = e.body.x + dir_ * kFormationStepX;
    if (candidateX < 0 || candidateX + kEnemyWidth > Framebuffer::width()) {
      wouldLeaveScreen = true;
      break;
    }
  }

  if (wouldLeaveScreen) {
    dir_ = -dir_;
    offsetY_ += kFormationDescendY;
  } else {
    offsetX_ += dir_ * kFormationStepX;
  }
  updateEnemyPositions();
}

void GalacticInvasion::checkFormationThreshold(bool contactResolved) {
  bool anyPast = false;
  for (const Enemy& e : enemies_) {
    if (!isAlive(e)) continue;
    if (e.body.y + kEnemyHeight >= kFormationThresholdY) {
      anyPast = true;
      break;
    }
  }

  // AC-5.2: "...without any entity directly overlapping the player" -- a
  // genuine hit resolved this same tick means some entity does overlap,
  // so this one tick defers to AC-5.1's resolution entirely rather than
  // also ending the round. No latched "already saw this" state (review
  // F1): once `outcome_` actually leaves NONE, GameSession leaves PLAYING
  // and this function is never called again this round, so there is
  // nothing to guard against re-firing -- and re-deriving `anyPast` fresh
  // every tick is what lets AC-10.6 hold ("regardless of invulnerability"):
  // the very next tick, once invulnerability starts skipping further
  // contact checks and `contactResolved` reads false again, this same
  // still-true `anyPast` ends the round rather than staying silenced for
  // the rest of it.
  if (anyPast && !contactResolved && outcome_ == Outcome::NONE) {
    outcome_ = Outcome::LOSS;
  }
}

void GalacticInvasion::stepPlayerShot(bool fireHeld) {
  // AC-2.3: an in-flight shot advances and is removed once it clears the
  // top edge, checked before a new spawn so a shot that clears this very
  // tick immediately frees the slot for AC-2.1's next spawn.
  if (playerShot_.w > 0) {
    playerShot_.y -= kProjectileSpeedY;
    if (playerShot_.y + playerShot_.h <= 0) {
      playerShot_ = Entity{0, 0, 0, 0};
    }
  }

  // AC-2.1/AC-2.2: spawn only if none is currently in flight -- at most
  // one player projectile exists at any time.
  if (fireHeld && playerShot_.w == 0) {
    const int32_t spawnX = player_.x + (kPlayerWidth - kProjectileWidth) / 2;
    const int32_t spawnY = kPlayerY - kProjectileHeight;
    playerShot_ = Entity{spawnX, spawnY, kProjectileWidth, kProjectileHeight};
  }
}

void GalacticInvasion::resolvePlayerShotHits() {
  // AC-4.1: the shipped checkCollision, never a hand-rolled overlap test.
  // Checking playerShot_.w > 0 before each call is enough to stop after
  // the first hit -- a destroyed shot is zero-size and overlaps() defines
  // that as overlapping nothing, so a stale loop tail is harmless anyway.
  for (Enemy& e : enemies_) {
    if (playerShot_.w == 0 || !isAlive(e)) continue;
    checkCollision(playerShot_, e.body, [this](Entity& shot, Entity& enemy) {
      shot = Entity{0, 0, 0, 0};
      enemy = Entity{0, 0, 0, 0};
      score_ += kScorePerKill;
    });
  }

  // AC-4.3/AC-11.1: clearing every enemy wins -- GameState itself never
  // learns this; outcome_ is the only record (NFR-5). Write-once (review
  // F2): this runs first in tick order, so an ordinary win is never at
  // risk, but guards against the rare case where a still-in-flight enemy
  // shot also lands this same tick.
  bool anyAlive = false;
  for (const Enemy& e : enemies_) {
    if (isAlive(e)) {
      anyAlive = true;
      break;
    }
  }
  if (!anyAlive && outcome_ == Outcome::NONE) outcome_ = Outcome::WIN;
}

bool GalacticInvasion::resolvePlayerContact() {
  // AC-10.3: invulnerability skips contact checking entirely -- no life
  // lost, no re-respawn, and the overlapping enemy itself is left
  // untouched (never called with a mutating callback here, unlike
  // resolvePlayerShotHits()).
  if (invulnTicks_ > 0) return false;

  for (Enemy& e : enemies_) {
    if (!isAlive(e)) continue;
    bool hit = false;
    checkCollision(player_, e.body,
                    [&hit](Entity&, Entity&) { hit = true; });
    if (hit) {
      resolvePlayerHit();
      return true;
    }
  }
  return false;
}

bool GalacticInvasion::resolvePlayerHit() {
  if (lives_ > 1) {
    --lives_;
    playerShot_ = Entity{0, 0, 0, 0};
    player_.x = kPlayerStartX;
    invulnTicks_ = kInvulnerabilityTicks;
    return false;
  }
  // Write-once (review F2): this is the last-life path, so outcome_ is
  // ordinarily still NONE here, but the guard keeps the rule uniform
  // across all three writer sites rather than trusting tick order alone.
  if (outcome_ == Outcome::NONE) outcome_ = Outcome::LOSS;
  return true;
}

void GalacticInvasion::stepEnemyShots() {
  // AC-8.3: advance every in-flight shot; one reaching the bottom edge is
  // removed.
  for (Entity& shot : enemyShots_) {
    if (shot.w == 0) continue;
    shot.y += kEnemyShotSpeedY;
    if (shot.y >= Framebuffer::height()) {
      shot = Entity{0, 0, 0, 0};
    }
  }

  // AC-8.1: one fire opportunity every kEnemyFireIntervalTicks ticks.
  if (++enemyFireTicks_ < kEnemyFireIntervalTicks) return;
  enemyFireTicks_ = 0;

  int32_t freeSlot = -1;
  for (int32_t i = 0; i < kMaxEnemyShots; ++i) {
    if (enemyShots_[i].w == 0) {
      freeSlot = i;
      break;
    }
  }
  if (freeSlot == -1) return;  // pool full -- no spawn this opportunity

  int32_t aliveCount = 0;
  for (const Enemy& e : enemies_) {
    if (isAlive(e)) ++aliveCount;
  }
  if (aliveCount == 0) return;

  // A11/NFR-3: an explicit constant-seeded LCG, never <random>/rand()/a
  // clock -- the same input sequence always drives the same shooter
  // sequence. Advanced once per fire opportunity, not per tick.
  rngState_ = rngState_ * 1664525u + 1013904223u;
  const int32_t n = static_cast<int32_t>(
      (rngState_ >> kRngShiftBits) % static_cast<uint32_t>(aliveCount));

  int32_t seen = 0;
  for (const Enemy& e : enemies_) {
    if (!isAlive(e)) continue;
    if (seen == n) {
      const int32_t spawnX = e.body.x + (kEnemyWidth - kProjectileWidth) / 2;
      const int32_t spawnY = e.body.y + kEnemyHeight;
      enemyShots_[freeSlot] =
          Entity{spawnX, spawnY, kProjectileWidth, kProjectileHeight};
      return;
    }
    ++seen;
  }
}

bool GalacticInvasion::resolveEnemyShotHits() {
  // AC-10.3: invulnerability protects against an enemy shot exactly as it
  // does against direct contact -- the shot itself is left untouched.
  if (invulnTicks_ > 0) return false;

  for (Entity& shot : enemyShots_) {
    if (shot.w == 0) continue;
    bool hit = false;
    checkCollision(player_, shot, [&hit](Entity&, Entity&) { hit = true; });
    if (hit) {
      shot = Entity{0, 0, 0, 0};
      resolvePlayerHit();
      return true;
    }
  }
  return false;
}

}  // namespace steamcore::games
