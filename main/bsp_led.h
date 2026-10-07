#pragma once
#include <stdbool.h>
#include "esp_err.h"

#define BSP_LED_GPIO 21

esp_err_t bsp_led_init(void);
void bsp_led_set(bool on);
void bsp_led_toggle(void);
