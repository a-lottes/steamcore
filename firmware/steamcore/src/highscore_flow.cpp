#include "steamcore/highscore_flow.h"

#include "steamcore/highscore_screen.h"

namespace steamcore {

void HighscoreFlow::begin(int32_t score) {
  score_ = score;
  phase_ = Phase::ENTRY;
  entry_.begin();
  prevStart_ = true;  // TABLE's own latch, set again on entering TABLE below
}

HighscoreFlowStep HighscoreFlow::update(const GameInput& input) {
  HighscoreFlowStep step{};

  if (phase_ == Phase::ENTRY) {
    if (entry_.update(input)) {  // true exactly on the tick the third letter locks in
      for (int32_t i = 0; i < kInitialsCount; ++i) {
        initials_[i] = entry_.letter(i);
      }
      phase_ = Phase::TABLE;
      // R5, restated for this flow's own `start` tracker: latched `true`
      // so a `start` already held the instant TABLE is entered cannot
      // read as a fresh edge (AC-3.3 needs a genuine rising edge).
      prevStart_ = true;
      step.submitted = true;
    }
  } else if (phase_ == Phase::TABLE) {
    const bool startEdge = input.start && !prevStart_;
    prevStart_ = input.start;
    if (startEdge) {
      phase_ = Phase::INACTIVE;
      step.finished = true;
    }
  }

  step.active = active();
  return step;
}

void HighscoreFlow::initials(char (&out)[kInitialsCount]) const {
  for (int32_t i = 0; i < kInitialsCount; ++i) out[i] = initials_[i];
}

void HighscoreFlow::render(Framebuffer& fb, const HighscoreTable& table,
                            const char* displayName) const {
  if (phase_ == Phase::ENTRY) {
    drawInitialsEntryScreen(fb, score_, entry_);
  } else if (phase_ == Phase::TABLE) {
    drawHighscoreTableScreen(fb, displayName, table);
  }
  // INACTIVE: draws nothing.
}

}  // namespace steamcore
