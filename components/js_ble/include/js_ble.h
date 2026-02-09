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
ble_state_t js_ble_get_state(void);
void js_ble_set_state(ble_state_t state);
