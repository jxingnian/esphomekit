/*
 * @Author: xingnina j_xingnian@163.com
 * @Date: 2025-01-04 13:19:29
 * @LastEditors: xingnina j_xingnian@163.com
 * @LastEditTime: 2025-01-04 18:07:23
 * @FilePath: \esphomekit\main\esp_homekit.c
 * @Description: esp_homekit主文件
 */
#include "esp_homekit.h"

#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_log.h>
#include <driver/gpio.h>

#include <esp_hap_core/hap.h>
#include <esp_hap_platform/hap_platform_os.h>
#include <esp_hap_apple_profiles/hap_apple_servs.h>
#include <esp_hap_apple_profiles/hap_apple_chars.h>

#include <app_wifi.h>
#include <app_hap_setup_payload.h>

static const char *TAG = "HAP outlet";

#define SMART_OUTLET_TASK_PRIORITY  1
#define SMART_OUTLET_TASK_STACKSIZE 4 * 1024
#define SMART_OUTLET_TASK_NAME      "hap_outlet"

#define OUTLET_IN_USE_GPIO GPIO_NUM_0

#define ESP_INTR_FLAG_DEFAULT 0

static QueueHandle_t s_esp_evt_queue = NULL;
/**
 * @brief the recover outlet in use gpio interrupt function
 */
static void IRAM_ATTR outlet_in_use_isr(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(s_esp_evt_queue, &gpio_num, NULL);
}

/**
 * Enable a GPIO Pin for Outlet in Use Detection
 */
static void outlet_in_use_key_init(uint32_t key_gpio_pin)
{
    gpio_config_t io_conf;
    /* Interrupt for both the edges  */
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    /* Bit mask of the pins */
    io_conf.pin_bit_mask = 1ULL << key_gpio_pin;
    /* Set as input mode */
    io_conf.mode = GPIO_MODE_INPUT;
    /* Enable internal pull-up */
    io_conf.pull_up_en = 1;
    /* Disable internal pull-down */
    io_conf.pull_down_en = 0;
    /* Set the GPIO configuration */
    gpio_config(&io_conf);

    /* Install gpio isr service */
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    /* Hook isr handler for specified gpio pin */
    gpio_isr_handler_add(key_gpio_pin, outlet_in_use_isr, (void*)key_gpio_pin);
}

/**
 * Initialize the Smart Outlet Hardware.Here, we just enebale the Outlet-In-Use detection.
 */
void smart_outlet_hardware_init(gpio_num_t gpio_num)
{
    s_esp_evt_queue = xQueueCreate(2, sizeof(uint32_t));
    if (s_esp_evt_queue != NULL) {
        outlet_in_use_key_init(gpio_num);
    }
}

/* Mandatory identify routine for the accessory.
 * In a real accessory, something like LED blink should be implemented
 * got visual identification
 */
static int outlet_identify(hap_acc_t *ha)
{
    ESP_LOGI(TAG, "Accessory identified");
    return HAP_SUCCESS;
}

/* A dummy callback for handling a write on the "On" characteristic of Outlet.
 * In an actual accessory, this should control the hardware
 */
