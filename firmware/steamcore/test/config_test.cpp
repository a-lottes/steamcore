#include "steamcore/config.h"
#include "steamcore/color.h"
#include "test_harness.h"

using steamcore::Color;

STEAMCORE_TEST(config_screen_geometry) {
  CHECK_EQ(steamcore::kScreenWidth, 240);
  CHECK_EQ(steamcore::kScreenHeight, 160);
  CHECK_EQ(steamcore::kTileSize, 16);
  CHECK_EQ(steamcore::kTileCols, 15);
  CHECK_EQ(steamcore::kTileRows, 10);
  CHECK_EQ(steamcore::kTileCount, 150);
}

STEAMCORE_TEST(config_palette_has_exactly_four_colours) {
  CHECK_EQ(static_cast<int>(Color::BLACK), 0);
  CHECK_EQ(static_cast<int>(Color::DARK_ORANGE), 1);
  CHECK_EQ(static_cast<int>(Color::ORANGE), 2);
  CHECK_EQ(static_cast<int>(Color::BRIGHT_ORANGE), 3);
}
