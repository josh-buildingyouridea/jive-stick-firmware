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
#include "js_adc.h"
#include "js_leds.h"

// Defines
#define TAG "js_battery"
#define PIN_PWR_IN GPIO_NUM_3
#define BRIGHTNESS 10

// Forward Declarations
static void input_pin_isr(void *arg);
static void show_battery_state_task(void *arg);
static bool _show_battery_state = true;
static void show_battery_voltage(int voltage);

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
void js_set_show_battery_state(bool show) {
    _show_battery_state = show;
}

int js_battery_read_voltage(void) {
    return js_adc_battery_voltage();
}

bool js_battery_is_charging(void) {
    return gpio_get_level(PIN_PWR_IN);
}
/* ************************** Local Functions ************************** */
// Input Pin ISR
static void input_pin_isr(void *arg) {
    _show_battery_state = true;

    // Read the pin state
    // if (gpio_get_level(PIN_PWR_IN)) {
    //     // ESP_LOGI(TAG, "Battery is charging");
    //     _battery_charger_state = JS_BATTERY_CHARGER_STATE_CHARGING;
    // } else {
    //     // ESP_LOGI(TAG, "Battery is discharging");
    //     _battery_charger_state = JS_BATTERY_CHARGER_STATE_DISCHARGING;
    // }
}

// Show the battery state task
static void show_battery_state_task(void *arg) {
    // Set the inital battery state
    // battery_state = gpio_get_level(PIN_PWR_IN) ? JS_BATTERY_STATE_CHARGING : JS_BATTERY_STATE_DISCHARGING;
    bool is_charging = gpio_get_level(PIN_PWR_IN);
    bool LED_was_on = false;
    int show_battery_timeout_counter = 0;
    int battery_voltage = js_adc_battery_voltage();

    for (;;) {
        // Delay for blinking and to avoid spamming logs
        vTaskDelay(pdMS_TO_TICKS(500));

        // If we shouldn't show the battery state, just clear LEDs and skip
        if (!_show_battery_state) {
            js_leds_clear();
            continue;
        }

        // Check the charging state and battery voltage
        is_charging = gpio_get_level(PIN_PWR_IN);
        battery_voltage = js_adc_battery_voltage();

        // If not charging
        if (!is_charging) {
            // Show the battery voltage
            show_battery_voltage(battery_voltage);
            if (show_battery_timeout_counter++ > 10) { // After 5 seconds, stop showing the battery state to save power
                _show_battery_state = false;
                show_battery_timeout_counter = 0;
            }
            continue;
        }

        // If charging and was off, turn led ON
        if (!LED_was_on) {
            show_battery_voltage(battery_voltage);
            LED_was_on = true;
            continue;
        }

        // If fully charged, keep LED on
        if (battery_voltage > 4100) {
            show_battery_voltage(battery_voltage);
        } else {
            // Toggle off
            js_leds_clear();
            LED_was_on = false;
        }
    }
}

static void show_battery_voltage(int voltage) {
    // ESP_LOGI(TAG, "Battery Voltage: %d mV", voltage);
    if (voltage > 4100) {
        js_leds_set_color(0, BRIGHTNESS, 0); // Green for full
    } else if (voltage > 3700) {
        js_leds_set_color(BRIGHTNESS, BRIGHTNESS, 0); // Yellow for medium
    } else {
        js_leds_set_color(BRIGHTNESS, 0, 0); // Red for low
    }
}