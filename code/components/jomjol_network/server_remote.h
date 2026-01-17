#pragma once

#ifndef SERVER_REMOTE_H
#define SERVER_REMOTE_H

#include "defines.h"

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <nvs_flash.h>
#include <esp_netif.h>
#include <esp_eth.h>
#include <esp_tls_crypto.h>
#include <esp_http_server.h>

httpd_handle_t start_remote_webserver(void);
void stop_remote_webserver(httpd_handle_t server);

httpd_handle_t remote_webserver_register_uri(httpd_handle_t server);

#endif // SERVER_REMOTE_H
