#pragma once

#include <cstdint>

#include "galactic_invasion/galactic_invasion_art.h"
#include "steamcore/collision.h"
#include "steamcore/font.h"
#include "steamcore/framebuffer.h"
#include "steamcore/game_loop.h"
#include "steamcore/game_state.h"
#include "steamcore/round_result.h"

// SteamCore's first playable game (galactic-invasion plan §1) -- a
// Galaga/Space-Invaders homage composing nine already-shipped primitives
// (GameLoop, GameSession, collision.h, Framebuffer/Sprite/blit/drawText,
// drawTitleScreen) unmodified. Lives under games/, not include/steamcore/
// (constitution §5's own layout for a game, distinct from the engine's
// public surface).
//
// Example (a caller driving a full round exactly like game_loop.h's own
// doc-comment example -- this type needs nothing beyond GameLoop's
// already-shipped, unmodified public signature):
//   steamcore::games::GalacticInvasion game;
//   steamcore::Framebuffer fb;
//   steamcore::GameLoop<steamcore::games::GalacticInvasion> loop(game, fb);
//   loop.tick(steamcore::GameInput{true});  // start
//   GameInput moveLeftAndFire{};
//   moveLeftAndFire.fire = true;
//   moveLeftAndFire.left = true;
//   loop.tick(moveLeftAndFire);
//
// Contract:
//  - Drives GameSession (game_state.h) through all three of its states
//    unchanged: READY draws the unmodified drawTitleScreen; a rising edge
//    of `input.start` moves READY -> PLAYING and (on a fresh round only)
//    resets every piece of round state via resetRound(); GAME_OVER draws
//    the win or loss screen depending on the private Outcome recorded
//    below, and a further `input.start` edge restarts into PLAYING again.
//    None of this touches game_state.{h,cpp}'s three-state enum itself
//    (A3/AC-11.1) -- see the Outcome note below.
//  - `sessionEnded = true` is passed to `session_.advance()` on the tick
//    `outcome_` first leaves NONE, always *after* that tick's own
//    simulation has already run (game_state.h's own "advance after your
//    own simulation, not before" contract) so the ending reflects this
//    tick's state, not last tick's. Three conditions can set it:
//      1. the last surviving enemy is destroyed by the player's shot
//         (US-4/AC-4.3), checked first in tick order;
//      2. the player's last life is lost (US-10/AC-10.5) -- lives_ was
//         already 1 when resolvePlayerHit() ran this tick;
//      3. the formation's leading edge crosses kFormationThresholdY with
//         no entity directly overlapping the player this same tick
//         (US-5/AC-5.2's own worded precondition -- see
//         checkFormationThreshold()'s doc comment for the exact tick this
//         defers on and why).
//    `outcome_` is write-once per round: whichever of these first sets it
//    away from NONE wins, and every later write this same tick (review
//    F2 -- e.g. a stray enemy shot lands on the same tick the last enemy
//    is destroyed) is a no-op rather than a silent overwrite. In ordinary
//    play at most one of the three is ever true on a given tick; the
//    write-once rule exists for the rare same-tick coincidence, not as
//    the primary defense.
//  - The enemy formation is a fixed kEnemyRows x kEnemyCols (3x6) grid,
//    stepped every stepIntervalTicks(aliveCount) ticks (kFormationStepTicks
//    at full health, shortening toward kStepTicksMin as survivors thin --
//    US-9): each due step marches every survivor kFormationStepX pixels
//    in the current direction, or -- if that step would push any survivor
//    off-screen -- reverses direction and descends kFormationDescendY
//    once instead of marching (US-3/AC-3.2/AC-3.3). Positions are always
//    recomputed from offsetX_/offsetY_ plus each enemy's fixed grid slot,
//    never accumulated per-enemy (plan §1 Decision 5).
//  - At most one player projectile is ever in flight (US-2): firing while
//    one is already active is a no-op until it clears (by hitting an
//    enemy or leaving the top edge); w == 0 on playerShot_ is "no shot",
//    the same zero-size-means-absent convention Enemy and the enemy shot
//    pool both reuse.
//  - The player starts a round with kStartingLives (3) lives (lives_).
//    Losing one below the last respawns instantly at kPlayerStartX and opens a
//    kInvulnerabilityTicks (120-tick) window during which no further
//    contact or enemy-shot hit is resolved at all (US-10/AC-10.3) --
//    not merely "damage ignored", entirely skipped, so a hit inside the
//    window neither costs a life nor restarts the window. The window is
//    rendered with a kFlickerHalfPeriodTicks-on/-off (4 ticks) flicker
//    (AC-10.8) so it reads as temporary rather than a silent state
//    change; invulnTicks_ == 0 always falls in the visible half, so a
//    fully vulnerable ship is never accidentally hidden.
//  - Win and loss both route through the same GameState::GAME_OVER --
//    game_state.h gains no fourth state (A3/AC-11.1). Which one occurred
//    is tracked entirely inside this type's own private `Outcome`
//    (NONE/WIN/LOSS), set exactly once per round by whichever of the
//    three sessionEnded conditions above fired, and read back only by
//    render()'s GAME_OVER branch to choose the win or loss text plus the
//    final score. A caller cannot query it and is not meant to -- the
//    rendered screen is the only observable difference (NFR-5).
//  - Single-threaded, nothing throws, no error code, no dynamic
//    allocation -- same inherited contract as every other steamcore type.
//  - `roundResult()` (highscore-system AC-5.2, the one deliberate,
//    additive amendment to the "no accessor" claim below): returns
//    `{session_.state() == GameState::GAME_OVER, score_}`. Exists solely
//    so a caller composing above this type (e.g. a
//    `HighscoreGame<Game, Store>` wrapper, steamcore/highscore_game.h)
//    can learn a round ended and what it scored, without this type
//    knowing anything about highscores itself. `score` is stable from
//    the tick `ended` first becomes true until the next restart (this
//    type stops scoring the instant the round ends). No lives, formation
//    state or the private Outcome below is exposed by it or by anything
//    else.
//
// Public surface is this type's constructor, update(), render() and
// roundResult() (NFR-5, amended as above) -- no accessor for lives,
// formation state or outcome. Everything else a caller or a test can
// observe is a rendered pixel.
namespace steamcore::games {

// Player horizontal speed, in pixels per tick -- a tick count, never a
// wall-clock interval (constitution §3, spec A11).
inline constexpr int32_t kPlayerSpeedX = 2;

// The player's fixed vertical position (never changes -- US-1/AC-1.3)
// and its horizontal starting position for a fresh round, both derived
// from the screen size and the player sprite's own size, never a bare
// literal.
// Review F6: a named margin, not a bare pixel literal (CLAUDE.md's "derive
// on-screen layout, never a literal").
inline constexpr int32_t kPlayerBottomMarginPx = 4;
inline constexpr int32_t kPlayerY =
    Framebuffer::height() - kPlayerHeight - kPlayerBottomMarginPx;
inline constexpr int32_t kPlayerStartX =
    (Framebuffer::width() - kPlayerWidth) / 2;

static_assert(kPlayerY >= 0 && kPlayerY + kPlayerHeight <= Framebuffer::height(),
              "the player's fixed row must be fully on-screen");
static_assert(kPlayerStartX >= 0 &&
                  kPlayerStartX + kPlayerWidth <= Framebuffer::width(),
              "the player's starting column must be fully on-screen");

// The 3x6 enemy formation's geometry (spec A6, plan §1 Decision 6) --
// pure layout data, derived from the enemy sprite's own size, never a
// bare literal. T5 places the actual Enemy array using these; they are
// declared here because T4's HUD-disjointness proof already needs to
// know where the formation's starting band sits.
inline constexpr int32_t kEnemyRows = 3;
inline constexpr int32_t kEnemyCols = 6;
inline constexpr int32_t kEnemyCount = kEnemyRows * kEnemyCols;

inline constexpr int32_t kEnemyGapX = kEnemyWidth;
inline constexpr int32_t kEnemyPitchX = kEnemyWidth + kEnemyGapX;
inline constexpr int32_t kEnemyGapY = kEnemyHeight / 2 + 1;
inline constexpr int32_t kEnemyPitchY = kEnemyHeight + kEnemyGapY;

inline constexpr int32_t kFormationWidth =
    (kEnemyCols - 1) * kEnemyPitchX + kEnemyWidth;
inline constexpr int32_t kFormationStartX =
    (Framebuffer::width() - kFormationWidth) / 2;
// Row 0 of the HUD, row 1 left empty for spacing (§8 finding 4), the
// formation starts on glyph row 2.
inline constexpr int32_t kFormationStartY = 2 * kGlyphHeight;

// The threshold failsafe's line (US-5/AC-5.2): the player's own fixed
// row -- any survivor reaching it ends the round regardless of lives.
inline constexpr int32_t kFormationThresholdY = kPlayerY;

// Formation timing (US-3, constitution §3 timing -- tick counts, never a
// wall-clock read, spec A11): every kFormationStepTicks ticks, every
// surviving enemy steps kFormationStepX pixels horizontally; a step that
// would push any survivor off-screen instead flips direction and
// descends kFormationDescendY once (plan §1 Decision 7).
inline constexpr int32_t kFormationStepX = 4;
inline constexpr int32_t kFormationStepTicks = 18;
inline constexpr int32_t kFormationDescendY = kEnemyPitchY / 2;

// US-9 (Should): the step interval shortens as survivors are destroyed --
// kFormationStepTicks is the interval at full health (kEnemyCount alive,
// where this formula evaluates to exactly kFormationStepTicks itself, so
// every pre-existing behavior at 18 survivors is unchanged), shortening
// linearly to kStepTicksMin at the last survivor. Pure integer arithmetic
// on the current survivor count, evaluated fresh each time a step is
// checked for -- never a wall-clock read (A11).
inline constexpr int32_t kStepTicksMin = 6;

inline constexpr int32_t stepIntervalTicks(int32_t aliveCount) {
  return kStepTicksMin + (aliveCount - 1) * (kFormationStepTicks - kStepTicksMin) /
                             (kEnemyCount - 1);
}

// US-2: the player's single active projectile, in pixels per tick.
inline constexpr int32_t kProjectileSpeedY = 4;

// US-4/A4: score awarded for destroying one enemy.
inline constexpr int32_t kScorePerKill = 10;

// A1: lives a fresh round starts with -- named so the member initializer
// and resetRound() cannot drift apart (review F6).
inline constexpr int32_t kStartingLives = 3;

// highscore-system plan §1 Decision 6: this game's own identity, for
// whichever caller composes it with a Highscore System (e.g.
// `HighscoreGame<GalacticInvasion, Store>`) -- data that sits next to the
// game that owns it, not this class's own surface, and therefore outside
// AC-5.2's "one member" budget by the same reading that already lets this
// header export its layout constants. This game itself knows nothing
// about highscores; it never reads either constant.
inline constexpr int32_t kHighscoreSlot = 0;
inline constexpr char kHighscoreName[] = "GALACTIC INVASION";

// US-10/A12: ticks of post-respawn invulnerability, and the flicker that
// makes the window visible (AC-10.8) -- 4 on / 4 off, chosen so 120 divides
// evenly and invulnTicks_ == 0 (not invulnerable) falls in the visible
// half, so the vulnerable ship is never accidentally hidden.
inline constexpr int32_t kInvulnerabilityTicks = 120;
inline constexpr int32_t kFlickerHalfPeriodTicks = 4;
inline constexpr int32_t kFlickerPeriodTicks = 2 * kFlickerHalfPeriodTicks;

// US-8 (Should): enemy return fire. At most kMaxEnemyShots in flight at
// once, one fire opportunity every kEnemyFireIntervalTicks, travelling
// downward at kEnemyShotSpeedY px/tick. kRngSeed is an explicit constant
// (never std::random_device, never a clock) so the shooter-choice sequence
// is reproducible byte-for-byte (A11/NFR-3).
inline constexpr int32_t kMaxEnemyShots = 3;
inline constexpr int32_t kEnemyFireIntervalTicks = 45;
inline constexpr int32_t kEnemyShotSpeedY = 3;
inline constexpr uint32_t kRngSeed = 1;

// The LCG's low bits are the least random (a classic weakness of this
// generator family); the shooter index is drawn from the upper half of
// each 32-bit state instead. This is the one line in games/ the T14 lint
// block's tile-size-literal check excludes by name (mirroring how that
// same check excludes config.h's own definition) -- this 16 is an
// unrelated bit-shift width, not a tile dimension, and picking a
// different shift purely to dodge the literal ban would change the whole
// deterministic enemy-fire sequence for no reason connected to the rule
// it was dodging (tried once, reverted: two already-passing tests broke).
inline constexpr int32_t kRngShiftBits = 16;

static_assert(kFormationStartX >= 0 &&
                  kFormationStartX + kFormationWidth <= Framebuffer::width(),
              "the formation's starting band must be fully on-screen");

// The vertical band the formation occupies at the start of a round --
// full screen width, since the exact per-enemy x extent is a T5 runtime
// concern, but the compile-time HUD-disjointness proof only needs to
// know the band never reaches into the HUD row regardless of x.
inline constexpr Entity kFormationBandBounds{
    0, kFormationStartY, Framebuffer::width(),
    (kEnemyRows - 1) * kEnemyPitchY + kEnemyHeight};

// The player's fixed row, as a full-width band for the same reason.
inline constexpr Entity kPlayerBandBounds{0, kPlayerY, Framebuffer::width(),
                                           kPlayerHeight};

// HUD text bounds (US-7/US-10, plan §1 Decision 6): `SCORE: nnnn`
// (zero-padded so its width never changes) top-left, `LIVES: n` top-right,
// both derived from kGlyphAdvance/kGlyphHeight — never a bare
// `240`/`160`/`8`/`43`. The glyph counts themselves are derived from a
// representative example string via `sizeof`, not a bare digit count
// (CLAUDE.md's own "derive layout, never a literal" convention,
// mirroring title_screen.h's kExpectedWordmarkText) — kLivesGlyphCount's
// value (8) would otherwise coincidentally collide with font.h's own
// glyph-metric literal ban.
inline constexpr char kScoreGlyphCountExample[] = "SCORE: 0000";
inline constexpr int32_t kScoreGlyphCount =
    static_cast<int32_t>(sizeof(kScoreGlyphCountExample) - 1);
inline constexpr char kLivesGlyphCountExample[] = "LIVES: 0";
inline constexpr int32_t kLivesGlyphCount =
    static_cast<int32_t>(sizeof(kLivesGlyphCountExample) - 1);

inline constexpr Entity kScoreBounds{0, 0, kScoreGlyphCount * kGlyphAdvance,
                                      kGlyphHeight};
inline constexpr Entity kLivesBounds{
    Framebuffer::width() - kLivesGlyphCount * kGlyphAdvance, 0,
    kLivesGlyphCount * kGlyphAdvance, kGlyphHeight};

// The full HUD row, both strings' union -- what must never intersect the
// formation band (AC-7.2).
inline constexpr Entity kHudRowBounds{0, 0, Framebuffer::width(),
                                       kGlyphHeight};

static_assert(kScoreBounds.x >= 0 &&
                  kScoreBounds.x + kScoreBounds.w <= Framebuffer::width() &&
                  kScoreBounds.y >= 0 &&
                  kScoreBounds.y + kScoreBounds.h <= Framebuffer::height(),
              "kScoreBounds must be fully on-screen");
static_assert(kLivesBounds.x >= 0 &&
                  kLivesBounds.x + kLivesBounds.w <= Framebuffer::width() &&
                  kLivesBounds.y >= 0 &&
                  kLivesBounds.y + kLivesBounds.h <= Framebuffer::height(),
              "kLivesBounds must be fully on-screen");

// The four required disjointness proofs (plan §1 Decision 3): the
// general four-way separating-axis test via the already-shipped,
// already-tested constexpr overlaps() -- never a second, hand-rolled
// intersection expression (CLAUDE.md: a bespoke Rect here would be
// exactly the drift class that note warns about).
static_assert(!overlaps(kScoreBounds, kLivesBounds),
              "kScoreBounds and kLivesBounds must not overlap");
static_assert(!overlaps(kScoreBounds, kPlayerBandBounds),
              "kScoreBounds and kPlayerBandBounds must not overlap");
static_assert(!overlaps(kLivesBounds, kPlayerBandBounds),
              "kLivesBounds and kPlayerBandBounds must not overlap");
static_assert(!overlaps(kHudRowBounds, kFormationBandBounds),
              "the HUD row and the formation's starting band must not overlap");

// One formation slot. Destruction (T7) sets body.w = body.h = 0, which
// collision.h already defines as "overlaps nothing" -- a dead enemy is
// structurally unhittable, not conditionally skipped, and isAlive() is
// its own single source of truth (plan §1 Decision 5): no parallel
// `bool alive` that could drift from it.
struct Enemy {
  Entity body;
};

inline constexpr bool isAlive(const Enemy& e) { return e.body.w > 0; }

class GalacticInvasion {
 public:
  GalacticInvasion() = default;

