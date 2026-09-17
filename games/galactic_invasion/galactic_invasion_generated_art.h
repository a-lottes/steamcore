// GENERATED -- do not edit by hand. Produced by
// tools/generate_sprite_data.py (galactic-invasion-artwork plan §1
// decisions 2-5). A hand edit here is invisible to
// tools/check_constraints.sh's provenance check (AC-3.9) and will
// silently drift from the source it claims to come from.
//
// Source: assets/sprites/galactic_invation.png
// Source sha256: 5164bfbe912bfecde43272cad030df0369715acb0fe78c8640a44c31c69cf96a
//
// The parameters below are shared by every artifact in this file;
// each artifact's own extracted region and background finding are
// recorded just above its row data.
//
// Background rule: alpha gate first (alpha < 128 -> BLACK),
// then a border-seeded chroma flood (bg-tol 100) and a halo
// closure to fixpoint (halo-tol 200), all integer
// arithmetic, applied before fit and quantisation.
// Fit: integer box mapping, coverage-min 15%; a cast
// sprite fills its frozen box (anisotropic scaling permitted), the
// logo instead preserves its source region's aspect ratio.
// Quantisation: nearest of the four palette colours by squared
// Euclidean RGB distance on integers; ties break to the lowest
// Color enumerator (BLACK < DARK_ORANGE < ORANGE < BRIGHT_ORANGE).
//
// Regenerate with:
//   python3 tools/generate_sprite_data.py --emit --source assets/sprites/galactic_invation.png --output games/galactic_invasion/galactic_invasion_generated_art.h --alpha-min 128 --bg-tol 100 --halo-tol 200 --coverage-min 15 --artifact Logo=45,20,287,95,56 --artifact PlayerGenerated=557,49,56,63,12,12

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "steamcore/color.h"
#include "steamcore/sprite.h"

