# ESP32 MQTT 项目

By. 星年

## 项目介绍

这是一个基于 ESP32 的 MQTT 项目，旨在提供一个简单易用的 MQTT 客户端，用户可以通过该客户端连接到 MQTT 代理，进行消息的发布和订阅。该项目支持多种功能，包括设备状态监控和数据传输。

### 特性

- 支持 MQTT 连接和消息发布/订阅
- 实时设备状态更新
- 支持 WiFi 配置和管理
- 简洁的代码结构，易于扩展和维护

### 硬件要求

- 芯片：ESP32-S3
- Flash：8MB 或更大
- 支持 WiFi 功能

## 使用说明

### 1. 首次使用

1. 在 ESP32 启动后，配置 WiFi 连接。
2. 使用 MQTT 客户端连接到指定的 MQTT 代理。
3. 通过代码发布和订阅消息。

### 2. 修改 MQTT 配置

在 `mqtt_xn.c` 文件中配置 MQTT 代理地址和端口。

### 3. 查看设备状态

- 通过 MQTT 消息实时接收设备状态更新。
- 可以通过订阅特定主题来获取状态信息。

## 项目结构

- `/main` - 主要源代码
  - `mqtt_xn.c` - MQTT 客户端实现
  - `wifi_manager.c` - WiFi 管理功能
- `/CMakeLists.txt` - CMake 构建配置
- `/sdkconfig` - SDK 配置文件

---

## 注意事项

1. 确保 WiFi 配置正确。
2. 确保 MQTT 代理可访问。
3. 检查网络连接状态。

## 自定义配置

### 1. 修改分区表

项目使用了自定义的分区表，可以在 `partitions.csv` 中进行修改。

### 2. 移植到其他项目

1. 复制相关源文件到目标项目。
2. 确保目标项目包含必要的组件配置。
3. 在主程序中调用 MQTT 初始化和连接函数。
4. menuconfig开启mqtt5

## 开发环境

- ESP-IDF 版本：v5.0
- 芯片：ESP32-S3
- 编译器：GCC
- 开发工具：VS Code + ESP-IDF 插件

## 许可证

MIT License

## 链接

- GitHub 仓库：https://github.com/jxingnian/esp_mqtt
