# XIAO ESP32-S3 Sense 固件工程

本项目是基于乐鑫原生 **ESP-IDF v5.3 LTS** 为 **Seeed Studio XIAO ESP32-S3 Sense** 开发的综合驱动与监控固件。

实现了 8MB Octal PSRAM、OV3660 摄像头图传、MSM261D 数字麦克风音频采集、MicroSD 卡 SPI 存储、板载用户 LED，以及 Wi-Fi SoftAP + 双端口 Web 实时监控仪表盘。

---

## 硬件规格与引脚映射

| 外设模块 | 核心型号 / 特性 | 引脚配置 | 驱动实现 |
| :--- | :--- | :--- | :--- |
| **主控芯片** | ESP32-S3 (240MHz 双核, Xtensa LX7) | 内置 | 原生 IDF v5.3 |
| **存储规格** | 8MB Quad Flash + 8MB Octal PSRAM (AP 64Mbit) | 内部八线接口 (80MHz) | `sdkconfig.defaults` |
| **摄像头** | OV3660 (PID `0x3660`), 8-bit DVP 并口, SVGA (800x600) | D0~D7: `15, 17, 18, 16, 14, 12, 11, 48`<br>XCLK: `10` (20MHz), PCLK: `13`<br>VSYNC: `38`, HREF: `47`<br>SDA: `40`, SCL: `39` | `main/bsp_camera.c`<br>(esp32-camera 组件) |
| **数字麦克风** | MSM261D3526H1CPM, PDM 接口, 16kHz 16-bit Mono | CLK: `GPIO 42`<br>DATA: `GPIO 41` | `main/bsp_mic.c`<br>(I2S PDM RX 驱动) |
| **MicroSD 卡** | 标准 SPI 模式, FATFS 挂载到 `/sdcard` | SCK: `GPIO 7`<br>MISO: `GPIO 8`<br>MOSI: `GPIO 9`<br>CS: `GPIO 21` | `main/bsp_sdcard.c`<br>(esp_vfs_fat_sdspi) |
| **用户指示灯** | 橙色 LED (低电平点亮, 1Hz 心跳闪烁) | `GPIO 21` (与 SD 卡 CS 复用) | `main/bsp_led.c` |

> **引脚复用安全保护**：GPIO 21 同时连接 LED 与 SD 卡 CS 引脚。固件中心跳任务通过 `bsp_sdcard_is_mounted()` 进行动态保护，在挂载 SD 卡时自动暂停 LED 翻转，避免干扰 SPI 总线。

---

## 系统特性与架构设计

1. **双端口 HTTP 解耦架构**：
   - **Port 80 (控制台与 API)**：托管单页 HTML5 监控面板，提供 `/api/status`（每 500ms 刷新运行时间、内存余量、声音分贝条与 SD 卡状态）。
   - **Port 81 (独立视频流)**：独立 FreeRTOS 任务运行 MJPEG 推流服务（`/stream`），彻底避免单线程 HTTP 服务被死循环推流阻塞导致的 API 饥饿。
2. **多核多任务编排**：
   - 麦克风音频采集任务绑定在 **Core 1**，独立以 16kHz 采样率采集并计算 RMS 能量。
   - 摄像头采用 PSRAM 双帧缓冲（`fb_count = 2`），兼顾帧率与稳定性。
3. **高容错与性能优化**：
   - MicroSD 卡初始化具备错误容忍机制，未插卡或挂载失败时不中断系统主流程。
   - SD 卡剩余容量查询增加 5 秒轻量缓存，防止前端高频轮询引起频繁 FAT 表遍历。

---

## 快速上手与体验

固件烧录后将自动启动 Wi-Fi 热点：

1. **连接热点**：
   - **SSID**：`XIAO_ESP32S3_Sense`
   - **密码**：`seeedstudio`
2. **访问监控面板**：
   - **仪表盘首页**：打开浏览器访问 `http://192.168.4.1`
   - **独立视频流**：访问 `http://192.168.4.1:81/stream`
   - **系统状态 JSON**：访问 `http://192.168.4.1/api/status`

---

## 开发与构建指南

### 1. 环境准备

确保已安装 ESP-IDF v5.3 LTS 与工具链：

```bash
# 激活 ESP-IDF 环境变量
. ~/esp/esp-idf/export.sh

# 检查编译器版本
xtensa-esp32s3-elf-gcc --version
```

### 2. 编译项目

```bash
idf.py build
```

编译产物位于 `build/xiao_esp32s3_firmware.bin`。

### 3. 烧录与串口监视

开发板通过 Type-C 数据线连接至电脑（默认端口通常为 `/dev/cu.usbmodem1101` 或 `/dev/ttyACM0`）：

```bash
# 烧录固件
idf.py -p /dev/cu.usbmodem1101 flash

# 开启串口监视器 (退出快捷键: Ctrl + ])
idf.py -p /dev/cu.usbmodem1101 monitor
```

---

## 目录结构

```text
├── CMakeLists.txt              # 顶层 CMake 构建配置
├── sdkconfig.defaults          # 8MB Flash/8MB Octal PSRAM/USB-Serial 默认配置
├── partitions.csv              # 双 3.5MB app 分区表 (支持 OTA)
├── main/
│   ├── CMakeLists.txt          # 主组件源文件与依赖注册
│   ├── idf_component.yml       # espressif/esp32-camera 组件声明
│   ├── main.c                  # 系统主入口与多外设编排
│   ├── bsp_led.c / .h          # 用户指示灯驱动
│   ├── bsp_camera.c / .h       # OV3660 摄像头采集与翻转配置
│   ├── bsp_mic.c / .h          # MSM261D PDM 麦克风采集与 RMS 计算
│   ├── bsp_sdcard.c / .h       # MicroSD 卡 SPI 挂载与容量查询
│   └── web_server.c / .h       # SoftAP 热点与双端口 HTTP 服务
└── docs/                       # 设计与技术文档
```
