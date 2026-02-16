#pragma once

// Includes
#include "esp_err.h"
#include <stdbool.h>

// Types
typedef struct {
    uint8_t hour;       // 0–23 (local time)
    uint8_t minute;     // 0–59
    uint8_t song_index; // 0–3
    bool enabled;
} js_alarm_t;

typedef struct {
    char timezone[64]; // POSIX TZ string
    uint8_t alarm_count;
    js_alarm_t alarms[10]; // Support up to 10 alarms for now.
} js_user_prefs_t;

// Functions
esp_err_t js_user_settings_init(void);
const char *js_user_settings_get_timezone();
esp_err_t js_user_settings_set_timezone(const char *tz);
const char *js_user_settings_get_alarms();
esp_err_t js_user_settings_set_alarms(const char *alarm_str);
esp_err_t js_user_settings_seconds_until_next_alarm(uint64_t *seconds_until_alarm, int *next_alarm_song_index);

/*
A:09:00,1,1;11:00,1,2;13:41,1,3;13:45,1,3
A:09:00,0,1
*/