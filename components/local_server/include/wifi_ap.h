#ifndef __WIFI_AP_H__
#define __WIFI_AP_H__

#include "esp_err.h"
#include "nvs_flash.h"
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"


/*******************************************************
 *                Function Declarationss
 *******************************************************/
/*void ip_event_handler(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data);*/
/*void wifi_event_handler(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data);*/
esp_err_t wifi_init(void);
esp_err_t wifi_scan(void);
esp_err_t wifi_connect(char*, char*);
//void gsm_init(void);

#endif /* __WIFI_AP_H__ */
