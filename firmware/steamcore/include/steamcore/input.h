#pragma once

#include <cstdint>

#include "steamcore/game_loop.h"

// The pure/impure seam (input-driver spec US-1/US-2, plan §1 Decision 2):
// InputReader owns the whole sample -> debounce -> populate loop,
// parameterized on a Source type rather than a virtual interface -- the
// same compile-time-binding rationale TilePusher<Transmitter> already
// established for this codebase (no vtable, nothing virtual exists
// anywhere in it).
//
// A Source must provide:
//   bool readSignal(InputSignal signal) const;
// returning the LOGICAL raw level for that signal ("pressed" == true).
// Electrical polarity (e.g. active-low with an internal pull-up) is the
// Source's own concern -- steamcore::port::esp32::GpioInputSource
// satisfies this concept on the real device; host tests supply a fake
// source that replays a scripted per-tick bounce pattern.
//
// Example (steamcore::port::esp32::Ili9488Display's own header shows the
// device-side equivalent of this pattern):
//   InputReader<MySource> reader;
//   MySource source;
//   const GameInput input = reader.read(source);  // once per tick
//
// One mechanism (Debouncer), instantiated seven independent times inside
// InputReader -- never seven bespoke per-signal lookalikes (AC-2.2, A10).
//
// Contract: single-threaded, nothing throws, no error code, no dynamic
// allocation -- same inherited contract as every other steamcore type.
// Reads no clock and no RNG: a tick is the only timing input
// (constitution §3/§4, NFR-3) -- unlike display-output timing
// (display-driver NFR-5), input feeds game logic directly, so a clock
// here would break GameLoop's replay-determinism guarantee. Returns a
// LEVEL, never an edge: GameSession (game_state.h) is the sole owner of
// edge-detection, and only for `start`.

namespace steamcore {

// One of GameInput's seven signals, in exactly GameInput's own
// declaration order. No explicit enumerator values -- a signal is
// named, never numbered, the same convention game_state.h's GameState
// already established.
enum class InputSignal {
  kStart,
  kFire,
  kSelect,
  kUp,
  kDown,
  kLeft,
  kRight,
};

inline constexpr int32_t kInputSignalCount = 7;

// The debounce window, in samples: a raw level must agree with itself
// for this many consecutive ticks before Debouncer reports it as the
// new stable level. Not milliseconds -- the caller's tick rate is the
// only timing input (NFR-3). At the 60Hz tick this is a <=33ms
// confirmation window (NFR-1; the chosen value is recorded in qa.md).
inline constexpr int32_t kDebounceSamples = 2;

// N-consecutive-agreeing-samples debounce for one signal. No clock, no
// history word: a raw sample that disagrees with the current stable
// level increments a counter; an agreeing one resets it; the level
// flips only once the counter reaches `Samples`. `Samples` is a
// non-type template parameter (defaulted to kDebounceSamples) so tests
// can prove the threshold at 1, 2 and 5 without editing the mechanism.
template <int32_t Samples = kDebounceSamples>
class Debouncer {
  static_assert(Samples >= 1,
                "Debouncer needs at least one agreeing sample to flip; "
                "Samples <= 0 would silently behave like Samples == 1");

 public:
  // Feeds one raw sample in and returns the (possibly unchanged)
  // current stable level.
  bool sample(bool raw) {
    if (raw == level_) {
      counter_ = 0;
    } else if (++counter_ >= Samples) {
      level_ = raw;
      counter_ = 0;
    }
    return level_;
  }

  bool level() const { return level_; }

 private:
  bool level_ = false;
  int32_t counter_ = 0;
};

namespace detail {

// GameInput's seven fields, addressed by pointer-to-member, in exactly
// InputSignal's (== GameInput's) declaration order -- the one place a
// signal's identity and its GameInput field are connected. No signal
// name appears anywhere else in this file's control flow:
// InputReader::read() is a single loop over this table, so a bespoke
// per-signal code path cannot be written without deleting the loop.
inline constexpr bool GameInput::* kInputFields[kInputSignalCount] = {
    &GameInput::start, &GameInput::fire,  &GameInput::select,
    &GameInput::up,    &GameInput::down,  &GameInput::left,
    &GameInput::right,
};

// Guards against `kInputSignalCount` growing without this initializer
// list growing to match: a short initializer list silently zero-pads the
// remaining entries to a null member pointer instead of failing to
// compile, and `input.*nullptr` is undefined behavior, not a caught
// error. Checking the last entry is enough -- a short list always
// leaves it null.
//
// The real ESP-IDF GCC toolchain (unlike host clang) applies -Waddress
// to this comparison and, correctly but unhelpfully, observes that
// `&GameInput::right` -- what `kInputFields[kInputSignalCount - 1]`
// constant-folds to today -- can never be null; it has no way to know
// this check exists for a FUTURE miscount, not today's already-correct
// list. Suppressed narrowly, GCC only (clang defines __GNUC__ too but
// also __clang__, and never warned here).
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress"
#endif
static_assert(kInputFields[kInputSignalCount - 1] != nullptr,
              "kInputFields must have exactly kInputSignalCount entries");
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

}  // namespace detail

// Drives one Source through all seven signals, once per call, returning
// a fully populated GameInput -- a partially populated GameInput is
// unrepresentable. Owns seven independent Debouncer<Samples> instances,
// one per signal; no shared state between them.
template <typename Source, int32_t Samples = kDebounceSamples>
class InputReader {
 public:
  GameInput read(Source& source) {
    GameInput input;
    for (int32_t i = 0; i < kInputSignalCount; ++i) {
      const InputSignal signal = static_cast<InputSignal>(i);
      const bool level = debouncers_[i].sample(source.readSignal(signal));
      input.*detail::kInputFields[i] = level;
    }
    return input;
  }

 private:
  Debouncer<Samples> debouncers_[kInputSignalCount];
};

}  // namespace steamcore
