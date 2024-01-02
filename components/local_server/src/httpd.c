#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "mdns.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "esp_eth.h"
//#include "protocol_examples_common.h"
#include <esp_http_server.h>

#include "wifi_ap.h"
#include "httpd.h"

#define STACK_SIZE 2048
/* A simple example that demonstrates how to create GET and POST
 * handlers for the web server.
 */

static const char *TAG = "http-server";
wifi_ap_record_t ap_records[10];
uint16_t ap_num;
TaskHandle_t ota_task_handler = NULL;
//uint8_t base_mac_addr[6] = {0};
static httpd_handle_t server = NULL;
extern nvs_handle_t wifi_cred_flash_handle; 
/* An HTTP GET handler */

/*
*
*
    the handler that is responsible for displayin gthe html file
*/
static esp_err_t index_get_handler(httpd_req_t *req)
{
    //const char* resp_str = (const char*) req->user_ctx;
    //httpd_resp_send(req, resp_str, strlen(resp_str));

    /* Send HTML file header */
    httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html><body>");
    /* Get handle to embedded file upload script */
    extern const unsigned char esp_index_start[] asm("_binary_esp_index_html_start");
    extern const unsigned char esp_index_end[]   asm("_binary_esp_index_html_end");
    const size_t esp_index_size = (esp_index_end - esp_index_start);
    /* Add file upload form and script which on execution sends a POST request to /upload */
    httpd_resp_send_chunk(req, (const char *)esp_index_start, esp_index_size);
    /* Send empty chunk to signal HTTP response completion */
    httpd_resp_sendstr_chunk(req, NULL);

    /* After sending the HTTP response the old HTTP request
     * headers are lost. Check if HTTP request headers can be read now. */
    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        ESP_LOGI(TAG, "Request headers lost");
    }
    return ESP_OK;
}

static const httpd_uri_t index_html = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = NULL
};

/*
*
*
    this handler sedns the scanned wifi setworks by the device to the webpage 
*/
static esp_err_t wifi_list_get_handler(httpd_req_t *req)
{
    wifi_scan();
    if (ap_num > 0)
    {
        char * reading = NULL;
        char * readings_list = NULL;
        char * ssid_list = NULL;
        int n = 0;
        int old_n = 0;
        int reading_sz = 0;
        for (int x = 0; x<ap_num; x++)
        {
            n = asprintf(&reading, "\"%s\",", ap_records[x].ssid);
            old_n = old_n + n;
            readings_list = realloc(readings_list, old_n);
            memmove(&readings_list[reading_sz], reading, n);
            reading_sz = old_n;
            free(reading);
        }
        char * readings_list_copy = NULL;
        readings_list_copy = calloc(reading_sz, sizeof(char));
        memmove(readings_list_copy, readings_list, reading_sz-1);
        asprintf(&ssid_list, "{\"list\":[%s]}", readings_list_copy);
        printf("%s\n", ssid_list);
    
        /* Send HTML file header */
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr_chunk(req, ssid_list);
        /* Get handle to embedded file upload script */
        //httpd_resp_send_chunk(req, (const char *)esp_index_start, esp_index_size);
        /* Send empty chunk to signal HTTP response completion */
        httpd_resp_sendstr_chunk(req, NULL);

        free(readings_list_copy);
        free(readings_list);
        free(ssid_list);
    }
    else {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr_chunk(req, "{\"list\":[]}");
        httpd_resp_sendstr_chunk(req, NULL);
    }
    /* After sending the HTTP response the old HTTP request
     * headers are lost. Check if HTTP request headers can be read now. */
    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        ESP_LOGI(TAG, "Request headers lost");
    }
    return ESP_OK;
}

static const httpd_uri_t wifi = {
    .uri       = "/search-wifi",
    .method    = HTTP_GET,
    .handler   = wifi_list_get_handler,
    .user_ctx  = NULL
};


