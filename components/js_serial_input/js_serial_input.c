/**
 * TODO: Implement handling of the serial input commands
 * TODO: Echo after each character not on enter
 */

// Self Include
#include "js_serial_input.h"

// Library Includes
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
// For calling events
#include "esp_event.h"
#include "js_events.h"

// Defines
#define TAG "js_serial_input"

// Forward Declarations
static void serial_input_handler(void *arg);

// Initialize the serial input handler
esp_err_t js_serial_input_init(void)
{
	ESP_LOGI(TAG, "js_serial_input_init");
	// setvbuf(stdout, NULL, _IONBF, 0); // unbuffered stdout (for echo typing)
	xTaskCreate(serial_input_handler, "serial", 4096, NULL, 5, NULL);
	return ESP_OK;
}

// Monitor serial input and trigger action on return
static void serial_input_handler(void *arg)
{
	char line[64];
	int idx = 0;

	ESP_LOGI(TAG, "Serial task started. Type commands and press Enter.");

	while (1)
	{
		int c = getchar(); // returns -1 if no data

		// If no data is available, yield CPU
		if (c < 0)
		{
			vTaskDelay(pdMS_TO_TICKS(100)); // yield CPU
			continue;
		}

		// Check for newline or full buffer
		if (c == '\r' || c == '\n' || idx >= (int)sizeof(line) - 1)
		{
			// Ignore if no characters have been read
			if (idx == 0)
				continue;

			// Print out the received line
			line[idx] = 0; // null-terminate the string
			idx = 0;	   // reset index

			// --------------- Handle the input ----------------
			printf("%s: Received: %s\n", TAG, line);

			switch (line[0])
			{
			case 't':
				ESP_LOGI(TAG, "Time command received");
				esp_event_post(JS_EVENT_BASE, JS_SET_TIME, line, strlen(line) + 1, 0);
				break;

			case 's':
				ESP_LOGI(TAG, "Go to sleep command received");
				esp_event_post(JS_EVENT_BASE, JS_EVENT_GOTO_SLEEP, line, strlen(line) + 1, 0);
				break;

			default:
				ESP_LOGW(TAG, "Unknown command: %s", line);
				break;
			}

			// ESP_LOGI(TAG, "Received: %s", line);
			continue;
		}

		// Add character to line buffer
		line[idx++] = (char)c;

		// Echo the character back
		// putchar(c);
	}
}
