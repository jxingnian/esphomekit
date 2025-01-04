#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_wifi.h"
#include "esp_event.h"

// WiFi初始化函数
esp_err_t wifi_init_softap(void);

#endif // WIFI_MANAGER_H
