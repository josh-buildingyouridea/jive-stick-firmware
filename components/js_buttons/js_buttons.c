// Self Include
#include "js_buttons.h"

// Library Includes
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_timer.h"
// For calling events
#include "esp_event.h"
#include "js_events.h"

// Defines
#define TAG "js_buttons"
#define DEBOUNCE_TIME 1		 // ms
#define LONG_PRESS_TIME 1000 // ms

#define BTN_RED GPIO_NUM_23
#define BTN_BLUE GPIO_NUM_17
#define BTN_YELLOW GPIO_NUM_16
#define BTN_IS_PRESSED 0 // Active low
static const uint32_t button_pins[] = {
	GPIO_NUM_23,
	GPIO_NUM_17,
	GPIO_NUM_16,
};
#define BTN_COUNT (sizeof(button_pins) / sizeof(button_pins[0]))

// Button event types
typedef enum
{
	BUTTON_EVENT_PRESS,			// On press down
	BUTTON_EVENT_LONG_PRESS,	// On long press timer
	BUTTON_EVENT_RELEASE_SHORT, // On short press release
	BUTTON_EVENT_RELEASE_LONG,	// On long press release
} button_event_type_t;

// Struct for the button state
typedef struct
{
	uint32_t pin;
	int64_t press_start_time;	  // Time when button was first pressed
	int64_t debounce_timeout_end; // Time when debounce period ends
	bool was_pressed;
	TimerHandle_t long_press_timer;
} button_props_t;

// Struct to pass from ISR on button press
typedef struct
{
	uint32_t pin;
	button_event_type_t type;
} button_event_t;

// Forward Declarations
static QueueHandle_t button_press_queue = NULL;
static void button_press_handler(void *arg);
static void button_isr(void *arg);
static void long_press_timer_callback(TimerHandle_t xTimer);
static button_props_t button_props[BTN_COUNT];

/** Initialize button GPIOs, ISRs and handlers */
esp_err_t js_buttons_init(void)
{
	esp_err_t ret = ESP_FAIL;
	static uint64_t button_pin_mask = 0;
	ESP_LOGI(TAG, "js_buttons_init...");

	// *********** Pin configs ***********
	// Create the pin mask from button_pins array
	for (int i = 0; i < BTN_COUNT; i++)
	{
		button_pin_mask |= (1ULL << button_pins[i]);
	}
	// Define and set the pin configuration
	gpio_config_t btn_config = {
		.pin_bit_mask = button_pin_mask,	   // Pins to configure
		.mode = GPIO_MODE_INPUT,			   // Set as input
		.pull_up_en = GPIO_PULLUP_ENABLE,	   // Enable pull-up resistor
		.pull_down_en = GPIO_PULLDOWN_DISABLE, // Disable pull-down resistor
		.intr_type = GPIO_INTR_ANYEDGE		   // Interrupt on any edge
	};
	ESP_GOTO_ON_ERROR(gpio_config(&btn_config), error, TAG, "js_buttons_init: Failed to configure GPIOs");

	// Create the initial props for each button
	for (int i = 0; i < BTN_COUNT; i++)
	{
		button_props[i].pin = button_pins[i];
		button_props[i].press_start_time = 0;
		button_props[i].debounce_timeout_end = 0;
		button_props[i].was_pressed = false;
		button_props[i].long_press_timer = NULL;
	}

	// Create long-press timers for each button
	for (int i = 0; i < BTN_COUNT; i++)
	{
		button_props[i].long_press_timer = xTimerCreate("long_press_timer", pdMS_TO_TICKS(LONG_PRESS_TIME), pdFALSE, (void *)i, long_press_timer_callback);
		ESP_GOTO_ON_FALSE(button_props[i].long_press_timer != NULL, ESP_FAIL, error, TAG, "js_buttons_init: Failed to create long-press timer");
	}

	// Init the button press handler
	button_press_queue = xQueueCreate(10, sizeof(button_event_t));
	ESP_GOTO_ON_ERROR(button_press_queue == NULL ? ESP_FAIL : ESP_OK, error, TAG, "js_buttons_init: Failed to create button press queue");
	xTaskCreate(button_press_handler, "button_press_handler", 2048, NULL, 10, NULL);

	// Install GPIO ISR service
	ESP_GOTO_ON_ERROR(gpio_install_isr_service(0), error, TAG, "js_buttons_init: Failed to install ISR service");
	for (int i = 0; i < BTN_COUNT; i++)
	{
		// Pass in the button_props/button_pins index
		ESP_GOTO_ON_ERROR(gpio_isr_handler_add(button_pins[i], button_isr, (void *)i), error, TAG, "js_buttons_init: Failed to add ISR handler for button");
	}

	// Return OK
	return ESP_OK;

error:
	return ret;
}

