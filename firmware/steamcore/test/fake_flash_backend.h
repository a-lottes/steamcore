#pragma once

#include <cstdint>

#include "steamcore/highscore.h"

// A host stand-in for real flash storage (NVS, T14's own
// `port::esp32::NvsHighscoreBackend`) -- a plain byte array plus a
// "has anything ever been written" flag (mirroring a never-written NVS
// key returning "not found", not an error) and an optional scripted
// write failure, so HighscoreStore's own handling of a missing or
// failing backend is testable without real hardware.
namespace steamcore::test {

class FakeFlashBackend {
 public:
  bool read(uint8_t* out, int32_t length) const {
    if (!present_) return false;
    for (int32_t i = 0; i < length; ++i) out[i] = bytes_[i];
    return true;
  }

  bool write(const uint8_t* data, int32_t length) {
    if (failWrites_) return false;
    for (int32_t i = 0; i < length; ++i) bytes_[i] = data[i];
    present_ = true;
    return true;
  }

  // Test-only controls, never part of the `Backend` concept itself.
  void setFailWrites(bool fail) { failWrites_ = fail; }
  uint8_t* rawBytes() { return bytes_; }
  bool present() const { return present_; }

 private:
  uint8_t bytes_[kBlockSize] = {};
  bool present_ = false;
  bool failWrites_ = false;
};

}  // namespace steamcore::test
