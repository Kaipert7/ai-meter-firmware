#pragma once

#ifndef SERVER_OTA_H
#define SERVER_OTA_H

#include "defines.h"

#include <string>
#include <esp_log.h>
#include <esp_http_server.h>

void CheckOTAUpdateStatus(void);
bool CheckOTAUpdateAvailability(void);

void doReboot(void);
void doRebootOTA(void);
void hard_restart(void);

void ota_register_uri(httpd_handle_t server);

#endif // SERVEROTA_H
