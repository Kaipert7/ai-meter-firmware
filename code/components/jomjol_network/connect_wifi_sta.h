#pragma once

#ifndef CONNECT_WIFI_STA_H
#define CONNECT_WIFI_STA_H

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

int wifi_init_sta(void);
void wifi_deinit_sta(void);

bool get_wifi_sta_is_connected(void);

int get_wifi_rssi(void);

std::string wifi_scan_ap(void);

#endif // CONNECT_WIFI_STA_H
