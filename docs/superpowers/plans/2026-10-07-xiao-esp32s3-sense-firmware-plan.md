# XIAO ESP32-S3 Sense 综合测试固件落地实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 基于乐鑫原生 ESP-IDF v5.3 LTS，为 Seeed Studio XIAO ESP32-S3 Sense 开发板构建一套完整的硬件初始化、外设驱动及 SoftAP Web 实时音视频监控控制台测试固件。

**Architecture:** 采用分层 BSP（板级支持包）架构，将板载 LED、OV3660 摄像头、MSM261D 数字麦克风和 MicroSD 卡驱动解耦封装；结合 FreeRTOS 双核特性，Core 0 调度 Wi-Fi SoftAP 与 HTTP 服务，Core 1 调度多媒体流采集与音频能量计算；前端通过嵌入式轻量 HTML5 仪表盘实现零依赖访问。

**Tech Stack:** ESP-IDF v5.3 LTS, FreeRTOS, esp32-camera component (OV3660), I2S PDM RX Driver, SDSPI VFS FATFS, esp_http_server, Wi-Fi SoftAP.

## Global Constraints

- 硬件平台：Seeed Studio XIAO ESP32-S3 Sense (ESP32-S3 v0.2, 8MB Quad Flash, 8MB Octal PSRAM)
- 串口设备：`/dev/cu.usbmodem1101` (USB-Serial/JTAG 默认控制台，波特率 115200)
- 摄像头型号：OV3660 (PID `0x3660`, 8-bit DVP 并口, XCLK 20MHz, PSRAM 双缓冲)
- 麦克风型号：MSM261D PDM (CLK=GPIO 42, DATA=GPIO 41, 16kHz / 16-bit)
- SD 卡接口：SPI 模式 (SCK=GPIO 7, MISO=GPIO 8, MOSI=GPIO 9, CS=GPIO 21)
- 指示灯：GPIO 21 (低电平点亮，与 SD CS 复用，空闲时呼吸心跳)
- Wi-Fi 凭据：SSID: `XIAO_ESP32S3_Sense`, 密码: `seeedstudio`, 默认 IP: `192.168.4.1`
- IDF 安装路径：`~/esp/esp-idf`，工具链路径：`~/.espressif`

---

### Task 1: 准备与安装 ESP-IDF v5.3 工具链环境

**Files:**
- Create: `~/esp/esp-idf` (Git Clone)
- Modify: `~/.espressif` (Tools installation)

**Interfaces:**
- Produces: `idf.py` CLI 工具与 xtensa-esp32s3-elf-gcc 交叉编译环境

- [ ] **Step 1: 检查并创建 `~/esp` 目录并浅克隆 ESP-IDF release/v5.3**

```bash
mkdir -p ~/esp
cd ~/esp
if [ ! -d "esp-idf" ]; then
  git clone -b release/v5.3 --depth 1 https://github.com/espressif/esp-idf.git
fi
```

- [ ] **Step 2: 执行 `install.sh esp32s3` 安装编译器与依赖工具**

```bash
cd ~/esp/esp-idf
./install.sh esp32s3
```

- [ ] **Step 3: 验证 ESP-IDF 导出脚本与 `idf.py` 可执行性**

```bash
. ~/esp/esp-idf/export.sh
idf.py --version
```
Expected: 输出 `ESP-IDF v5.3...`

- [ ] **Step 4: 提交环境部署记录**

```bash
git commit --allow-empty -m "chore: verify ESP-IDF v5.3 installation environment"
```

---

### Task 2: 搭建项目基础工程结构与 Kconfig 核心配置

**Files:**
- Create: `CMakeLists.txt`
- Create: `sdkconfig.defaults`
- Create: `partitions.csv`
- Create: `main/CMakeLists.txt`
- Create: `main/idf_component.yml`

**Interfaces:**
- Produces: 具备 8MB Flash、8MB Octal PSRAM 和 esp32-camera 组件依赖的空工程骨架

