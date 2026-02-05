// Self Include
#include "js_sleep.h"

// Library Includes
#include <stdio.h>
#include "esp_log.h"
#include "esp_sleep.h"

// Defines
#define TAG "js_sleep"

void js_sleep_handle_wakeup(void)
{
	ESP_LOGI(TAG, "js_sleep_handle_wakeup");
	esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();

	ESP_LOGI(TAG, "Wakeup cause: %d", (int)wakeup_cause);

	// switch (wakeup_cause)
	// {
	// case ESP_SLEEP_WAKEUP_UNDEFINED:
	// 	ESP_LOGI(TAG, "Normal boot (not from deep sleep)");
	// 	break;
	// case ESP_SLEEP_WAKEUP_TIMER:
	// 	ESP_LOGI(TAG, "Wakeup caused by timer");
	// 	break;
	// case ESP_SLEEP_WAKEUP_GPIO:
	// 	ESP_LOGI(TAG, "Wakeup caused by GPIO");
	// 	break;
	// case ESP_SLEEP_WAKEUP_UART:
	// 	ESP_LOGI(TAG, "Wakeup caused by UART");
	// 	break;
	// default:
	// 	ESP_LOGI(TAG, "Wakeup cause: %d", (int)cause);
	// 	break;
	// }
}
