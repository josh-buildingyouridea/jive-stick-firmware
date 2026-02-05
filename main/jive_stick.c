// Library Includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_check.h"

// Managed Components Includes
// #include "led_strip.h"

// Components Includes
#include "js_leds.h"
#include "js_sleep.h"
#include "js_serial_input.h"

// Defines
#define TAG "main"

// Forward Declarations
static esp_err_t systemInits(void);
static esp_err_t componentInits(void);

/*************************** Main Loop ***************************/
void app_main(void)
{
	// Print startup message
	vTaskDelay(pdMS_TO_TICKS(1500));
	printf("%s: Starting Jive Stick Firmware...\n", TAG);

	// Inits
	ESP_ERROR_CHECK(systemInits());
	ESP_ERROR_CHECK(componentInits());

	// Handle Wake-Up Reason
	js_sleep_handle_wakeup();

	// *************** Temp ******************
	while (1)
	{
		js_leds_set_color(10, 0, 0); // Red
		printf("LED ON\n");
		vTaskDelay(pdMS_TO_TICKS(500));

		js_leds_clear();
		printf("LED OFF\n");
		vTaskDelay(pdMS_TO_TICKS(500));
	}
}

/*************************** Local Functions ***************************/

/** Init system functionality needed for multiple components */
static esp_err_t systemInits(void)
{
	esp_err_t ret = ESP_FAIL;
	ESP_LOGI(TAG, "systemInits starting...");

	// Inits
	ESP_GOTO_ON_ERROR(nvs_flash_init(), error, TAG, "systemInits:Failed to initialize NVS");
	ESP_GOTO_ON_ERROR(js_serial_input_init(), error, TAG, "systemInits:Failed to initialize JS Serial Input");
	return ESP_OK;

error:
	return ret;
}

/** Init components */
static esp_err_t componentInits(void)
{
	esp_err_t ret = ESP_FAIL;
	ESP_LOGI(TAG, "componentInits starting...");

	// Inits
	ESP_GOTO_ON_ERROR(js_leds_init(), error, TAG, "componentInits:Failed to initialize JS LEDs");
	return ESP_OK;

error:
	return ret;
}

/*************************** Event Handlers ***************************/
