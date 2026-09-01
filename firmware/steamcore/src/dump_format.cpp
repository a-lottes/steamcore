#include "steamcore/dump_format.h"

namespace steamcore {

namespace {

void writeU16LE(uint8_t* out, uint16_t value) {
  out[0] = static_cast<uint8_t>(value & 0xFF);
  out[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

}  // namespace

size_t serializeDump(const Framebuffer& fb, uint8_t* out, size_t capacity) {
  const size_t width = static_cast<size_t>(Framebuffer::width());
  const size_t height = static_cast<size_t>(Framebuffer::height());
  const size_t total = kDumpHeaderSize + width * height;
  if (capacity < total) return 0;

  out[0] = 'S';
  out[1] = 'C';
  out[2] = 'F';
  out[3] = 'B';
  writeU16LE(out + 4, kDumpFormatVersion);
  writeU16LE(out + 6, static_cast<uint16_t>(width));
  writeU16LE(out + 8, static_cast<uint16_t>(height));

  uint8_t* payload = out + kDumpHeaderSize;
  for (size_t y = 0; y < height; ++y) {
    for (size_t x = 0; x < width; ++x) {
      const int32_t px = static_cast<int32_t>(x);
      const int32_t py = static_cast<int32_t>(y);
      payload[y * width + x] = static_cast<uint8_t>(fb.pixel(px, py));
    }
  }

  return total;
}

}  // namespace steamcore
