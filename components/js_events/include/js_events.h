#pragma once
#include "esp_event.h"

// Define the event base for Jive Stick events
ESP_EVENT_DECLARE_BASE(JS_EVENT_BASE); // Main handler

// Define the events for the event handler
typedef enum {
    // System Events
    JS_EVENT_GOTO_SLEEP,

    // Time Events
    JS_EVENT_READ_SYSTEM_TIME,
    JS_EVENT_WRITE_SYSTEM_TIME,
    JS_EVENT_SET_NEXT_ALARM,

    // User Settings Events
    JS_EVENT_READ_TIMEZONE,
    JS_EVENT_WRITE_TIMEZONE,
    JS_EVENT_READ_ALARMS,
    JS_EVENT_WRITE_ALARMS,

    // Battery Events
    JS_EVENT_SHOW_BATTERY_STATUS,
    JS_EVENT_HIDE_BATTERY_STATUS,
    JS_EVENT_READ_BATTERY,
    JS_EVENT_READ_CHARGER,

    // Audio Events
    JS_EVENT_EMERGENCY_BUTTON_PRESSED,
    JS_EVENT_PLAY_AUDIO,
    JS_EVENT_STOP_AUDIO,

    // BLE Events
    JS_EVENT_START_PAIRING,
    JS_EVENT_STOP_BLE,
    JS_EVENT_BLE_CONNECTED,
    JS_EVENT_BLE_DISCONNECTED,

} app_event_id_t;
