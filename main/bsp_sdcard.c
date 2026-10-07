#include "bsp_sdcard.h"
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "esp_log.h"

static const char *TAG = "bsp_sdcard";
static sdmmc_card_t *s_card = NULL;
static bool s_is_mounted = false;

esp_err_t bsp_sdcard_init(void) {
    esp_err_t ret;
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_PIN_MOSI,
        .miso_io_num = SD_PIN_MISO,
        .sclk_io_num = SD_PIN_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SPI bus initialize failed: %s", esp_err_to_name(ret));
        return ret;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_PIN_CS;
    slot_config.host_id = host.slot;

    ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_config, &mount_config, &s_card);
    if (ret == ESP_OK) {
        s_is_mounted = true;
        ESP_LOGI(TAG, "SD Card mounted successfully. Size: %lluMB",
                 ((uint64_t)s_card->csd.capacity) * s_card->csd.sector_size / (1024 * 1024));
    } else {
        s_is_mounted = false;
        ESP_LOGW(TAG, "No SD Card detected or mount failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

bool bsp_sdcard_is_mounted(void) {
    return s_is_mounted;
}

void bsp_sdcard_get_info(uint64_t *total_bytes, uint64_t *free_bytes) {
    if (!s_is_mounted) {
        if (total_bytes) *total_bytes = 0;
        if (free_bytes) *free_bytes = 0;
        return;
    }
    FATFS *fs;
    DWORD fre_clust, fre_sect, tot_sect;
    if (f_getfree("0:", &fre_clust, &fs) == FR_OK) {
        tot_sect = (fs->n_fatent - 2) * fs->csize;
        fre_sect = fre_clust * fs->csize;
        if (total_bytes) *total_bytes = ((uint64_t)tot_sect) * 512;
        if (free_bytes) *free_bytes = ((uint64_t)fre_sect) * 512;
    } else {
        if (total_bytes) *total_bytes = 0;
        if (free_bytes) *free_bytes = 0;
    }
}
