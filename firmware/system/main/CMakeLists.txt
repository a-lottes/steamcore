# Two build systems must both learn about a new engine .cpp: the host
# Makefile globs firmware/steamcore/src/*.cpp automatically, this list
# does not (display-driver plan §1, §5 "Two build systems" risk). A
# deliberate, explicit SRCS list was chosen over a globbing ESP-IDF
# component precisely because it fails LOUDLY -- an undefined reference at
# link time -- when it drifts, instead of quietly compiling a stale set.
# So: adding a file under firmware/steamcore/src/ that the device needs
# means adding it here too.
#
# INCLUDE_DIRS: "../../steamcore/include" is the engine's public surface;
# "../.." (i.e. firmware/) exists only so the device harness can reach
# "steamcore/port/esp32/..." and the shared test fixture
# "steamcore/test/fixture_pattern.h". "../../../games" mirrors the host
# Makefile's own `-I$(GAMES_DIR)` so `#include "galactic_invasion/..."`
# resolves identically on both toolchains (galactic-invasion T16).
idf_component_register(
    SRCS "app_main.cpp"
         "../../steamcore/src/framebuffer.cpp"
         "../../steamcore/src/dirty_tracker.cpp"
         "../../steamcore/src/panel_format.cpp"
         "../../steamcore/src/game_state.cpp"
         "../../steamcore/src/title_screen.cpp"
         "../../steamcore/src/text.cpp"
         "../../steamcore/src/font.cpp"
         "../../steamcore/src/dump_format.cpp"
         "../../steamcore/src/highscore.cpp"
         "../../steamcore/src/highscore_flow.cpp"
         "../../steamcore/src/highscore_screen.cpp"
         "../../steamcore/src/initials_entry.cpp"
         "../../steamcore/port/esp32/ili9488_display.cpp"
         "../../steamcore/port/esp32/gpio_input_source.cpp"
         "../../steamcore/port/esp32/analog_joystick_source.cpp"
         "../../steamcore/port/esp32/nvs_highscore_backend.cpp"
         "../../../games/galactic_invasion/galactic_invasion.cpp"
    INCLUDE_DIRS "." "../../steamcore/include" "../.." "../../../games"
    REQUIRES driver esp_timer esp_adc nvs_flash
)
