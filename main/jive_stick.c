// Library Includes
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <time.h>

#include <dirent.h> // for reading files

// Managed Components Includes
// #include "led_strip.h"
#include "esp_littlefs.h"

// Components Includes
#include "js_adc.h"
#include "js_audio.h"
#include "js_battery.h"
#include "js_ble.h"
#include "js_buttons.h"
#include "js_events.h"
#include "js_i2c.h"
#include "js_leds.h"
#include "js_serial_input.h"
#include "js_sleep.h"
#include "js_time.h"
#include "js_user_settings.h"

// Defines
#define TAG "main"

// Forward Declarations
static esp_err_t init_system(void);
static esp_err_t init_components(void);
static void app_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data);

/*************************** Main Loop ***************************/
void app_main(void) {
    // Print startup message
    // vTaskDelay(pdMS_TO_TICKS(1500));
    printf("%s: Starting Jive Stick Firmware...\n", TAG);

    // Inits
    ESP_ERROR_CHECK_WITHOUT_ABORT(init_system());
    ESP_ERROR_CHECK_WITHOUT_ABORT(init_components());

    // Handle Wake-Up Reason
    js_sleep_handle_wakeup();

    // *************** Temp ******************
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/fs",
        .partition_label = "storage",
        .format_if_mount_failed = false,
    };
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_vfs_littlefs_register(&conf));

    DIR *dir = opendir("/fs");
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        printf("  - %s\n", entry->d_name);
    }
    closedir(dir);

    // *************** Temp END ******************

    // Run the app
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/*************************** Local Functions ***************************/

/** Init system functionality needed for multiple components */
static esp_err_t init_system(void) {
    esp_err_t ret = ESP_FAIL;
    ESP_LOGI(TAG, "init_system starting...");

    // Inits
    ESP_GOTO_ON_ERROR(nvs_flash_init(), error, TAG, "init_system: Failed to initialize NVS");
    ESP_GOTO_ON_ERROR(gpio_install_isr_service(0), error, TAG, "init_system: Failed to install ISR service");
    ESP_GOTO_ON_ERROR(js_adc_init(), error, TAG, "init_system: Failed to initialize JS ADC");
    ESP_GOTO_ON_ERROR(js_i2c_init(), error, TAG, "init_system: Failed to initialize JS I2C");
    ESP_GOTO_ON_ERROR(esp_event_loop_create_default(), error, TAG, "init_system:Failed to create default event loop");
    ESP_GOTO_ON_ERROR(esp_event_handler_register(JS_EVENT_BASE, ESP_EVENT_ANY_ID, app_event_handler, NULL), error, TAG, "init_system:Failed to register event handler");
    ESP_GOTO_ON_ERROR(js_serial_input_init(), error, TAG, "init_system: Failed to initialize JS Serial Input");
    return ESP_OK;

error:
    return ret;
}

/** Init components */
static esp_err_t init_components(void) {
    esp_err_t ret = ESP_FAIL;
    ESP_LOGI(TAG, "init_components starting...");

    // Inits
    ESP_GOTO_ON_ERROR(js_user_settings_init(), error, TAG, "init_components: Failed to initialize JS User Settings");
    ESP_GOTO_ON_ERROR(js_leds_init(), error, TAG, "init_components: Failed to initialize JS LEDs");
    ESP_GOTO_ON_ERROR(js_buttons_init(), error, TAG, "init_components: Failed to initialize JS Buttons");
    ESP_GOTO_ON_ERROR(js_battery_init(), error, TAG, "init_components: Failed to initialize JS Battery");
    ESP_GOTO_ON_ERROR(js_audio_init(), error, TAG, "init_components: Failed to initialize JS Audio");
    ESP_GOTO_ON_ERROR(js_ble_init(), error, TAG, "init_components: Failed to initialize JS BLE");
    ESP_GOTO_ON_ERROR(js_time_init(), error, TAG, "init_components: Failed to initialize JS RTC");
    return ESP_OK;

error:
    return ret;
}

