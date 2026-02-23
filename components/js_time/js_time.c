// Self Include
#include "js_time.h"

// Library Includes
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

// Local Includes
#include "js_events.h"
#include "js_i2c.h"
#include "js_user_settings.h"

// Defines
#define TAG "js_time"

// PCF8523 Register Addresses
#define I2C_ADDR 0x68    // PCF8523 I2C address
#define REG_SECONDS 0x03 // 0x03..0x09 = sec,min,hour,day,weekday,month,year
                         // OS bit is bit 7 of seconds register

// Forward Declarations
static i2c_master_dev_handle_t i2c_rtc_handle = NULL;
static bool rtc_time_is_time_valid();

static void set_system_time(uint64_t unix_seconds);
static esp_err_t set_rtc_time(uint64_t unix_seconds);

static uint8_t bcd_to_bin(uint8_t bcd);
static uint8_t bin_to_bcd(uint8_t bin);

static time_t tm_to_utc(const struct tm *t);

// Move to I2S?
static esp_err_t read_register(uint8_t reg_addr, uint8_t *data, size_t len);
static esp_err_t write_register(uint8_t reg_addr, uint8_t *data, size_t len);

// Timer stuff
static esp_timer_handle_t alarm_timer_handle = NULL;
static int alarm_song_index = 0;
static void alarm_timer_callback(void *arg);

/** Initialize JS RTC (PCF8523) */
esp_err_t js_time_init(void) {
    esp_err_t ret = ESP_FAIL;
    ESP_LOGI(TAG, "js_time_init...");

    // Confirm that the I2C bus is initialized
    ESP_GOTO_ON_FALSE(i2c_bus_handle != NULL, ESP_ERR_INVALID_STATE, error, TAG, "I2C bus not initialized");

    // Create the RTC device on the I2C bus
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = I2C_ADDR,
        .scl_speed_hz = 100000,
    };

    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(i2c_bus_handle, &dev_config, &i2c_rtc_handle), TAG, "bus_add_device failed");

    // Check if the RTC time is valid
    if (!rtc_time_is_time_valid()) {
        ESP_LOGE(TAG, "RTC oscillator stop flag is set. Time may be invalid.");
        // return ESP_OK;
    }

    // Set the timezone from user settings (this will handle DST and local time conversions)
    js_time_set_timezone(js_user_settings_get_timezone());

    // Read the unix time from the RTC and set the system time
    uint64_t unix_time;
    ESP_GOTO_ON_ERROR(js_time_read_rtc(&unix_time), error, TAG, "Failed to read time from RTC");
    set_system_time(unix_time);

    // Init the alarm timer (but don't start it yet)
    esp_timer_create_args_t timer_args = {
        .callback = alarm_timer_callback,
        .name = "alarm_timer",
    };
    ESP_GOTO_ON_ERROR(esp_timer_create(&timer_args, &alarm_timer_handle), error, TAG, "Failed to create alarm timer");

    return ESP_OK;

error:
    return ret;
}

/* ************************** Global Functions ************************** */
// Sets the timezone for the system to handle DST and local time conversions.
esp_err_t js_time_set_timezone(const char *tz) {
    ESP_LOGI(TAG, "Setting timezone to: %s", tz);
    setenv("TZ", tz, 1);
    tzset();
    return ESP_OK;
}

