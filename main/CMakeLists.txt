idf_component_register(SRCS "hello_world_main.cpp" "SDcard.cpp"
    PRIV_REQUIRES M5GFX
    REQUIRES "fatfs" "esp_lcd"
    INCLUDE_DIRS
    "."
)
