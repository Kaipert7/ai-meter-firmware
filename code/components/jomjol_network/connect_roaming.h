#pragma once

#ifndef CONNECT_ROAMING_H
#define CONNECT_ROAMING_H

#include "sdkconfig.h"

#include <string>

#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_wnm.h>
#include <esp_rrm.h>
#include <esp_mbo.h>
#include <esp_mac.h>
#include <esp_netif.h>

#include <esp_err.h>
#include <esp_log.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>

#include "defines.h"
#include "Helper.h"

std::string get_auth_mode_name(const wifi_auth_mode_t auth_mode);

#if (defined WLAN_USE_MESH_ROAMING)
extern void esp_bss_rssi_low_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
#endif

#if (defined WLAN_USE_MESH_ROAMING && defined WLAN_USE_MESH_ROAMING_ACTIVATE_CLIENT_TRIGGERED_QUERIES)
void wifi_roaming_query(void);
#endif

#ifdef WLAN_USE_ROAMING_BY_SCANNING
void wifi_roaming_by_scanning(void);
#endif

#endif // CONNECT_ROAMING_H
