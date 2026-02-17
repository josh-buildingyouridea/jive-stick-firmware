// Self Include
#include "js_ble_gatt.h"

// Libraray includes
#include "esp_event.h"
#include "esp_log.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "js_events.h"
#include <string.h>

// Defines
#define TAG "js_ble_gatt"

// Service and Characteristics addresses (UUIDs)
static const ble_uuid128_t SVC_UUID = BLE_UUID128_INIT(0x9E, 0x81, 0x20, 0x01, 0x22, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E);        // 6E400001-B5A3-F393-E0A9-E5220120819E
static const ble_uuid128_t CHR_WRITE_UUID = BLE_UUID128_INIT(0x9E, 0x81, 0x20, 0x01, 0x22, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x02, 0x00, 0x40, 0x6E);  // 6E400002-B5A3-F393-E0A9-E5220120819E
static const ble_uuid128_t CHR_NOTIFY_UUID = BLE_UUID128_INIT(0x9E, 0x81, 0x20, 0x01, 0x22, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x03, 0x00, 0x40, 0x6E); // 6E400003-B5A3-F393-E0A9-E5220120819E

// Forward Declarations
static int ble_write_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
static int ble_notify_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
static uint16_t s_notify_val_handle;
static uint16_t ble_conn_handle = BLE_HS_CONN_HANDLE_NONE; // Connection handle passed from js_ble.c for use in notifications

/* ****************** Service / Characteristics Definitions ***************** */
// Custom characteristics definition
static const struct ble_gatt_chr_def gatt_chrs[] = {
    {
        .uuid = (const ble_uuid_t *)&CHR_WRITE_UUID,
        .access_cb = ble_write_callback,
        .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
    },
    {
        .uuid = (const ble_uuid_t *)&CHR_NOTIFY_UUID,
        .access_cb = ble_notify_callback,   // Notify characteristic callback (not used)
        .val_handle = &s_notify_val_handle, // Handle for sending notifications back to the client
        .flags = BLE_GATT_CHR_F_NOTIFY,
    },
    {0}};

// Applying the above characteristics to the service
static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = (const ble_uuid_t *)&SVC_UUID,
        .characteristics = gatt_chrs,
    },
    {0}};

// Passing back to js_ble.c for registration
const struct ble_gatt_svc_def *js_ble_get_gatt_svcs(void) {
    return gatt_svcs;
}

// Getting the notify handle from js_ble.c
void js_ble_gatt_set_conn_handle(uint16_t conn_handle) {
    ble_conn_handle = conn_handle;
}

/* ************************** Global Notify Function ************************** */

esp_err_t js_ble_notify(const char *s) {
    ESP_LOGI(TAG, "js_ble_notify called with: %s", s);

    if (!s) return ESP_ERR_INVALID_ARG;
    if (ble_conn_handle == BLE_HS_CONN_HANDLE_NONE) return ESP_ERR_INVALID_STATE;
    if (s_notify_val_handle == 0) return ESP_ERR_INVALID_STATE;

    size_t len = strlen(s);
    if (len == 0) return ESP_ERR_INVALID_ARG;

    struct os_mbuf *om = ble_hs_mbuf_from_flat(s, len);
    if (!om) return ESP_ERR_NO_MEM;

    int rc = ble_gatts_notify_custom(ble_conn_handle, s_notify_val_handle, om);
    if (rc != 0) {
        ESP_LOGE(TAG, "Notify failed: %d", rc);
        return ESP_FAIL;
    }

    return ESP_OK;
}

/* ************************** Callback Function ************************** */
// Callback for when the client (phone) write a command. Response will be sent back on the notify channel if needed.
static int ble_write_callback(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg) {
    // This callback is only for writes, so check op
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
    }

    // Create a buffer to hold the incoming data. Largest is alarms and 128 should be enough for 10 alarms
    char line[128];

    // Confirm that length passed is valid and not larger than our buffer
    int len = OS_MBUF_PKTLEN(ctxt->om);
    if (len <= 0 || len >= (int)sizeof(line)) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    // Copy the data from the mbuf to our buffer and null-terminate it
    int rc = ble_hs_mbuf_to_flat(ctxt->om, line, sizeof(line) - 1, NULL);
    if (rc != 0) return BLE_ATT_ERR_UNLIKELY;
    line[len] = '\0';

    // Parse the command and post to the main event loop for handling
    ESP_LOGI(TAG, "ble_write_callback received: %s", line);

    // Post to main event loop (copies bytes internally)
    // esp_event_post(JS_EVENT_BASE, JS_EVENT_READ_SYSTEM_TIME, NULL, 0, portMAX_DELAY);
    switch (line[0]) {
    // ********************* Time Events *********************
    case 't': // Read system time
        ESP_LOGI(TAG, "Read Time command received");
        esp_event_post(JS_EVENT_BASE, JS_EVENT_READ_SYSTEM_TIME, NULL, 0, 0);
        break;

    case 'T': // Write system time
        ESP_LOGI(TAG, "Write Time command received");
        // Strip out the first two character (T:) before posting the event
        esp_event_post(JS_EVENT_BASE, JS_EVENT_WRITE_SYSTEM_TIME, line + 2, strlen(line + 2) + 1, 0);
        break;

    // ***************** User Settings Events ****************
    case 'l': // Read Location/Timezone
        ESP_LOGI(TAG, "Read timezone command received");
        esp_event_post(JS_EVENT_BASE, JS_EVENT_READ_TIMEZONE, NULL, 0, 0);
        break;

    case 'L': // Write Location/Timezone
        ESP_LOGI(TAG, "Write timezone command received");
        // Strip out the first two character (L:) before posting the event with a null-terminated string
        esp_event_post(JS_EVENT_BASE, JS_EVENT_WRITE_TIMEZONE, line + 2, strlen(line + 2) + 1, 0);
        break;

    case 'a': // Read Alarms
        ESP_LOGI(TAG, "Read Alarms command received");
        esp_event_post(JS_EVENT_BASE, JS_EVENT_READ_ALARMS, NULL, 0, 0);
        break;

    case 'A': // Write Alarms
        ESP_LOGI(TAG, "Alarms command received");
        // Strip out the first two character (A:) before posting the event with a null-terminated string
        esp_event_post(JS_EVENT_BASE, JS_EVENT_WRITE_ALARMS, line + 2, strlen(line + 2) + 1, 0);
        break;

        // No payload response here (ATT-level write response is handled by stack)
        return 0;

    default:
        ESP_LOGW(TAG, "Unknown command: %s", line);
        js_ble_notify("Unknown command received");
        break;
    }

    // No payload response here (ATT-level write response is handled by stack)
    return 0;
}

// Notify Callback. This is needed for NimBLE but not used
static int ble_notify_callback(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg) {
    // We don’t allow reads/writes on the notify characteristic.
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) return BLE_ATT_ERR_READ_NOT_PERMITTED;
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
    return BLE_ATT_ERR_UNLIKELY;
}