  // Satisfies GameLoop<Game>'s Game concept.
  void update(const GameInput& input);
  void render(Framebuffer& fb);

  // highscore-system AC-5.2: the one deliberate, additive exception to
  // this type's no-accessor rule -- see this file's top comment for the
  // full amendment. No lives, formation state or Outcome is exposed by
  // this or any other public member.
  RoundResult roundResult() const {
    return {session_.state() == GameState::GAME_OVER, score_};
  }

 private:
  // Re-initializes every piece of round state to a fresh round's starting
  // values. Called once, the tick a restart lands in PLAYING (plan §1
  // Decision 8) -- the single place "what does a restart reset" is
  // answered.
  void resetRound();

  // Recomputes every enemy's body.x/body.y from offsetX_/offsetY_ and
  // its fixed grid slot (row/col) -- positions are always derived, never
  // accumulated (plan §1 Decision 5), so a dead enemy's grid slot stays
  // coherent and a restart is a plain re-initialisation. body.w/body.h
  // (the alive/dead bit) are untouched by this call.
  void updateEnemyPositions();

  // Advances the formation by one tick: counts toward the next step, and
  // on a due step either marches every survivor kFormationStepX pixels
  // or reverses direction and descends once (plan §1 Decision 7,
  // US-3/AC-3.2/AC-3.3).
  void stepFormation();

