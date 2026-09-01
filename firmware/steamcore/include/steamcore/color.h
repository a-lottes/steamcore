#pragma once

#include <cstdint>

namespace steamcore {

// The engine's whole palette. Exactly these four values exist — an
// invalid palette index cannot be constructed, so there is no
// truncate/mask/assert question for out-of-range colour (constitution §3).
enum class Color : uint8_t {
  BLACK = 0,
  DARK_ORANGE = 1,
  ORANGE = 2,
  BRIGHT_ORANGE = 3,
};

}  // namespace steamcore
