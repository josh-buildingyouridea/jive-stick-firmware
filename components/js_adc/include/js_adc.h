#pragma once

// Includes
#include "esp_err.h"

// Functions
esp_err_t js_adc_init(void);
int js_adc_battery_voltage();