  // US-5/A7/A13: any surviving enemy whose bottom edge has reached
  // kFormationThresholdY ends the round as a LOSS outright -- regardless
  // of lives_ remaining and regardless of invulnerability (AC-5.2/AC-10.6:
  // this failsafe closes the "dodge forever" exploit, so nothing about the
  // lives/invulnerability system is allowed to soften it). `contactResolved`
  // is AC-5.2's own precondition made explicit: the failsafe applies only
  // "without any entity directly overlapping the player" -- when
  // resolvePlayerContact() or resolveEnemyShotHits() has already resolved a
  // genuine (non-invulnerable) hit this same tick, that overlap is AC-5.1's
  // territory instead, and this one tick defers to it entirely rather than
  // also ending the round. Deliberately re-evaluated fresh every tick from
  // current enemy positions, with no latched "already saw this crossing"
  // state (review F1: an earlier version latched the crossing permanently
  // the first time it was seen, including a deferred one -- a single
  // coincidental hit on the very tick the formation first crossed the line
  // then silenced this failsafe for the rest of the round, since the
  // formation only ever descends, never re-crossing "fresh". The fix is
  // that this check has no memory at all: it fires on the very next tick
  // whose contact is *not* deferred, which for an ordinary hit is the tick
  // immediately after -- once invulnerability starts skipping further
  // contact checks, `contactResolved` reads false again and the failsafe
  // resumes, per AC-10.6's "regardless of invulnerability". Once it fires,
  // `outcome_` leaves NONE and GameSession leaves PLAYING, so this function
  // is never called again this round -- there is no "re-firing every tick"
  // to guard against, only the single tick of genuine overlap to respect).
  void checkFormationThreshold(bool contactResolved);

