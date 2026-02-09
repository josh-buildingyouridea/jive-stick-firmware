// Library Includes
#include "esp_check.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <stdio.h>

#include <dirent.h> // for reading files

// Managed Components Includes
// #include "led_strip.h"
#include "esp_littlefs.h"

// Components Includes
#include "js_audio.h"
#include "js_buttons.h"
#include "js_events.h"
#include "js_leds.h"
#include "js_serial_input.h"
#include "js_sleep.h"

// Defines
#define TAG "main"

// Forward Declarations
static esp_err_t systemInits(void);
static esp_err_t componentInits(void);
static void app_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data);

/*************************** Main Loop ***************************/
void app_main(void) {
    // Print startup message
    // vTaskDelay(pdMS_TO_TICKS(1500));
    printf("%s: Starting Jive Stick Firmware...\n", TAG);

    // Inits
    ESP_ERROR_CHECK(systemInits());
    ESP_ERROR_CHECK(componentInits());

    // Handle Wake-Up Reason
    js_sleep_handle_wakeup();

    // *************** Temp ******************
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/fs",
        .partition_label = "storage",
        .format_if_mount_failed = false,
    };
    ESP_ERROR_CHECK(esp_vfs_littlefs_register(&conf));

    DIR *dir = opendir("/fs");
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        printf("  - %s\n", entry->d_name);
    }
    closedir(dir);

    while (1) {
        js_leds_set_color(10, 0, 0); // Red
        // printf("LED ON\n");
        vTaskDelay(pdMS_TO_TICKS(500));

        js_leds_clear();
        // printf("LED OFF\n");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/*************************** Local Functions ***************************/

/** Init system functionality needed for multiple components */
static esp_err_t systemInits(void) {
    esp_err_t ret = ESP_FAIL;
    ESP_LOGI(TAG, "systemInits starting...");

    // Inits
    ESP_GOTO_ON_ERROR(esp_event_loop_create_default(), error, TAG, "systemInits:Failed to create default event loop");
    ESP_GOTO_ON_ERROR(esp_event_handler_register(JS_EVENT_BASE, ESP_EVENT_ANY_ID, app_event_handler, NULL), error, TAG, "systemInits:Failed to register event handler");
    ESP_GOTO_ON_ERROR(nvs_flash_init(), error, TAG, "systemInits:Failed to initialize NVS");
    ESP_GOTO_ON_ERROR(js_serial_input_init(), error, TAG, "systemInits:Failed to initialize JS Serial Input");
    return ESP_OK;

error:
    return ret;
}

/** Init components */
static esp_err_t componentInits(void) {
    esp_err_t ret = ESP_FAIL;
    ESP_LOGI(TAG, "componentInits starting...");

    // Inits
    ESP_GOTO_ON_ERROR(js_buttons_init(), error, TAG, "componentInits:Failed to initialize JS Buttons");
    ESP_GOTO_ON_ERROR(js_leds_init(), error, TAG, "componentInits:Failed to initialize JS LEDs");
    ESP_GOTO_ON_ERROR(js_audio_init(), error, TAG, "componentInits:Failed to initialize JS Audio");
    return ESP_OK;

error:
    return ret;
}

/*************************** Event Handler ***************************/
static void app_event_handler(void *arg, esp_event_base_t base, int32_t id,
                              void *data) {
    printf("Event received: base=%s, id=%ld\n", base, id);

    // Skip if not our event base
    if (base != JS_EVENT_BASE) return;

    switch (id) {
    case JS_EVENT_GOTO_SLEEP:
        ESP_LOGI(TAG, "Sleep command received");
        break;

    case JS_SET_TIME:
        ESP_LOGI(TAG, "Set time command received with data: %s", (char *)data);
        break;

    case JS_EVENT_PLAY_AUDIO:
        ESP_LOGI(TAG, "Play audio command received");
        js_audio_play_pause("/fs/Groovin_120s_16k_adpcm_3db.wav");
        break;

    case JS_EVENT_EMERGENCY_BUTTON_PRESSED:
        ESP_LOGI(TAG, "Emergency button pressed");
        js_audio_play_pause("/fs/OldTimeRockAndRoll_120s_16k_adpcm_6db.wav");
        break;

    default:
        ESP_LOGW(TAG, "Unknown event ID: %ld", id);
        break;
    }
}