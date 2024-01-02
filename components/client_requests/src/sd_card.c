#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "freertos/task.h"

#include "ledc.h"
#include "sd_card.h"

/*******************************************************
 *                Variables Declarations
 *******************************************************/
static const char *SD_TAG = "SD-card";

/*******************************************************
 *                Constants
 *******************************************************/
#define PIN_NUM_MISO 12  
#define PIN_NUM_MOSI 13
#define PIN_NUM_CLK  14
#define PIN_NUM_CS   15
#define SPI_DMA_CHAN  1

bool spi_bus_init_stat = false;
/*******************************************************
 *                Function Definitions
 *******************************************************/
esp_err_t sd_init(void)
{
    esp_err_t ret;
    sdmmc_card_t* card;

    ESP_LOGI(SD_TAG, "Initializing SD card");
    ESP_LOGI(SD_TAG, "Using SPI peripheral");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = 19000;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 0,  // uses default size of 4094
    };

    if (spi_bus_init_stat == true){
        spi_bus_free(host.slot);
    }

    ret = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CHAN);
    if (ret != ESP_OK) {
        ESP_LOGE(SD_TAG, "Failed to initialize bus.");
        return ESP_FAIL;
    }    
    else
    {
        spi_bus_init_stat = true;
    }
    

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;
    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 2,
        .allocation_unit_size = 16 * 1024
    };

    // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc_mount is an all-in-one convenience function.
    // Please check its source code and implement error recovery when developing
    // production applications.
    ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(SD_TAG, "Failed to mount filesystem. "
                "If you want the card to be formatted, set format_if_mount_failed = true.");
        } else {
            ESP_LOGE(SD_TAG, "Failed to initialize the card (%s). "
                "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    // Card has been initialized, print its properties
    printf("\n");
    sdmmc_card_print_info(stdout, card);
    printf("\n");
    return ESP_OK;
}

esp_err_t sd_reset(uint8_t tri_num){
    uint8_t num =0;
    esp_err_t ret;;

    esp_vfs_fat_sdmmc_unmount();
    vTaskDelay(500 / portTICK_PERIOD_MS);
    do{
        num++;
        ret = sd_init();
        if(ret != ESP_OK){
            if (num>= tri_num)
                break;
            led_blink3(0, 150);
            led_blink3(1, 150);
        }
        else
            vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    while(ret != ESP_OK);

    return ret;

}