/*************************** Event Handler ***************************/
static void app_event_handler(void *arg, esp_event_base_t base, int32_t id,
                              void *data) {
    ESP_LOGI(TAG, "Event received: base=%s, id=%ld", base, id);

    // Skip if not our event base
    if (base != JS_EVENT_BASE) return;

    switch (id) {
    // ********************* Time Events *********************
    case JS_EVENT_READ_SYSTEM_TIME:
        // ESP_LOGI(TAG, "Read time command received");
        uint64_t rtc_unix_time;
        uint64_t sys_unix_time;
        if (js_time_read_rtc(&rtc_unix_time) == ESP_OK) {
            printf("Current RTC Unix Time: %lld\n", rtc_unix_time);
        } else {
            ESP_LOGE(TAG, "Failed to read time from RTC");
        }
        if (js_time_read_sys_unix(&sys_unix_time) == ESP_OK) {
            printf("Current System Unix Time: %lld -> %s \n", sys_unix_time, ctime((time_t *)&sys_unix_time));
        } else {
            ESP_LOGE(TAG, "Failed to read time from system");
        }
        break;

    case JS_EVENT_WRITE_SYSTEM_TIME:
        // ESP_LOGI(TAG, "Set time command received with data: %s", (char *)data);
        uint64_t new_time = strtoull((char *)data, NULL, 10);
        printf("Setting system time to: %lld\n", new_time);
        js_time_set(new_time);
        break;

    case JS_EVENT_SET_NEXT_ALARM:
        ESP_LOGI(TAG, "JS_EVENT_SET_NEXT_ALARM command received");
        uint64_t seconds_until_alarm;
        int next_alarm_song_index;
        if (js_user_settings_seconds_until_next_alarm(&seconds_until_alarm, &next_alarm_song_index) == ESP_OK) {
            if (seconds_until_alarm == UINT64_MAX) {
                printf("No enabled alarms found\n");
            } else {
                printf("Seconds until next alarm: %lld\n", seconds_until_alarm);
                printf("Next alarm song index: %d\n", next_alarm_song_index);
            }
        } else {
            ESP_LOGE(TAG, "Failed to calculate seconds until next alarm");
        }
        break;

    // ***************** User Settings Events ****************
    case JS_EVENT_READ_TIMEZONE:
        // ESP_LOGI(TAG, "Read timezone command received");
        const char *tz = js_user_settings_get_timezone();
        printf("Current timezone: %s\n", tz);
        break;

    case JS_EVENT_WRITE_TIMEZONE:
        ESP_LOGI(TAG, "Write timezone command received with data: %s", (char *)data);
        js_user_settings_set_timezone((char *)data); // Set the timezone in user settings (and nvs)
        js_time_set_timezone((char *)data);          // Update the system time settings
        break;

    case JS_EVENT_READ_ALARMS:
        // ESP_LOGI(TAG, "Read alarms command received");
        const char *alarms = js_user_settings_get_alarms();
        printf("Current Alarms: %s\n", alarms);
        break;

    case JS_EVENT_WRITE_ALARMS:
        ESP_LOGI(TAG, "Write alarms command received with data: %s", (char *)data);
        esp_err_t rsp = js_user_settings_set_alarms((char *)data);
        if (rsp == ESP_OK) {
            printf("Alarms updated successfully\n");
        } else {
            printf("Failed to update alarms: %s\n", esp_err_to_name(rsp));
        }
        break;
    // // Time Events
    // case JS_EVENT_READ_SYSTEM_TIME:
    //     ESP_LOGI(TAG, "Read time command received");
    //     uint64_t unix_time;
    //     if (js_time_read_rtc(&unix_time) == ESP_OK) {
    //         ESP_LOGI(TAG, "Current RTC Unix Time: %lld", unix_time);
    //         js_time_read_sys();
    //     } else {
    //         ESP_LOGE(TAG, "Failed to read time from RTC");
    //     }
    //     break;

    // AUDIO.....
    case JS_EVENT_PLAY_AUDIO:
        ESP_LOGI(TAG, "Play audio command received");
        js_audio_play_pause("/fs/Groovin_120s_16k_adpcm_3db.wav");
        break;

    case JS_EVENT_EMERGENCY_BUTTON_PRESSED:
        ESP_LOGI(TAG, "Emergency button pressed");
        js_audio_play_pause("/fs/OldTimeRockAndRoll_120s_16k_adpcm_6db.wav");
        break;

    // BLE.....
    case JS_EVENT_START_PAIRING:
        ESP_LOGI(TAG, "JS_EVENT_START_PAIRING command received");
        js_ble_set_state(BLE_STATE_CONNECTED);
        break;

    case JS_EVENT_STOP_BLE:
        ESP_LOGI(TAG, "JS_EVENT_STOP_BLE command received");
        js_ble_set_state(BLE_STATE_DISCONNECTED);
        break;

    // Battery.....
    case JS_EVENT_SHOW_BATTERY_STATUS:
        ESP_LOGI(TAG, "JS_EVENT_SHOW_BATTERY_STATUS command received");
        js_set_show_battery_state(true);
        break;

    case JS_EVENT_HIDE_BATTERY_STATUS:
        ESP_LOGI(TAG, "JS_EVENT_HIDE_BATTERY_STATUS command received");
        js_set_show_battery_state(false);
        break;

        // ***************** User Settings Events ****************

    default:
        ESP_LOGW(TAG, "Unknown event ID: %ld", id);
        break;
    }
}