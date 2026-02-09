// Slef Include
#include "js_leds.h"

// Library Includes
#include "driver/gpio.h"
#include "esp_log.h"
#include <stdio.h>

// Managed Components Includes
#include "led_strip.h"

// Local Includes
#include "js_ble.h"

// Defines
#define TAG "js_leds"
#define LED_STRIP_GPIO GPIO_NUM_8
#define PIN_LED_BLE GPIO_NUM_7
#define LED_BLE_ON 0 // LED is on when low
#define PIN_MASK ((1ULL << PIN_LED_BLE))

// Forward Declarations
static led_strip_handle_t led_strip;
static void ble_led_task(void *arg);

/** Initialize JS LEDs
 * Init the pins and code for the Neopixel LED
 * Init the pins and code for the BLE LED
 */
esp_err_t js_leds_init(void) {
    ESP_LOGI(TAG, "js_leds_init...");

    // Regular LED Output Config
    gpio_config_t led_config = {
        .pin_bit_mask = PIN_MASK,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&led_config);
    gpio_set_level(PIN_LED_BLE, !LED_BLE_ON); // Start off

    // Task to handle BLE LED
    xTaskCreate(ble_led_task, "ble_led_task", 2048, NULL, 5, NULL);

    /* LED strip config */
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO,
        .max_leds = 1, // Strip Length
    };

    /* RMT backend config for C6 */
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
    };

    led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);

    // Start Off
    led_strip_clear(led_strip);
    led_strip_refresh(led_strip);

    // Return OK
    return ESP_OK;
}

/* ************************** Global Functions ************************** */
/** Set JS LEDs to specific color */
void js_leds_set_color(uint8_t red, uint8_t green, uint8_t blue) {
    led_strip_set_pixel(led_strip, 0, red, green, blue);
    led_strip_refresh(led_strip);
}

/** Clear JS LEDs */
void js_leds_clear(void) {
    led_strip_clear(led_strip);
    led_strip_refresh(led_strip);
}

/* ************************** Local Functions ************************** */
// Set the ble_led based on the ble status
static void ble_led_task(void *arg) {
    bool led_was_on = false;

    while (1) {
        // Delay 500mS
        vTaskDelay(pdMS_TO_TICKS(500));

        // Get the BLE state
        ble_state_t ble_state = js_ble_get_state();

        // If connected, turn on the LED
        if (ble_state == BLE_STATE_CONNECTED) {
            gpio_set_level(PIN_LED_BLE, LED_BLE_ON);
            led_was_on = true;
            continue;
        }

        // If pairing, toggle the LED
        if (ble_state == BLE_STATE_PAIRING) {
            gpio_set_level(PIN_LED_BLE, led_was_on ? !LED_BLE_ON : LED_BLE_ON);
            led_was_on = !led_was_on;
            continue;
        }

        // If disconnected, turn off the LED
        if (ble_state == BLE_STATE_DISCONNECTED) {
            gpio_set_level(PIN_LED_BLE, !LED_BLE_ON);
            led_was_on = false;
            continue;
        }
    }
}
