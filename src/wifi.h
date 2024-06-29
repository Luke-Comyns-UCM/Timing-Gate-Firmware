#ifndef WIFI_H
#define WIFI_H

#include "esp_wifi.h"

typedef void (*wifi_fn_t)(int, char*);

extern int num_connections;

void init_wifi();
void send_data(int conn_number, const char* data);
void register_recv_task(wifi_fn_t new_fn);

#endif // WIFI_H