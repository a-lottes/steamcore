#pragma once

#include "steamcore/input.h"

// A scripted, per-tick raw-level fake for InputReader<Source> -- mirrors
// TilePusher<Transmitter>'s FakeTransmitter test double. setLevel(signal,
// raw) sets what readSignal() reports for that signal from the next
// read() call onward, until changed again; tests build a bounce pattern
// by calling this once per simulated tick, between InputReader::read()
// calls. Test-only: not part of the engine's public surface.

namespace steamcore::test {

class FakeInputSource {
 public:
  void setLevel(InputSignal signal, bool raw) {
    levels_[static_cast<int32_t>(signal)] = raw;
  }

  bool readSignal(InputSignal signal) const {
    return levels_[static_cast<int32_t>(signal)];
  }

 private:
  bool levels_[kInputSignalCount] = {};
};

}  // namespace steamcore::test
