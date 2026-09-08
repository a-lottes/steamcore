#pragma once

#include <cstdint>

#include "nvs.h"

// The real device-side Backend for HighscoreStore<Backend> (highscore-
// system plan §1 Decision 5) -- NVS-dependent, therefore outside the host
// `make test` gate by construction, the same boundary GpioInputSource and
// Ili9488Display already established (constitution §4).
//
// Example (illustrative -- see firmware/system/main/highscore_harness_game.h
// for the real on-device composition):
//   steamcore::port::esp32::NvsHighscoreBackend backend;
//   if (!backend.init()) { /* halt, same posture as every other init() */ }
//   steamcore::HighscoreStore<steamcore::port::esp32::NvsHighscoreBackend>
//       store(backend);
//   store.load();
//
// Contract:
//  - init() calls nvs_flash_init(), erasing and retrying once on
//    ESP_ERR_NVS_NO_FREE_PAGES or ESP_ERR_NVS_NEW_VERSION_FOUND (the
//    documented "partition needs a fresh format" outcomes -- a factory-
//    fresh or format-mismatched partition, not a hardware fault), then
//    opens this feature's own NVS namespace. Returns false on any
//    unrecoverable failure; called once, before the first read()/write().
//  - read()/write() satisfy HighscoreStore<Backend>'s own Backend concept
//    exactly (highscore.h): `bool read(uint8_t*, int32_t)` and
//    `bool write(const uint8_t*, int32_t)`. A missing key (nothing ever
//    written) or a stored blob whose size does not match `length` both
//    make read() return false -- first boot and a format-version change
//    both look like this, and HighscoreStore already treats a failed
//    read as "every table empty" (highscore.h's own contract), never a
//    crash.
//  - Single-threaded, nothing throws, no error code, no dynamic
//    allocation -- same inherited contract as every other steamcore type.
namespace steamcore::port::esp32 {

class NvsHighscoreBackend {
 public:
  NvsHighscoreBackend() = default;

  // Initializes the NVS partition and opens this feature's namespace.
  // Call once before the first read()/write(). Returns false if either
  // step fails unrecoverably.
  bool init();

  // Satisfies HighscoreStore<Backend>'s Backend concept.
  bool read(uint8_t* out, int32_t length) const;
  bool write(const uint8_t* data, int32_t length);

 private:
  nvs_handle_t handle_ = 0;
  bool initialized_ = false;
};

}  // namespace steamcore::port::esp32
