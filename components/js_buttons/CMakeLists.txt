idf_component_register(
    SRCS "js_buttons.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer esp_event js_events
)