  // US-2: spawns/advances/removes the single player projectile.
  void stepPlayerShot(bool fireHeld);

  // US-4/US-11: checks the player's shot against every surviving enemy via
  // the shipped collision.h checkCollision (no bespoke overlap math in this
  // file, A3/AC-4.1) -- on a hit, destroys both and awards kScorePerKill;
  // if that leaves zero survivors, records outcome_ = WIN (AC-4.3/AC-11.1).
  // GameSession itself never learns of WIN -- see Outcome below.
  void resolvePlayerShotHits();

  // US-5/US-10: the player ship's own contact with a surviving enemy,
  // checked via the shipped checkCollision (A3/AC-4.1's "no bespoke
  // overlap math" applies here too) -- entirely skipped while
  // invulnTicks_ > 0 (AC-10.3: no life lost, no re-respawn, the enemy
  // itself untouched). On an actual hit, delegates to resolvePlayerHit()
  // and checks no further enemy this tick (plan §1 Decision 9). Returns
  // whether a genuine hit was resolved this tick -- checkFormationThreshold
  // uses this to implement AC-5.2's own "without any entity overlapping"
  // precondition, rather than re-deriving overlap a second time.
  bool resolvePlayerContact();

  // US-10/AC-5.1: the single hit-resolution rule (US-8's enemy fire will
  // call this unchanged). With lives_ > 1: decrements, clears any in-
  // flight player shot, respawns at kPlayerStartX, and starts the
  // kInvulnerabilityTicks window -- no ending this tick. At lives_ == 1:
  // records outcome_ = LOSS instead (AC-10.5) -- no respawn. Returns
  // whether this hit ended the round (LOSS), so callers can stop
  // resolving further contact once it has.
  bool resolvePlayerHit();

