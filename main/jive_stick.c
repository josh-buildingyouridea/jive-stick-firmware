#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"

#include <stdio.h>

#define LED_STRIP_GPIO 8
#define LED_STRIP_NUM 1

void app_main(void)
{
	led_strip_handle_t led_strip;

	/* LED strip config */
	led_strip_config_t strip_config = {
		.strip_gpio_num = LED_STRIP_GPIO,
		.max_leds = LED_STRIP_NUM,
	};

	/* RMT backend config for C6 */
	led_strip_rmt_config_t rmt_config = {
		.resolution_hz = 10 * 1000 * 1000, // 10MHz
	};

	led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);

	while (1)
	{
		led_strip_set_pixel(led_strip, 0, 255, 0, 0); // Red
		led_strip_refresh(led_strip);
		vTaskDelay(pdMS_TO_TICKS(500));

		led_strip_clear(led_strip);
		led_strip_refresh(led_strip);
		vTaskDelay(pdMS_TO_TICKS(500));
	}
}