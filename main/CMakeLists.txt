idf_component_register(SRCS "esp_homekit.c" "main.c" "wifi_manager.c" "http_server.c" "mqtt_xn.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi esp_http_server nvs_flash json spiffs mqtt driver 
                            esp_hap_core esp_hap_platform esp_hap_apple_profiles
                            hkdf-sha json_parser json_generator mu_srp
                    PRIV_INCLUDE_DIRS 
                        "${CMAKE_CURRENT_SOURCE_DIR}/../components/esp-homekit-sdk/include"
                        "${CMAKE_CURRENT_SOURCE_DIR}/../common/app_wifi"
                        "${CMAKE_CURRENT_SOURCE_DIR}/../common/app_hap_setup_payload"
)

# 设置编译选项
# 为当前组件库添加私有编译选项，禁用格式警告
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")