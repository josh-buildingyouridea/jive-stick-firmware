// Self Include
#include "js_ble.h"
#include "js_ble_gatt.h"

// Includes
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

// Defines
#define TAG "js_ble"

// Forward Declarations
static bool _stack_is_ready = false;
static uint8_t s_own_addr_type;
//
static void on_stack_ready(void);
static void start_advertising(void);
// Connection tasks
static int gap_event_cb(struct ble_gap_event *event, void *arg);
static void ble_host_task(void *param);
static uint16_t ble_conn_handle = BLE_HS_CONN_HANDLE_NONE;

/** Initialize the BLE Stack */
esp_err_t js_ble_init(void) {
    esp_err_t ret = ESP_FAIL;
    ESP_LOGI(TAG, "js_ble_init...");

    // Initialize the default NimBLE stack drivers
    nimble_port_init();
    ble_svc_gap_init();
    ble_svc_gatt_init();

    // Set the device name (this will show up when scanning for BLE devices)
    ble_svc_gap_device_name_set("JiveStick");

    // Configure the GATT server with the custom service and characteristic in js_ble_gatt.c
    const struct ble_gatt_svc_def *gatt_svcs = js_ble_get_gatt_svcs();
    ESP_GOTO_ON_FALSE(ble_gatts_count_cfg(gatt_svcs) == 0, ESP_ERR_INVALID_STATE, error, TAG, "Invalid GATT configuration");
    ESP_GOTO_ON_FALSE(ble_gatts_add_svcs(gatt_svcs) == 0, ESP_ERR_INVALID_STATE, error, TAG, "Failed to add GATT services");

    // When the BLE stack is ready, it will call on_stack_ready which sets _stack_is_ready to true
    ble_hs_cfg.sync_cb = on_stack_ready;

    nimble_port_freertos_init(ble_host_task);

    return ESP_OK;

error:
    return ret;
}

// Handle request to start advertising (called from main app when BLE stack is ready)
esp_err_t js_ble_start_advertising(void) {
    if (!_stack_is_ready) {
        ESP_LOGW(TAG, "BLE stack not ready, cannot start advertising");
        return ESP_ERR_INVALID_STATE;
    }

    ble_state_t state = js_ble_get_state();
    if (state == BLE_STATE_CONNECTED) {
        ESP_LOGW(TAG, "Already connected, cannot start advertising");
        return ESP_ERR_INVALID_STATE;
    }

    if (state == BLE_STATE_PAIRING) {
        ESP_LOGW(TAG, "Already advertising, cannot start advertising again");
        return ESP_ERR_INVALID_STATE;
    }

    // Start advertising logic here
    start_advertising();
    return ESP_OK;
}

// Disconnect and stop advertising (called from main app when requested)
esp_err_t js_ble_stop(void) {
    // If currently connected, disconnect
    if (ble_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        int rc = ble_gap_terminate(ble_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        if (rc != 0) {
            ESP_LOGE(TAG, "Failed to terminate connection: %d", rc);
            return ESP_FAIL;
        }
    }

    // If currently advertising, stop advertising
    if (ble_gap_adv_active()) {
        int rc = ble_gap_adv_stop();
        if (rc != 0) {
            ESP_LOGE(TAG, "Failed to stop advertising: %d", rc);
            return ESP_FAIL;
        }
    }

    ESP_LOGI(TAG, "BLE stopped and disconnected");
    return ESP_OK;
}

// Check the BLE state
ble_state_t js_ble_get_state(void) {
    if (ble_gap_adv_active()) return BLE_STATE_PAIRING;

    if (ble_conn_handle != BLE_HS_CONN_HANDLE_NONE) return BLE_STATE_CONNECTED;

    return BLE_STATE_DISCONNECTED;
}

/* **************************** Connecting/Disconnecting *************************** */
// Start BLE advertising with the custom service UUID
static void start_advertising(void) {
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP; // Discoverable and BR/EDR unsupported (BLE only)

    // Set the device name in the advertising data
    const char *name = ble_svc_gap_device_name(); // Set in js_ble_init -> ble_svc_gap_device_name_set("JiveStick");
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    // Set the advertising fields to NimBLE
    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set advertising fields: %d", rc);
        return;
    }

    // Configure advertising parameters (connectable undirected advertising)
    struct ble_gap_adv_params adv = {0};
    adv.conn_mode = BLE_GAP_CONN_MODE_UND; // connectable
    adv.disc_mode = BLE_GAP_DISC_MODE_GEN; // discoverable

    // Start advertising with defined callback for GAP events
    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &adv, gap_event_cb, NULL);

    if (rc != 0)
        ESP_LOGE(TAG, "adv_start rc=%d", rc);
    else
        ESP_LOGI(TAG, "Advertising...");
}

/* ******************************* Callback Handlers ******************************* */
static int gap_event_cb(struct ble_gap_event *event, void *arg) {
    switch (event->type) {

    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            ble_conn_handle = event->connect.conn_handle;
            ESP_LOGI(TAG, "Connected");
        } else {
            ble_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            ESP_LOGW(TAG, "Connect failed; restart adv");
            start_advertising();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Disconnected");
        ble_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        return 0;

    default:
        return 0;
    }
}

/* ***************************** Local Set-Up Functions **************************** */
// On BLE stack ready, set _stack_is_ready to true
static void on_stack_ready(void) {
    ESP_LOGI(TAG, "BLE stack is ready");
    ble_hs_id_infer_auto(0, &s_own_addr_type); // Ensure address type is set (needed for adv_start)
    // ESP_LOGI(TAG, "Device Address: %s", util_addr_str(ble_hs_id_get()));
    _stack_is_ready = true; // Set ready flag so we can start advertising when requested
}

// NimBLE task that runs the BLE event loop
static void ble_host_task(void *param) {
    nimble_port_run();             // NimBLE event loop (blocks)
    nimble_port_freertos_deinit(); // cleanup if it ever exits
}