/** Long-press timer callback */
static void long_press_timer_callback(TimerHandle_t xTimer)
{
	uint32_t btn_idx = (uint32_t)pvTimerGetTimerID(xTimer);
	button_props_t *button_prop = &button_props[btn_idx];

	// Don't send if already released
	if (gpio_get_level(button_prop->pin) != BTN_IS_PRESSED)
		return;

	// Send the event
	button_event_t event = {.pin = button_prop->pin, .type = BUTTON_EVENT_LONG_PRESS};
	xQueueSend(button_press_queue, &event, 0);
}

/** ISR for button presses */
static void IRAM_ATTR button_isr(void *arg)
{
	button_event_type_t event_type; // Event type to send

	// Get the button state object from the index
	uint32_t btn_idx = (uint32_t)arg;
	button_props_t *button_prop = &button_props[btn_idx];

	// Get the current time
	int64_t current_time = esp_timer_get_time() / 1000; // ms

	// Check if we are in a debounce period
	if (current_time < button_prop->debounce_timeout_end)
		return; // Still in debounce period, ignore

	// Set the debounce timeout
	button_prop->debounce_timeout_end = current_time + DEBOUNCE_TIME;

	// Check if the button is currently pressed
	bool is_pressed = gpio_get_level(button_prop->pin) == BTN_IS_PRESSED;

	// If the button state hasn't changed, ignore
	if (is_pressed == button_prop->was_pressed)
		return;

	// Otherwise, update the button state
	button_prop->was_pressed = is_pressed;

	// If this is a new press
	if (is_pressed)
	{
		// Start the long-press timer
		xTimerStartFromISR(button_prop->long_press_timer, NULL);

		// Set the press start time
		button_prop->press_start_time = current_time;

		// Set the event type to send
		event_type = BUTTON_EVENT_PRESS;
	}
	else
	{
		// On release, stop the long-press timer
		xTimerStopFromISR(button_prop->long_press_timer, NULL);

		// Check if this was a short or long press
		if (current_time - button_prop->press_start_time < LONG_PRESS_TIME)
		{
			event_type = BUTTON_EVENT_RELEASE_SHORT; // Short Press
		}
		else
		{
			event_type = BUTTON_EVENT_RELEASE_LONG; // Long Press
		}
	}

	// Send short press event only, long press is handled by the timer
	button_event_t event = {.pin = button_prop->pin, .type = event_type};
	xQueueSendFromISR(button_press_queue, &event, NULL);
}

/** Handle button press events from the queue */
static void button_press_handler(void *arg)
{
	button_event_t event;

	for (;;)
	{
		if (xQueueReceive(button_press_queue, &event, portMAX_DELAY))
		{
			// printf("GPIO[%" PRIu32 "]: %d\n", event.pin, event.type);

			switch (event.pin)
			{
			case BTN_RED:
				if (event.type == BUTTON_EVENT_RELEASE_SHORT)
				{
					ESP_LOGI(TAG, "RED button SHORT pressed");
					// Send event to main task handler
					esp_event_post(JS_EVENT_BASE, JS_EVENT_EMERGENCY_BUTTON_PRESSED, NULL, 0, 0);
				}
				if (event.type == BUTTON_EVENT_LONG_PRESS)
				{
					ESP_LOGI(TAG, "RED button LONG pressed");
				}
				break;

			case BTN_BLUE:
				if (event.type == BUTTON_EVENT_RELEASE_SHORT)
				{
					ESP_LOGI(TAG, "BLUE button SHORT pressed");
				}
				if (event.type == BUTTON_EVENT_LONG_PRESS)
				{
					ESP_LOGI(TAG, "BLUE button LONG pressed");
				}
				break;

			case BTN_YELLOW:
				if (event.type == BUTTON_EVENT_RELEASE_SHORT)
				{
					ESP_LOGI(TAG, "YELLOW button SHORT pressed");
				}
				if (event.type == BUTTON_EVENT_LONG_PRESS)
				{
					ESP_LOGI(TAG, "YELLOW button LONG pressed");
				}
				break;

			default:
				ESP_LOGW(TAG, "Unknown button pressed\n");
				break;
			}
		}
	}
}