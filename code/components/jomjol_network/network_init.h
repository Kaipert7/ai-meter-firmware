#pragma once

#ifndef NETWORK_INIT_H
#define NETWORK_INIT_H

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_eth.h"

#include <esp_http_server.h>

// extern httpd_handle_t my_httpd_server;

esp_err_t init_network(void);

esp_err_t init_webserver(void);
esp_err_t init_remote_server(void);

#endif // NETWORK_INIT_H
