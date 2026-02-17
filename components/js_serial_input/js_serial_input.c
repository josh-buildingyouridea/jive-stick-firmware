/**
 * TODO: Implement handling of the serial input commands
 * TODO: Echo after each character not on enter
 */

// Self Include
#include "js_serial_input.h"

// Library Includes
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
// For calling events
#include "esp_event.h"
#include "js_events.h"

// Defines
#define TAG "js_serial_input"

// Forward Declarations
static void serial_input_handler(void *arg);

// Initialize the serial input handler
esp_err_t js_serial_input_init(void) {
    ESP_LOGI(TAG, "js_serial_input_init");
    // setvbuf(stdout, NULL, _IONBF, 0); // unbuffered stdout (for echo typing)
    xTaskCreate(serial_input_handler, "serial", 4096, NULL, 5, NULL);
    return ESP_OK;
}

// Monitor serial input and trigger action on return
static void serial_input_handler(void *arg) {
    char line[128];
    int idx = 0;

    ESP_LOGI(TAG, "Serial task started. Type commands and press Enter.");

    while (1) {
        int c = getchar(); // returns -1 if no data

        // If no data is available, yield CPU
        if (c < 0) {
            vTaskDelay(pdMS_TO_TICKS(100)); // yield CPU
            continue;
        }

        // Check for newline or full buffer
        if (c == '\r' || c == '\n' || idx >= (int)sizeof(line) - 1) {
            // Ignore if no characters have been read
            if (idx == 0)
                continue;

            // Print out the received line
            line[idx] = 0; // null-terminate the string
            idx = 0;       // reset index

            // --------------- Handle the input ----------------
            ESP_LOGI(TAG, "Received: %s", line);

            switch (line[0]) {
            // ********************* Time Events *********************
            case 't': // Read system time
                ESP_LOGI(TAG, "Read Time command received");
                esp_event_post(JS_EVENT_BASE, JS_EVENT_READ_SYSTEM_TIME, NULL, 0, 0);
                break;

            case 'T': // Write system time
                ESP_LOGI(TAG, "Write Time command received");
                // Strip out the first two character (T:) before posting the event
                esp_event_post(JS_EVENT_BASE, JS_EVENT_WRITE_SYSTEM_TIME, line + 2, strlen(line + 2) + 1, 0);
                break;

            case 'n': // Set the next alarm (for testing)
                ESP_LOGI(TAG, "JS_EVENT_SET_NEXT_ALARM");
                esp_event_post(JS_EVENT_BASE, JS_EVENT_SET_NEXT_ALARM, NULL, 0, 0);
                break;

            // ***************** User Settings Events ****************
            case 'l': // Read Location/Timezone
                ESP_LOGI(TAG, "Read timezone command received");
                esp_event_post(JS_EVENT_BASE, JS_EVENT_READ_TIMEZONE, NULL, 0, 0);
                break;

            case 'L': // Write Location/Timezone
                ESP_LOGI(TAG, "Write timezone command received");
                // Strip out the first two character (L:) before posting the event with a null-terminated string
                esp_event_post(JS_EVENT_BASE, JS_EVENT_WRITE_TIMEZONE, line + 2, strlen(line + 2) + 1, 0);
                break;

            case 'a': // Read Alarms
                ESP_LOGI(TAG, "Read Alarms command received");
                esp_event_post(JS_EVENT_BASE, JS_EVENT_READ_ALARMS, NULL, 0, 0);
                break;

            case 'A': // Write Alarms
                ESP_LOGI(TAG, "Alarms command received");
                // Strip out the first two character (A:) before posting the event with a null-terminated string
                esp_event_post(JS_EVENT_BASE, JS_EVENT_WRITE_ALARMS, line + 2, strlen(line + 2) + 1, 0);
                break;

            // ******************** Audio Events ********************
            case 'P': // Play audio (P:[idx])
                ESP_LOGI(TAG, "Play Audio command received");
                // Check for an index after the P: (e.g. P:1)
                if (strlen(line) > 2 && line[1] == ':') {
                    // Pass the index of the audio file to play as an integer (e.g. "1" -> 1)
                    uint8_t audio_index = atoi(line + 2);
                    esp_event_post(JS_EVENT_BASE, JS_EVENT_PLAY_AUDIO, &audio_index, sizeof(audio_index), 0);
                } else {
                    ESP_LOGW(TAG, "Invalid Play Audio command format. Use P:[index]");
                }
                break;

            case 'e':
                ESP_LOGI(TAG, "Emergency Button Pressed command received");
                esp_event_post(JS_EVENT_BASE, JS_EVENT_EMERGENCY_BUTTON_PRESSED, NULL, 0, 0);
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