// Reads the current time from the RTC and writes to passed pointer as unix seconds
esp_err_t js_time_read_rtc(uint64_t *unix_seconds) {
    esp_err_t ret = ESP_FAIL;

    // Read the data in BCD format from the RTC registers
    uint8_t time_data[7];
    ESP_GOTO_ON_ERROR(read_register(REG_SECONDS, time_data, sizeof(time_data)), error, TAG, "Failed to read time from RTC");

    // Print the raw BCD data for debugging
    // ESP_LOGI(TAG, "RTC Time Data: %02x %02x %02x %02x %02x %02x %02x", time_data[0], time_data[1], time_data[2], time_data[3], time_data[4], time_data[5], time_data[6]);

    // Mask off the OS bit from seconds
    time_data[0] &= 0x7F;

    // Convert BCD to binary
    for (int i = 0; i < 7; i++) {
        time_data[i] = bcd_to_bin(time_data[i]);
    }

    // Print in human-readable format
    ESP_LOGI(TAG, "RTC Time: 20%02d-%02d-%02d %02d:%02d:%02d",
             time_data[6], time_data[5], time_data[3], time_data[2], time_data[1], time_data[0]);

    // Build the time struct tm
    struct tm timeStruct = {
        .tm_sec = time_data[0],
        .tm_min = time_data[1],
        .tm_hour = time_data[2],
        .tm_mday = time_data[3],
        .tm_wday = time_data[4],
        .tm_mon = time_data[5] - 1,    // tm_mon is 0-11
        .tm_year = time_data[6] + 100, // tm_year is years since 1900, and RTC gives years since 2000
        .tm_isdst = 0,                 // RTC does not store DST info
    };

    // Convert to unix time
    time_t utc_seconds = tm_to_utc(&timeStruct);
    *unix_seconds = (uint64_t)utc_seconds;
    // printf("Converted Unix Time: %lld\n", (int64_t)*unix_seconds);

    return ESP_OK;

error:
    return ret;
}

// read the current system time and return the unix seconds
esp_err_t js_time_read_sys_unix(uint64_t *unix_seconds) {
    time_t now = time(NULL);
    *unix_seconds = (uint64_t)now;
    return ESP_OK;
}

// Read the current system time and passes back
esp_err_t js_time_read_sys() {
    // Get the unix time from the system
    time_t now = time(NULL);
    ESP_LOGI(TAG, "Current system time: %lld", (int64_t)now);

    // Get the local time struct
    struct tm local_time;
    localtime_r(&now, &local_time);
    ESP_LOGI(TAG, "Current local time: %04d-%02d-%02d %02d:%02d:%02d",
             local_time.tm_year + 1900, local_time.tm_mon + 1, local_time.tm_mday,
             local_time.tm_hour, local_time.tm_min, local_time.tm_sec);

    return ESP_OK;
}

// Set the RTC time using unix seconds and resets the OS bit to indicate time is valid
esp_err_t js_time_set(uint64_t unix_seconds) {
    esp_err_t ret = ESP_FAIL;

    // Set the RTC time
    ESP_GOTO_ON_ERROR(set_rtc_time(unix_seconds), error, TAG, "Failed to set time on RTC");

    // Also set the system time so that time functions work correctly
    set_system_time(unix_seconds);

    return ESP_OK;

error:
    return ret;
}

esp_err_t js_time_set_next_alarm(uint64_t seconds_from_now, int song_index) {
    int64_t us = (int64_t)seconds_from_now * 1000000; // Convert to microseconds
    ESP_LOGI(TAG, "Setting next alarm for %lld seconds from now (us: %lld) with song index %d", seconds_from_now, us, song_index);

    // Stop the timer if it's already running
    esp_timer_stop(alarm_timer_handle);

    // Set the global song index for the alarm callback to use
    alarm_song_index = song_index;

    // Start the timer with the new alarm time
    ESP_RETURN_ON_ERROR(esp_timer_start_once(alarm_timer_handle, us), TAG, "Failed to start alarm timer");

    return ESP_OK;
}

/* ************************** Local Functions ************************** */

// Check if the RTC time is valid by checking the Oscillator Stop (OS) bit in the seconds register
bool rtc_time_is_time_valid() {
    uint8_t seconds;
    esp_err_t err = read_register(REG_SECONDS, &seconds, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read seconds register: %s", esp_err_to_name(err));
        return false; // If we can't read the register, assume time is invalid
    }
    return (seconds & 0x80) == 0; // OS bit is bit 7 of seconds register
}

// Set system time
void set_system_time(uint64_t unix_seconds) {
    struct timeval tv = {
        .tv_sec = (time_t)unix_seconds,
        .tv_usec = 0,
    };
    settimeofday(&tv, NULL);
    ESP_LOGI(TAG, "Set system local time to: %s", ctime(&tv.tv_sec));
}

