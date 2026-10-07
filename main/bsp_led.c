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