namespace steamcore::games {

namespace detail {

// The four-symbol row-string convention this generator emits:
// ' '=BLACK, '.'=DARK_ORANGE, '+'=ORANGE, '#'=BRIGHT_ORANGE.
// Mirrors galactic_invasion_art.h's own hand-authored
// spritePixel()/rowIsExactWidth() convention (font.cpp's
// original), widened from two symbols to four -- named
// distinctly (generatedXxx) so this file and
// galactic_invasion_art.h can be included in the same
// translation unit without a redefinition.

constexpr bool generatedRowIsExactWidth(const char* row, int32_t width) {
  return row[width] == '\0';
}

[[noreturn]] inline void reportInvalidGeneratedArt() { std::abort(); }

constexpr Color generatedSpritePixel(const char* row, int32_t col,
                                      int32_t width) {
  if (!generatedRowIsExactWidth(row, width)) {
    reportInvalidGeneratedArt();
    return Color::BLACK;
  }
  switch (row[col]) {
    case ' ': return Color::BLACK;
    case '.': return Color::DARK_ORANGE;
    case '+': return Color::ORANGE;
    case '#': return Color::BRIGHT_ORANGE;
    default: break;
  }
  reportInvalidGeneratedArt();
  return Color::BLACK;
}

}  // namespace detail

// Artifact: Logo
//   region: x=45 y=20 w=287 h=95 (aspect preserved)
//   target: 169x56
//   background mode: chroma, reference colour: (2, 1, 0)
inline constexpr int32_t kLogoWidth = 169;
inline constexpr int32_t kLogoHeight = 56;

namespace detail {

// clang-format off
inline constexpr const char* kLogoRows[kLogoHeight] = {
    "                                                                                                                                                                         ",
    "                                                                                                                                                                         ",
    "                                                                                                                                                                         ",
    "                                                                                                                                                                         ",
    "                                                                                                                                                                         ",
    "                   ++++++++++++         +++++++++++      #++++#              +++++++++++         +++++++++++      +++++++++++++++   ++++++      ++++++++++++             ",
    "                  ++++++++++###+      ++++++++++++++     ##++++            ++++++++++++++      ++++++++++++++     ##++++++++++++    +++++#     +#+++++++++++#            ",
    " +               ++++++++++#####+     +++++++++++++#+    #+++++            ##++++++++++++     +++++++++++++++#   ++++++++++++++++   ++++++    +##+++++++++++++           ",
    "                 ++++++    +###++    ++++++    +###++    #+++++           ####++    ++++++    ++++++     ###+#        ++++++        ++++++   +####++     +++++           ",
    "                 ++++++    +##+++    +++++.     ##+++    #+++++           ####++     +++++    ++++++     +++++        ++++++        ++++++   +#++#++     +++++           ",
    "                 ++++#+    +#++++    ++++#.     ##+++    ++++++           #+###+     +++++    ++++++     +++++        #+++++        ++++++   +++++#+     +++++           ",
    "                 ++++#+     +++++    ++++#.     ##+++    ++++++           #+###+     +++++    ++++++     +++++        #+++++        ++++++   +++++#+     +++++           ",
    "                 ++++#+              +++++.     #++++    ++++++           ++++++     ++++++   ++++++                  ++++++        ++++++   +++++++                     ",
    "                 ++++++              +++++.     +++++    ++++++           ++++++     ++++++   ++++++                  ++++++        ++++++   +++++++                     ",
    "                 ++++++              +++++.     +++++    ++++++           ++++++     ++++++   ++++++                  ++++++        ++++++   +++++++                     ",
    "                 ++++++              +++++.     +++++    ++++++           ++++++     ++++#+   ++++++                  ++++++        ++++++   +++++++                     ",
    "                 ++++++              ###++.     +++++    #+++++           ++++++     ++++#+   ++++++                  ++++++        ++++++   +++++++                     ",
    "                 ++++++  ++++++++    ####+.     +++++    #+++#+           ++++++    .##++#+   ++++++                  ++++++        ++++++   +++++++                     ",
    "                 ++++++  ++++++++    ####++++++++++++    #+####           +++++++++++++++#+   ++++++                  ++++++        ++++++   +++++++                     ",
    "                 ++++++    ++++++    ####++++++++++++    ######           +++++++++++++++#+   ++++++                  #+++++        ++++++   +++++++                     ",
    "                 ++++++     +++#+    ###+++++++++++++    ######           ++++++++++++++++    ++++++                  #+++++        ++++++   +++++++                     ",
    "                 ++++++    +####+    #+++++  ++++++++    ######           ++++++++++++++++    ++++++                  #+++++        ++++++   +++++++                     ",
    "                 ++++++    +####+    ++++++    ++++++    ######           ++++++    ++++++    +#++++     +++++        ###+++        ++++++   +++++++     +++++           ",
    "+                ++++++    +###++    ++++++    ++++++    #####+           ++++++    ++++++    ###+++     +++++        ###+++        ++++++   +#+++++     +++++           ",
    "#+               ++++#+    +###++    ++++++    +#+###    ###+++           ++++++    ++++++    ####++     +++++        #+##++        ++++++   +#+++++     +++++           ",
    "####+            +++##+    +###++    ++++++    +#####    #+++++           ++++++    ++++++    +#####+    +++++        ++++++        +++++#   +#+++++    ++++#+           ",
    "#++              #+############++    ++++++    +#####    #+++++++++++++   ++++++    ++++++    ++######++++####        ++++++        +++++#   ++#+++++++++++##+           ",
    "++                ####++++++++++     ++++++    +####+    #+++++++++++++   ++++++    +#++++     +++##+++++####+        ++++++        ++++++     ++++++++++++#+            ",
    "+                  +##+++++++++      ++++++    +####+    ++++++++++++++   ++++++    +##+++       +++++++++++          ++++++        #+++++      +#######++++           +#",
    "                                                                                                                                                                       ++",
    "                                                                                                                                                                         ",
    "                                                                                                                                                                         ",
    "                                                                                                                                                                         ",
    "                                                                                                                                                                         ",
    "                                                                                                                                                                         ",
    "                              +++++++   #++#      ++#     ####+    +####     ++++++##         ####++++      ++        +########     ####+    ++++                        ",
    "                            +########   +###      +#+     +###+    +###+     +######++        +######+      ++++      +#######+     ####+    ####+                       ",
    "                            +########   +###++   +####+   +###+    +###+    ##########+      +###++++##+    ++++     ###########    #####+   #####+                      ",
    "                            +########   +####+    +###+   +####    +###+   +####+.+#####   +####    +#++   +##++    +###++++###++   ######+  #####+                      ",
    "                               ###+     +#####+    ####   +####    +###+  +####.  ..####   +###+    ++#+   +####    ####.   .##++   ######+    ###+                      ",
    "                               ###+     +#####++   ####   +####    +###+  +####.    ####   +###+    ++++   +####    ####    .##++   ########   ###+                      ",
    "                               ###+     +###+#+++  ###+   +####    +###+  +####.    ####   +###+           +####    ####    .##++   #######++  ###+                      ",
    "                    +          +##+     +### ++++ +###+   +####    +###+  +####.    ####   +###++          +####    ####    .##++   ##### +#+  ###+        +             ",
    "                    ++         +##+     +### ++########   +####    +###+  +####.    ####   +++###++##+     +####    ####    .###+   ##### ###++###+        +             ",
    "               ++++###+        +#++     +###   ########   +####    +###+  +####.    ####     ++######++    +####    ####    .###+   #####  +######+      ++#++           ",
    "                    +          +##+     +###+  +#######   ####+    ####+  +####.++++####       ++#####++   +####    ####    .###+   #####   +######        +             ",
    "                               +##+     +###+   +######    +##++  ++###+  +#############          #####++  +####    ####    .##++   #####   +######                      ",
    "                               +##+     +###+   ++#####   ++####  +####+  #####+++++####          #####+   +####    ####    .##++   ####+   +######                      ",
    "                               +##+     +###+    +#####     ####++####+   ####+    +####            +##+   +###+    ###+    .###+   ####+    ######                      ",
    "                               +##+     +###+    +#####     +########++   +###+     ####   ####+    +##+   +###+    ####    +###+   ####+    #####+                      ",
    "                               ###+     +###+    ++####      ########     +###+     +###   ####+    +###+  +###+    ####+  +####+   ####+    #####+                      ",
    "                             +#######   +###+     +####       +####+      +###+     +###   #+###++#####+   +###+    ###########+    ###++    +####+                      ",
    "                             ########   ####+     +####        ++#++      +###+     +##+     ##########    +###+     ##+#######+    ###++    +####+                      ",
    "                             ++++++++   +++++     +####        ++++       +++++     ++++     +#+++++++     +++++      ++++++##+     #+#++     +##++                      ",
    "                                                                                                                                                       +++###+++         ",
    "                                                                                                                                                     ++##########+       ",
};
// clang-format on

constexpr std::array<Color, kLogoWidth * kLogoHeight> buildLogoPixels() {
  std::array<Color, kLogoWidth * kLogoHeight> pixels{};
  int32_t offset = 0;
  for (int32_t row = 0; row < kLogoHeight; ++row) {
    for (int32_t col = 0; col < kLogoWidth; ++col) {
      pixels[static_cast<size_t>(offset++)] =
          generatedSpritePixel(kLogoRows[row], col, kLogoWidth);
    }
  }
  return pixels;
}

inline constexpr std::array<Color, kLogoWidth * kLogoHeight> kLogoPixels = buildLogoPixels();

static_assert(sizeof(kLogoPixels) / sizeof(Color) ==
                  static_cast<size_t>(kLogoWidth * kLogoHeight),
              "kLogoPixels must hold exactly kLogoWidth*kLogoHeight cells");

}  // namespace detail

inline constexpr Sprite kLogoSprite{detail::kLogoPixels.data(),
                                       kLogoWidth, kLogoHeight,
                                       /*stride=*/kLogoWidth};

static_assert(kLogoSprite.width == kLogoWidth &&
                  kLogoSprite.height == kLogoHeight &&
                  kLogoSprite.stride == kLogoWidth,
              "kLogoSprite must be tightly packed at its declared size");

// Artifact: PlayerGenerated (cast member -- pixels only, no Sprite of its own; consumed by galactic_invasion_art.h)
//   region: x=557 y=49 w=56 h=63 (box filled, anisotropic)
//   target: 12x12
//   background mode: chroma, reference colour: (65, 20, 0)
namespace detail {

// clang-format off
inline constexpr const char* kPlayerGeneratedRows[12] = {
    "     ##     ",
    "     ##     ",
    "     ##     ",
    "    ###     ",
    "    ####    ",
    "    ####    ",
    "   ###### # ",
    " ########## ",
    "####### ####",
    "####### ### ",
    "##   ##  ###",
    " #         #",
};
// clang-format on

constexpr std::array<Color, 12 * 12> buildPlayerGeneratedPixels() {
  std::array<Color, 12 * 12> pixels{};
  int32_t offset = 0;
  for (int32_t row = 0; row < 12; ++row) {
    for (int32_t col = 0; col < 12; ++col) {
      pixels[static_cast<size_t>(offset++)] =
          generatedSpritePixel(kPlayerGeneratedRows[row], col, 12);
    }
  }
  return pixels;
}

inline constexpr std::array<Color, 12 * 12> kPlayerGeneratedPixels = buildPlayerGeneratedPixels();

}  // namespace detail

}  // namespace steamcore::games