// Write the time to the RTC
esp_err_t set_rtc_time(uint64_t unix_seconds) {
    // Convert unix seconds to struct tm
    struct tm timeinfo;
    time_t t = (time_t)unix_seconds;
    gmtime_r(&t, &timeinfo);

    // Handle years before 2000 (PCF8523 counts years since 2000)
    int full_year = timeinfo.tm_year + 1900;
    if (full_year < 2000 || full_year > 2099) return ESP_ERR_INVALID_ARG;

    // Convert to BCD format
    uint8_t time_data[7];
    time_data[0] = bin_to_bcd(timeinfo.tm_sec) & 0x7F; // clears OS bit
    time_data[1] = bin_to_bcd(timeinfo.tm_min) & 0x7F;
    time_data[2] = bin_to_bcd(timeinfo.tm_hour) & 0x3F;
    time_data[3] = bin_to_bcd(timeinfo.tm_mday) & 0x3F;
    time_data[4] = bin_to_bcd(timeinfo.tm_wday) & 0x07;
    time_data[5] = bin_to_bcd(timeinfo.tm_mon + 1) & 0x1F;  // 1-12
    time_data[6] = bin_to_bcd((uint8_t)(full_year - 2000)); // 00-99

    // Write the data to the RTC registers (starting at seconds register)
    return write_register(REG_SECONDS, time_data, sizeof(time_data));
}

/* **************************** I2C Helpers **************************** */
static esp_err_t read_register(uint8_t reg_addr, uint8_t *data, size_t len) {
    return i2c_master_transmit_receive(i2c_rtc_handle, &reg_addr, 1, data, len, -1);
}

static esp_err_t write_register(uint8_t reg_addr, uint8_t *data, size_t len) {
    uint8_t buffer[1 + len];
    buffer[0] = reg_addr;
    memcpy(buffer + 1, data, len);
    return i2c_master_transmit(i2c_rtc_handle, buffer, 1 + len, -1);
}

/* ******************** Binary Coded Decimal Helpers ******************** */
// PCF8523 stores time in BCD format, so we need to convert between BCD and binary
static uint8_t bcd_to_bin(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

static uint8_t bin_to_bcd(uint8_t bin) {
    return ((bin / 10) << 4) | (bin % 10);
}

/* *********************** Struct To UTC Helpers ************************ */
// Helper for tm_to_utc to determine if a year is a leap year
static bool is_leap(int y) {
    return (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
}

// Helper function to take the time struct and convert to UTC seconds without timezone adjustments
static time_t tm_to_utc(const struct tm *t) {
    // expects tm_year = years since 1900, tm_mon 0-11
    int year = t->tm_year + 1900;
    int mon = t->tm_mon;
    int day = t->tm_mday;

    static const int mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    // days since 1970-01-01
    int64_t days = 0;
    for (int y = 1970; y < year; y++)
        days += is_leap(y) ? 366 : 365;

    for (int m = 0; m < mon; m++) {
        days += mdays[m];
        if (m == 1 && is_leap(year)) days += 1;
    }
    days += (day - 1);

    int64_t secs = days * 86400LL + t->tm_hour * 3600 + t->tm_min * 60 + t->tm_sec;
    return (time_t)secs;
}

/*********************** Alarm Timer Callback *************************/
static void alarm_timer_callback(void *arg) {
    ESP_LOGI(TAG, "Alarm timer callback triggered! Playing alarm song index: %d", alarm_song_index);
    // Call to play the song
    uint8_t song_idx = alarm_song_index;
    esp_event_post(JS_EVENT_BASE, JS_EVENT_PLAY_AUDIO, &song_idx, sizeof(song_idx), portMAX_DELAY);

    // Set the next alarm based on user settings
    esp_event_post(JS_EVENT_BASE, JS_EVENT_SET_NEXT_ALARM, NULL, 0, portMAX_DELAY);
}