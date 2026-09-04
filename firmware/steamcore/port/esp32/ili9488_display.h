#pragma once

#include <cstddef>
#include <cstdint>

#include "driver/spi_master.h"
#include "steamcore/dirty_tracker.h"
#include "steamcore/framebuffer.h"
#include "steamcore/panel_format.h"
#include "steamcore/tile_pusher.h"

// The real ILI9488 panel driver -- ESP-IDF/SPI-dependent, therefore
// outside the host `make test` gate by construction (display-driver plan
// §1 Decision 1; constitution §4's "no ESP-IDF header in include/ or
// src/" rule stays absolute because this file lives in neither). Ports
// the bring-up spike's proven init sequence (deleted,
// firmware/system/main/ili9488_display.{h,cpp}) verbatim except for what
// was wrong for a tile driver specifically -- see the .cpp.
//
// Example (a GameLoop-driven harness, illustrative -- see
// firmware/system/main/app_main.cpp for the real one):
//   steamcore::port::esp32::Ili9488Display display;
//   if (!display.init()) { /* log + decide whether to halt, see below */ }
//
//   steamcore::Framebuffer fb;
//   steamcore::DirtyTracker tracker;
//   steamcore::GameLoop<MyConsumer> loop(consumer, fb);
//   for (;;) {
//     loop.tick(input);                    // update() then render()
//     display.pushDirty(fb, tracker);      // never from inside tick()
//   }
//
// Contract:
//  - Single-threaded, nothing throws, no error code -- same inherited
//    contract as every other steamcore type.
//  - init() has two phases with different failure policies, both
//    covered by spec A6/AC-4.4's "an abort at this stage is acceptable,
//    but never silent": bus/GPIO *setup* (`gpio_config`,
//    `spi_bus_initialize`, `spi_bus_add_device`) is unconditionally
//    fatal via `ESP_ERROR_CHECK` -- nothing past that point can mean
//    anything without a working bus, so this mirrors the bring-up
//    spike's own precedent and is never silent (ESP-IDF's own panic
//    handler names the failing call). The *panel command sequence*
//    after that (software reset, sleep-out, pixel format, ...) never
//    aborts itself: each step is logged explicitly and a failure there
//    returns false, leaving the choice of whether to halt to the caller
//    -- so the driver's own testability doesn't depend on a live panel
//    responding correctly to every command (review F2: this file
//    previously claimed init() "never aborts itself" without this
//    distinction).
//  - pushDirty() never aborts on a steady-state failure, ever (AC-4.3):
//    only the tiles that actually transferred are ever committed to
//    DirtyTracker, and a failed tile simply stays dirty for the next
//    push (US-4) -- see tile_pusher.h for the exact retry/commit
//    contract this delegates to unmodified.
//  - Waiting for an SPI transfer to complete is display-*output* timing.
//    It is decoupled from, and never feeds back into, the 60Hz
//    game-logic determinism guarantee (constitution §3/§4, spec NFR-5):
//    a slow or fast panel changes how promptly a frame reaches the
//    glass, never what GameLoop::tick() computes.
namespace steamcore::port::esp32 {

class Ili9488Display {
 public:
  Ili9488Display() = default;

  // Resets and initializes the panel over SPI2_HOST: software reset,
  // sleep-out, 18bpp pixel format (COLMOD=0x66), landscape memory access
  // control, display on. Bus/GPIO setup is fatal on failure (see the
  // class contract above); past that point, returns true iff every
  // panel command succeeded, with each step's outcome logged explicitly
  // either way (AC-4.4).
  bool init();

  // Pushes fb's DirtyTracker-reported dirty tiles to the real panel via
  // a TilePusher<SpiTransmitter> -- see tile_pusher.h's push() for the
  // exact scan/expand/transmit/commit-only-transferred contract, which
  // this method inherits unmodified. Logs tiles sent, tiles failed and
  // the elapsed transfer time (NFR-1, NFR-9).
  PushResult pushDirty(const Framebuffer& fb, DirtyTracker& tracker);

 private:
  // Satisfies TilePusher's Transmitter concept by wrapping the real SPI
  // calls; never aborts on a steady-state failure (AC-4.3) -- returns
  // false and lets TilePusher decide the tile stays dirty.
  class SpiTransmitter {
   public:
    explicit SpiTransmitter(spi_device_handle_t device) : device_(device) {}
    bool transmitTile(const PanelWindow& window, const uint8_t* bytes,
                       size_t count);

   private:
    spi_device_handle_t device_ = nullptr;
  };

  spi_device_handle_t spiDevice_ = nullptr;
  TilePusher<SpiTransmitter> pusher_;
};

}  // namespace steamcore::port::esp32
