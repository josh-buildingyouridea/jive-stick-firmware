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

// Defines
#define TAG "js_buttons"
#define DEBOUNCE_TIME 50	 // ms
#define LONG_PRESS_TIME 1000 // ms

#define BTN_RED GPIO_NUM_23
#define BTN_BLUE GPIO_NUM_17
#define BTN_YELLOW GPIO_NUM_16
static const uint32_t button_pins[] = {
	GPIO_NUM_23,
	GPIO_NUM_17,
	GPIO_NUM_16,
};
#define BTN_COUNT (sizeof(button_pins) / sizeof(button_pins[0]))

// Struct for the button state
typedef struct
{
	uint32_t pin;
	int64_t last_down_time; // Time when button last went LOW (pressed)
	bool is_pressed;
	TimerHandle_t long_press_timer;
} button_state_t;

// Init button states
static button_state_t button_states[BTN_COUNT] = {
	{.pin = BTN_RED, .last_down_time = 0, .is_pressed = false, .long_press_timer = NULL},
	{.pin = BTN_BLUE, .last_down_time = 0, .is_pressed = false, .long_press_timer = NULL},
	{.pin = BTN_YELLOW, .last_down_time = 0, .is_pressed = false, .long_press_timer = NULL},
};

// Struct to pass from ISR on button press
typedef struct
{
	uint32_t pin;
	bool is_long_press;
} button_event_t;

// Forward Declarations
static QueueHandle_t button_press_queue = NULL;
static void button_press_handler(void *arg);
static void button_isr(void *arg);
static void long_press_timer_callback(TimerHandle_t xTimer);

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

	// Create long-press timers for each button
	for (int i = 0; i < BTN_COUNT; i++)
	{
		button_states[i].long_press_timer = xTimerCreate(
			"long_press_timer",
			pdMS_TO_TICKS(LONG_PRESS_TIME),
			pdFALSE,   // One-shot
			(void *)i, // Timer ID = button index
			long_press_timer_callback);
		// Handle errors
		if (button_states[i].long_press_timer == NULL)
		{
			ESP_LOGE(TAG, "js_buttons_init: Failed to create long-press timer for button %d", i);
			ESP_GOTO_ON_ERROR(ESP_FAIL, error, TAG, "js_buttons_init: Failed to create long-press timer");
		}
	}

	// Init the button press handler
	button_press_queue = xQueueCreate(10, sizeof(button_event_t));
	ESP_GOTO_ON_ERROR(button_press_queue == NULL ? ESP_FAIL : ESP_OK, error, TAG, "js_buttons_init: Failed to create button press queue");
	xTaskCreate(button_press_handler, "button_press_handler", 2048, NULL, 10, NULL);

	// Install GPIO ISR service
	ESP_GOTO_ON_ERROR(gpio_install_isr_service(0), error, TAG, "js_buttons_init: Failed to install ISR service");
	for (int i = 0; i < BTN_COUNT; i++)
	{
		// Pass in the button_pins index
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
	button_state_t *state = &button_states[btn_idx];

	// Only send event if button is still pressed
	button_event_t event = {
		.pin = state->pin,
		.is_long_press = true};
	xQueueSend(button_press_queue, &event, 0);
	// ESP_LOGI(TAG, "Long-press timer fired for button index %lu", btn_idx);

	// Stop the timer and update the button state
	xTimerStop(state->long_press_timer, 0);
	state->is_pressed = false;
}

/** ISR for button presses */
static void IRAM_ATTR button_isr(void *arg)
{
	// Get the button state object from the index
	uint32_t btn_idx = (uint32_t)arg;
	button_state_t *state = &button_states[btn_idx];

	// Get the current time
	int64_t current_time = esp_timer_get_time() / 1000; // ms

	if (gpio_get_level(state->pin) == 0)
	{
		// If already pressed, ignore
		if (state->is_pressed == true)
			return;

		// Otherwise, record the press
		state->last_down_time = current_time;
		state->is_pressed = true;

		// Start the long-press timer
		xTimerStartFromISR(state->long_press_timer, NULL);
	}
	else
	{
		int64_t press_duration = current_time - state->last_down_time;

		// If less than the debounce time, ignore
		if (press_duration < DEBOUNCE_TIME)
			return;

		// If long press, ignore since this is handled by the timer
		if (press_duration >= LONG_PRESS_TIME)
			return;

		// Stop the long-press timer
		xTimerStopFromISR(state->long_press_timer, NULL);

		// Reset the button state
		state->is_pressed = false;

		// Send short press event only, long press is handled by the timer
		button_event_t event = {.pin = state->pin, .is_long_press = false};
		xQueueSendFromISR(button_press_queue, &event, NULL);
	}
}

/** Handle button press events from the queue */
static void button_press_handler(void *arg)
{
	button_event_t event;

	for (;;)
	{
		if (xQueueReceive(button_press_queue, &event, portMAX_DELAY))
		{
			// printf("GPIO[%" PRIu32 "]: %s\n", event.pin, event.is_long_press ? "LONG" : "SHORT");

			switch (event.pin)
			{
			case BTN_RED:
				ESP_LOGI(TAG, "RED button pressed: %s\n", event.is_long_press ? "LONG" : "SHORT");
				break;

			case BTN_BLUE:
				ESP_LOGI(TAG, "BLUE button pressed: %s\n", event.is_long_press ? "LONG" : "SHORT");
				break;

			case BTN_YELLOW:
				ESP_LOGI(TAG, "YELLOW button pressed: %s\n", event.is_long_press ? "LONG" : "SHORT");
				break;

			default:
				ESP_LOGW(TAG, "Unknown button pressed\n");
				break;
			}
		}
	}
}