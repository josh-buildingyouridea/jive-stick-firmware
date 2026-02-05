idf_component_register(
    SRCS "js_serial_input.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_event js_events
)