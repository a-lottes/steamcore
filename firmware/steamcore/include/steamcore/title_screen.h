#pragma once

#include <cstdint>

#include "steamcore/font.h"
#include "steamcore/framebuffer.h"
#include "steamcore/game_state.h"

// The console's own "not playing yet" screen (start-screen spec US-1/US-2):
// a fixed "STEAMCORE" wordmark and "PRESS START" prompt, both drawn with
// the shipped drawText -- no new drawing primitive, no bespoke art. This
// is a free function, not a type: it holds no state of its own, the same
// shape drawText itself already established for stateless drawing in this
// codebase (plan §1 Decision 1).
//
// Example (a game composing this into its own render step -- literally
// game_state.h's own doc-comment example, packaged as a reusable call):
//   struct MyGame {
//     steamcore::GameSession session_;
//     void update(const steamcore::GameInput& input) {
//       session_.advance(input, /*sessionEnded=*/false);
//     }
//     void render(steamcore::Framebuffer& fb) {
//       fb.clear(steamcore::Color::BLACK);
//       steamcore::drawTitleScreen(fb, session_.state());
//     }
//   };
//
// Contract:
//  - Draws only when `state == GameState::READY`; PLAYING and GAME_OVER
//    draw nothing at all (plan §1 Decision 2) -- a future GameState
//    enumerator is caught at compile time by the exhaustive switch this
//    is built on.
//  - Never clears the framebuffer and never erases what it drew on a
//    later call, even once `state` has moved on (plan §1 Decision 3).
//    The caller owns the frame, exactly as GameLoop's own shipped
//    contract already requires of any game that wants a clean tick --
//    forgetting to clear leaves title pixels visible under whatever a
//    playing game draws next; that is a caller bug, not this function's.
//  - Composes only drawText (which itself composes only
//    Framebuffer::blit) -- no setPixel, no fillRect, no bespoke Sprite of
//    its own (enforced structurally, see tools/check_constraints.sh).
//    Both elements are drawn in Color::BRIGHT_ORANGE, the one palette
//    colour that clears the WCAG 4.5:1 contrast floor against BLACK
//    (spec §8 Design Review) -- the wordmark reads "STEAMCORE" at
//    kTitleWordmarkBounds, the prompt "PRESS START" at
//    kTitlePromptBounds; both rects are exported so a caller (or a test)
//    never has to restate a coordinate this file already computed.
//  - Single-threaded, nothing throws, no error code, no dynamic
//    allocation -- same inherited contract as every other steamcore type.
namespace steamcore {

// A rectangle in framebuffer pixel coordinates: top-left (x, y), size
// (w, h). Plain data, no invariant of its own beyond what the two
// exported instances below already satisfy at compile time.
struct TitleBounds {
  int32_t x;
  int32_t y;
  int32_t w;
  int32_t h;
};

namespace detail {

// The two title-screen strings. Not part of the public surface (plan §1
// Decision 6, NFR-5) -- kept in `detail` so a test restating either
// string independently checks the exported bounds' width, rather than
// reading it back from the same constant the drawing code uses.
inline constexpr char kWordmarkText[] = "STEAMCORE";
inline constexpr char kPromptText[] = "PRESS START";

}  // namespace detail

// Both elements' positions are derived, never a literal (constitution
// §3): each width is `(sizeof(text) - 1) * kGlyphAdvance`, each is
// horizontally centred on `Framebuffer::width()`, and each `y` is a
// `kGlyphHeight` multiple -- the wordmark on glyph row 6, the prompt on
// glyph row 14, leaving a 56-row gap between them on the 20-row-tall
// (160 / kGlyphHeight) canvas.
inline constexpr TitleBounds kTitleWordmarkBounds{
    (Framebuffer::width() -
     static_cast<int32_t>(sizeof(detail::kWordmarkText) - 1) * kGlyphAdvance) /
        2,
    6 * kGlyphHeight,
    static_cast<int32_t>(sizeof(detail::kWordmarkText) - 1) * kGlyphAdvance,
    kGlyphHeight,
};

inline constexpr TitleBounds kTitlePromptBounds{
    (Framebuffer::width() -
     static_cast<int32_t>(sizeof(detail::kPromptText) - 1) * kGlyphAdvance) /
        2,
    14 * kGlyphHeight,
    static_cast<int32_t>(sizeof(detail::kPromptText) - 1) * kGlyphAdvance,
    kGlyphHeight,
};

// T2: an explicit guard, redundant with the derivation above by
// construction today, against a future edit that hardcodes `.w` instead
// of deriving it -- the rect and the string can never disagree.
static_assert(kTitleWordmarkBounds.w ==
                  static_cast<int32_t>(sizeof(detail::kWordmarkText) - 1) *
                      kGlyphAdvance,
              "kTitleWordmarkBounds.w must track kWordmarkText's length");
static_assert(kTitlePromptBounds.w ==
                  static_cast<int32_t>(sizeof(detail::kPromptText) - 1) *
                      kGlyphAdvance,
              "kTitlePromptBounds.w must track kPromptText's length");

static_assert(kTitleWordmarkBounds.x >= 0 &&
                  kTitleWordmarkBounds.x + kTitleWordmarkBounds.w <=
                      Framebuffer::width() &&
                  kTitleWordmarkBounds.y >= 0 &&
                  kTitleWordmarkBounds.y + kTitleWordmarkBounds.h <=
                      Framebuffer::height(),
              "kTitleWordmarkBounds must be fully on-screen");
static_assert(kTitlePromptBounds.x >= 0 &&
                  kTitlePromptBounds.x + kTitlePromptBounds.w <=
                      Framebuffer::width() &&
                  kTitlePromptBounds.y >= 0 &&
                  kTitlePromptBounds.y + kTitlePromptBounds.h <=
                      Framebuffer::height(),
              "kTitlePromptBounds must be fully on-screen");

// AC-1.6: the general four-way separating-axis test -- true whenever
// either rectangle's span on the x axis or the y axis entirely misses
// the other's, not merely "the wordmark is above the prompt" (a
// statement that would silently stop being checked if the layout above
// ever moved the wordmark below the prompt instead).
static_assert(
    kTitleWordmarkBounds.x + kTitleWordmarkBounds.w <= kTitlePromptBounds.x ||
        kTitlePromptBounds.x + kTitlePromptBounds.w <= kTitleWordmarkBounds.x ||
        kTitleWordmarkBounds.y + kTitleWordmarkBounds.h <=
            kTitlePromptBounds.y ||
        kTitlePromptBounds.y + kTitlePromptBounds.h <= kTitleWordmarkBounds.y,
    "kTitleWordmarkBounds and kTitlePromptBounds must not overlap");

// Draws the wordmark and prompt when `state == GameState::READY`;
// draws nothing for PLAYING or GAME_OVER. See the file-level contract
// above.
void drawTitleScreen(Framebuffer& fb, GameState state);

}  // namespace steamcore
