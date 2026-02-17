#pragma once
#include "host/ble_gatt.h"
#include <stdint.h>

// Passing handles between js_ble.c and js_ble_gatt.c
const struct ble_gatt_svc_def *js_ble_get_gatt_svcs(void);
void js_ble_gatt_set_conn_handle(uint16_t conn_handle);

// Send a notify payload on the notify characteristic
esp_err_t js_ble_notify(const char *s);