/*
*
*
    once the user enters the password data to the webpage, it is sebd bakc ot the local 
    server through this post HTTP POST handler 
*/
static esp_err_t wifi_pass_post_handler(httpd_req_t *req)
{
    char buf[100];
    int ret = 0, remaining = req->content_len;

    while (remaining > 0) {
        /* Read the data for the request */
        if ((ret = httpd_req_recv(req, buf,
                        MIN(remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry receiving if timeout occurred */
                continue;
            }
            return ESP_FAIL;
        }

        /* Send back the same data */
        //httpd_resp_send_chunk(req, buf, ret);
        remaining -= ret;

        /* Log data received */
        ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
        ESP_LOGI(TAG, "%.*s", ret, buf);
        ESP_LOGI(TAG, "====================================");
    }
    if (ret>0){
        char * rec_buf = NULL;
        char * rec_buf_ptr = NULL;
        rec_buf = calloc(ret+1, sizeof(char));
        memcpy(rec_buf, buf, ret);

        char * rec_ssid = calloc(30, sizeof(char));
        char * rec_pass = calloc(30, sizeof(char));

        strcpy(rec_pass, strtok_r(&rec_buf[9], "\"", &rec_buf_ptr));
        strcpy(rec_ssid, strtok(rec_buf_ptr+9, "\""));

        printf("ssid: %s, password: %s\n",rec_ssid,  rec_pass);
        esp_err_t err_msg = wifi_connect(rec_ssid, rec_pass);
        if (err_msg == ESP_FAIL){
            char *resp = calloc(90, sizeof(char));
            asprintf(&resp, "{\"resp\": \"The network key you entered is wrong\"}");
            ESP_LOGW(TAG, "%s", resp);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr_chunk(req, resp);
        }
        else{
            nvs_open("storage", NVS_READWRITE, &wifi_cred_flash_handle);
            nvs_set_str(wifi_cred_flash_handle, "SSID", rec_ssid);
            nvs_set_str(wifi_cred_flash_handle, "PASS", rec_pass);
            nvs_commit(wifi_cred_flash_handle);
            nvs_close(wifi_cred_flash_handle);

            ESP_LOGI(TAG, "*** Connected to WiFi ***");
            //ESP_LOGW(TAG, "*** Webserver stopping ***");
            //httpd_stop(server);
            vTaskDelay(7000/portTICK_PERIOD_MS);
            //ESP_LOGI(TAG, "Getting authentication ticket...");
            //https_auth_request();
            //ESP_LOGI(TAG, "Performing OTA...");
            //xTaskCreate(&ota_task, "OTA-update", STACK_SIZE*2, NULL, 4, &ota_task_handler);
        }
        free(rec_buf);
    }
    // End response
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

static const httpd_uri_t password = {
    .uri       = "/submit-wifi",
    .method    = HTTP_POST,
    .handler   = wifi_pass_post_handler,
    .user_ctx  = NULL
};

/* This handler allows the custom error handling functionality to be
 * tested from client side. For that, when a PUT request 0 is sent to
 * URI /ctrl, the /hello and /echo URIs are unregistered and following
 * custom error handler http_404_error_handler() is registered.
 * Afterwards, when /hello or /echo is requested, this custom error
 * handler is invoked which, after sending an error message to client,
 * either closes the underlying socket (when requested URI is /echo)
 * or keeps it open (when requested URI is /hello). This allows the
 * client to infer if the custom error handler is functioning as expected
 * by observing the socket state.
 */
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    if (strcmp("/", req->uri) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/ URI is not available");
        /* Return ESP_OK to keep underlying socket open */
        return ESP_OK;
    } else if (strcmp("/echo", req->uri) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/echo URI is not available");
        /* Return ESP_FAIL to close underlying socket */
        return ESP_FAIL;
    }
    /* For any other URI send 404 and close socket */
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Some 404 error message");
    return ESP_FAIL;
}

/* An HTTP PUT handler. This demonstrates realtime
 * registration and deregistration of URI handlers
 */
static esp_err_t ctrl_put_handler(httpd_req_t *req)
{
    char buf;
    int ret;

    if ((ret = httpd_req_recv(req, &buf, 1)) <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }

    if (buf == '0') {
        /* URI handlers can be unregistered using the uri string */
        ESP_LOGI(TAG, "Unregistering / and /echo URIs");
        httpd_unregister_uri(req->handle, "/");
        /* Register the custom error handler */
        httpd_register_err_handler(req->handle, HTTPD_404_NOT_FOUND, http_404_error_handler);
    }
    else {
        ESP_LOGI(TAG, "Registering / and /echo URIs");
        httpd_register_uri_handler(req->handle, &index_html);
        /* Unregister custom error handler */
        httpd_register_err_handler(req->handle, HTTPD_404_NOT_FOUND, NULL);
    }

    /* Respond with empty body */
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t ctrl = {
    .uri       = "/ctrl",
    .method    = HTTP_PUT,
    .handler   = ctrl_put_handler,
    .user_ctx  = NULL
};

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = STACK_SIZE*3;
    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &index_html);
        httpd_register_uri_handler(server, &ctrl);
        httpd_register_uri_handler(server, &wifi);
        httpd_register_uri_handler(server, &password);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}

static void disconnect_handler(void* arg, esp_event_base_t event_base, 
                               int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        ESP_LOGI(TAG, "Stopping webserver");
        stop_webserver(*server);
        *server = NULL;
    }
}

