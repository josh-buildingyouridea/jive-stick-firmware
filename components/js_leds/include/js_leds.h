#ifndef JS_LEDS_H
#define JS_LEDS_H

// Includes
#include "esp_err.h"

// Functions
esp_err_t js_leds_init(void);
void js_leds_set_color(uint8_t red, uint8_t green, uint8_t blue);
void js_leds_clear(void);

#endif // JS_LEDS_H