  // US-8/AC-8.1/AC-8.3: advances every in-flight enemy shot, removing one
  // that reaches the bottom edge; every kEnemyFireIntervalTicks ticks, if a
  // pool slot is free and at least one enemy survives, advances the LCG
  // once and spawns a new shot from the n-th surviving enemy's bottom-
  // centre, n = (state >> 16) % aliveCount.
  void stepEnemyShots();

  // US-8/AC-8.2: identical shape to resolvePlayerContact() but against the
  // enemy shot pool instead of enemy bodies -- skipped entirely while
  // invulnerable, and delegates to the same unchanged resolvePlayerHit()
  // on a hit. Returns whether a genuine hit was resolved, for the same
  // checkFormationThreshold() deferral resolvePlayerContact() feeds.
  bool resolveEnemyShotHits();

  // A3/AC-11.1: GameSession (game_state.h) is never modified to add a WIN
  // value -- both endings still route through sessionEnded=true into its
  // existing three-state GAME_OVER. This private enum is the only place
  // "which ending occurred" is recorded (NFR-5).
  enum class Outcome { NONE, WIN, LOSS };

  GameSession session_;

  // Horizontal-only: y is fixed at kPlayerY for the type's whole
  // lifetime (US-1/AC-1.3) -- only x ever changes, and only while
  // PLAYING.
  Entity player_{kPlayerStartX, kPlayerY, kPlayerWidth, kPlayerHeight};

