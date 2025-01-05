# ESPHomeKit 项目

By. 星年

## 项目介绍

这是一个基于 ESP32 的 HomeKit 智能家居项目，集成了 MQTT 和 HTTP 服务器功能。项目支持 HomeKit 配件接入、MQTT 消息通信以及 Web 配网功能，为智能家居设备提供完整的控制和管理解决方案。

### 特性

- HomeKit 配件支持
- MQTT 消息发布/订阅
- Web 配网界面
- SPIFFS 文件系统支持
- WiFi AP/STA 模式切换
- 设备状态实时更新

### 硬件要求

- 芯片：ESP32-S3
- Flash：8MB 或更大
- RAM：至少 512KB
- 支持 WiFi 功能

## 使用说明

### 1. 首次使用

1. ESP32 首次启动后会进入 AP 模式
2. 连接到设备的 WiFi 热点
3. 通过 Web 界面配置设备的 WiFi 连接信息
4. 设备连接到指定 WiFi 后，可通过 HomeKit 或 MQTT 进行控制

### 2. 配置说明

主要配置项在 `sdkconfig` 文件中：

- WiFi 配置
- MQTT 服务器地址和端口
- HomeKit 配置
- HTTP 服务器端口

### 3. HomeKit 接入

1. 打开 iOS 家庭 App
2. 添加配件
3. 扫描设备二维码或输入配对码
4. 按照提示完成配对

## 项目结构

- `/main` - 主要源代码
  - `main.c` - 程序入口
  - `esp_homekit.c/h` - HomeKit 功能实现
  - `mqtt_xn.c/h` - MQTT 客户端实现
  - `wifi_manager.c/h` - WiFi 管理
  - `http_server.c/h` - Web 服务器
- `/components` - 组件目录
  - `esp-homekit-sdk` - HomeKit SDK
- `/spiffs` - Web 页面文件
- `/common` - 通用功能模块

## 开发环境

- ESP-IDF 版本：v5.0
- 编译器：GCC
- 开发工具：VS Code + ESP-IDF 插件

## 编译说明

1. 安装 ESP-IDF v5.0
2. 克隆项目代码
3. 运行以下命令：
   ```bash
   idf.py set-target esp32s3
   idf.py menuconfig    # 配置项目参数
   idf.py build        # 编译项目
   idf.py flash        # 烧录固件
   ```

## 注意事项

1. 确保正确配置 WiFi 和 MQTT 参数
2. HomeKit 配对码需要妥善保管
3. 首次使用需要通过 Web 配网
4. 确保 SPIFFS 分区大小足够存储 Web 文件

## 许可证

MIT License

## 技术支持

- 项目仓库：git@github.com:jxingnian/esphomekit.git
- 问题反馈：j_xingnian@163.com
