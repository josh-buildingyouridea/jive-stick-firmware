#pragma once
#include "esp_event.h"

// Define the event base for Jive Stick events
ESP_EVENT_DECLARE_BASE(JS_EVENT_BASE); // Main handler

// Define the events for the event handler
typedef enum
{
	// System Events
	JS_EVENT_GOTO_SLEEP,
	JS_SET_TIME,

	// Audio Events
	JS_EVENT_EMERGENCY_BUTTON_PRESSED,
	JS_EVENT_PLAY_AUDIO,
	JS_EVENT_STOP_AUDIO,
} app_event_id_t;
