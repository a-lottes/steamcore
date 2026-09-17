#include <cstdio>

#include "galactic_invasion/galactic_invasion.h"

#include "steamcore/dump_format.h"
#include "steamcore/framebuffer.h"
#include "steamcore/game_loop.h"
#include "test_harness.h"

#ifndef STEAMCORE_GI_LOGO_DUMP
#error "STEAMCORE_GI_LOGO_DUMP must be defined by the build (see Makefile)"
#endif

using steamcore::Framebuffer;
using steamcore::GameInput;
using steamcore::GameLoop;
using steamcore::kDumpHeaderSize;
using steamcore::serializeDump;
using steamcore::games::GalacticInvasion;

namespace {
constexpr size_t kBufferCapacity =
    kDumpHeaderSize +
    static_cast<size_t>(Framebuffer::width()) * Framebuffer::height();
}  // namespace

// T13/AC-1.7/AC-2.4: dumps the READY screen (the generated logo plus the
// "PRESS START" prompt) so `make view` can render it as a legible PNG for
// the design judgement -- the same pattern title_screen_dump_test.cpp's
// own dump test established for TITLE_DUMP, restated for this game's own
// logo screen (spec A1: this game stops calling drawTitleScreen and draws
// its own instead, see galactic_invasion_logo.h's file-level contract).
STEAMCORE_TEST(galactic_invasion_logo_dumps_to_disk_for_visual_check) {
  GalacticInvasion game;
  Framebuffer fb;
  GameLoop<GalacticInvasion> loop(game, fb);
  loop.tick(GameInput{});  // one tick: fresh instance starts in READY

  static uint8_t buffer[kBufferCapacity];
  const size_t written = serializeDump(fb, buffer, sizeof(buffer));
  CHECK_EQ(written, kBufferCapacity);

  FILE* out = std::fopen(STEAMCORE_GI_LOGO_DUMP, "wb");
  if (out == nullptr) {
    CHECK(false && "could not open " STEAMCORE_GI_LOGO_DUMP
                    " for writing -- run make from the repo root");
    return;
  }
  const size_t fwritten = std::fwrite(buffer, 1, written, out);
  std::fclose(out);
  CHECK_EQ(fwritten, written);
}
