
#include <stdio.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"

#include "esp_log.h"
#include "esp_websocket_client.h"
#include "esp_event.h"

#include "wifi_ap.h"
#include "httpd.h"

#define NO_DATA_TIMEOUT_SEC 60
#define led1_GPIO 2
#define led2_GPIO 4
#define led3_GPIO 18
#define led4_GPIO 19
#define led5_GPIO 21
#define reset_GPIO 32
#define STACK_SIZE 2048
#define GATE_INPUT_SEL  ((1ULL<<reset_GPIO))
#define GATE_OUTPUT_SEL  ((1ULL<<led1_GPIO) | (1ULL<<led2_GPIO) | (1ULL<<led3_GPIO) | (1ULL<<led4_GPIO) | (1ULL<<led5_GPIO))
#define ID "11"


static const char *TAG = "WEBSOCKET";
uint8_t base_mac_addr[6] = {0};
static TimerHandle_t shutdown_signal_timer;
static SemaphoreHandle_t shutdown_sema;
char * soc_data = NULL;

wifi_ap_record_t ap_records[10];
uint16_t ap_num;

nvs_handle_t wifi_cred_flash_handle;

static void shutdown_signaler(TimerHandle_t xTimer)
{
    ESP_LOGI(TAG, "No data received for %d seconds, signaling shutdown", NO_DATA_TIMEOUT_SEC);
    xSemaphoreGive(shutdown_sema);
}

#if CONFIG_WEBSOCKET_URI_FROM_STDIN
static void get_string(char *line, size_t size)
{
    int count = 0;
    while (count < size) {
        int c = fgetc(stdin);
        if (c == '\n') {
            line[count] = '\0';
            break;
        } else if (c > 0 && c < 127) {
            line[count] = c;
            ++count;
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

#endif /* CONFIG_WEBSOCKET_URI_FROM_STDIN */

/*
*
*
    this is the event handler definition of the websockt communication. from here when a received data
    event is triggered, we parse the data and determines which relay we should turn on or off. 
    the structure of the communication is decided on from the cloud first 
*/
static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED");
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
        break;
    case WEBSOCKET_EVENT_DATA:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_DATA");
        ESP_LOGI(TAG, "Received opcode=%d", data->op_code);
        ESP_LOGW(TAG, "Received=%.*s", data->data_len, (char *)data->data_ptr);
        if (data->data_len > 0)
        {
            soc_data = calloc((data->data_len)+1, 1);
            memcpy(soc_data, data->data_ptr, data->data_len);
            printf("%s\n", soc_data);
    
            if (strcmp((char *)soc_data, "relay1on") == 0){
                gpio_set_level(led1_GPIO, 3);
                printf("leds 1 on\n");
                free(soc_data);
            }
            else if(strcmp((char *)soc_data, "relay1off") == 0){
                gpio_set_level(led1_GPIO, false);
                printf("leds 1 off\n");
                free(soc_data);
            } 
            else if (strcmp((char *)soc_data, "relay2on") == 0){
                gpio_set_level(led2_GPIO, 3);
                printf("leds 2 on\n");
                free(soc_data);
            }
            else if(strcmp((char *)soc_data, "relay2off") == 0){
                gpio_set_level(led2_GPIO, false);
                printf("leds 2 off\n");
                free(soc_data);
            } 
            else if (strcmp((char *)soc_data, "relay3on") == 0){
                gpio_set_level(led3_GPIO, 3);
                printf("leds 3 on\n");
                free(soc_data);
            }
            else if(strcmp((char *)soc_data, "relay3off") == 0){
                gpio_set_level(led3_GPIO, false);
                printf("leds 3 off\n");
                free(soc_data);
            } 
            else if (strcmp((char *)soc_data, "relay4on") == 0){
                gpio_set_level(led4_GPIO, 3);
                printf("leds 4 on\n");
                free(soc_data);
            }
            else if(strcmp((char *)soc_data, "relay4off") == 0){
                gpio_set_level(led4_GPIO, false);
                printf("leds 4 off\n");
                free(soc_data);
            } 
            else if (strcmp((char *)soc_data, "relay5on") == 0){
                gpio_set_level(led5_GPIO, 3);
                printf("leds 5 on\n");
                free(soc_data);
            }
            else if(strcmp((char *)soc_data, "relay5off") == 0){
                gpio_set_level(led5_GPIO, false);
                printf("leds 5 off\n");
                free(soc_data);
            } 
            else
                printf("no action\n");

        }
        ESP_LOGW(TAG, "Total payload length=%d, data_len=%d, current payload offset=%d\r\n", data->payload_len, data->data_len, data->payload_offset);

        xTimerReset(shutdown_signal_timer, portMAX_DELAY);
        break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_ERROR");
        break;
    }
}

/*
*
*
    this is where the websocket connection is opened. this funcotion is called in the main function
*/
void websocket_app_task(void)
{
    char *header = NULL;
    asprintf(&header, "id: %X:%X:%X:%X:%X:%X\r\n", base_mac_addr[0], base_mac_addr[1], base_mac_addr[2], base_mac_addr[3], base_mac_addr[4], base_mac_addr[5] );
    esp_websocket_client_config_t websocket_cfg = {};

    shutdown_signal_timer = xTimerCreate("Websocket shutdown timer", NO_DATA_TIMEOUT_SEC * 1000 / portTICK_PERIOD_MS,
                                         pdFALSE, NULL, shutdown_signaler);
    shutdown_sema = xSemaphoreCreateBinary();

#if CONFIG_WEBSOCKET_URI_FROM_STDIN
    char line[128];

    ESP_LOGI(TAG, "Please enter uri of websocket endpoint");
    get_string(line, sizeof(line));

    websocket_cfg.uri = line;
    ESP_LOGI(TAG, "Endpoint uri: %s\n", line);

#else
    websocket_cfg.headers = header;
    //websocket_cfg.uri = "ws://192.168.1.34:8333";
    websocket_cfg.uri = "wss://enersee.io/ws";
    //websocket_cfg.uri = "ws://192.168.1.41:4040";
    //printf("%s\n", websocket_cfg.headers);
    //websocket_cfg.host = "192.168.1.41";
    //websocket_cfg.port = 8080;

#endif /* CONFIG_WEBSOCKET_URI_FROM_STDIN */

    ESP_LOGI(TAG, "Connecting to %s...", websocket_cfg.uri);

    esp_websocket_client_handle_t client = esp_websocket_client_init(&websocket_cfg);
    esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)client);

    esp_websocket_client_start(client);
    xTimerStart(shutdown_signal_timer, portMAX_DELAY);
    char data[32];
    int i = 0;
    while (i < 10) {
        if (esp_websocket_client_is_connected(client)) {
            int len = sprintf(data, "hello %04d", i++);
            ESP_LOGI(TAG, "Sending %s", data);
            esp_websocket_client_send_text(client, data, len, portMAX_DELAY);
        }
        vTaskDelay(1000 / portTICK_RATE_MS);
    }

    xSemaphoreTake(shutdown_sema, portMAX_DELAY);
    esp_websocket_client_stop(client);
    ESP_LOGI(TAG, "Websocket Stopped");
    esp_websocket_client_destroy(client);
}

