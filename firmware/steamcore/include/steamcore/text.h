#pragma once

#include <cstdint>

#include "steamcore/color.h"
#include "steamcore/framebuffer.h"

// Example:
//   steamcore::Framebuffer fb;
//   steamcore::drawText(fb, 10, 10, "SYSTEM READY!", steamcore::Color::BRIGHT_ORANGE);

namespace steamcore {

// Draws `text` at (x, y) using the engine's one 8x8 monospace font
// (font.h), left to right, advancing kGlyphAdvance pixels per character,
// in `ink`. Composed entirely from the existing Framebuffer::blit — no
// new low-level drawing primitive; clipping is exactly blit's, applied
// per glyph cell (a glyph entirely off-screen is skipped without being
// drawn, a partially off-screen one is clipped like any other blit).
//
// Character set: the 43 characters space, `!-.0123456789:>?` and
// `A`-`Z` (font.h). Of those, 40 are characters README.md's own
// committed Boot Experience/Main Menu/Highscore-System text already
// uses; the remaining 3 (`:`, `!`, `?`) are a deliberate margin the
// project chose knowingly, not a demonstrated need (spec A3). Any other
// character `text` contains -- lowercase, `\n`, any control character --
// draws the one deterministic placeholder ("tofu") glyph instead, with
// no special case for line breaks: `text` is always drawn as a single
// line, never wrapped or split.
//
// Ink and background: a source glyph pixel that is "on" is drawn in
// `ink`; every "off" pixel leaves the destination unchanged (i.e. it is
// transparent, the same contract as Sprite::blit's own `transparent`
// parameter). This holds even when `ink` is Color::BLACK: internally the
// transparent key becomes BRIGHT_ORANGE instead of BLACK for that one
// call, so black text still draws real black pixels rather than
// vanishing into a BLACK background.
//
// `text` may be nullptr (a no-op) or an empty string (also a no-op) --
// neither is a precondition violation. Single-threaded, like every other
// Framebuffer-drawing call in this engine: nothing here synchronizes
// concurrent access. Nothing throws and no error code is returned;
// invalid input is always clipped, substituted with the placeholder
// glyph, or ignored, never undefined behaviour.
void drawText(Framebuffer& fb, int32_t x, int32_t y, const char* text, Color ink);

}  // namespace steamcore
