#pragma once

// Includes
#include "esp_err.h"

// Functions
esp_err_t js_audio_init(void);
void js_audio_play_pause_song(uint8_t song_index);
void js_audio_play_pause_emergency_audio(void);