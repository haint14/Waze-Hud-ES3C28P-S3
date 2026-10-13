#pragma once

#include "esp_err.h"

namespace waze_hud {

// Starts the overspeed alert task (ES3C28P only). Returns ESP_ERR_NOT_SUPPORTED
// on other boards or when CONFIG_WAZE_HUD_AUDIO_ALERT is disabled.
esp_err_t startOverspeedAudio();

}  // namespace waze_hud
