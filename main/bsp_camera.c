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
