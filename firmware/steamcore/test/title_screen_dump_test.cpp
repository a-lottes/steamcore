#include <cstdio>

#include "steamcore/dump_format.h"
#include "steamcore/framebuffer.h"
#include "steamcore/game_state.h"
#include "steamcore/title_screen.h"
#include "test_harness.h"

#ifndef STEAMCORE_TITLE_DUMP
#error "STEAMCORE_TITLE_DUMP must be defined by the build (see Makefile)"
#endif

using steamcore::Color;
using steamcore::drawTitleScreen;
using steamcore::Framebuffer;
using steamcore::GameState;
using steamcore::kDumpHeaderSize;
using steamcore::serializeDump;

namespace {
constexpr size_t kBufferCapacity =
    kDumpHeaderSize +
    static_cast<size_t>(Framebuffer::width()) * Framebuffer::height();
}  // namespace

// T9/NFR-1: dumps the READY screen so `make view` can render it as a
// legible PNG -- the same pattern text_fixture_test.cpp's own dump test
// established for TEXT_DUMP.
STEAMCORE_TEST(title_screen_dumps_to_disk_for_visual_check) {
  Framebuffer fb;
  fb.clear(Color::BLACK);
  drawTitleScreen(fb, GameState::READY);

  static uint8_t buffer[kBufferCapacity];
  const size_t written = serializeDump(fb, buffer, sizeof(buffer));
  CHECK_EQ(written, kBufferCapacity);

  FILE* out = std::fopen(STEAMCORE_TITLE_DUMP, "wb");
  if (out == nullptr) {
    CHECK(false && "could not open " STEAMCORE_TITLE_DUMP
                    " for writing -- run make from the repo root");
    return;
  }
  const size_t fwritten = std::fwrite(buffer, 1, written, out);
  std::fclose(out);
  CHECK_EQ(fwritten, written);
}