- [ ] **Step 1: 创建根目录 `CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(xiao_esp32s3_firmware)
```

- [ ] **Step 2: 创建 `sdkconfig.defaults` 配置 Flash、PSRAM 与 USB 控制台**

```ini
CONFIG_IDF_TARGET="esp32s3"
CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y
CONFIG_ESPTOOLPY_FLASHMODE_QIO=y
CONFIG_ESPTOOLPY_FLASHFREQ_80M=y

# 8MB Octal PSRAM 配置
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_TYPE_ESPPSRAM64=y
CONFIG_SPIRAM_USE_MALLOC=y
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384

# USB-Serial-JTAG 控制台
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y

# CPU 主频与系统时钟
CONFIG_ESP32S3_DEFAULT_CPU_FREQ_240=y
CONFIG_FREERTOS_HZ=1000

# 分区表自定义
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_PARTITION_TABLE_FILENAME="partitions.csv"
```

- [ ] **Step 3: 创建 `partitions.csv` 自定义分区表**

```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x380000,
app1,     app,  ota_1,   0x390000,0x380000,
storage,  data, spiffs,  0x710000,0xE0000,
```

- [ ] **Step 4: 创建 `main/idf_component.yml` 添加 `esp32-camera` 组件依赖**

```yaml
dependencies:
  espressif/esp32-camera: "^2.0.9"
```

- [ ] **Step 5: 提交工程基础配置**

```bash
git add CMakeLists.txt sdkconfig.defaults partitions.csv main/idf_component.yml
git commit -m "chore: setup project build structure and sdkconfig defaults"
```

---

### Task 3: 实现板级用户指示灯模块 (`bsp_led`)

**Files:**
- Create: `main/bsp_led.h`
- Create: `main/bsp_led.c`

**Interfaces:**
- Produces:
  - `esp_err_t bsp_led_init(void)`
  - `void bsp_led_set(bool on)`
  - `void bsp_led_toggle(void)`

- [ ] **Step 1: 编写 `main/bsp_led.h` 接口头文件**

```c
#pragma once
#include <stdbool.h>
#include "esp_err.h"

#define BSP_LED_GPIO 21

esp_err_t bsp_led_init(void);
void bsp_led_set(bool on);
void bsp_led_toggle(void);
```

- [ ] **Step 2: 编写 `main/bsp_led.c` 实现（低电平点亮）**

```c
#include "bsp_led.h"
#include "driver/gpio.h"

static bool s_led_state = false;

esp_err_t bsp_led_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BSP_LED_GPIO),
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&io_conf);
    if (ret == ESP_OK) {
        bsp_led_set(false); // 默认熄灭 (GPIO21置高)
    }
    return ret;
}

void bsp_led_set(bool on) {
    s_led_state = on;
    // 低电平点亮，高电平熄灭
    gpio_set_level(BSP_LED_GPIO, on ? 0 : 1);
}

void bsp_led_toggle(void) {
    bsp_led_set(!s_led_state);
}
```

- [ ] **Step 3: 提交 LED 模块**

```bash
git add main/bsp_led.h main/bsp_led.c
git commit -m "feat(bsp): implement user led driver on gpio 21"
```

---

### Task 4: 实现 MicroSD 卡 SPI 挂载模块 (`bsp_sdcard`)

**Files:**
- Create: `main/bsp_sdcard.h`
- Create: `main/bsp_sdcard.c`

**Interfaces:**
- Produces:
  - `esp_err_t bsp_sdcard_init(void)`
  - `bool bsp_sdcard_is_mounted(void)`
  - `void bsp_sdcard_get_info(uint64_t *total_bytes, uint64_t *free_bytes)`

- [ ] **Step 1: 编写 `main/bsp_sdcard.h`**

```c
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define SD_PIN_SCK  7
#define SD_PIN_MISO 8
#define SD_PIN_MOSI 9
#define SD_PIN_CS   21

esp_err_t bsp_sdcard_init(void);
bool bsp_sdcard_is_mounted(void);
void bsp_sdcard_get_info(uint64_t *total_bytes, uint64_t *free_bytes);
```

