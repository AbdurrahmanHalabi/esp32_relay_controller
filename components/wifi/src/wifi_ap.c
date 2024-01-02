#include "wifi_ap.h"

/*******************************************************
 *                Constants
 *******************************************************/
#define AV_APs   5                          // the number of available access points
#define MAX_APs 10                          // maximum number of access points to scan

/*******************************************************
 *                Variable Definitions
 *******************************************************/
bool found = false; 
static wifi_sta_config_t ap_list[AV_APs];
static wifi_sta_config_t connected_ap_info;
static const char *WIFI_TAG = "wifi station";
static uint8_t reconnect_c = 0;
static uint8_t scan_c = 0;
extern wifi_ap_record_t ap_records[MAX_APs];
extern uint16_t ap_num;
bool is_connected = false; 

esp_netif_t * wifi_sta;
wifi_config_t wifi_config;
/*******************************************************
 *                Function Definitions
 *******************************************************/

/*void ip_event_handler(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    ESP_LOGI(WIFI_TAG, "<IP_EVENT_STA_GOT_IP>IP:" IPSTR, IP2STR(&event->ip_info.ip));

}*/

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{

    switch (event_id)
    {

    case WIFI_EVENT_AP_STACONNECTED: {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(WIFI_TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
                 }
    break;
        
    case WIFI_EVENT_AP_STADISCONNECTED: {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(WIFI_TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
    break;

    case WIFI_EVENT_STA_CONNECTED: {
        wifi_event_sta_connected_t *connected_sta = (wifi_event_sta_connected_t*) event_data;
        ESP_LOGI(WIFI_TAG, "** Connected to router. SSID: %s /BSSID: %s **\n", (char*)connected_sta->ssid, 
                                                                                (char*)connected_sta->bssid);
        is_connected = true;
    }
    break;
    
    case WIFI_EVENT_STA_DISCONNECTED: {  // importnat to reconnect to internet either through wifi or gsm 
        wifi_event_sta_disconnected_t *disconected_sta = (wifi_event_sta_disconnected_t*) event_data;
        ESP_LOGE(WIFI_TAG, "** Disconnected from router. SSID: %s **\n", (char*)disconected_sta->ssid);
        if(reconnect_c < 20){
        ESP_LOGW(WIFI_TAG, "** Resetting wifi connection.... **\n");
        esp_wifi_connect();
        reconnect_c++;
        is_connected = false;
        //  Wifi disconnects after too many requests and unable to reconnect so probably we should initialize wifi again here.  

        // if(reconnect_c == 10){ 
        //     wifi_init_scan();
        // }
        vTaskDelay(10000/portTICK_PERIOD_MS);
        }
        else
        {
            ESP_LOGW(WIFI_TAG, "** Connecting through GSM (2G).... **\n");
            //gsm_init();
        }
    }
    break;

    case WIFI_EVENT_SCAN_DONE: {
        ESP_LOGI(WIFI_TAG, "** Scan finished **\n");
    }
    break;

    case WIFI_EVENT_STA_START: {
        ESP_LOGI(WIFI_TAG, "** started WIFI **\n");
    }
    break;
    }
}
//
// initializing wifi and softAP
esp_err_t wifi_init(void){

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,NULL,NULL));

    wifi_config = (wifi_config_t){
        .ap = {
            .ssid = "ANTORIN_SOFTAP\0",
            .ssid_len = 0,
            .channel = 8,
            .password = "open_softAP",
            .max_connection = 1,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        }
    };
    //if (strlen("open_softAP") == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    //}

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_set_ps(WIFI_PS_NONE);

    ESP_LOGI(WIFI_TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             wifi_config.ap.ssid, wifi_config.ap.password, wifi_config.ap.channel);
    return ESP_OK;
}
//
// scanning wifi access points
esp_err_t wifi_scan(void){

    if (scan_c > 0){
       // ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT  , IP_EVENT_STA_GOT_IP, &ip_event_handler));
        //ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID   , &wifi_event_handler));
    }
    //ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT  , IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL));
    //ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID   , &wifi_event_handler, NULL));
	ESP_ERROR_CHECK(esp_wifi_scan_stop());
	// configure and run the scan process in blocking mode
	wifi_scan_config_t scan_config = {
		.ssid = 0,
		.bssid = 0,
		.channel = 0,
        .show_hidden = true
    };
	//Manually start scan. Will automatically stop when run to completion
	ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
    scan_c++;
	// get the list of APs found in the last scan
    ap_num = MAX_APs;
	ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_num, ap_records));
	// print the list AND process scan results
	printf("Found %d access points:\n", ap_num);
	printf("\n");
	printf("               SSID              | Channel | RSSI\n");
	printf("----------------------------------------------------------------\n");
	for(int i = 0; i < ap_num; i++){
		printf("%32s | %7d | %4d\n", (char *)ap_records[i].ssid, ap_records[i].primary, ap_records[i].rssi);
    }
	printf("----------------------------------------------------------------\n");

    return ESP_OK;
}
//
// connecting to wifi as a station
esp_err_t wifi_connect(char*ssid, char* pass){

    strcpy((char *) wifi_config.sta.ssid    , (char*) ssid);
    strcpy((char *) wifi_config.sta.password, (char*) pass);
    //memcpy(wifi_config.sta.bssid, connected_ap_info.bssid, sizeof(connected_ap_info.bssid));
    wifi_config.sta.bssid_set   = 0;
    wifi_config.sta.scan_method = 0;
    wifi_config.sta.pmf_cfg.required = 0;
    wifi_config.sta.channel = 0;

    printf("%s, %s\n", wifi_config.sta.ssid, wifi_config.sta.password);

    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    esp_err_t con_err = esp_wifi_connect();
    vTaskDelay(3000/portTICK_PERIOD_MS);
    if (is_connected == false)
        return ESP_FAIL;
    else
        return ESP_OK;
    /*if (con_err == ESP_FAIL)
        return ESP_FAIL;
    else
        return ESP_OK;*/
}