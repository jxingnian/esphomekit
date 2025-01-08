/*
 * @Author: jxingnian j_xingnian@163.com
 * @Date: 2025-01-02 00:07:02
 * @Description: HTTP服务器实现
 */

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_spiffs.h>
#include <esp_system.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include "http_server.h"
#include <sys/stat.h>
#include "nvs_flash.h"
#include "lwip/ip4_addr.h"
#include "esp_homekit.h"

static const char *TAG = "http_server";
static httpd_handle_t server = NULL;

// 处理根路径请求 - 返回index.html
static esp_err_t root_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    FILE *fd = NULL;
    struct stat file_stat;
    char *chunk = NULL;
    esp_err_t ret = ESP_OK;
    
    // 构建完整的文件路径
    strlcpy(filepath, "/spiffs/index.html", sizeof(filepath));
    
    // 获取文件信息
    if (stat(filepath, &file_stat) == -1) {
        ESP_LOGE(TAG, "Failed to stat file : %s", filepath);
        ret = httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read file");
        goto exit;
    }
    
    fd = fopen(filepath, "r");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to read file : %s", filepath);
        ret = httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read file");
        goto exit;
    }
    
    // 设置Content-Type和其他响应头
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Connection", "close");
    
    // 分配内存
    chunk = malloc(CHUNK_SIZE);
    if (chunk == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for chunk");
        ret = httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to allocate memory");
        goto exit;
    }
    
    // 发送文件内容
    size_t chunksize;
    do {
        chunksize = fread(chunk, 1, CHUNK_SIZE, fd);
        if (chunksize > 0) {
            if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
                ESP_LOGE(TAG, "File sending failed!");
                ret = httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                goto exit;
            }
        }
    } while (chunksize != 0);
    
    // 发送结束标记
    ret = httpd_resp_send_chunk(req, NULL, 0);

exit:
    if (fd) {
        fclose(fd);
    }
    if (chunk) {
        free(chunk);
    }
    return ret;
}

// 处理WiFi扫描请求
static esp_err_t scan_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "收到WiFi扫描请求: %s", req->uri);
    
    // // 检查WiFi状态
    // wifi_mode_t mode;
    // esp_wifi_get_mode(&mode);
    // if (mode & WIFI_MODE_STA) {
    //     // 检查是否正在连接
    //     wifi_ap_record_t ap_info;
    //     if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
    //         esp_wifi_disconnect();
    //         vTaskDelay(pdMS_TO_TICKS(500)); // 等待断开完成
    //     }
    // }
    
    ESP_LOGI(TAG, "清除之前的扫描结果");
    esp_wifi_scan_stop();  // 停止可能正在进行的扫描
    vTaskDelay(pdMS_TO_TICKS(100)); // 等待扫描停止
    
    ESP_LOGI(TAG, "开始WiFi扫描...");
    // 配置扫描参数
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_PASSIVE,
        .scan_time = {
            .passive = 500  // 被动扫描时间设置为500ms
        }
    };
    
    // 开始扫描
    esp_err_t err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WiFi扫描失败: %s", esp_err_to_name(err));
        char err_msg[128];
        snprintf(err_msg, sizeof(err_msg), "{\"status\":\"error\",\"message\":\"Scan failed: %s\"}", esp_err_to_name(err));
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, err_msg, strlen(err_msg));
        return ESP_OK;
    }

    // 获取扫描结果
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    
    if (ap_count == 0) {
        const char *response = "{\"status\":\"success\",\"networks\":[]}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response, strlen(response));
        return ESP_OK;
    }

    wifi_ap_record_t *ap_records = malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (ap_records == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        const char *response = "{\"status\":\"error\",\"message\":\"Memory allocation failed\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response, strlen(response));
        return ESP_OK;
    }

    esp_wifi_scan_get_ap_records(&ap_count, ap_records);
    ESP_LOGI(TAG, "找到 %d 个WiFi网络", ap_count);

    // 创建JSON响应
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "success");
    cJSON *networks = cJSON_AddArrayToObject(root, "networks");

    for (int i = 0; i < ap_count; i++) {
        cJSON *ap = cJSON_CreateObject();
        cJSON_AddStringToObject(ap, "ssid", (char *)ap_records[i].ssid);
        cJSON_AddNumberToObject(ap, "rssi", ap_records[i].rssi);
        cJSON_AddNumberToObject(ap, "authmode", ap_records[i].authmode);
        cJSON_AddItemToArray(networks, ap);
    }

    char *response = cJSON_PrintUnformatted(root);
    ESP_LOGI(TAG, "WiFi扫描完成，发送响应");
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, response);

    free(response);
    free(ap_records);
    cJSON_Delete(root);
    return ESP_OK;
}

