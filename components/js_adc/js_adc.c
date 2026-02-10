/** NOTE: THIS ASSUMES ONLY ADC 1 */

// Self Include
#include "js_adc.h"

// Library Includes
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Local Includes
// #include "js_leds.h"

// Defines
#define TAG "js_adc"
#define PIN_BATTERY_READ ADC_CHANNEL_2
#define ADC_ATTEN ADC_ATTEN_DB_12

// Forward Declarations
adc_oneshot_unit_handle_t adc1_handle;
adc_cali_handle_t battery_read_cal_handle;
static int read_pin(adc_oneshot_unit_handle_t handle, adc_channel_t channel, adc_cali_handle_t cal_handle, int loops);
static esp_err_t adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle);
// static void adc_calibration_deinit(adc_cali_handle_t handle);

/** Initialize JS Battery
 * Init the pins and code for the battery monitoring
 */
esp_err_t js_adc_init(void) {
    esp_err_t ret = ESP_FAIL;
    ESP_LOGI(TAG, "js_adc_init...");

    //-------------ADC1 Init---------------//
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc1_handle));

    //-------------ADC1 Config---------------//
    adc_oneshot_chan_cfg_t chan_config = {
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, PIN_BATTERY_READ, &chan_config));

    //-------------ADC1 Calibration Init---------------//
    ESP_GOTO_ON_ERROR(adc_calibration_init(ADC_UNIT_1, PIN_BATTERY_READ, ADC_ATTEN, &battery_read_cal_handle), error, TAG, "Failed to initialize ADC calibration");

    return ESP_OK;

error:
    return ret;
}

/* ************************** Global Functions ************************** */
// Get the current battery state
int js_adc_battery_voltage() {
    int reading = read_pin(adc1_handle, PIN_BATTERY_READ, battery_read_cal_handle, 3);
    ESP_LOGI(TAG, "Battery Voltage: %d mV", reading);
    return reading * 2; // Scale the reading by 2 because of the voltage divider
}

/* ************************** Local Functions ************************** */

static esp_err_t adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle) {
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;

    ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = unit,
        .chan = channel,
        .atten = atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_GOTO_ON_ERROR(adc_cali_create_scheme_curve_fitting(&cali_config, &handle), error, TAG, "create curve fitting scheme failed");

    // Output the handle to the caller
    *out_handle = handle;

    return ESP_OK;

error:
    return ret;
}

// static void adc_calibration_deinit(adc_cali_handle_t handle) {
//     ESP_LOGI(TAG, "deregister %s calibration scheme", "Curve Fitting");
//     ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));
// }

static int read_pin(adc_oneshot_unit_handle_t handle, adc_channel_t channel, adc_cali_handle_t cal_handle, int loops) {
    int raw;
    int voltage;
    int raw_sum = 0;

    // Read the oneshot ADC value multiple times and average to reduce noise
    for (int i = 0; i < loops; i++) {
        ESP_ERROR_CHECK(adc_oneshot_read(handle, channel, &raw));
        raw_sum += raw;
        vTaskDelay(pdMS_TO_TICKS(1)); // Small delay between readings
    }
    raw = raw_sum / loops; // Average the raw readings

    // Convert the raw reading to a voltage in mV using the calibration handle and return
    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cal_handle, raw, &voltage));
    return voltage;
}