#pragma once

#include <cstdint>

// Internal clipping helper shared by pixel, rect and blit
// operations — one place to get the arithmetic right instead of three
// (plan §1 Decision 5 / Risks). Not a public header: lives in src/, never
// under include/steamcore/.

namespace steamcore::detail {

// A rectangle clipped against [0, boundsWidth) x [0, boundsHeight). When
// `visible` is false the rectangle is entirely off-screen (or had
// non-positive width/height to begin with) and x/y/width/height are 0.
struct ClippedRect {
  int32_t x = 0;
  int32_t y = 0;
  int32_t width = 0;
  int32_t height = 0;
  bool visible = false;
};

// Clips [x, x+width) x [y, y+height) against the given bounds. All
// arithmetic happens in int64_t: every int32_t input (including
// INT32_MIN/INT32_MAX, and a width/height that would make x+width or
// y+height overflow signed 32-bit) is representable without overflow, so
// the result is computed exactly before being narrowed back to int32_t.
inline ClippedRect clipRect(int32_t x, int32_t y, int32_t width,
                             int32_t height, int32_t boundsWidth,
                             int32_t boundsHeight) {
  if (width <= 0 || height <= 0) return ClippedRect{};

  const int64_t x0 = static_cast<int64_t>(x);
  const int64_t y0 = static_cast<int64_t>(y);
  const int64_t x1 = x0 + static_cast<int64_t>(width);   // exclusive
  const int64_t y1 = y0 + static_cast<int64_t>(height);  // exclusive

  const int64_t clippedX0 = x0 < 0 ? 0 : x0;
  const int64_t clippedY0 = y0 < 0 ? 0 : y0;
  const int64_t clippedX1 = x1 > boundsWidth ? boundsWidth : x1;
  const int64_t clippedY1 = y1 > boundsHeight ? boundsHeight : y1;

  if (clippedX1 <= clippedX0 || clippedY1 <= clippedY0) return ClippedRect{};

  ClippedRect result;
  result.x = static_cast<int32_t>(clippedX0);
  result.y = static_cast<int32_t>(clippedY0);
  result.width = static_cast<int32_t>(clippedX1 - clippedX0);
  result.height = static_cast<int32_t>(clippedY1 - clippedY0);
  result.visible = true;
  return result;
}

// True iff (x, y) falls inside [0, boundsWidth) x [0, boundsHeight). No
// arithmetic combination of two int32_t values here can overflow.
inline bool pixelInBounds(int32_t x, int32_t y, int32_t boundsWidth,
                           int32_t boundsHeight) {
  return x >= 0 && x < boundsWidth && y >= 0 && y < boundsHeight;
}

}  // namespace steamcore::detail