// 处理配网请求
static esp_err_t configure_post_handler(httpd_req_t *req)
{
    char buf[200];
    int ret, remaining = req->content_len;
    
    if (remaining > sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too long");
        return ESP_FAIL;
    }
    
    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive data");
        return ESP_FAIL;
    }
    
    buf[ret] = '\0';
    
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to parse JSON");
        return ESP_FAIL;
    }
    
    cJSON *ssid = cJSON_GetObjectItem(root, "ssid");
    cJSON *password = cJSON_GetObjectItem(root, "password");
    
    if (!ssid || !cJSON_IsString(ssid)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing SSID");
        return ESP_FAIL;
    }
    
    // 配置WiFi连接
    wifi_config_t wifi_config = {0};
    strlcpy((char *)wifi_config.sta.ssid, ssid->valuestring, sizeof(wifi_config.sta.ssid));
    if (password && cJSON_IsString(password)) {
        strlcpy((char *)wifi_config.sta.password, password->valuestring, sizeof(wifi_config.sta.password));
    }
    
    // 保存WiFi配置到NVS
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("wifi_config", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        err = nvs_set_blob(nvs_handle, "sta_config", &wifi_config, sizeof(wifi_config_t));
        if (err == ESP_OK) {
            err = nvs_commit(nvs_handle);
        }
        nvs_close(nvs_handle);
    }
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "保存WiFi配置失败: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "WiFi配置已保存到NVS");
    }
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_connect());
    
    cJSON_Delete(root);
    
    const char *response = "{\"status\":\"success\",\"message\":\"WiFi配置已提交，正在连接...\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    
    return ESP_OK;
}

// 获取WiFi连接状态
static esp_err_t wifi_status_get_handler(httpd_req_t *req)
{
    char *buffer = NULL;
    size_t buffer_len = 0;
    static char last_ip[16] = {0};  // 用于存储上次的IP地址
    char current_ip[16] = {0};
    
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        esp_netif_ip_info_t ip_info;
        esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info);
        
        snprintf(current_ip, sizeof(current_ip), IPSTR, IP2STR(&ip_info.ip));
        
        // 只有当IP地址发生变化时才输出日志
        if (strcmp(current_ip, last_ip) != 0) {
            ESP_LOGI(TAG, "当前IP地址: %s", current_ip);
            strncpy(last_ip, current_ip, sizeof(last_ip));
        }

        cJSON_AddStringToObject(root, "status", "connected");
        cJSON_AddStringToObject(root, "ssid", (char *)ap_info.ssid);
        cJSON_AddStringToObject(root, "ip", current_ip);
        cJSON_AddNumberToObject(root, "rssi", ap_info.rssi);
        char bssid_str[18];
        snprintf(bssid_str, sizeof(bssid_str), "%02x:%02x:%02x:%02x:%02x:%02x",
                ap_info.bssid[0], ap_info.bssid[1], ap_info.bssid[2],
                ap_info.bssid[3], ap_info.bssid[4], ap_info.bssid[5]);
        cJSON_AddStringToObject(root, "bssid", bssid_str);
    } else {
        cJSON_AddStringToObject(root, "status", "disconnected");
    }

    buffer = cJSON_PrintUnformatted(root);
    buffer_len = strlen(buffer);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    esp_err_t ret = httpd_resp_send(req, buffer, buffer_len);
    free(buffer);
    
    return ret;
}

// 获取已保存的WiFi列表
static esp_err_t saved_wifi_get_handler(httpd_req_t *req)
{
    wifi_config_t wifi_config;
    cJSON *root = cJSON_CreateArray();
    char *response = NULL;

    esp_err_t err = esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_config);
    if (err == ESP_OK && strlen((char*)wifi_config.sta.ssid) > 0) {
        cJSON *wifi = cJSON_CreateObject();
        cJSON_AddStringToObject(wifi, "ssid", (char*)wifi_config.sta.ssid);
        cJSON_AddItemToArray(root, wifi);
    }

    response = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, response);

    free(response);
    cJSON_Delete(root);
    return ESP_OK;
}

