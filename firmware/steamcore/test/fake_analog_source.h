#pragma once

#include "steamcore/analog_axis.h"
#include "steamcore/input.h"

// A scripted fake Source for InputReader<Source>, mirroring
// FakeInputSource -- but shaped for AnalogJoystickSource: two raw ADC
// axis values plus three scripted digital button levels. Composes the
// same pure axisLevel()/directionActive() the real device-side Source
// will use (analog-joystick-input plan §1 Decisions 2/5), so a test
// driving this fake exercises the identical pure logic path. Test-only:
// not part of the engine's public surface.

namespace steamcore::test {

class FakeAnalogSource {
 public:
  void setRawX(int32_t raw) { rawX_ = raw; }
  void setRawY(int32_t raw) { rawY_ = raw; }

  void setButtonLevel(InputSignal signal, bool raw) {
    switch (signal) {
      case InputSignal::kStart:
        start_ = raw;
        break;
      case InputSignal::kFire:
        fire_ = raw;
        break;
      case InputSignal::kSelect:
        select_ = raw;
        break;
      default:
        break;
    }
  }

  bool readSignal(InputSignal signal) const {
    switch (signal) {
      case InputSignal::kStart:
        return start_;
      case InputSignal::kFire:
        return fire_;
      case InputSignal::kSelect:
        return select_;
      default:
        return directionActive(signal, axisLevel(rawX_), axisLevel(rawY_));
    }
  }

 private:
  int32_t rawX_ = kAxisAdcCenter;
  int32_t rawY_ = kAxisAdcCenter;
  bool start_ = false;
  bool fire_ = false;
  bool select_ = false;
};

}  // namespace steamcore::test
