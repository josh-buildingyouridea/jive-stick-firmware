// Slef Include
#include "js_ble.h"

// Includes
#include "esp_log.h"

// Defines
#define TAG "js_ble"

// Forward Declarations
static ble_state_t current_state = BLE_STATE_DISCONNECTED;

/** Initialize JS BLE
 * Init the BLE stack and set initial state
 */
esp_err_t js_ble_init(void) {
    ESP_LOGI(TAG, "js_ble_init...");
    current_state = BLE_STATE_DISCONNECTED;
    return ESP_OK;
}

/** Set BLE State */
void js_ble_set_state(ble_state_t state) {
    current_state = state;
    ESP_LOGI(TAG, "BLE State changed to %d", state);
}

/** Get BLE State */
ble_state_t js_ble_get_state(void) {
    return current_state;
}