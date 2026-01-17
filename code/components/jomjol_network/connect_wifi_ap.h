#pragma once

#ifndef CONNECT_WIFI_AP_H
#define CONNECT_WIFI_AP_H

#include <string>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_tls_crypto.h"
#include <esp_http_server.h>

esp_err_t wifi_init_ap(void);
void wifi_deinit_ap(void);

bool get_wifi_ap_is_connected(void);

#endif // CONNECT_WIFI_AP_H
