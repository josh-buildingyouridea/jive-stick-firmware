// Slef Include
#include "js_battery.h"

// Library Includes
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

// Local Includes
#include "js_leds.h"

// Defines
#define TAG "js_battery"
#define PIN_PWR_IN GPIO_NUM_3

// Forward Declarations
static void input_pin_isr(void *arg);
static void show_battery_state_task(void *arg);
static bool _show_battery_state = false;
static js_battery_state_t battery_state = JS_BATTERY_STATE_UNKNOWN;

/** Initialize JS Battery
 * Init the pins and code for the battery monitoring
 */
esp_err_t js_battery_init(void) {
    esp_err_t ret = ESP_FAIL;
    ESP_LOGI(TAG, "js_battery_init...");

    // Power Input Pin Config
    gpio_config_t pwr_config = {
        .pin_bit_mask = (1ULL << PIN_PWR_IN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE // Interrupt on any edge
    };
    ESP_GOTO_ON_ERROR(gpio_config(&pwr_config), error, TAG, "Failed to configure power input pin");

    // Install the ISR
    ESP_GOTO_ON_ERROR(gpio_isr_handler_add(PIN_PWR_IN, input_pin_isr, NULL), error, TAG, "Failed to add ISR handler");

    // Task to show the battery state
    xTaskCreate(show_battery_state_task, "show_battery_state_task", 2048, NULL, 10, NULL);
    // Return OK
    return ESP_OK;

error:
    return ret;
}

/* ************************** Global Functions ************************** */
// Get the current battery state
js_battery_state_t js_battery_get_state(void) {
    return battery_state;
}

/* ************************** Local Functions ************************** */
// Input Pin ISR
static void input_pin_isr(void *arg) {
    // Read the pin state
    if (gpio_get_level(PIN_PWR_IN)) {
        // ESP_LOGI(TAG, "Battery is charging");
        battery_state = JS_BATTERY_STATE_CHARGING;
    } else {
        // ESP_LOGI(TAG, "Battery is discharging");
        battery_state = JS_BATTERY_STATE_DISCHARGING;
    }
}

// Show the battery state task
static void show_battery_state_task(void *arg) {
    // Set the inital battery state
    battery_state = gpio_get_level(PIN_PWR_IN) ? JS_BATTERY_STATE_CHARGING : JS_BATTERY_STATE_DISCHARGING;

    for (;;) {
        switch (battery_state) {
        case JS_BATTERY_STATE_CHARGING:
            // ESP_LOGI(TAG, "Battery is charging");
            break;
        case JS_BATTERY_STATE_DISCHARGING:
            // ESP_LOGI(TAG, "Battery is discharging");
            if (_show_battery_state) {
                // TODO: Read battery voltage
                js_leds_set_color(10, 10, 0);
            } else {
                js_leds_clear();
            }
            break;
        default:
            // ESP_LOGI(TAG, "Battery state unknown");
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
