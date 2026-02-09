idf_component_register(
    SRCS "js_leds.c"
    INCLUDE_DIRS "include"
    REQUIRES led_strip driver esp_timer js_ble
)