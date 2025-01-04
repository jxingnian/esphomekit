/*
 * @Author: jxingnian j_xingnian@163.com
 * @Date: 2025-01-02 00:07:02
 * @Description: esp_homekit头文件
 */

#ifndef _ESP_HOMEKIT_H_
#define _ESP_HOMEKIT_H_

#include <esp_err.h>

void app_homeassistant_start();

/**
 * @brief 获取HomeKit配置URL
 * 
 * @param url_buffer 用于存储URL的缓冲区
 * @param buffer_size 缓冲区大小
 * @return esp_err_t ESP_OK: 成功, ESP_FAIL: 失败, ESP_ERR_INVALID_ARG: 参数无效
 */
esp_err_t esp_homekit_get_setup_url(char *url_buffer, size_t buffer_size);

#endif