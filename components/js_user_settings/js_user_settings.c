// Self Include
#include "js_user_settings.h"

// Library Includes
#include "esp_check.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <sys/time.h>
#include <time.h>

// Local Includes

// Defines
#define TAG "js_user_settings"
#define SETTINGS_NAMESPACE "user_prefs"

// Forward Declarations
js_user_prefs_t user_prefs;
esp_err_t save_to_nvs(void);

/** Initialize JS User Settings */
esp_err_t js_user_settings_init(void) {
    ESP_LOGI(TAG, "js_user_settings_init...");

    // Open NVS namespace for reading
    nvs_handle_t nvs_handle;
    ESP_RETURN_ON_ERROR(nvs_open(SETTINGS_NAMESPACE, NVS_READWRITE, &nvs_handle), TAG, "Failed to open NVS namespace");

    // Load the settings object into user_prefs
    size_t required_size = sizeof(user_prefs);
    ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_get_blob(nvs_handle, "prefs", &user_prefs, &required_size));

    return ESP_OK;
}

/* ************************** Global Functions ************************** */
// Read the timezone setting string (returns default if not set)
const char *js_user_settings_get_timezone() {
    return user_prefs.timezone[0] != '\0'
               ? user_prefs.timezone
               : "EST5EDT,M3.2.0/2,M11.1.0/2"; // Default to US Eastern Time with DST. This can be changed as needed. Format is TZ database format: https://www.iana.org/time-zones
}

// Write the new timezone setting
esp_err_t js_user_settings_set_timezone(const char *tz) {
    ESP_LOGI(TAG, "Setting timezone to: %s", tz);
    if (strlen(tz) >= sizeof(user_prefs.timezone)) return ESP_ERR_INVALID_ARG;

    // Update the in-memory prefs
    strncpy(user_prefs.timezone, tz, sizeof(user_prefs.timezone));
    user_prefs.timezone[sizeof(user_prefs.timezone) - 1] = '\0';

    // Save to NVS
    ESP_RETURN_ON_ERROR(save_to_nvs(), TAG, "Failed to save user preferences to NVS");

    return ESP_OK;
}

/**
 * Read the alarm settings as a string
 * Format: "HH:MM,enabled,song_index;HH:MM,enabled,song_index;..."
 */
const char *js_user_settings_get_alarms() {
    static char buffer[256]; // TODO: Calculate required size based on alarm count
    size_t offset = 0;
    buffer[0] = '\0'; // Ensure buffer is empty

    for (uint8_t i = 0; i < user_prefs.alarm_count; i++) {
        const js_alarm_t *alarm = &user_prefs.alarms[i];
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%02d:%02d,%d,%d;",
                           alarm->hour, alarm->minute, alarm->enabled, alarm->song_index);
        if (offset >= sizeof(buffer)) {
            ESP_LOGW(TAG, "Alarm buffer overflow");
            break;
        }
    }

    // Check if the buffer is empty (no alarms) and return an appropriate message
    if (offset == 0) {
        snprintf(buffer, sizeof(buffer), "No alarms set");
    }

    return buffer;
}

/**
 * Fully overwrite the alarm settings from a passed in string
 * Format: "HH:MM,enabled,song_index;HH:MM,enabled,song_index;..."
 */
esp_err_t js_user_settings_set_alarms(const char *alarm_str) {
    ESP_LOGI(TAG, "Setting alarms from string: %s", alarm_str);
    js_alarm_t new_alarms[10];
    uint8_t new_alarm_count = 0;

    // Parse the input string
    char *input_copy = strdup(alarm_str);  // Create a temp copy to work from
    char *token = strtok(input_copy, ";"); // Split by semicolon to get each alarm

    // Loop through each alarm token and parse the details
    while (token != NULL && new_alarm_count < 10) {
        int hour, minute, enabled, song_index;
        if (sscanf(token, "%d:%d,%d,%d", &hour, &minute, &enabled, &song_index) == 4) {
            if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || enabled < 0 || enabled > 1 || song_index < 0 || song_index > 3) {
                free(input_copy);
                return ESP_ERR_INVALID_ARG; // Invalid alarm format
            }
            new_alarms[new_alarm_count++] = (js_alarm_t){.hour = hour, .minute = minute, .enabled = enabled, .song_index = song_index};
        } else {
            free(input_copy);
            return ESP_ERR_INVALID_ARG; // Invalid alarm format
        }
        token = strtok(NULL, ";");
    }
    free(input_copy);

    // Update the in-memory prefs
    user_prefs.alarm_count = new_alarm_count;
    memcpy(user_prefs.alarms, new_alarms, sizeof(js_alarm_t) * new_alarm_count);

    // Save to NVS
    ESP_RETURN_ON_ERROR(save_to_nvs(), TAG, "Failed to save user preferences to NVS");

    return ESP_OK;
}

esp_err_t js_user_settings_seconds_until_next_alarm(uint64_t *seconds_until_alarm, int *next_alarm_song_index) {
    // Get the current unix time
    time_t now = time(NULL);

    // Get the local time struct
    struct tm local_time;
    localtime_r(&now, &local_time);

    // Loop through enabled alarms to find the next one
    time_t soonest_alarm_time = 0;
    for (uint8_t i = 0; i < user_prefs.alarm_count; i++) {
        const js_alarm_t *alarm = &user_prefs.alarms[i];

        // Skip if the alarm is not enabled
        if (!alarm->enabled) continue;

        // Create a tm struct for the alarm time based on the local_time struct
        struct tm alarm_time = local_time;
        alarm_time.tm_hour = alarm->hour;
        alarm_time.tm_min = alarm->minute;
        alarm_time.tm_sec = 0;
        alarm_time.tm_isdst = -1; // Let the system determine if DST is in effect

        // Convert the alarm time to unix seconds (with timezone adjustments)
        time_t alarm_unix_sec = mktime(&alarm_time);

        // If the alarm time has already passed today, add 24 hours to get the next occurrence
        if (alarm_unix_sec <= now) {
            alarm_unix_sec += 24 * 60 * 60;
        }

        // Update the soonest alarm time if this alarm is sooner
        if (soonest_alarm_time == 0 || alarm_unix_sec < soonest_alarm_time) {
            soonest_alarm_time = alarm_unix_sec;
            if (next_alarm_song_index != NULL) {
                *next_alarm_song_index = alarm->song_index;
            }
        }
    }

    if (soonest_alarm_time == 0) {
        printf("No enabled alarms found\n");
        *seconds_until_alarm = UINT64_MAX; // No enabled alarms
    } else {
        *seconds_until_alarm = soonest_alarm_time - now;
    }
    return ESP_OK;
}

/* ************************** Local Functions ************************** */
esp_err_t save_to_nvs(void) {
    nvs_handle_t nvs_handle;
    ESP_RETURN_ON_ERROR(nvs_open(SETTINGS_NAMESPACE, NVS_READWRITE, &nvs_handle), TAG, "Failed to open NVS namespace");
    ESP_RETURN_ON_ERROR(nvs_set_blob(nvs_handle, "prefs", &user_prefs, sizeof(user_prefs)), TAG, "Failed to write user preferences to NVS");
    ESP_RETURN_ON_ERROR(nvs_commit(nvs_handle), TAG, "Failed to commit user preferences to NVS");
    return ESP_OK;
}