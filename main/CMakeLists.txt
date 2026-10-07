idf_component_register(SRCS "main.c"
                            "bsp_led.c"
                            "bsp_sdcard.c"
                            "bsp_mic.c"
                            "bsp_camera.c"
                            "bsp_temp.c"
                            "web_server.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp32-camera esp_wifi esp_http_server esp_timer fatfs nvs_flash driver esp_driver_tsens)
