idf_component_register(
    SRCS "js_time.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_timer js_i2c js_user_settings esp_event js_events
)