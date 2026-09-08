#include "galactic_invasion/galactic_invasion.h"

#include "galactic_invasion_fixture.h"
#include "steamcore/color.h"
#include "steamcore/framebuffer.h"
#include "steamcore/game_loop.h"
#include "test_harness.h"

using steamcore::Color;
using steamcore::Framebuffer;
using steamcore::GameInput;
using steamcore::GameLoop;
using steamcore::games::GalacticInvasion;
using steamcore::games::kEnemyCols;
using steamcore::games::kEnemyPitchX;
using steamcore::games::kEnemyPitchY;
using steamcore::games::kEnemyRows;
using steamcore::games::kEnemySprite;
using steamcore::games::kEnemyWidth;
using steamcore::games::kFormationDescendY;
using steamcore::games::kFormationStartX;
using steamcore::games::kFormationStartY;
using steamcore::games::kFormationStepTicks;
using steamcore::games::kFormationStepX;
using steamcore::test::findFormationBounds;
using steamcore::test::FormationBounds;
using steamcore::test::runTicks;

namespace {

void enterPlaying(GameLoop<GalacticInvasion>& loop) {
  loop.tick(GameInput{});
  loop.tick(GameInput{/*start=*/true});
}

// Builds a reference frame with 18 enemies blitted at the grid formula's
// own positions, computed independently of GalacticInvasion's internals
// -- never by calling updateEnemyPositions().
Framebuffer buildExpectedFormationFrame(int32_t offsetX, int32_t offsetY) {
  Framebuffer fb;
  for (int32_t row = 0; row < kEnemyRows; ++row) {
    for (int32_t col = 0; col < kEnemyCols; ++col) {
      const int32_t x = kFormationStartX + col * kEnemyPitchX + offsetX;
      const int32_t y = kFormationStartY + row * kEnemyPitchY + offsetY;
      fb.blit(kEnemySprite, x, y);
    }
  }
  return fb;
}

}  // namespace

// AC-3.1: a fresh round renders exactly the 18 computed grid slots.
STEAMCORE_TEST(galactic_invasion_fresh_formation_matches_grid) {
  GalacticInvasion game;
  Framebuffer fb;
  GameLoop<GalacticInvasion> loop(game, fb);
  enterPlaying(loop);

  const Framebuffer expected = buildExpectedFormationFrame(0, 0);
  for (int32_t y = kFormationStartY; y < kFormationStartY + 60; ++y) {
    for (int32_t x = 0; x < Framebuffer::width(); ++x) {
      CHECK(fb.pixel(x, y) == expected.pixel(x, y));
    }
  }
}

// AC-3.2: no movement on ticks 1..(kFormationStepTicks-1); every
// survivor moves by exactly kFormationStepX on the step-due tick.
STEAMCORE_TEST(galactic_invasion_formation_moves_only_on_the_due_tick) {
  GalacticInvasion game;
  Framebuffer fb;
  GameLoop<GalacticInvasion> loop(game, fb);
  enterPlaying(loop);

  const FormationBounds start = findFormationBounds(fb);

  for (int32_t i = 1; i < kFormationStepTicks; ++i) {
    loop.tick(GameInput{});
    const FormationBounds b = findFormationBounds(fb);
    CHECK_EQ(b.minX, start.minX);
    CHECK_EQ(b.maxX, start.maxX);
  }

  loop.tick(GameInput{});  // the kFormationStepTicks-th tick: due
  const FormationBounds afterStep = findFormationBounds(fb);
  CHECK_EQ(afterStep.minX, start.minX + kFormationStepX);
  CHECK_EQ(afterStep.maxX, start.maxX + kFormationStepX);
  CHECK_EQ(afterStep.minY, start.minY);  // no descent on an ordinary step
}

// AC-3.3: running the formation all the way to the right edge produces
// exactly one reversal-and-descend, with no horizontal movement on that
// specific step, and the formation continues moving in the new
// (leftward) direction afterward -- across two full sweeps, the
// formation's rendered bounding box never leaves [0, width()).
STEAMCORE_TEST(galactic_invasion_formation_reverses_and_descends_at_the_edge) {
  GalacticInvasion game;
  Framebuffer fb;
  GameLoop<GalacticInvasion> loop(game, fb);
  enterPlaying(loop);

  const FormationBounds start = findFormationBounds(fb);
  CHECK(start.minX >= 0);

  // Step until maxX would next exceed the screen -- track bounds after
  // every step and assert on-screen throughout, rather than assuming
  // the exact step count (keeps this test valid even if the geometry
  // constants are retuned later).
  FormationBounds previous = start;
  bool sawReversal = false;
  bool sawDescend = false;
  int32_t dirSign = 1;  // +1 while maxX still growing, matches dir_'s start

  for (int32_t step = 0; step < 60 && !sawReversal; ++step) {
    runTicks(loop, GameInput{}, kFormationStepTicks);
    const FormationBounds current = findFormationBounds(fb);
    CHECK(current.minX >= 0);
    CHECK(current.maxX <= Framebuffer::width());

    if (current.minY > previous.minY) {
      // A descend step: no horizontal movement on this exact step.
      CHECK_EQ(current.minX, previous.minX);
      CHECK_EQ(current.maxX, previous.maxX);
      CHECK_EQ(current.minY, previous.minY + kFormationDescendY);
      sawReversal = true;
      sawDescend = true;
    } else {
      CHECK_EQ(current.minX, previous.minX + dirSign * kFormationStepX);
    }
    previous = current;
  }
  CHECK(sawDescend);

  // After the reversal, the formation moves left (dirSign flips) --
  // confirmed by the next ordinary step decreasing minX.
  runTicks(loop, GameInput{}, kFormationStepTicks);
  const FormationBounds afterReversalStep = findFormationBounds(fb);
  CHECK_EQ(afterReversalStep.minX, previous.minX - kFormationStepX);

  // Continue for a second full sweep back toward (and past) the start,
  // checking on-screen bounds at every step boundary throughout.
  FormationBounds sweepBack = afterReversalStep;
  for (int32_t step = 0; step < 60; ++step) {
    runTicks(loop, GameInput{}, kFormationStepTicks);
    const FormationBounds current = findFormationBounds(fb);
    CHECK(current.minX >= 0);
    CHECK(current.maxX <= Framebuffer::width());
    sweepBack = current;
  }
  (void)sweepBack;
}
