// Slef Include
#include "js_leds.h"

// Library Includes
#include <stdio.h>
#include "esp_log.h"

// Managed Components Includes
#include "led_strip.h"

// Defines
#define TAG "js_leds"
#define LED_STRIP_GPIO 8

// Forward Declarations
static led_strip_handle_t led_strip;

/** Initialize JS LEDs
 * Init the pins and code for the Neopixel LED
 */
esp_err_t js_leds_init(void)
{
	ESP_LOGI(TAG, "js_leds_init...");

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
void js_leds_set_color(uint8_t red, uint8_t green, uint8_t blue)
{
	led_strip_set_pixel(led_strip, 0, red, green, blue);
	led_strip_refresh(led_strip);
}

/** Clear JS LEDs */
void js_leds_clear(void)
{
	led_strip_clear(led_strip);
	led_strip_refresh(led_strip);
}
