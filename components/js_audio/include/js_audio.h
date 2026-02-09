#pragma once

// Includes
#include "esp_err.h"

// Functions
esp_err_t js_audio_init(void);
void js_audio_play_pause(const char *path);
