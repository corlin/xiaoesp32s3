#ifndef BSP_TEMP_H
#define BSP_TEMP_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化板载芯片内置温度传感器
 * @return esp_err_t ESP_OK 表示成功
 */
esp_err_t bsp_temp_init(void);

/**
 * @brief 读取当前芯片核心结温（摄氏度）
 * @return float 当前温度 (°C)，若读取失败返回 -1.0f
 */
float bsp_temp_get_celsius(void);

#ifdef __cplusplus
}
#endif

#endif // BSP_TEMP_H
