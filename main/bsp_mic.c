#include "bsp_mic.h"
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
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
