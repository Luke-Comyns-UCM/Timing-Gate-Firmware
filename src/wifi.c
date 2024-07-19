/*  WiFi softAP Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#ifndef CONFIG_FREERTOS_HZ
#include "build/config/sdkconfig.h"
#endif

#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "wifi.h"

// You can change the ESP IP address here.
// const esp_netif_ip_info_t _g_esp_netif_soft_ap_ip = {
//         .ip = { .addr = ESP_IP4TOADDR( 192, 168, 4, 2 ) },
//         .gw = { .addr = ESP_IP4TOADDR( 192, 168, 4, 2) },
//         .netmask = { .addr = ESP_IP4TOADDR( 255, 255, 255, 0) },
// };


/* 
   ESP is at 192.168.4.1
*/
#define ESP_WIFI_SSID      "UCM Timing System"
#define ESP_WIFI_PASS      "uoafsae47"
#define ESP_WIFI_CHANNEL   1
#define MAX_STA_CONN       4

#define MAX_DATA_LEN        4092
#define HEADER_LEN          4

int num_connections = 0;
// Expose an analogous value of connections rather than the actual number.
// This ensures the user cannot change the number of connections as used
// in this file, and num_connections is reset every time a connection is
// made or closed.
static int connections = 0;

static int sockets[MAX_STA_CONN];

static const char *TAG = "wifi softAP";

static void default_server_response(int sock, char* rcvd);

static wifi_fn_t rcvd_fn = &default_server_response;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d, reason=%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    }
}

void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = ESP_WIFI_SSID,
            .ssid_len = strlen(ESP_WIFI_SSID),
            .channel = ESP_WIFI_CHANNEL,
            .password = ESP_WIFI_PASS,
            .max_connection = MAX_STA_CONN,
#ifdef CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
            .authmode = WIFI_AUTH_WPA3_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
#else // CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
            .authmode = WIFI_AUTH_WPA2_PSK,
#endif // CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
            .pmf_cfg = {
                    .required = true,
            },
        },
    };
    if (strlen(ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_set_ps(WIFI_PS_NONE);
    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             ESP_WIFI_SSID, ESP_WIFI_PASS, ESP_WIFI_CHANNEL);
}

static unsigned int create_packet(char* dest, const char* data, unsigned short n_additional_packets)
{
    unsigned int packet_l;
    if (n_additional_packets == 0) {
        packet_l = strlen(data) + HEADER_LEN;
        if (packet_l > MAX_DATA_LEN) {
            return 0;
        }
    } else {
        packet_l = MAX_DATA_LEN + HEADER_LEN;
    }
    unsigned char header_1[2] = {packet_l >> 4, ((packet_l & 0b1111) << 4) + n_additional_packets};
    unsigned char header_2[2] = {0,0};
    dest[0] = header_1[0];
    dest[1] = header_1[1];
    dest[2] = header_2[0];
    dest[3] = header_2[1];
    strncpy(dest + HEADER_LEN, data, packet_l - HEADER_LEN);
    dest[packet_l] = 0;
    return packet_l;
}

void send_data(int conn_number, const char* data)
{
    if (conn_number >= connections) {
        // Invalid socket
        return;
    }
    static char packet[MAX_DATA_LEN + HEADER_LEN + 1] = {0};
    unsigned short n_packets;
    long data_l = strlen(data);
    if (data_l > MAX_DATA_LEN) {
        // Split into multiple packets
        n_packets = (data_l / MAX_DATA_LEN) + (data_l % MAX_DATA_LEN);
    } else {
        n_packets = 1;
    }
    unsigned long prev = 0;
    unsigned short i = 1;
    do {
        data_l = create_packet(packet, data + prev, n_packets - i);
        prev += data_l - 4;
        send(sockets[conn_number], packet, data_l, 0);
    } while (i < n_packets);
}

static void default_server_response(int sock,char* rcvd)
{
    ESP_LOGI(TAG, "Received: %s", rcvd);
    char* response = "Data received successfully";
    send_data(response, sock);
}

static void socket_cleanup(int sock_num)
{
    // sock_num is the INDEX where the socket is within sockets
    int next_sock;
    close(sockets[sock_num]);
    sockets[sock_num] = 0;
    for (int num = sock_num + 1; num < MAX_STA_CONN; num++) {
        next_sock = sockets[num];
        sockets[num - 1] = next_sock;
    }
    sockets[--connections] = 0;
    num_connections = connections;
}

static void tcp_server_task(void *pvParameters) {
    char rx_buffer[MAX_DATA_LEN + HEADER_LEN + 1];
    // char addr_str[128];
    int addr_family = (int)pvParameters;
    int ip_protocol = 0;
    struct sockaddr_in dest_addr;

    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(80);
    ip_protocol = IPPROTO_IP;
    addr_family = AF_INET;

    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }
    err = listen(listen_sock, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }
    while (1) {
        ESP_LOGI(TAG, "Socket listening");
        struct sockaddr_in6 source_addr;
        unsigned int addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }
        // inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
        // TODO: Move the conn_number stuff to an interrupt?
        int conn_number = connections++;
        num_connections = connections;
        sockets[conn_number] = sock;
        while (1) {
            int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            if (len < 0) {
                ESP_LOGE(TAG, "recv failed: errno %d", errno);
                break;
            } else if (len == 0) {
                break;
            } else {
                rx_buffer[len] = 0;
                rcvd_fn(conn_number, rx_buffer + HEADER_LEN);
            }
        }
        socket_cleanup(sock);
    }
    close(listen_sock);
    vTaskDelete(NULL);
}

void register_recv_task(wifi_fn_t new_fn)
{
    rcvd_fn = new_fn;
}

void init_wifi()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_softap();
    // Wifi functionality is not pinned to any specific core to improve performance
    xTaskCreate(tcp_server_task, "TCP Server", 8192, (void*)AF_INET, 5, NULL);
}