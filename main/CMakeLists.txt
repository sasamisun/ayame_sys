idf_component_register(
    SRCS
        "hello_world_main.cpp"
        "SDcard.cpp"
        "TouchHandler.cpp"
        "Button.cpp"
    PRIV_REQUIRES
        M5GFX
    REQUIRES
        "fatfs"
        "esp_lcd"
        "driver"
        "esp_timer"
        "tinyusb"
        "esp_tinyusb"
    INCLUDE_DIRS
        "."
)


