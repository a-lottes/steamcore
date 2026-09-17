#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "galactic_invasion/galactic_invasion_generated_art.h"
#include "steamcore/color.h"
#include "steamcore/sprite.h"

// The four pixel-art sprites this game ships, in exactly font.cpp's own
// row-string convention (galactic-invasion plan §1 Decision 10): picture
// rows written as string literals, converted to Color by a constexpr
// function, validated at compile time. No PNG decoding, no runtime
// asset pipeline, and no build-time tool are ever part of the firmware
// build (galactic-invasion-artwork AC-3.2) -- that holds regardless of
// which of the two routes below a sprite's *data* actually took.
//
// Two routes coexist here (galactic-invasion-artwork plan §1 decisions
// 2/6). The player sprite is sourced from tools/generate_sprite_data.py's
// generated header (galactic_invasion_generated_art.h, T9) -- a
// generated sprite is expressed in this exact row-string convention too,
// just produced by a tool run instead of typed by hand. The enemy and
// both shots are hand-authored (AC-3.11) here directly: the enemy was
// generated and adopted in T10 (its FIGHTER-derived silhouette passed
// the ≥2-outline-concavities mechanical gate), then reverted in T13
// after `/look-and-feel`'s design review found it failed AC-2.4a's
// qualitative "opposite family from the player" read -- the generated
// shape was a symmetric diamond sharing the player's own bilateral
// symmetry, not the horizontally-elongated, notched silhouette the
// shipped invader (this project's original release) already had. Kept
// per AC-2.7 and brought into this feature's shading language by hand
// (a DARK_ORANGE interior core, ORANGE outline) rather than shipped
// unchanged. The two shots stay hand-authored and always will be: each
// is 2x6 = twelve cells, too small for a downscale to derive anything a
// human wouldn't type faster directly.
//
// Shading: up to three of the four palette inks are allowed per sprite
// (spec A7), under one outline/interior split that never moves
// (AC-2.3) -- BRIGHT_ORANGE marks the player side's silhouette,
// ORANGE the enemy side's, and DARK_ORANGE is interior-only, never on a
// sprite narrower than 3px (a 2-wide shot has no interior at all). Both
// shots stay flat and single-ink by design (AC-2.6): twelve cells carry
// no shading. Identity for the 2x6 pair comes from silhouette, not
// colour alone (AC-3.10/AC-2.4a): the enemy shot's silhouette is
// deliberately segmented, unlike the player shot's solid bar -- ORANGE
// was chosen for it over the shipped game's original DARK_ORANGE
// because 1.594:1 contrast is below every floor for the one object the
// player must react to (AC-3.10/NFR-7).

