#pragma once
#include <stdint.h>
#include "esp_err.h"

#define MIC_PDM_CLK_IO  42
#define MIC_PDM_DATA_IO 41

esp_err_t bsp_mic_init(void);
uint8_t bsp_mic_get_level_percent(void);
