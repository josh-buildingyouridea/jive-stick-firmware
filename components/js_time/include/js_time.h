#pragma once

// Includes
#include "esp_err.h"

// Functions
esp_err_t js_time_init(void);
esp_err_t js_time_read_rtc(uint64_t *out_unix);
esp_err_t js_time_read_sys();
esp_err_t js_time_read_sys_unix(uint64_t *unix_seconds);
esp_err_t js_time_set(uint64_t unix_seconds);
esp_err_t js_time_set_timezone(const char *tz);

// Alarm functions
esp_err_t js_time_set_next_alarm(uint64_t seconds_from_now, int song_index);

/*
t:1770917169
*/