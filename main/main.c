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
        if (!bsp_sdcard_is_mounted()) {
            bsp_led_toggle();
        }
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
    ESP_LOGI(TAG, "[OK] PSRAM Total: %u bytes, Free: %u bytes", (unsigned int)psram_size, (unsigned int)psram_free);

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