- [ ] **Step 2: 编写 `main/bsp_sdcard.c` 实现（非阻塞容错探测）**

```c
#include "bsp_sdcard.h"
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "esp_log.h"

static const char *TAG = "bsp_sdcard";
static sdmmc_card_t *s_card = NULL;
static bool s_is_mounted = false;

esp_err_t bsp_sdcard_init(void) {
    esp_err_t ret;
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_PIN_MOSI,
        .miso_io_num = SD_PIN_MISO,
        .sclk_io_num = SD_PIN_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SPI bus initialize failed: %s", esp_err_to_name(ret));
        return ret;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_PIN_CS;
    slot_config.host_id = host.slot;

    ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_config, &mount_config, &s_card);
    if (ret == ESP_OK) {
        s_is_mounted = true;
        ESP_LOGI(TAG, "SD Card mounted successfully. Size: %lluMB",
                 ((uint64_t)s_card->csd.capacity) * s_card->csd.sector_size / (1024 * 1024));
    } else {
        s_is_mounted = false;
        ESP_LOGW(TAG, "No SD Card detected or mount failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

bool bsp_sdcard_is_mounted(void) {
    return s_is_mounted;
}

void bsp_sdcard_get_info(uint64_t *total_bytes, uint64_t *free_bytes) {
    if (!s_is_mounted) {
        if (total_bytes) *total_bytes = 0;
        if (free_bytes) *free_bytes = 0;
        return;
    }
    FATFS *fs;
    DWORD fre_clust, fre_sect, tot_sect;
    if (f_getfree("0:", &fre_clust, &fs) == FR_OK) {
        tot_sect = (fs->n_fatent - 2) * fs->csize;
        fre_sect = fre_clust * fs->csize;
        if (total_bytes) *total_bytes = ((uint64_t)tot_sect) * 512;
        if (free_bytes) *free_bytes = ((uint64_t)fre_sect) * 512;
    }
}
```

- [ ] **Step 3: 提交 SD 卡驱动模块**

```bash
git add main/bsp_sdcard.h main/bsp_sdcard.c
git commit -m "feat(bsp): implement microSD SPI mount driver"
```

---

### Task 5: 实现 MSM261D 数字麦克风 PDM 采集模块 (`bsp_mic`)

**Files:**
- Create: `main/bsp_mic.h`
- Create: `main/bsp_mic.c`

**Interfaces:**
- Produces:
  - `esp_err_t bsp_mic_init(void)`
  - `uint8_t bsp_mic_get_level_percent(void)`

- [ ] **Step 1: 编写 `main/bsp_mic.h`**

```c
#pragma once
#include <stdint.h>
#include "esp_err.h"

#define MIC_PDM_CLK_IO  42
#define MIC_PDM_DATA_IO 41

esp_err_t bsp_mic_init(void);
uint8_t bsp_mic_get_level_percent(void);
```

- [ ] **Step 2: 编写 `main/bsp_mic.c`（I2S PDM RX 模式与后台 RMS 音量计算）**

