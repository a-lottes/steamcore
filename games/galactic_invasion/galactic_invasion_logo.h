#pragma once

#include <cstdint>

#include "galactic_invasion/galactic_invasion_generated_art.h"
#include "steamcore/font.h"
#include "steamcore/framebuffer.h"
#include "steamcore/game_state.h"

// This game's own logo screen (galactic-invasion-artwork spec US-1): the
// generated GALACTIC INVASION emblem plus a "PRESS START" prompt, drawn
// only in GameState::READY. Composes exactly steamcore::drawTitleScreen's
// shipped contract (title_screen.h's own doc comment) but replaces the
// generic STEAMCORE wordmark with this game's identity (spec A1) -- the
// engine's own screen stays intact and untouched (NFR-5), this game
// simply stops calling it.
//
// Contract, mirroring drawTitleScreen exactly:
//  - Draws only when state == GameState::READY; PLAYING and GAME_OVER
//    draw nothing at all, via an exhaustive switch so a future GameState
//    enumerator is caught at compile time.
//  - Never clears the framebuffer and never erases what it drew on a
//    later tick -- the caller (GalacticInvasion::render) already owns
//    the fb.clear(BLACK) it performs before this runs.
//  - Composes only Framebuffer::blit (the one generated logo sprite) and
//    drawText (the prompt) -- no setPixel, no fillRect, no bespoke
//    drawing of its own.
namespace steamcore::games {

// A rectangle in framebuffer pixel coordinates. Deliberately this game's
// own type, not steamcore::TitleBounds: this screen depends on the
// shared font and Sprite/Framebuffer primitives only, never on
// title_screen.h, so the decoupling spec A1 calls for is real, not just
// declared. AC-1.12's host test is what keeps kGiPromptBounds honestly
// identical to the engine's own kTitlePromptBounds despite deriving it
// independently, rather than by sharing the type or the constant.
struct GiRect {
  int32_t x;
  int32_t y;
  int32_t w;
  int32_t h;
};

namespace detail {
inline constexpr char kPromptText[] = "PRESS START";
}  // namespace detail

// Both elements' positions are derived, never a literal (constitution
// §3, CLAUDE.md's layout convention): the logo's box is its own
// generated width/height, centred horizontally, on glyph row 4; the
// prompt's box is `(chars) * kGlyphAdvance` wide, centred, on glyph row
// 14 -- the same row `title_screen.h`'s own kTitlePromptBounds uses,
// which is not a coincidence AC-1.12 leaves to chance: a host test
// compares the two field-by-field.
inline constexpr GiRect kLogoBounds{
    (Framebuffer::width() - kLogoWidth) / 2,
    4 * kGlyphHeight,
    kLogoWidth,
    kLogoHeight,
};

inline constexpr GiRect kGiPromptBounds{
    (Framebuffer::width() -
     static_cast<int32_t>(sizeof(detail::kPromptText) - 1) * kGlyphAdvance) /
        2,
    14 * kGlyphHeight,
    static_cast<int32_t>(sizeof(detail::kPromptText) - 1) * kGlyphAdvance,
    kGlyphHeight,
};

static_assert(kLogoBounds.x >= 0 &&
                  kLogoBounds.x + kLogoBounds.w <= Framebuffer::width() &&
                  kLogoBounds.y >= 0 &&
                  kLogoBounds.y + kLogoBounds.h <= Framebuffer::height(),
              "kLogoBounds must be fully on-screen");
static_assert(
    kGiPromptBounds.x >= 0 &&
        kGiPromptBounds.x + kGiPromptBounds.w <= Framebuffer::width() &&
        kGiPromptBounds.y >= 0 &&
        kGiPromptBounds.y + kGiPromptBounds.h <= Framebuffer::height(),
    "kGiPromptBounds must be fully on-screen");

// AC-1.4: the general four-way separating-axis expression, not "logo is
// above prompt" -- CLAUDE.md's own layout convention, since a statement
// like that silently stops being checked if the layout is ever
// reordered.
static_assert(
    kLogoBounds.x + kLogoBounds.w <= kGiPromptBounds.x ||
        kGiPromptBounds.x + kGiPromptBounds.w <= kLogoBounds.x ||
        kLogoBounds.y + kLogoBounds.h <= kGiPromptBounds.y ||
        kGiPromptBounds.y + kGiPromptBounds.h <= kLogoBounds.y,
    "kLogoBounds and kGiPromptBounds must not overlap");

// Draws the logo and prompt when `state == GameState::READY`; draws
// nothing for PLAYING or GAME_OVER. See the file-level contract above.
void drawGalacticInvasionLogo(Framebuffer& fb, GameState state);

}  // namespace steamcore::games
