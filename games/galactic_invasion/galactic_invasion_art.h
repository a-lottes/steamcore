#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "steamcore/color.h"
#include "steamcore/sprite.h"

// The three hand-authored pixel-art sprites this game ships, in exactly
// font.cpp's own convention (galactic-invasion plan §1 Decision 10):
// picture rows written as ' '/'#' string literals, converted to Color by
// a constexpr function, validated at compile time. No PNG decoding, no
// runtime asset pipeline, no build-time tool (spec A14).
//
// One on-colour per sprite, not per-pixel shading -- identity comes from
// silhouette alone (AC-12.4, constitution §6 "clear silhouettes"):
// player BRIGHT_ORANGE (a vertically-symmetric wedge, narrow at the nose,
// wide at the base -- "this is the thing I steer"), enemy ORANGE (a
// horizontally-symmetric blocky/notched invader profile with antennae
// and legs -- the opposite shape family from the ship), projectile
// BRIGHT_ORANGE (a slim 2-wide bar -- a different aspect ratio from
// both, so its motion reads as "a shot" at a glance).

namespace steamcore::games {

inline constexpr int32_t kPlayerWidth = 12;
inline constexpr int32_t kPlayerHeight = 12;
inline constexpr int32_t kEnemyWidth = 12;
inline constexpr int32_t kEnemyHeight = 10;
inline constexpr int32_t kProjectileWidth = 2;
inline constexpr int32_t kProjectileHeight = 6;

namespace detail {

// clang-format off
inline constexpr const char* kPlayerRows[kPlayerHeight] = {
    "     ##     ",
    "     ##     ",
    "    ####    ",
    "    ####    ",
    "   ######   ",
    "   ######   ",
    "  ########  ",
    "  ########  ",
    " ########## ",
    " ########## ",
    "############",
    "############",
};

inline constexpr const char* kEnemyRows[kEnemyHeight] = {
    "  ##    ##  ",
    "   ######   ",
    "  ########  ",
    " ########## ",
    "############",
    "############",
    "## ###### ##",
    "##  ####  ##",
    "  ##    ##  ",
    " ##      ## ",
};

inline constexpr const char* kProjectileRows[kProjectileHeight] = {
    "##",
    "##",
    "##",
    "##",
    "##",
    "##",
};
// clang-format on

// Reading `row[width]` here also doubles as the width check, exactly
// font.cpp's own rowIsExactWidth trick: a row string literal shorter
// than `width` characters makes this an out-of-bounds read, which
// constant evaluation itself rejects as not a constant expression --
// caught at compile time with no explicit length check.
constexpr bool rowIsExactWidth(const char* row, int32_t width) {
  return row[width] == '\0';
}

// A non-constexpr function: calling it during constant evaluation is not
// a constant expression, so hitting it below fails the BUILD. Given a
// real, out-of-line body (never left declared-only) so host clang's
// `-Wundefined-internal` doesn't trip on a function that must never
// actually execute -- font.cpp's own precedent. std::abort(), never
// `throw`: ESP-IDF's device build disables exceptions (-fno-exceptions),
// which host clang/g++ never do -- the exact device-build scar font.cpp
// carried until start-screen T11 found it. Explicit <cstdlib> include
// here too, never relied on transitively -- font.cpp's second scar.
// `inline`: unlike font.cpp's own reportInvalidGlyphArt (defined in a
// .cpp, included by exactly one TU), this header-only file is included
// by multiple translation units, so a non-inline definition here would
// violate the one-definition rule at link time.
[[noreturn]] inline void reportInvalidSpriteArt() { std::abort(); }

constexpr Color spritePixel(const char* row, int32_t col, int32_t width,
                             Color on) {
  if (!rowIsExactWidth(row, width)) {
    reportInvalidSpriteArt();
    return Color::BLACK;
  }
  if (row[col] == '#') return on;
  if (row[col] == ' ') return Color::BLACK;
  reportInvalidSpriteArt();
  return Color::BLACK;
}

constexpr std::array<Color, kPlayerWidth * kPlayerHeight> buildPlayerSprite() {
  std::array<Color, kPlayerWidth * kPlayerHeight> pixels{};
  int32_t offset = 0;
  for (int32_t row = 0; row < kPlayerHeight; ++row) {
    for (int32_t col = 0; col < kPlayerWidth; ++col) {
      pixels[static_cast<size_t>(offset++)] =
          spritePixel(kPlayerRows[row], col, kPlayerWidth,
                      Color::BRIGHT_ORANGE);
    }
  }
  return pixels;
}

constexpr std::array<Color, kEnemyWidth * kEnemyHeight> buildEnemySprite() {
  std::array<Color, kEnemyWidth * kEnemyHeight> pixels{};
  int32_t offset = 0;
  for (int32_t row = 0; row < kEnemyHeight; ++row) {
    for (int32_t col = 0; col < kEnemyWidth; ++col) {
      pixels[static_cast<size_t>(offset++)] =
          spritePixel(kEnemyRows[row], col, kEnemyWidth, Color::ORANGE);
    }
  }
  return pixels;
}

constexpr std::array<Color, kProjectileWidth * kProjectileHeight>
buildProjectileSprite() {
  std::array<Color, kProjectileWidth * kProjectileHeight> pixels{};
  int32_t offset = 0;
  for (int32_t row = 0; row < kProjectileHeight; ++row) {
    for (int32_t col = 0; col < kProjectileWidth; ++col) {
      pixels[static_cast<size_t>(offset++)] =
          spritePixel(kProjectileRows[row], col, kProjectileWidth,
                      Color::BRIGHT_ORANGE);
    }
  }
  return pixels;
}

// US-8: the enemy's return-fire shot reuses the player projectile's own
// slim-bar silhouette (same shape, same aspect ratio -- both read as "a
// shot" at a glance) but in Color::DARK_ORANGE, not BRIGHT_ORANGE -- the
// one palette shade nothing else in this game uses. This is deliberate,
// not just a visual nicety: every existing test (T1-T9) that scans for
// BRIGHT_ORANGE to find "the player's own shot" would otherwise also match
// an enemy shot sharing the screen, breaking assumptions built before this
// story existed. Blit has no per-call colour override (it copies each
// sprite's own baked-in Color), so this is its own small array, not a
// tinted reuse of kProjectileSprite.
constexpr std::array<Color, kProjectileWidth * kProjectileHeight>
buildEnemyShotSprite() {
  std::array<Color, kProjectileWidth * kProjectileHeight> pixels{};
  int32_t offset = 0;
  for (int32_t row = 0; row < kProjectileHeight; ++row) {
    for (int32_t col = 0; col < kProjectileWidth; ++col) {
      pixels[static_cast<size_t>(offset++)] =
          spritePixel(kProjectileRows[row], col, kProjectileWidth,
                      Color::DARK_ORANGE);
    }
  }
  return pixels;
}

inline constexpr std::array<Color, kPlayerWidth * kPlayerHeight>
    kPlayerPixels = buildPlayerSprite();
inline constexpr std::array<Color, kEnemyWidth * kEnemyHeight> kEnemyPixels =
    buildEnemySprite();
inline constexpr std::array<Color, kProjectileWidth * kProjectileHeight>
    kProjectilePixels = buildProjectileSprite();
inline constexpr std::array<Color, kProjectileWidth * kProjectileHeight>
    kEnemyShotPixels = buildEnemyShotSprite();

static_assert(sizeof(kPlayerPixels) / sizeof(Color) ==
                  static_cast<size_t>(kPlayerWidth * kPlayerHeight),
              "kPlayerPixels must hold exactly kPlayerWidth*kPlayerHeight cells");
static_assert(sizeof(kEnemyPixels) / sizeof(Color) ==
                  static_cast<size_t>(kEnemyWidth * kEnemyHeight),
              "kEnemyPixels must hold exactly kEnemyWidth*kEnemyHeight cells");
static_assert(sizeof(kProjectilePixels) / sizeof(Color) ==
                  static_cast<size_t>(kProjectileWidth * kProjectileHeight),
              "kProjectilePixels must hold exactly "
              "kProjectileWidth*kProjectileHeight cells");
static_assert(sizeof(kEnemyShotPixels) / sizeof(Color) ==
                  static_cast<size_t>(kProjectileWidth * kProjectileHeight),
              "kEnemyShotPixels must hold exactly "
              "kProjectileWidth*kProjectileHeight cells");

}  // namespace detail

inline constexpr Sprite kPlayerSprite{detail::kPlayerPixels.data(),
                                       kPlayerWidth, kPlayerHeight,
                                       /*stride=*/kPlayerWidth};
inline constexpr Sprite kEnemySprite{detail::kEnemyPixels.data(), kEnemyWidth,
                                      kEnemyHeight, /*stride=*/kEnemyWidth};
inline constexpr Sprite kProjectileSprite{detail::kProjectilePixels.data(),
                                           kProjectileWidth,
                                           kProjectileHeight,
                                           /*stride=*/kProjectileWidth};
inline constexpr Sprite kEnemyShotSprite{detail::kEnemyShotPixels.data(),
                                          kProjectileWidth, kProjectileHeight,
                                          /*stride=*/kProjectileWidth};

static_assert(kPlayerSprite.width == kPlayerWidth &&
                  kPlayerSprite.height == kPlayerHeight &&
                  kPlayerSprite.stride == kPlayerWidth,
              "kPlayerSprite must be tightly packed at its declared size");
static_assert(kEnemySprite.width == kEnemyWidth &&
                  kEnemySprite.height == kEnemyHeight &&
                  kEnemySprite.stride == kEnemyWidth,
              "kEnemySprite must be tightly packed at its declared size");
static_assert(kProjectileSprite.width == kProjectileWidth &&
                  kProjectileSprite.height == kProjectileHeight &&
                  kProjectileSprite.stride == kProjectileWidth,
              "kProjectileSprite must be tightly packed at its declared size");
static_assert(kEnemyShotSprite.width == kProjectileWidth &&
                  kEnemyShotSprite.height == kProjectileHeight &&
                  kEnemyShotSprite.stride == kProjectileWidth,
              "kEnemyShotSprite must be tightly packed at its declared size");

}  // namespace steamcore::games