```c
#include "bsp_mic.h"
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_pdm.h"
#include "esp_log.h"

static const char *TAG = "bsp_mic";
static i2s_chan_handle_t s_rx_chan = NULL;
static volatile uint8_t s_current_level_percent = 0;

static void mic_task(void *pvParameters) {
    int16_t sample_buffer[512];
    size_t bytes_read = 0;
    while (1) {
        if (i2s_channel_read(s_rx_chan, sample_buffer, sizeof(sample_buffer), &bytes_read, portMAX_DELAY) == ESP_OK) {
            int samples_count = bytes_read / sizeof(int16_t);
            int64_t sum_squares = 0;
            for (int i = 0; i < samples_count; i++) {
                sum_squares += ((int32_t)sample_buffer[i]) * sample_buffer[i];
            }
            double mean = (double)sum_squares / (samples_count > 0 ? samples_count : 1);
            double rms = sqrt(mean);
            // 归一化为 0~100 百分比
            int level = (int)((rms / 3000.0) * 100.0);
            if (level > 100) level = 100;
            s_current_level_percent = (uint8_t)level;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

esp_err_t bsp_mic_init(void) {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    esp_err_t ret = i2s_new_channel(&chan_cfg, NULL, &s_rx_chan);
    if (ret != ESP_OK) return ret;

    i2s_pdm_rx_config_t pdm_rx_cfg = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .clk = MIC_PDM_CLK_IO,
            .din = MIC_PDM_DATA_IO,
            .invert_flags = { .clk_inv = false },
        },
    };

    ret = i2s_channel_init_pdm_rx_mode(s_rx_chan, &pdm_rx_cfg);
    if (ret != ESP_OK) return ret;

    ret = i2s_channel_enable(s_rx_chan);
    if (ret != ESP_OK) return ret;

    xTaskCreatePinnedToCore(mic_task, "mic_task", 4096, NULL, 5, NULL, 1);
    ESP_LOGI(TAG, "MSM261D PDM mic initialized at 16kHz.");
    return ESP_OK;
}

uint8_t bsp_mic_get_level_percent(void) {
    return s_current_level_percent;
}
```

- [ ] **Step 3: 提交麦克风模块**

```bash
git add main/bsp_mic.h main/bsp_mic.c
git commit -m "feat(bsp): implement MSM261D PDM microphone driver"
```

---

### Task 6: 实现 OV3660 摄像头驱动与帧流模块 (`bsp_camera`)

**Files:**
- Create: `main/bsp_camera.h`
- Create: `main/bsp_camera.c`

**Interfaces:**
- Produces:
  - `esp_err_t bsp_camera_init(framesize_t initial_framesize)`
  - `camera_fb_t *bsp_camera_fb_get(void)`
  - `void bsp_camera_fb_return(camera_fb_t *fb)`
  - `esp_err_t bsp_camera_set_framesize(framesize_t framesize)`

- [ ] **Step 1: 编写 `main/bsp_camera.h`**

```c
#pragma once
#include "esp_err.h"
#include "esp_camera.h"

// XIAO ESP32-S3 Sense 相机引脚定义
#define CAM_PIN_PWDN    -1
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    10
#define CAM_PIN_SIOD    40
#define CAM_PIN_SIOC    39

#define CAM_PIN_D7      48
#define CAM_PIN_D6      11
#define CAM_PIN_D5      12
#define CAM_PIN_D4      14
#define CAM_PIN_D3      16
#define CAM_PIN_D2      18
#define CAM_PIN_D1      17
#define CAM_PIN_D0      15

#define CAM_PIN_VSYNC   38
#define CAM_PIN_HREF    47
#define CAM_PIN_PCLK    13

esp_err_t bsp_camera_init(framesize_t initial_framesize);
camera_fb_t *bsp_camera_fb_get(void);
void bsp_camera_fb_return(camera_fb_t *fb);
esp_err_t bsp_camera_set_framesize(framesize_t framesize);
```

- [ ] **Step 2: 编写 `main/bsp_camera.c` 实现（PSRAM 双缓冲与 OV3660 初始化）**

