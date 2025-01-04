# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# 添加组件搜索路径
set(EXTRA_COMPONENT_DIRS 
    ${CMAKE_CURRENT_LIST_DIR}/components/esp-homekit-sdk/components/homekit/esp_hap_core
    ${CMAKE_CURRENT_LIST_DIR}/components/esp-homekit-sdk/components/homekit/esp_hap_platform
    ${CMAKE_CURRENT_LIST_DIR}/components/esp-homekit-sdk/components/homekit/esp_hap_apple_profiles
    ${CMAKE_CURRENT_LIST_DIR}/components/esp-homekit-sdk/components/homekit/hkdf-sha
    ${CMAKE_CURRENT_LIST_DIR}/components/esp-homekit-sdk/components/homekit/json_parser
    ${CMAKE_CURRENT_LIST_DIR}/components/esp-homekit-sdk/components/homekit/json_generator
    ${CMAKE_CURRENT_LIST_DIR}/components/esp-homekit-sdk/components/homekit/mu_srp
    ${CMAKE_CURRENT_LIST_DIR}/common
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esphomekit)

# 添加SPIFFS文件系统支持
spiffs_create_partition_image(storage ./spiffs FLASH_IN_PROJECT)
