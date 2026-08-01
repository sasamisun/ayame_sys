# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ayame_sys)

# TinyUSB（espressif/tinyusb, espressif/esp_tinyusb）は idf_component.yml 経由で
# managed_components/ に取得される。IDF v5.4 以降はコア側に components/tinyusb が
# 存在しないため、EXTRA_COMPONENT_DIRS の追加指定は不要。