```c
#include "bsp_camera.h"
#include "esp_log.h"

static const char *TAG = "bsp_camera";

esp_err_t bsp_camera_init(framesize_t initial_framesize) {
    camera_config_t config = {
        .pin_pwdn = CAM_PIN_PWDN,
        .pin_reset = CAM_PIN_RESET,
        .pin_xclk = CAM_PIN_XCLK,
        .pin_sccb_sda = CAM_PIN_SIOD,
        .pin_sccb_scl = CAM_PIN_SIOC,

        .pin_d7 = CAM_PIN_D7,
        .pin_d6 = CAM_PIN_D6,
        .pin_d5 = CAM_PIN_D5,
        .pin_d4 = CAM_PIN_D4,
        .pin_d3 = CAM_PIN_D3,
        .pin_d2 = CAM_PIN_D2,
        .pin_d1 = CAM_PIN_D1,
        .pin_d0 = CAM_PIN_D0,

        .pin_vsync = CAM_PIN_VSYNC,
        .pin_href = CAM_PIN_HREF,
        .pin_pclk = CAM_PIN_PCLK,

        .xclk_freq_hz = 20000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = initial_framesize,
        .jpeg_quality = 12, // 10~15 质量良好
        .fb_count = 2,      // PSRAM 双缓冲
        .fb_location = CAMERA_FB_IN_PSRAM,
        .grab_mode = CAMERA_GRAB_LATEST
    };

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", err);
        return err;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (s != NULL) {
        ESP_LOGI(TAG, "Camera probe success! PID: 0x%04x", s->id.PID);
        // 对 OV3660 传感器进行默认色彩微调
        s->set_vflip(s, 1); // 根据结构放置纠正垂直翻转
    }
    return ESP_OK;
}

camera_fb_t *bsp_camera_fb_get(void) {
    return esp_camera_fb_get();
}

void bsp_camera_fb_return(camera_fb_t *fb) {
    esp_camera_fb_return(fb);
}

esp_err_t bsp_camera_set_framesize(framesize_t framesize) {
    sensor_t *s = esp_camera_sensor_get();
    if (!s) return ESP_FAIL;
    return s->set_framesize(s, framesize) == 0 ? ESP_OK : ESP_FAIL;
}
```

- [ ] **Step 3: 提交摄像头驱动模块**

```bash
git add main/bsp_camera.h main/bsp_camera.c
git commit -m "feat(bsp): implement OV3660 camera driver with dual PSRAM buffers"
```

---

### Task 7: 实现 Wi-Fi SoftAP 与 Web 监控控制台服务 (`web_server`)

**Files:**
- Create: `main/web_server.h`
- Create: `main/web_server.c`

**Interfaces:**
- Produces:
  - `esp_err_t web_server_start(void)`

- [ ] **Step 1: 编写 `main/web_server.h`**

```c
#pragma once
#include "esp_err.h"

esp_err_t web_server_start(void);
```

- [ ] **Step 2: 编写 `main/web_server.c`（包含 SoftAP 初始化、MJPEG `/stream` 流与内嵌 HTML 控制台）**

