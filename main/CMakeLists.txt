idf_component_register(
    SRCS
        "hello_world_main.cpp"
        "SDcard.cpp"
        "TouchHandler.cpp"
        "Button.cpp"
        "TypoWrite.cpp"
        "VLWFontParser.cpp"
        "ScreenTransition.cpp"
    PRIV_REQUIRES
        M5GFX
    REQUIRES
        "fatfs"
        "esp_lcd"
        "driver"
        "esp_timer"
        "tinyusb"
        "esp_tinyusb"
        "esp_psram"
    INCLUDE_DIRS
        "."
        "fonts"
)


