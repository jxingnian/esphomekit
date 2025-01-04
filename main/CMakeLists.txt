idf_component_register(SRCS "main.c" "wifi_manager.c" "http_server.c" "mqtt_xn.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi esp_http_server nvs_flash json spiffs mqtt)

# 设置编译选项
# 为当前组件库添加私有编译选项，禁用格式警告
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")