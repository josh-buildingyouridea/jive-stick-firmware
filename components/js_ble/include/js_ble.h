#pragma once

// Includes
#include "esp_err.h"

// BLE States
typedef enum {
    BLE_STATE_DISCONNECTED,
    BLE_STATE_PAIRING,
    BLE_STATE_CONNECTED,
} ble_state_t;

// Functions
esp_err_t js_ble_init(void);
esp_err_t js_ble_start_advertising(void);
ble_state_t js_ble_get_state(void);
esp_err_t js_ble_stop(void);