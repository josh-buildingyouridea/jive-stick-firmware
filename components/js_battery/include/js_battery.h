#pragma once

// Includes
#include "esp_err.h"
#include <stdbool.h>

// Functions
esp_err_t js_battery_init(void);
void js_set_show_battery_state(bool show);
int js_battery_read_voltage(void);
bool js_battery_is_charging(void);
