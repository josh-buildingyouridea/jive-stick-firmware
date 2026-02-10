#pragma once

// Includes
#include "esp_err.h"

// Battery States
typedef enum {
    JS_BATTERY_STATE_UNKNOWN = 0,
    JS_BATTERY_STATE_CHARGING,
    JS_BATTERY_STATE_DISCHARGING,
    JS_BATTERY_STATE_FULL
} js_battery_state_t;

// Functions
esp_err_t js_battery_init(void);
js_battery_state_t js_battery_get_state(void);
