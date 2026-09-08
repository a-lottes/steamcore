#include "steamcore/initials_entry.h"

namespace steamcore {

void InitialsEntry::begin() {
  for (int32_t& letter : letters_) letter = 0;
  cursor_ = 0;
  complete_ = false;
  // R5: latched true, not false -- see this type's own header contract.
  prevUp_ = true;
  prevDown_ = true;
  prevFire_ = true;
}

bool InitialsEntry::update(const GameInput& input) {
  const bool upEdge = input.up && !prevUp_;
  const bool downEdge = input.down && !prevDown_;
  const bool fireEdge = input.fire && !prevFire_;
  prevUp_ = input.up;
  prevDown_ = input.down;
  prevFire_ = input.fire;

  if (complete_) return false;

  if (upEdge) {
    letters_[cursor_] = (letters_[cursor_] + 1) % kLetterCount;
  }
  if (downEdge) {
    letters_[cursor_] = (letters_[cursor_] - 1 + kLetterCount) % kLetterCount;
  }

  if (fireEdge) {
    ++cursor_;
    if (cursor_ >= kInitialsCount) {
      complete_ = true;
      return true;
    }
  }
  return false;
}

int32_t InitialsEntry::letterIndex(int32_t position) const {
  if (position < 0 || position >= kInitialsCount) return 0;
  return letters_[position];
}

char InitialsEntry::letter(int32_t position) const {
  return static_cast<char>(kFirstLetter + letterIndex(position));
}

}  // namespace steamcore
