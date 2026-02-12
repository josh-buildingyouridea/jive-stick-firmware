#pragma once

// Includes
#include "esp_err.h"

// Functions
esp_err_t js_time_init(void);
esp_err_t js_time_read_rtc(uint64_t *out_unix);
esp_err_t js_time_read_sys();
esp_err_t js_time_set(uint64_t unix_seconds);
esp_err_t js_time_set_timezone(const char *tz);

/*
t:1770917169
*/