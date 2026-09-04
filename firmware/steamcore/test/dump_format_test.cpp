#include <cstdio>

#include "fixture_pattern.h"
#include "steamcore/dump_format.h"
#include "steamcore/framebuffer.h"
#include "steamcore/sprite.h"
#include "test_harness.h"

#ifndef STEAMCORE_FIXTURE_DUMP
#error "STEAMCORE_FIXTURE_DUMP must be defined by the build (see Makefile)"
#endif

using steamcore::Color;
using steamcore::Framebuffer;
using steamcore::kDumpFormatVersion;
using steamcore::kDumpHeaderSize;
using steamcore::serializeDump;
using steamcore::Sprite;
using steamcore::test::drawFixturePattern;
using steamcore::test::kFixtureAnchors;

namespace {

constexpr size_t kBufferCapacity =
    kDumpHeaderSize +
    static_cast<size_t>(Framebuffer::width()) * Framebuffer::height();

}  // namespace

STEAMCORE_TEST(dump_format_fixture_roundtrips_through_disk) {
  Framebuffer fb;
  drawFixturePattern(fb);

  static uint8_t buffer[kBufferCapacity];
  const size_t written = serializeDump(fb, buffer, sizeof(buffer));
  CHECK_EQ(written, kBufferCapacity);

  FILE* out = std::fopen(STEAMCORE_FIXTURE_DUMP, "wb");
  if (out == nullptr) {
    CHECK(false && "could not open " STEAMCORE_FIXTURE_DUMP
                    " for writing -- run make from the repo root");
    return;
  }
  const size_t fwritten = std::fwrite(buffer, 1, written, out);
  std::fclose(out);
  CHECK_EQ(fwritten, written);

  FILE* in = std::fopen(STEAMCORE_FIXTURE_DUMP, "rb");
  if (in == nullptr) {
    CHECK(false && "could not re-open the fixture that was just written");
    return;
  }
  uint8_t header[kDumpHeaderSize];
  const size_t hread = std::fread(header, 1, sizeof(header), in);
  std::fclose(in);
  CHECK_EQ(hread, sizeof(header));

  CHECK_EQ(header[0], 'S');
  CHECK_EQ(header[1], 'C');
  CHECK_EQ(header[2], 'F');
  CHECK_EQ(header[3], 'B');

  const uint16_t version =
      static_cast<uint16_t>(header[4]) | (static_cast<uint16_t>(header[5]) << 8);
  const uint16_t width =
      static_cast<uint16_t>(header[6]) | (static_cast<uint16_t>(header[7]) << 8);
  const uint16_t height =
      static_cast<uint16_t>(header[8]) | (static_cast<uint16_t>(header[9]) << 8);

  CHECK_EQ(version, kDumpFormatVersion);
  CHECK_EQ(width, static_cast<uint16_t>(Framebuffer::width()));
  CHECK_EQ(height, static_cast<uint16_t>(Framebuffer::height()));
}

STEAMCORE_TEST(dump_format_capacity_too_small_writes_nothing_and_returns_zero) {
  Framebuffer fb;
  drawFixturePattern(fb);

  // The sentinel is what makes the "writes nothing" half of this test's
  // name true: without it, only the return value was ever asserted and a
  // serializer that scribbled into an undersized buffer before giving up
  // would still pass (review F8). dump_format.h documents `out` as left
  // untouched on a capacity failure -- this is that contract, asserted.
  constexpr uint8_t kSentinel = 0xA5;
  uint8_t tiny[kDumpHeaderSize - 1];
  for (uint8_t& byte : tiny) byte = kSentinel;

  const size_t written = serializeDump(fb, tiny, sizeof(tiny));
  CHECK_EQ(written, static_cast<size_t>(0));

  int32_t touched = 0;
  for (const uint8_t byte : tiny) {
    if (byte != kSentinel) ++touched;
  }
  CHECK_EQ(touched, 0);
}

// AC-2.4: the serialized payload matches Framebuffer::pixel() at every
// single coordinate, not a sample -- this is the writer's own
// serialization logic proven correct independent of what the pattern is.
STEAMCORE_TEST(dump_format_payload_matches_every_pixel) {
  Framebuffer fb;
  drawFixturePattern(fb);

  static uint8_t buffer[kBufferCapacity];
  const size_t written = serializeDump(fb, buffer, sizeof(buffer));
  CHECK_EQ(written, kBufferCapacity);

  const uint8_t* payload = buffer + kDumpHeaderSize;
  int32_t mismatches = 0;
  for (int32_t y = 0; y < Framebuffer::height(); ++y) {
    for (int32_t x = 0; x < Framebuffer::width(); ++x) {
      const uint8_t expected = static_cast<uint8_t>(fb.pixel(x, y));
      const uint8_t actual = payload[y * Framebuffer::width() + x];
      if (expected != actual) ++mismatches;
    }
  }
  CHECK_EQ(mismatches, 0);
}

// The anchor table itself (AC-2.2, AC-1.6): the one bug class a
// transitive "payload matches pixel()" comparison cannot catch is an
// offset/axis error made identically in the drawing code and in the
// comparison -- these are independent, hand-picked absolute truths.
STEAMCORE_TEST(dump_format_matches_published_anchor_table) {
  Framebuffer fb;
  drawFixturePattern(fb);

  for (const steamcore::test::FixtureAnchor& anchor : kFixtureAnchors) {
    CHECK(fb.pixel(anchor.x, anchor.y) == anchor.expected);
  }
}

// NFR-3: determinism. The same drawing code run twice must serialize to
// byte-identical output.
STEAMCORE_TEST(dump_format_serialization_is_deterministic) {
  Framebuffer a;
  Framebuffer b;
  drawFixturePattern(a);
  drawFixturePattern(b);

  static uint8_t bufferA[kBufferCapacity];
  static uint8_t bufferB[kBufferCapacity];
  const size_t writtenA = serializeDump(a, bufferA, sizeof(bufferA));
  const size_t writtenB = serializeDump(b, bufferB, sizeof(bufferB));
  CHECK_EQ(writtenA, writtenB);

  int32_t mismatches = 0;
  for (size_t i = 0; i < writtenA; ++i) {
    if (bufferA[i] != bufferB[i]) ++mismatches;
  }
  CHECK_EQ(mismatches, 0);
}