// 删除保存的WiFi
static esp_err_t delete_wifi_post_handler(httpd_req_t *req)
{
    char buf[100];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *ssid = cJSON_GetObjectItem(root, "ssid");
    if (!ssid || !cJSON_IsString(ssid)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing or invalid SSID");
        return ESP_FAIL;
    }

    wifi_config_t wifi_config;
    if (esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_config) == ESP_OK) {
        if (strcmp((char*)wifi_config.sta.ssid, ssid->valuestring) == 0) {
            // 先断开WiFi连接
            esp_wifi_disconnect();
            vTaskDelay(pdMS_TO_TICKS(1000));  // 等待断开连接
            
            // 清除运行时的WiFi配置
            memset(&wifi_config, 0, sizeof(wifi_config_t));
            esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
            
            // 清除自定义NVS中的WiFi配置
            nvs_handle_t nvs_handle;
            esp_err_t err = nvs_open("wifi_config", NVS_READWRITE, &nvs_handle);
            if (err == ESP_OK) {
                err = nvs_erase_all(nvs_handle);
                if (err == ESP_OK) {
                    err = nvs_commit(nvs_handle);
                    ESP_LOGI(TAG, "已清除自定义NVS中的WiFi配置");
                }
                nvs_close(nvs_handle);
            }
            
            // 清除连接失败计数
            err = nvs_open("wifi_state", NVS_READWRITE, &nvs_handle);
            if (err == ESP_OK) {
                nvs_set_u8(nvs_handle, "connection_failed", 0);
                nvs_commit(nvs_handle);
                nvs_close(nvs_handle);
            }
            
            // 停止并重启WiFi以确保配置被完全清除
            esp_wifi_stop();
            vTaskDelay(pdMS_TO_TICKS(500));
            esp_wifi_start();
            
            ESP_LOGI(TAG, "WiFi配置已完全删除");
        }
    }

    cJSON_Delete(root);
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

// 获取HomeKit设置URL
static esp_err_t homekit_url_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "收到HomeKit URL请求: %s", req->uri);
    
    char url[128] = {0};
    esp_err_t ret = esp_homekit_get_setup_url(url, sizeof(url));
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "获取HomeKit setup URL失败，错误码: %d", ret);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to get HomeKit URL");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "获取到HomeKit URL: %s", url);
    
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "创建JSON对象失败");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create response");
        return ESP_FAIL;
    }
    
    cJSON_AddStringToObject(root, "url", url);
    
    char *json_str = cJSON_Print(root);
    if (json_str == NULL) {
        ESP_LOGE(TAG, "生成JSON字符串失败");
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create response");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "返回JSON响应: %s", json_str);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    
    free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

// 处理恢复出厂设置请求
static esp_err_t factory_reset_post_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "收到恢复出厂设置请求");
    
    // 擦除NVS分区
    esp_err_t ret = nvs_flash_erase();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "擦除NVS失败: %s", esp_err_to_name(ret));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to reset");
        return ESP_FAIL;
    }
    
    // 重新初始化NVS
    ret = nvs_flash_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "重新初始化NVS失败: %s", esp_err_to_name(ret));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to reinit NVS");
        return ESP_FAIL;
    }

    // 返回成功响应
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    
    // 延迟2秒后重启设备
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    
    return ESP_OK;
}

// URI处理结构
static const httpd_uri_t root = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t scan = {
    .uri       = "/scan",
    .method    = HTTP_GET,
    .handler   = scan_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t configure = {
    .uri       = "/configure",
    .method    = HTTP_POST,
    .handler   = configure_post_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t wifi_status = {
    .uri       = "/status",
    .method    = HTTP_GET,
    .handler   = wifi_status_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t saved_wifi = {
    .uri       = "/saved",
    .method    = HTTP_GET,
    .handler   = saved_wifi_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t delete_wifi = {
    .uri       = "/delete",
    .method    = HTTP_POST,
    .handler   = delete_wifi_post_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t homekit_url = {
    .uri       = "/homekit-url",
    .method    = HTTP_GET,
    .handler   = homekit_url_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t factory_reset = {
    .uri       = "/factory-reset",
    .method    = HTTP_POST,
    .handler   = factory_reset_post_handler,
    .user_ctx  = NULL
};

// 启动Web服务器
esp_err_t start_webserver(void)
{
    // 初始化NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.max_uri_handlers = 10;
    config.server_port = 8080;
    config.max_open_sockets = 7;  // 增加最大连接数
    config.backlog_conn = 5;      // 设置等待连接队列大小
    config.recv_wait_timeout = 3;  // 减少接收超时
    config.send_wait_timeout = 3;  // 减少发送超时
    config.core_id = 0;           // 固定在核心0上运行
    config.stack_size = 8192;     // 增加堆栈大小
    
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &scan);
        httpd_register_uri_handler(server, &configure);
        httpd_register_uri_handler(server, &wifi_status);
        httpd_register_uri_handler(server, &saved_wifi);
        httpd_register_uri_handler(server, &delete_wifi);
        httpd_register_uri_handler(server, &homekit_url);
        httpd_register_uri_handler(server, &factory_reset);  // 添加恢复出厂设置处理程序
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Error starting server!");
    return ESP_FAIL;
}

// 停止Web服务器
esp_err_t stop_webserver(void)
{
    if (server) {
        httpd_stop(server);
        server = NULL;
    }
    return ESP_OK;
}