namespace steamcore::games {

inline constexpr int32_t kPlayerWidth = 12;
inline constexpr int32_t kPlayerHeight = 12;
inline constexpr int32_t kEnemyWidth = 12;
inline constexpr int32_t kEnemyHeight = 10;
inline constexpr int32_t kProjectileWidth = 2;
inline constexpr int32_t kProjectileHeight = 6;

namespace detail {

// clang-format off
// Hand-authored, not generated (AC-3.11): the shipped invader silhouette
// (this project's original release) reinstated after T13's
// `/look-and-feel` review rejected the T10-adopted generated version on
// AC-2.4a's qualitative "opposite family from the player" clause (see
// the file-level comment above) -- horizontally-symmetric, notched
// antennae/shoulders/legs, unchanged from the original shape. '#' marks
// the ORANGE outline, '+' a DARK_ORANGE interior core (rows 3-7, cols
// 4-7, a contiguous 4x5 block, comfortably inside AC-2.10(i)'s
// bounding-box ring and D6's >=3px-wide rule) -- the one edit AC-2.7
// calls for: bringing hand-authored art into this feature's shading
// language without regenerating it.
inline constexpr const char* kEnemyRows[kEnemyHeight] = {
    "  ##    ##  ",
    "   ######   ",
    "  ########  ",
    " ###++++### ",
    "####++++####",
    "####++++####",
    "## #++++# ##",
    "##  ++++  ##",
    "  ##    ##  ",
    " ##      ## ",
};

// Hand-authored, not generated (AC-3.11): twelve cells leave nothing to
// derive. Kept as the shipped solid bar (AC-2.7) -- flat, one ink,
// categorically (AC-2.6).
inline constexpr const char* kProjectileRows[kProjectileHeight] = {
    "##",
    "##",
    "##",
    "##",
    "##",
    "##",
};

// Hand-authored, not generated (AC-3.11): twelve cells leave nothing to
// derive. Deliberately segmented rather than reusing kProjectileRows
// (AC-3.10) -- with the player shot now BRIGHT_ORANGE-exclusive and the
// enemy shot ORANGE, colour alone already separates the two on this
// hardware, but AC-2.4a and NFR-7 both require the pair to stay
// distinguishable with colour ignored entirely, and a 2-wide sprite has
// no interior to shade (AC-2.3). A silhouette split is therefore the
// only tool left, and the only one that survives a monochrome or
// colour-blind read of the screen.
inline constexpr const char* kEnemyShotRows[kProjectileHeight] = {
    "##",
    "##",
    "  ",
    "  ",
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

// Two-ink variant of spritePixel above, for kEnemyRows' shading (T13):
// '#' is the outline ink, '+' the interior one, ' ' stays BLACK. Every
// other row array in this file uses spritePixel's single-ink form, so
// this stays local to the one sprite that needs it rather than widening
// spritePixel itself and touching every other call site.
constexpr Color spritePixel2(const char* row, int32_t col, int32_t width,
                              Color outlineOn, Color interiorOn) {
  if (!rowIsExactWidth(row, width)) {
    reportInvalidSpriteArt();
    return Color::BLACK;
  }
  if (row[col] == '#') return outlineOn;
  if (row[col] == '+') return interiorOn;
  if (row[col] == ' ') return Color::BLACK;
  reportInvalidSpriteArt();
  return Color::BLACK;
}

constexpr std::array<Color, kEnemyWidth * kEnemyHeight> buildEnemySprite() {
  std::array<Color, kEnemyWidth * kEnemyHeight> pixels{};
  int32_t offset = 0;
  for (int32_t row = 0; row < kEnemyHeight; ++row) {
    for (int32_t col = 0; col < kEnemyWidth; ++col) {
      pixels[static_cast<size_t>(offset++)] =
          spritePixel2(kEnemyRows[row], col, kEnemyWidth, Color::ORANGE,
                       Color::DARK_ORANGE);
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

// US-8/galactic-invasion-artwork AC-3.10: the enemy's return-fire shot
// has its own segmented silhouette (kEnemyShotRows above), not the
// player projectile's solid bar, in Color::ORANGE -- the enemy side's
// own identity ink (AC-2.3), not the shipped game's original
// DARK_ORANGE (1.594:1 contrast, below every floor for the one object
// the player must react to, NFR-7). Blit has no per-call colour override
// (it copies each sprite's own baked-in Color), so this is its own small
// array either way, not a tinted reuse of kProjectileSprite.
constexpr std::array<Color, kProjectileWidth * kProjectileHeight>
buildEnemyShotSprite() {
  std::array<Color, kProjectileWidth * kProjectileHeight> pixels{};
  int32_t offset = 0;
  for (int32_t row = 0; row < kProjectileHeight; ++row) {
    for (int32_t col = 0; col < kProjectileWidth; ++col) {
      pixels[static_cast<size_t>(offset++)] =
          spritePixel(kEnemyShotRows[row], col, kProjectileWidth,
                      Color::ORANGE);
    }
  }
  return pixels;
}

// Sourced from the offline generator (galactic-invasion-artwork T9), not
// hand-typed rows: kPlayerGeneratedPixels lives in
// galactic_invasion_generated_art.h's own detail namespace, distinctly
// named to avoid a redefinition where both headers' `detail` coexist.
// kPlayerWidth/kPlayerHeight above are untouched (A6) -- the generator
// filled that exact, frozen box; it never chose it.
inline constexpr std::array<Color, kPlayerWidth * kPlayerHeight>
    kPlayerPixels = kPlayerGeneratedPixels;
// Hand-authored (AC-3.11), not generated -- see kEnemyRows' own comment
// above and the file-level comment for T13's reversal.
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