static int outlet_write(hap_write_data_t write_data[], int count,
        void *serv_priv, void *write_priv)
{
    int i, ret = HAP_SUCCESS;
    hap_write_data_t *write;
    for (i = 0; i < count; i++) {
        write = &write_data[i];
        if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_ON)) {
            ESP_LOGI(TAG, "Received Write. Outlet %s", write->val.b ? "On" : "Off");
            /* TODO: Control Actual Hardware */
            hap_char_update_val(write->hc, &(write->val));
            *(write->status) = HAP_STATUS_SUCCESS;
        } else {
            *(write->status) = HAP_STATUS_RES_ABSENT;
        }
    }
    return ret;
}
/* Main application thread */
static void smart_outlet_thread_entry(void *p)
{
    hap_acc_t *accessory;
    hap_serv_t *service;

    /* 初始化 HAP 核心 */
    hap_init(HAP_TRANSPORT_WIFI);

    /* 初始化配件的必要参数，这些参数将作为必要服务内部添加 */
    hap_acc_cfg_t cfg = {
        .name = "Esp-Smart-Outlet",
        .manufacturer = "Espressif",
        .model = "EspSmartOutlet01",
        .serial_num = "001122334455",
        .fw_rev = "0.9.0",
        .hw_rev = NULL,
        .pv = "1.1.0",
        .identify_routine = outlet_identify,
        .cid = HAP_CID_OUTLET,
    };
    /* 创建配件对象 */
    accessory = hap_acc_create(&cfg);

    /* 添加虚拟产品数据 */
    uint8_t product_data[] = {'E','S','P','3','2','H','A','P'};
    hap_acc_add_product_data(accessory, product_data, sizeof(product_data));

    /* 添加 Wi-Fi 传输服务，这是 HAP Spec R16 所要求的 */
    hap_acc_add_wifi_transport_service(accessory, 0);

    /* 创建插座服务。包括"name"，因为这是用户可见的服务 */
    service = hap_serv_outlet_create(false, false);
    hap_serv_add_char(service, hap_char_name_create("My Smart Outlet"));

    /* 获取插座使用中特征的指针，我们需要监控其状态变化 */
    hap_char_t *outlet_in_use = hap_serv_get_char_by_uuid(service, HAP_CHAR_UUID_OUTLET_IN_USE);

    /* 为服务设置写入回调 */
    hap_serv_set_write_cb(service, outlet_write);

    /* 将插座服务添加到配件对象 */
    hap_acc_add_serv(accessory, service);

    /* 将配件添加到 HomeKit 数据库 */
    hap_add_accessory(accessory);

    /* 初始化特定设备的硬件。这启用了插座使用检测 */
    smart_outlet_hardware_init(OUTLET_IN_USE_GPIO);

    /* 对于生产配件，设置代码不应该被编程到设备中。
     * 相反，应该使用从设置代码派生的设置信息。
     * 使用 factory_nvs_gen 工具生成此数据，然后将其刷写到工厂 NVS 分区中。
     *
     * 默认情况下，设置 ID 和设置信息将从工厂 NVS Flash 分区读取，
     * 因此不需要在此处显式设置。
     *
     * 然而，出于测试目的，可以使用 hap_set_setup_code() 和 hap_set_setup_id() API 覆盖它，
     * 就像这里所做的那样。
     */
#ifdef CONFIG_EXAMPLE_USE_HARDCODED_SETUP_CODE
    /* 格式为 xxx-xx-xxx 的唯一设置代码。默认：111-22-333 */
    hap_set_setup_code(CONFIG_EXAMPLE_SETUP_CODE);
    /* 唯一的四字符设置 ID。默认：ES32 */
    hap_set_setup_id(CONFIG_EXAMPLE_SETUP_ID);
#ifdef CONFIG_APP_WIFI_USE_WAC_PROVISIONING
    app_hap_setup_payload(CONFIG_EXAMPLE_SETUP_CODE, CONFIG_EXAMPLE_SETUP_ID, true, cfg.cid);
#else
    app_hap_setup_payload(CONFIG_EXAMPLE_SETUP_CODE, CONFIG_EXAMPLE_SETUP_ID, false, cfg.cid);
#endif
#endif

    /* 启用硬件 MFi 认证（仅适用于 SDK 的 MFi 变体） */
    hap_enable_mfi_auth(HAP_MFI_AUTH_HW);

    /* 初始化 Wi-Fi */
    // app_wifi_init();

    /* 完成所有初始化后，启动 HAP 核心 */
    hap_start();
    /* 启动 Wi-Fi */
    // app_wifi_start(portMAX_DELAY);

    uint32_t io_num = OUTLET_IN_USE_GPIO;
    hap_val_t appliance_value = {
        .b = true,
    };
    /* 监听插座使用状态变化事件。其他读/写功能将由 HAP 核心处理。
     * 当插座使用 GPIO 变低时，表示插座未使用。
     * 当插座使用 GPIO 变高时，表示插座正在使用。
     * 应用程序可以根据其硬件定义自己的逻辑。
     */
    while (1) {
        if (xQueueReceive(s_esp_evt_queue, &io_num, portMAX_DELAY) == pdFALSE) {
            ESP_LOGI(TAG, "插座使用触发失败");
        } else {
            appliance_value.b = gpio_get_level(io_num);
            /* 如果检测到任何状态变化，更新插座使用特征值 */
            hap_char_update_val(outlet_in_use, &appliance_value);
            ESP_LOGI(TAG, "插座使用触发 [%d]", appliance_value.b);
        }
    }
}

#define QRCODE_BASE_URL     "https://espressif.github.io/esp-homekit-sdk/qrcode.html"

esp_err_t esp_homekit_get_setup_url(char *url_buffer, size_t buffer_size)
{
    if (url_buffer == NULL || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    char *setup_payload = NULL;
#ifdef CONFIG_EXAMPLE_USE_HARDCODED_SETUP_CODE
    setup_payload = esp_hap_get_setup_payload(CONFIG_EXAMPLE_SETUP_CODE, 
                                                  CONFIG_EXAMPLE_SETUP_ID, 
                                                  false, 
                                                  HAP_CID_OUTLET);
#endif

    if (setup_payload) {
        int len = snprintf(url_buffer, buffer_size, "%s?data=%s", QRCODE_BASE_URL, setup_payload);
        free(setup_payload);
        
        if (len < 0 || len >= buffer_size) {
            return ESP_FAIL;
        }
        return ESP_OK;
    }
    return ESP_FAIL;
}

void app_homeassistant_start()
{
    /* Create the application thread */
    xTaskCreate(smart_outlet_thread_entry, SMART_OUTLET_TASK_NAME, SMART_OUTLET_TASK_STACKSIZE,
                NULL, SMART_OUTLET_TASK_PRIORITY, NULL);
}