  // US-7/US-10: displayed every PLAYING tick via the HUD (T4).
  int32_t score_ = 0;
  int32_t lives_ = kStartingLives;

  // US-10/A12: ticks of invulnerability remaining after a respawn; 0 means
  // vulnerable. Read at the top of a PLAYING tick (before
  // resolvePlayerContact()) and decremented at the bottom, so the tick a
  // hit sets it to kInvulnerabilityTicks does not itself decrement it
  // (plan §1 Decision 9) -- the window is exactly 120 ticks, not 119.
  int32_t invulnTicks_ = 0;

  // US-3: the formation. offsetX_/offsetY_ are added to every enemy's
  // grid-derived position; dir_ is +1 (moving toward +x) or -1;
  // stepTicks_ counts ticks since the last step, firing at
  // kFormationStepTicks.
  Enemy enemies_[kEnemyCount]{};
  int32_t offsetX_ = 0;
  int32_t offsetY_ = 0;
  int32_t dir_ = 1;
  int32_t stepTicks_ = 0;

  // US-2: w == 0 means "no shot in flight" -- the same zero-size-means-
  // absent convention Enemy uses (plan §1 Decision 5).
  Entity playerShot_{0, 0, 0, 0};

  // US-8 (Should): the enemy projectile pool (w == 0 means "free slot",
  // the same convention as playerShot_/Enemy), the ticks-until-next-fire
  // counter, and the explicit constant-seeded LCG state driving shooter
  // selection (A11) -- never <random>, never rand(), never a clock.
  Entity enemyShots_[kMaxEnemyShots]{};
  int32_t enemyFireTicks_ = 0;
  uint32_t rngState_ = kRngSeed;

  // US-11: which ending occurred, tracked outside GameState entirely.
  Outcome outcome_ = Outcome::NONE;
};

}  // namespace steamcore::games