static void connect_handler(void* arg, esp_event_base_t event_base, 
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}


void server_init(void)
{
    esp_netif_t* server_ap = NULL;
    esp_netif_t* client_sta = NULL;

    /*ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());*/
    server_ap = esp_netif_create_default_wifi_ap();
    client_sta = esp_netif_create_default_wifi_sta();
    ESP_ERROR_CHECK(esp_netif_dhcps_stop(server_ap));
    ESP_ERROR_CHECK(esp_netif_dhcpc_stop(client_sta));

    esp_netif_ip_info_t info;
    memset(&info, 0, sizeof(info));
	IP4_ADDR(&info.ip, 172, 16, 0, 0);
    IP4_ADDR(&info.gw, 172, 16, 0, 0);
    IP4_ADDR(&info.netmask, 255, 255, 255, 0);
	ESP_ERROR_CHECK(esp_netif_set_ip_info(server_ap, &info));
    ESP_ERROR_CHECK(esp_netif_set_ip_info(client_sta, &info));
	printf("- TCP adapter configured with IP 172.16.0.0/24\n");

    ESP_ERROR_CHECK(esp_netif_dhcps_start(server_ap));
    ESP_ERROR_CHECK(esp_netif_dhcpc_start(client_sta));

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    //ESP_ERROR_CHECK(example_connect());

    /* Register event handlers to stop the server when Wi-Fi or Ethernet is disconnected,
     * and re-start it upon connection.
     */
/*#ifdef CONFIG_EXAMPLE_CONNECT_WIFI
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));
#endif // CONFIG_EXAMPLE_CONNECT_WIFI
#ifdef CONFIG_EXAMPLE_CONNECT_ETHERNET
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, &disconnect_handler, &server));
#endif*/ // CONFIG_EXAMPLE_CONNECT_ETHERNET
    /*esp_efuse_mac_get_default(base_mac_addr);
    base_mac_addr[0] = 0x8C;
    base_mac_addr[1] = 0xAA;
    base_mac_addr[2] = 0xB5;
    base_mac_addr[3] = 0x94;
    base_mac_addr[4] = 0xC9;
    base_mac_addr[5] = 0x24;*/
    /* Start the server for the first time */
    server = start_webserver();
    //initialize wifi softAP
    ESP_ERROR_CHECK(wifi_init());
}