```c
#include "web_server.h"
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "bsp_camera.h"
#include "bsp_mic.h"
#include "bsp_sdcard.h"
#include "esp_system.h"
#include "esp_heap_caps.h"

static const char *TAG = "web_server";
static httpd_handle_t s_stream_httpd = NULL;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char *STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static const char INDEX_HTML[] = 
"<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>XIAO ESP32-S3 Sense 监控控制台</title><style>"
"body{font-family:-apple-system,BlinkMacSystemFont,sans-serif;margin:0;padding:20px;background:#121212;color:#eee}"
".card{background:#1e1e1e;border-radius:12px;padding:16px;margin-bottom:16px;box-shadow:0 4px 12px rgba(0,0,0,0.5)}"
"h1{font-size:20px;margin-top:0}img{width:100%;max-width:800px;border-radius:8px;background:#000}"
".bar-box{background:#333;border-radius:6px;height:24px;overflow:hidden;position:relative;margin:8px 0}"
".bar{background:#00e676;height:100%;width:0%;transition:width 0.1s ease}"
".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:12px}"
".stat{background:#2a2a2a;padding:12px;border-radius:8px}.stat-v{font-size:18px;font-weight:bold;color:#29b6f6}"
"</style></head><body>"
"<h1>XIAO ESP32-S3 Sense 综合测试控制台</h1>"
"<div class='card'><img id='stream' src='/stream' alt='实时视频流'/></div>"
"<div class='card'><h3>麦克风实时音频电平</h3><div class='bar-box'><div id='mic-bar' class='bar'></div></div><span id='mic-val'>0%</span></div>"
"<div class='card'><h3>系统与硬件运行状态</h3><div class='grid'>"
"<div class='stat'><div>8MB PSRAM 剩余</div><div class='stat-v' id='psram'>-</div></div>"
"<div class='stat'><div>内部 SRAM 剩余</div><div class='stat-v' id='sram'>-</div></div>"
"<div class='stat'><div>MicroSD 卡状态</div><div class='stat-v' id='sd'>-</div></div>"
"<div class='stat'><div>开机时长</div><div class='stat-v' id='uptime'>-</div></div>"
"</div></div>"
"<script>"
"setInterval(()=>{fetch('/api/status').then(r=>r.json()).then(d=>{"
"document.getElementById('mic-bar').style.width=d.mic_level+'%';"
"document.getElementById('mic-val').innerText=d.mic_level+'%';"
"document.getElementById('psram').innerText=(d.free_psram/1024/1024).toFixed(2)+' MB';"
"document.getElementById('sram').innerText=(d.free_sram/1024).toFixed(1)+' KB';"
"document.getElementById('sd').innerText=d.sd_mounted?('已挂载 ('+(d.sd_total/1024/1024).toFixed(0)+' MB)'):'未插卡';"
"document.getElementById('uptime').innerText=d.uptime+' 秒';"
"});},500);"
"</script></body></html>";

static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t status_handler(httpd_req_t *req) {
    char json[256];
    uint64_t sd_tot = 0, sd_free = 0;
    bsp_sdcard_get_info(&sd_tot, &sd_free);
    snprintf(json, sizeof(json),
             "{\"uptime\":%lu,\"free_sram\":%lu,\"free_psram\":%lu,\"mic_level\":%u,\"sd_mounted\":%s,\"sd_total\":%llu}",
             (unsigned long)(esp_timer_get_time() / 1000000),
             (unsigned long)esp_get_free_internal_heap_size(),
             (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             bsp_mic_get_level_percent(),
             bsp_sdcard_is_mounted() ? "true" : "false",
             sd_tot);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    char part_buf[128];

    res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
    if (res != ESP_OK) return res;

    while (true) {
        fb = bsp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            res = ESP_FAIL;
            break;
        }
        res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
        if (res == ESP_OK) {
            size_t hlen = snprintf(part_buf, sizeof(part_buf), STREAM_PART, fb->len);
            res = httpd_resp_send_chunk(req, part_buf, hlen);
        }
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
        }
        bsp_camera_fb_return(fb);
        if (res != ESP_OK) break;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return res;
}

static void wifi_init_softap(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "XIAO_ESP32S3_Sense",
            .ssid_len = strlen("XIAO_ESP32S3_Sense"),
            .channel = 1,
            .password = "seeedstudio",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Wi-Fi SoftAP started. SSID: XIAO_ESP32S3_Sense, Pass: seeedstudio, IP: 192.168.4.1");
}

esp_err_t web_server_start(void) {
    wifi_init_softap();

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.ctrl_port = 32768;

    if (httpd_start(&s_stream_httpd, &config) == ESP_OK) {
        httpd_uri_t index_uri = { .uri = "/", .method = HTTP_GET, .handler = index_handler };
        httpd_register_uri_handler(s_stream_httpd, &index_uri);

        httpd_uri_t status_uri = { .uri = "/api/status", .method = HTTP_GET, .handler = status_handler };
        httpd_register_uri_handler(s_stream_httpd, &status_uri);

        httpd_uri_t stream_uri = { .uri = "/stream", .method = HTTP_GET, .handler = stream_handler };
        httpd_register_uri_handler(s_stream_httpd, &stream_uri);

        ESP_LOGI(TAG, "Web server started on port 80");
        return ESP_OK;
    }
    return ESP_FAIL;
}
```

- [ ] **Step 3: 提交 Web 服务器模块**

```bash
git add main/web_server.h main/web_server.c
git commit -m "feat(server): implement SoftAP and HTTP MJPEG dashboard server"
```

---

### Task 8: 系统主入口编排与全量编译构建 (`main.c`)

**Files:**
- Create: `main/main.c`
- Modify: `main/CMakeLists.txt`

