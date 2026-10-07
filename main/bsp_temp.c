#include "bsp_temp.h"
#include "driver/temperature_sensor.h"
#include "esp_log.h"

static const char *TAG = "bsp_temp";
static temperature_sensor_handle_t s_temp_handle = NULL;

esp_err_t bsp_temp_init(void) {
    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 80);
    esp_err_t ret = temperature_sensor_install(&temp_sensor_config, &s_temp_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Install temperature sensor failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = temperature_sensor_enable(s_temp_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Enable temperature sensor failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "On-chip temperature sensor initialized successfully");
    return ESP_OK;
}

float bsp_temp_get_celsius(void) {
    if (!s_temp_handle) {
        return -1.0f;
    }
    float temp_c = 0.0f;
    esp_err_t ret = temperature_sensor_get_celsius(s_temp_handle, &temp_c);
    if (ret == ESP_OK) {
        return temp_c;
    }
    ESP_LOGW(TAG, "Read temperature failed: %s", esp_err_to_name(ret));
    return -1.0f;
}
