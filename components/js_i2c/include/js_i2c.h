#pragma once

// Includes
#include "driver/i2c_master.h"
#include "esp_err.h"

// Functions
esp_err_t js_i2c_init(void);

// Global i2c bus handle
extern i2c_master_bus_handle_t i2c_bus_handle;