/*
*
*
    main function
*/
void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("WEBSOCKET_CLIENT", ESP_LOG_DEBUG);
    esp_log_level_set("TRANS_TCP", ESP_LOG_DEBUG);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_efuse_mac_get_default(base_mac_addr);

    gpio_config_t io_conf;
    io_conf.pull_up_en = 0;
    io_conf.pull_down_en = 1;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GATE_OUTPUT_SEL;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_set_drive_capability(led1_GPIO, 3);
    gpio_set_drive_capability(led2_GPIO, 3);
    gpio_set_drive_capability(led3_GPIO, 3);
    gpio_set_drive_capability(led4_GPIO, 3);
    gpio_set_drive_capability(led5_GPIO, 3);
    gpio_config(&io_conf);

    gpio_config_t io_conf_in;
    io_conf_in.pull_up_en = 1;
    io_conf_in.pull_down_en = 0;
    io_conf_in.mode = GPIO_MODE_INPUT;
    io_conf_in.pin_bit_mask = GATE_INPUT_SEL;
    io_conf_in.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf_in);

    size_t required_size = 0;
    nvs_open("storage", NVS_READWRITE, &wifi_cred_flash_handle);
    esp_err_t nvs_err = nvs_get_str(wifi_cred_flash_handle, "SSID", NULL, &required_size);

    printf("%d\n",gpio_get_level(GPIO_NUM_32));
    /* if  no wiif credentials is already saved in the flash memory, or is GPIO 32 is low the device 
    starts its server mode to receive wifi data*/
    if (nvs_err == ESP_FAIL || gpio_get_level(GPIO_NUM_32) == 0 || required_size < 8){
        nvs_close(wifi_cred_flash_handle);
        printf("* * * * * * * * * ENTERING SERVER MODE * * * * * * * * *\n");
        server_init();
        vTaskDelay(10000/portTICK_PERIOD_MS);
        xTaskCreate(&websocket_app_task, "websocket_app_task", STACK_SIZE, NULL, 3, NULL);
    }
    else{   /* if wifi info already exist in the flash memory, it directly opens the websocket connection
            and starts the relay control*/
        char * rec_ssid = calloc(30, sizeof(char));
        char * rec_pass = calloc(30, sizeof(char));
        nvs_get_str(wifi_cred_flash_handle, "SSID", rec_ssid, &required_size);
        nvs_get_str(wifi_cred_flash_handle, "PASS", NULL, &required_size);
        nvs_get_str(wifi_cred_flash_handle, "PASS", rec_pass, &required_size);

        printf("%s, %s\n", rec_ssid,rec_pass);
        
        esp_netif_t* client_sta = NULL;
        client_sta = esp_netif_create_default_wifi_sta();
        ESP_ERROR_CHECK(esp_netif_dhcpc_start(client_sta));
        wifi_init();
        wifi_connect(rec_ssid, rec_pass);
        vTaskDelay(3000/portTICK_PERIOD_MS);
        //ESP_ERROR_CHECK(example_connect());

        // websocket_app_start();
        xTaskCreate(&websocket_app_task, "websocket_app_task", STACK_SIZE, NULL, 3, NULL);
    }

}
