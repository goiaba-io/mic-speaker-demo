idf_component_register(SRCS "main.c" "mic.c" "audio.c" "filters.c" "media.c"
    INCLUDE_DIRS "includes"
    REQUIRES esp-libopus driver esp_timer esp_psram nvs_flash
)

# idf_component_register(SRCS "main.c" "mic.c" "speaker.c" "wifi.c" "audio.c" "webrtc.c"
#     INCLUDE_DIRS "." "includes"
#     REQUIRES libpeer esp-libopus driver libpeer esp_wifi nvs_flash
# )

idf_component_get_property(lib esp-libopus COMPONENT_LIB)
target_compile_options(${lib} PRIVATE -Wno-error=maybe-uninitialized)
target_compile_options(${lib} PRIVATE -Wno-error=stringop-overread)
