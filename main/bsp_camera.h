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
