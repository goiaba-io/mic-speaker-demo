idf_component_register(SRCS "main.c" "mic.c" "audio.c"
    INCLUDE_DIRS "includes"
    REQUIRES driver esp_timer
)
