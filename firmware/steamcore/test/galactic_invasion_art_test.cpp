#include "galactic_invasion/galactic_invasion_art.h"

#include "steamcore/color.h"
#include "steamcore/framebuffer.h"
#include "test_harness.h"

using steamcore::Color;
using steamcore::Framebuffer;
using steamcore::games::kEnemyHeight;
using steamcore::games::kEnemySprite;
using steamcore::games::kEnemyWidth;
using steamcore::games::kPlayerHeight;
using steamcore::games::kPlayerSprite;
using steamcore::games::kPlayerWidth;
using steamcore::games::kProjectileHeight;
using steamcore::games::kProjectileSprite;
using steamcore::games::kProjectileWidth;

namespace {

// Restated independently of galactic_invasion_art.h's own
// detail::kPlayerRows/kEnemyRows/kProjectileRows (review precedent:
// title_screen_test.cpp's kExpectedWordmarkText) -- if this test instead
// imported and reused the production row strings, a drift introduced
// later would be invisible here, since "expected" and "actual" would
// silently agree on the same wrong value.
constexpr const char* kExpectedPlayerRows[kPlayerHeight] = {
    "     ##     ",
    "     ##     ",
    "    ####    ",
    "    ####    ",
    "   ######   ",
    "   ######   ",
    "  ########  ",
    "  ########  ",
    " ########## ",
    " ########## ",
    "############",
    "############",
};

constexpr const char* kExpectedEnemyRows[kEnemyHeight] = {
    "  ##    ##  ",
    "   ######   ",
    "  ########  ",
    " ########## ",
    "############",
    "############",
    "## ###### ##",
    "##  ####  ##",
    "  ##    ##  ",
    " ##      ## ",
};

constexpr const char* kExpectedProjectileRows[kProjectileHeight] = {
    "##", "##", "##", "##", "##", "##",
};

// Checks `sprite`'s Color array cell-by-cell against `rows`/`onColor`,
// never by calling any production build function.
void checkSpriteMatchesRows(const steamcore::Sprite& sprite,
                             const char* const* rows, int32_t width,
                             int32_t height, Color onColor) {
  for (int32_t row = 0; row < height; ++row) {
    for (int32_t col = 0; col < width; ++col) {
      const Color expected = rows[row][col] == '#' ? onColor : Color::BLACK;
      CHECK(sprite.pixels[row * sprite.stride + col] == expected);
    }
  }
}

}  // namespace

STEAMCORE_TEST(galactic_invasion_player_sprite_matches_art) {
  checkSpriteMatchesRows(kPlayerSprite, kExpectedPlayerRows, kPlayerWidth,
                          kPlayerHeight, Color::BRIGHT_ORANGE);
}

STEAMCORE_TEST(galactic_invasion_enemy_sprite_matches_art) {
  checkSpriteMatchesRows(kEnemySprite, kExpectedEnemyRows, kEnemyWidth,
                          kEnemyHeight, Color::ORANGE);
}

STEAMCORE_TEST(galactic_invasion_projectile_sprite_matches_art) {
  checkSpriteMatchesRows(kProjectileSprite, kExpectedProjectileRows,
                          kProjectileWidth, kProjectileHeight,
                          Color::BRIGHT_ORANGE);
}

// AC-12.2: blit's inherited transparency -- BLACK source cells leave the
// destination untouched, every '#' cell writes the sprite's own colour.
// Not reimplemented here: this proves composition, not blit's own
// contract (already proven elsewhere).
STEAMCORE_TEST(galactic_invasion_player_sprite_blits_with_inherited_transparency) {
  Framebuffer fb;
  fb.fillRect(0, 0, kPlayerWidth, kPlayerHeight, Color::DARK_ORANGE);

  fb.blit(kPlayerSprite, 0, 0);

  for (int32_t row = 0; row < kPlayerHeight; ++row) {
    for (int32_t col = 0; col < kPlayerWidth; ++col) {
      const bool onCell = kExpectedPlayerRows[row][col] == '#';
      const Color expected = onCell ? Color::BRIGHT_ORANGE : Color::DARK_ORANGE;
      CHECK(fb.pixel(col, row) == expected);
    }
  }
}

STEAMCORE_TEST(galactic_invasion_enemy_sprite_blits_with_inherited_transparency) {
  Framebuffer fb;
  fb.fillRect(0, 0, kEnemyWidth, kEnemyHeight, Color::DARK_ORANGE);

  fb.blit(kEnemySprite, 0, 0);

  for (int32_t row = 0; row < kEnemyHeight; ++row) {
    for (int32_t col = 0; col < kEnemyWidth; ++col) {
      const bool onCell = kExpectedEnemyRows[row][col] == '#';
      const Color expected = onCell ? Color::ORANGE : Color::DARK_ORANGE;
      CHECK(fb.pixel(col, row) == expected);
    }
  }
}

STEAMCORE_TEST(galactic_invasion_projectile_sprite_blits_with_inherited_transparency) {
  Framebuffer fb;
  fb.fillRect(0, 0, kProjectileWidth, kProjectileHeight, Color::DARK_ORANGE);

  fb.blit(kProjectileSprite, 0, 0);

  for (int32_t row = 0; row < kProjectileHeight; ++row) {
    for (int32_t col = 0; col < kProjectileWidth; ++col) {
      const bool onCell = kExpectedProjectileRows[row][col] == '#';
      const Color expected = onCell ? Color::BRIGHT_ORANGE : Color::DARK_ORANGE;
      CHECK(fb.pixel(col, row) == expected);
    }
  }
}
