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
