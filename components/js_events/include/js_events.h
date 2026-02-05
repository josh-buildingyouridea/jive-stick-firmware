#pragma once
#include "esp_event.h"

// Define the event base for Jive Stick events
ESP_EVENT_DECLARE_BASE(JS_EVENT_BASE); // Main handler

// Define the events for the event handler
typedef enum
{
	JS_EVENT_GOTO_SLEEP,
	JS_SET_TIME,
} app_event_id_t;
