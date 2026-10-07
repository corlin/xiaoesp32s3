# Seeed Studio XIAO ESP32-S3 Sense 综合测试与初始化固件设计文档

## 1. 项目概述与设计目标

本项目旨在为 **Seeed Studio XIAO ESP32-S3 Sense** 开发板构建一套基于乐鑫官方原生 **ESP-IDF v5.3 LTS** 的综合初始化、硬件调试与功能验证固件。
参考 [Seeed Studio 官方快速上手文档](https://wiki.seeedstudio.com/cn/xiao_esp32s3_getting_started/)，固件将全面初始化主控与扩展板上的全部关键外设，并提供零外部依赖的无线 Web 交互控制台与 USB 串口实时诊断能力。

---

## 2. 硬件规格与引脚定义

### 2.1 硬件核心参数
* **主控芯片**：ESP32-S3 (QFN56, Revision v0.2, 双核 Xtensa LX7 @ 240MHz)
* **片上存储**：8MB Flash (Quad SPI) + 8MB PSRAM (Octal SPI / OPI 模式，AP_3v3)
* **通信接口**：板载 USB-Serial/JTAG (`/dev/cu.usbmodem1101`)
* **无线射频**：2.4GHz Wi-Fi (802.11 b/g/n) + BLE 5.0（需连接 U.FL 外置天线）
* **供电与散热**：Type-C 5V 供电；OV3660 高负载图传建议贴附散热片。

### 2.2 引脚映射表（严格对照 Seeed 官方文档）

| 模块 | 功能引脚 | 对应 ESP32-S3 GPIO | 电气特性 / 说明 |
| :--- | :--- | :--- | :--- |
| **OV3660 摄像头数据线** | Y2 (D0) | GPIO 15 | DVP 8-bit 并口数据输入 |
| | Y3 (D1) | GPIO 17 | DVP 8-bit 并口数据输入 |
| | Y4 (D2) | GPIO 18 | DVP 8-bit 并口数据输入 |
| | Y5 (D3) | GPIO 16 | DVP 8-bit 并口数据输入 |
| | Y6 (D4) | GPIO 14 | DVP 8-bit 并口数据输入 |
| | Y7 (D5) | GPIO 12 | DVP 8-bit 并口数据输入 |
| | Y8 (D6) | GPIO 11 | DVP 8-bit 并口数据输入 |
| | Y9 (D7) | GPIO 48 | DVP 8-bit 并口数据输入 |
| **OV3660 时钟与控制** | XCLK | GPIO 10 | 摄像头主时钟输出 (20MHz) |
| | PCLK | GPIO 13 | 像素同步时钟输入 |
| | VSYNC | GPIO 38 | 场同步脉冲 |
| | HREF | GPIO 47 | 行同步/行有效参考 |
| | SIOD (SDA) | GPIO 40 | SCCB I2C 数据总线 (需上拉) |
| | SIOC (SCL) | GPIO 39 | SCCB I2C 时钟总线 (需上拉) |
| | PWDN / RESET | -1 | 板载硬件固定上拉/下拉，无需 GPIO 独立控制 |
| **MSM261D PDM 麦克风** | PDM CLK | GPIO 42 | 数字麦克风时钟（与 D11 复用） |
| | PDM DATA | GPIO 41 | 数字麦克风数据（与 D12 复用） |
| **MicroSD 卡槽 (SPI 模式)**| SCK | GPIO 7 | SPI 时钟 (D8) |
| | MISO | GPIO 8 | SPI 主入从出 (D9) |
| | MOSI | GPIO 9 | SPI 主出从入 (D10) |
| | CS | GPIO 21 | SPI 片选 (与用户 LED 复用) |
| **用户指示灯** | USER_LED | GPIO 21 | 橙色 LED，**低电平有效**（低亮高灭） |

---

## 3. 软件系统架构设计

### 3.1 FreeRTOS 任务调度划分
系统运行于 ESP32-S3 双核架构，分为通信核心与多媒体核心：
1. **Core 0（通信与服务核）**：
   * **Wi-Fi 驱动栈**：建立 SoftAP 热点（SSID: `XIAO_ESP32S3_Sense`, 密码: `seeedstudio`, 默认 IP: `192.168.4.1`）。
   * **HTTP Web Server**：基于 `esp_http_server` 处理客户端连接、网页资源服务、MJPEG 视频流传输与 RESTful API 响应。
2. **Core 1（多媒体与外设核）**：
   * **摄像头流控任务 (`camera_stream_task`)**：读取 OV3660 采集的 JPEG 帧（配置 PSRAM 双缓冲），按需推送至 HTTP 输出流。
   * **音频采样计算任务 (`audio_task`)**：利用 `driver/i2s_pdm.h` 以 16kHz / 16-bit 持续捕获麦克风 PCM 数据，计算音频实时 RMS 能量（0% ~ 100%）。
   * **状态健康监控任务 (`status_task`)**：周期性采集堆内存使用情况（内部 SRAM 与 8MB PSRAM）、CPU 负载与 SD 卡状态，并驱动 GPIO 21 指示灯进行规律心跳呼吸。

### 3.2 模块驱动实现细节
* **摄像头驱动 (`bsp_camera`)**：
  * 使用乐鑫官方 `espressif/esp32-camera` 组件。
  * 适配 OV3660（自动探测 PID `0x3660`）。
  * 默认视频流分辨率：SVGA (800x600) 或 HD (1280x720)，支持动态在前端下发切换。
  * 帧缓冲直接分配在 Octal PSRAM 中，保证平滑流畅、无内存碎片。
* **音频驱动 (`bsp_mic`)**：
  * 使用 ESP-IDF 5.x 现代 I2S 驱动模式 (`i2s_channel_init_pdm_rx_mode`)。
  * 采样配置：16000Hz、16-bit Mono，双 DMA 缓冲。
* **存储驱动 (`bsp_sdcard`)**：
  * 使用 SDSPI 驱动 (`esp_vfs_fat_sdspi_mount`)，挂载路径为 `/sdcard`。
  * 非阻塞容错探测：若未插卡，标记 `sd_mounted = false` 并记录状态，系统平稳运行。
* **LED 与片选复用协调 (`bsp_led`)**：
  * 默认为心跳呼吸指示灯。当 SD 卡发生文件读写时，CS 片选被硬件拉低，自然呈现读写状态指示。

### 3.3 Web 控制台与 API 端点设计
固件内置微型响应式前端（嵌入式压缩存储于 Flash）：
* `GET /`：Web 控制面板主页，包含实时视频画布、动态麦克风音量柱状图、硬件健康参数监控卡片。
* `GET /stream`：MJPEG 视频流端点（`multipart/x-mixed-replace`）。
* `GET /capture`：单张原画质 JPEG 图片抓拍下载。
* `GET /api/status`：返回系统状态 JSON 数据：
  ```json
  {
    "uptime_sec": 128,
    "cpu_freq_mhz": 240,
    "free_sram_bytes": 284300,
    "total_psram_bytes": 8388608,
    "free_psram_bytes": 6291456,
    "mic_level_percent": 34,
    "sd_mounted": true,
    "sd_total_mb": 30424,
    "sd_free_mb": 28100,
    "camera_model": "OV3660",
    "resolution": "800x600"
  }
  ```

---

## 4. 构建与工程配置规范

### 4.1 核心 Kconfig 配置 (`sdkconfig.defaults`)
```ini
CONFIG_IDF_TARGET="esp32s3"
CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y
CONFIG_ESPTOOLPY_FLASHMODE_QIO=y
CONFIG_ESPTOOLPY_FLASHFREQ_80M=y

# 启用 8MB Octal PSRAM
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_TYPE_ESPPSRAM64=y
CONFIG_SPIRAM_USE_MALLOC=y
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384

# 控制台通过 USB-Serial/JTAG 输出
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y

# 编译器优化与主频
CONFIG_ESP32S3_DEFAULT_CPU_FREQ_240=y
CONFIG_FREERTOS_HZ=1000
```

### 4.2 自定义分区表 (`partitions.csv`)
预留充裕的 App 分区以容纳 Wi-Fi、Camera 驱动与 Web 页面：
```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x380000,
app1,     app,  ota_1,   0x390000,0x380000,
storage,  data, spiffs,  0x710000,0xE0000,
```

---

## 5. 验收与验证流程

1. **串口自检验证**：
   * 打开 `/dev/cu.usbmodem1101`（波特率 115200）。
   * 确认开机自检日志成功输出，8MB PSRAM 挂载成功，OV3660 芯片成功识别。
2. **硬件 LED 指示验证**：
   * 观察开发板右侧橙色 LED 是否呈现均匀心跳闪烁。
3. **Wi-Fi SoftAP 热点测试**：
   * 手机或电脑扫描 Wi-Fi，搜索到 `XIAO_ESP32S3_Sense` 并使用密码 `seeedstudio` 连接成功。
4. **Web 监控交互验证**：
   * 浏览器访问 `http://192.168.4.1`。
   * 实时视频流流畅播放，色彩与曝光正常。
   * 敲击麦克风，界面音频柱状图产生实时跳动。
   * 检查 PSRAM 与 SD 卡信息准确无误。