**Interfaces:**
- Consumes: `bsp_led`, `bsp_sdcard`, `bsp_mic`, `bsp_camera`, `web_server`
- Produces: 完整的可执行固件镜像与符号文件

- [ ] **Step 1: 编写 `main/CMakeLists.txt` 注册所有组件与源码**

```cmake
idf_component_register(SRCS "main.c"
                            "bsp_led.c"
                            "bsp_sdcard.c"
                            "bsp_mic.c"
                            "bsp_camera.c"
                            "web_server.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp32-camera esp_wifi esp_http_server esp_timer fatfs nvs_flash)
```

- [ ] **Step 2: 编写 `main/main.c` 系统启动编排与自检汇报**

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "bsp_led.h"
#include "bsp_sdcard.h"
#include "bsp_mic.h"
#include "bsp_camera.h"
#include "web_server.h"

static const char *TAG = "app_main";

static void heartbeat_task(void *pvParameters) {
    while (1) {
        bsp_led_toggle();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "================ XIAO ESP32-S3 Sense Starting ================");

    // 1. 初始化 NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. 初始化 LED
    ESP_ERROR_CHECK(bsp_led_init());
    ESP_LOGI(TAG, "[OK] User LED initialized (GPIO21).");

    // 3. 检查 8MB Octal PSRAM
    size_t psram_size = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    ESP_LOGI(TAG, "[OK] PSRAM Total: %u bytes, Free: %u bytes", psram_size, psram_free);

    // 4. 初始化 MicroSD 卡
    bsp_sdcard_init();

    // 5. 初始化 MSM261D PDM 麦克风
    bsp_mic_init();

    // 6. 初始化 OV3660 摄像头 (默认 SVGA 800x600 分辨率)
    bsp_camera_init(FRAMESIZE_SVGA);

    // 7. 启动 Wi-Fi SoftAP 与 Web 服务
    web_server_start();

    // 8. 启动 LED 规律心跳任务
    xTaskCreate(heartbeat_task, "heartbeat_task", 2048, NULL, 1, NULL);

    ESP_LOGI(TAG, "================ All Subsystems Ready ================");
}
```

- [ ] **Step 3: 执行 `idf.py build` 验证完整工程编译**

```bash
. ~/esp/esp-idf/export.sh
idf.py build
```
Expected: `Project build complete. To flash, run this command: ...`

- [ ] **Step 4: 提交主应用代码**

```bash
git add main/CMakeLists.txt main/main.c
git commit -m "feat: orchestrate all subsystems and build complete firmware"
```

---

### Task 9: 硬件烧录、串口自检与端到端功能验证

**Files:**
- N/A (硬件固件烧录与交互验证)

**Interfaces:**
- Consumes: 编译生成的 `build/xiao_esp32s3_firmware.bin`
- Target: `/dev/cu.usbmodem1101`

- [ ] **Step 1: 烧录固件到开发板**

```bash
. ~/esp/esp-idf/export.sh
idf.py -p /dev/cu.usbmodem1101 flash
```
Expected: `Hash of data verified. Leaving... Hard resetting via RTS pin...`

- [ ] **Step 2: 捕获串口开机日志并验证自检清单**

```bash
. ~/esp/esp-idf/export.sh
idf.py -p /dev/cu.usbmodem1101 monitor
```
Expected Log:
- `[OK] PSRAM Total: 8388608 bytes`
- `Camera probe success! PID: 0x3660`
- `Wi-Fi SoftAP started. SSID: XIAO_ESP32S3_Sense`
- `Web server started on port 80`

- [ ] **Step 3: 硬件与 Web 端到端交互验证**
1. 观察开发板橙色 LED 是否规律呼吸闪烁。
2. 电脑或手机连接 Wi-Fi：`XIAO_ESP32S3_Sense`，密码 `seeedstudio`。
3. 打开浏览器访问 `http://192.168.4.1`。
4. 验证实时视频流渲染、麦克风音频音量条响应及 PSRAM 剩余统计。
