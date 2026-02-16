// Self Include
#include "js_ble_gatt.h"

// Libraray includes
#include "esp_log.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include <string.h>

// Service and Characteristics addresses (UUIDs)
static const ble_uuid128_t SVC_UUID = BLE_UUID128_INIT(0x9E, 0x81, 0x20, 0x01, 0x22, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E);      // 6E400001-B5A3-F393-E0A9-E5220120819E
static const ble_uuid128_t CHR_READ_UUID = BLE_UUID128_INIT(0x9E, 0x81, 0x20, 0x01, 0x22, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x02, 0x00, 0x40, 0x6E); // 6E400002-B5A3-F393-E0A9-E5220120819E
// static const ble_uuid128_t CHR_WRITE_UUID = BLE_UUID128_INIT(0x9E, 0x81, 0x20, 0x01, 0x22, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x03, 0x00, 0x40, 0x6E); // 6E400003-B5A3-F393-E0A9-E5220120819E

// Forward Declarations
static int hello_access_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);

/* ****************** Service / Characteristics Definitions ***************** */
// Custom characteristics definition
static const struct ble_gatt_chr_def gatt_chrs[] = {
    {
        .uuid = (const ble_uuid_t *)&CHR_READ_UUID,
        .access_cb = hello_access_cb,
        .flags = BLE_GATT_CHR_F_READ,
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

/* ************************** Callback Functions ************************** */
static int hello_access_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
        return BLE_ATT_ERR_READ_NOT_PERMITTED;
    }
    const char *msg = "hello world";
    return os_mbuf_append(ctxt->om, msg, strlen(msg)) == 0
               ? 0
               : BLE_ATT_ERR_INSUFFICIENT_RES;
}