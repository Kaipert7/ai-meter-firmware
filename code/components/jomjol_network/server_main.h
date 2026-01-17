#pragma once

#ifndef SERVER_MAIN_H
#define SERVER_MAIN_H

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
#include "server_GPIO.h"

#include <esp_http_server.h>

httpd_handle_t start_webserver(void);
void stop_webserver(httpd_handle_t server);

void webserver_register_uri(httpd_handle_t server, const char *base_path);

#endif // SERVER_MAIN_H
