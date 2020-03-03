/* Simple HTTP + SSL Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "esp_eth.h"
#include "protocol_examples_common.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include <esp_https_server.h>
#include <cJSON.h>

#include "driver/uart.h"
#include "driver/gpio.h"
#include <stdio.h>

//uart defines
#define ECHO_TEST_TXD  (GPIO_NUM_17)
#define ECHO_TEST_RXD  (GPIO_NUM_16)
#define ECHO_TEST_RTS  (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS  (UART_PIN_NO_CHANGE)
#define BUF_SIZE (10240)
#define UART_BUF_SIZE (1024)

char data[BUF_SIZE] = "[";
const char * comma = ","; // have to do this because need string type

/* An HTTP GET handler */
static esp_err_t root_get_handler(httpd_req_t *req)
{

	if (data[strlen(data)-1] == ',') {
	    		data[strlen(data)-1] = ']';
	}
	httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req,data, -1); // -1 = use strlen()
    memset((void *) &data, 0, sizeof(data));
    data[0] = '[';
    return ESP_OK;
}

static const httpd_uri_t root = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler
};

static const char * TAG = "sdf";

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;


    // Start the httpd server
    ESP_LOGI(TAG, "Starting server");

    httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();

    extern const unsigned char cacert_pem_start[] asm("_binary_cacert_pem_start");
    extern const unsigned char cacert_pem_end[]   asm("_binary_cacert_pem_end");
    conf.cacert_pem = cacert_pem_start;
    conf.cacert_len = cacert_pem_end - cacert_pem_start;

    extern const unsigned char prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
    extern const unsigned char prvtkey_pem_end[]   asm("_binary_prvtkey_pem_end");
    conf.prvtkey_pem = prvtkey_pem_start;
    conf.prvtkey_len = prvtkey_pem_end - prvtkey_pem_start;

    esp_err_t ret = httpd_ssl_start(&server, &conf);
    if (ESP_OK != ret) {
        ESP_LOGI(TAG, "Error starting server!");
        return NULL;
    }

    // Set URI handlers
    ESP_LOGI(TAG, "Registering URI handlers");
    httpd_register_uri_handler(server, &root);
    return server;
}

static void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_ssl_stop(server);
}

static void disconnect_handler(void* arg, esp_event_base_t event_base, 
                               int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        stop_webserver(*server);
        *server = NULL;
    }
}

static void connect_handler(void* arg, esp_event_base_t event_base, 
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        *server = start_webserver();
    }
}

void uartInit() {

	uart_config_t uart_config = {
	        .baud_rate = 9600,
	        .data_bits = UART_DATA_8_BITS,
	        .parity    = UART_PARITY_DISABLE,
	        .stop_bits = UART_STOP_BITS_1,
	        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
	        .source_clk = UART_SCLK_APB,
	};

	uart_driver_install(UART_NUM_1, BUF_SIZE * 2, 0, 0, NULL, 0);
	uart_param_config(UART_NUM_1, &uart_config);
	uart_set_pin(UART_NUM_1, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS);

}


void app_main(void)
{
    static httpd_handle_t server = NULL;

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_netif_set_ip_info());

    /* Register event handlers to start server when Wi-Fi or Ethernet is connected,
     * and stop server when disconnection happens.
     */

#ifdef CONFIG_EXAMPLE_CONNECT_WIFI
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));
#endif // CONFIG_EXAMPLE_CONNECT_WIFI
#ifdef CONFIG_EXAMPLE_CONNECT_ETHERNET
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, &disconnect_handler, &server));
#endif // CONFIG_EXAMPLE_CONNECT_ETHERNET

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    //initialize uart
    uartInit();
    // Read data from the UART
	vTaskDelay(5000 / portTICK_PERIOD_MS);
	printf("find the fucking ip addresss");
	vTaskDelay(5000 / portTICK_PERIOD_MS);
	uint8_t *incomingData = (uint8_t *) malloc(BUF_SIZE);

    while (1) {
    	//Everything is all good but the data format is wrong i think....
    	uart_read_bytes(UART_NUM_1,(uint8_t *) incomingData, UART_BUF_SIZE, 20 / portTICK_RATE_MS);
    	//only add it to the list if the list size is all good :)
    	if (strlen(data) <= BUF_SIZE) {
    		strcat(data,(char *) incomingData); //add incoming data to data array.
    		strcat(data, comma);
    	}

    }

}
