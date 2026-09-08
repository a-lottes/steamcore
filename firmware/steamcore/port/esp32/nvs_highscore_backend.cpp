#include "steamcore/port/esp32/nvs_highscore_backend.h"

#include "esp_log.h"
#include "nvs_flash.h"

namespace steamcore::port::esp32 {
namespace {

constexpr char kLogTag[] = "nvs_highscore_backend";
// This feature's own NVS namespace and single blob key -- HighscoreStore
// already encodes the whole HighscoreBlock (every game slot) into one
// contiguous byte array (highscore.h), so one key holds everything.
constexpr char kNamespace[] = "highscore";
constexpr char kBlobKey[] = "block";

}  // namespace

bool NvsHighscoreBackend::init() {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // Documented recoverable outcomes -- a factory-fresh or
    // format-mismatched partition, not a hardware fault (this file's own
    // header contract). Erase and retry once.
    ESP_LOGW(kLogTag, "nvs_flash_init reported %s -- erasing and retrying",
             esp_err_to_name(err));
    err = nvs_flash_erase();
    if (err != ESP_OK) {
      ESP_LOGE(kLogTag, "nvs_flash_erase failed: %s", esp_err_to_name(err));
      return false;
    }
    err = nvs_flash_init();
  }
  if (err != ESP_OK) {
    ESP_LOGE(kLogTag, "nvs_flash_init failed: %s", esp_err_to_name(err));
    return false;
  }

  err = nvs_open(kNamespace, NVS_READWRITE, &handle_);
  if (err != ESP_OK) {
    ESP_LOGE(kLogTag, "nvs_open failed: %s", esp_err_to_name(err));
    return false;
  }

  initialized_ = true;
  return true;
}

bool NvsHighscoreBackend::read(uint8_t* out, int32_t length) const {
  if (!initialized_) return false;

  size_t storedLength = static_cast<size_t>(length);
  const esp_err_t err = nvs_get_blob(handle_, kBlobKey, out, &storedLength);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    // Nothing ever written -- first boot, an ordinary outcome, not an
    // error (this file's own header contract).
    return false;
  }
  if (err != ESP_OK) {
    ESP_LOGW(kLogTag, "nvs_get_blob failed: %s", esp_err_to_name(err));
    return false;
  }
  if (storedLength != static_cast<size_t>(length)) {
    // A stored blob from a different format version (or genuine
    // corruption) -- treated exactly like "not found", never a crash.
    ESP_LOGW(kLogTag, "stored blob size %d does not match expected %d",
             static_cast<int>(storedLength), static_cast<int>(length));
    return false;
  }
  return true;
}

bool NvsHighscoreBackend::write(const uint8_t* data, int32_t length) {
  if (!initialized_) return false;

  esp_err_t err = nvs_set_blob(handle_, kBlobKey, data, static_cast<size_t>(length));
  if (err != ESP_OK) {
    ESP_LOGW(kLogTag, "nvs_set_blob failed: %s", esp_err_to_name(err));
    return false;
  }
  err = nvs_commit(handle_);
  if (err != ESP_OK) {
    ESP_LOGW(kLogTag, "nvs_commit failed: %s", esp_err_to_name(err));
    return false;
  }
  return true;
}

}  // namespace steamcore::port::esp32
