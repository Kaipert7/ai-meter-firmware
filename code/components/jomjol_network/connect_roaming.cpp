#include "connect_roaming.h"
#include "connect_wifi_sta.h"

#include <string.h>
#include <stdlib.h>
#include <fstream>
#include <vector>
#include <sstream>
#include <iostream>

#include <driver/gpio.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_wnm.h>
#include <esp_rrm.h>
#include <esp_mbo.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <netdb.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include <lwip/err.h>
#include <lwip/sys.h>
#include <interface_mqtt.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include "defines.h"
#include "Helper.h"

#include "ClassLogFile.h"
#include "read_network_config.h"
#include "statusled.h"

#include "../include/mdns.h"

#if defined HEAP_TRACING_MAIN_WIFI
#include <esp_heap_trace.h>
#endif

static const char *TAG = "WIFI ROAMING";

static bool wifi_sta_with_better_rssi = false;

std::string get_auth_mode_name(const wifi_auth_mode_t auth_mode)
{
    std::string AuthModeNames[] = {"OPEN", "WEP", "WPA PSK", "WPA2 PSK", "WPA WPA2 PSK", "WPA2 ENTERPRISE", "WPA3 PSK", "WPA2 WPA3 PSK", "WAPI_PSK", "MAX"};
    return AuthModeNames[auth_mode];
}

#ifdef WLAN_USE_MESH_ROAMING
int rrm_ctx = 0;

static inline uint32_t WPA_GET_LE32(const uint8_t *a)
{
    return ((uint32_t)a[3] << 24) | (a[2] << 16) | (a[1] << 8) | a[0];
}

#ifndef WLAN_EID_MEASURE_REPORT
#define WLAN_EID_MEASURE_REPORT 39
#endif
#ifndef MEASURE_TYPE_LCI
#define MEASURE_TYPE_LCI 9
#endif
#ifndef MEASURE_TYPE_LOCATION_CIVIC
#define MEASURE_TYPE_LOCATION_CIVIC 11
#endif
#ifndef WLAN_EID_NEIGHBOR_REPORT
#define WLAN_EID_NEIGHBOR_REPORT 52
#endif
#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif

#define MAX_NEIGHBOR_LEN 512
static char *get_btm_neighbor_list(uint8_t *report, size_t report_len)
{
    size_t len = 0;
    const uint8_t *data;
    int ret = 0;

/*
 * Neighbor Report element (IEEE P802.11-REVmc/D5.0)
 * BSSID[6]
 * BSSID Information[4]
 * Operating Class[1]
 * Channel Number[1]
 * PHY Type[1]
 * Optional Subelements[variable]
 */
#define NR_IE_MIN_LEN (ETH_ALEN + 4 + 1 + 1 + 1)

    if (!report || report_len == 0)
    {
        ESP_LOGD(TAG, "Roaming: RRM neighbor report is not valid");
        return NULL;
    }

    char *buf = (char *)calloc(1, MAX_NEIGHBOR_LEN);
    data = report;

    while (report_len >= 2 + NR_IE_MIN_LEN)
    {
        const uint8_t *nr;
        char lci[256 * 2 + 1];
        char civic[256 * 2 + 1];
        uint8_t nr_len = data[1];
        const uint8_t *pos = data, *end;

        if (pos[0] != WLAN_EID_NEIGHBOR_REPORT ||
            nr_len < NR_IE_MIN_LEN)
        {
            ESP_LOGD(TAG, "Roaming CTRL: Invalid Neighbor Report element: id=%u len=%u",
                     data[0], nr_len);
            ret = -1;
            goto cleanup;
        }

        if (2U + nr_len > report_len)
        {
            ESP_LOGD(TAG, "Roaming CTRL: Invalid Neighbor Report element: id=%u len=%zu nr_len=%u",
                     data[0], report_len, nr_len);
            ret = -1;
            goto cleanup;
        }
        pos += 2;
        end = pos + nr_len;

        nr = pos;
        pos += NR_IE_MIN_LEN;

        lci[0] = '\0';
        civic[0] = '\0';
        while (end - pos > 2)
        {
            uint8_t s_id, s_len;

            s_id = *pos++;
            s_len = *pos++;
            if (s_len > end - pos)
            {
                ret = -1;
                goto cleanup;
            }
            if (s_id == WLAN_EID_MEASURE_REPORT && s_len > 3)
            {
                /* Measurement Token[1] */
                /* Measurement Report Mode[1] */
                /* Measurement Type[1] */
                /* Measurement Report[variable] */
                switch (pos[2])
                {
                case MEASURE_TYPE_LCI:
                    if (lci[0])
                        break;
                    memcpy(lci, pos, s_len);
                    break;
                case MEASURE_TYPE_LOCATION_CIVIC:
                    if (civic[0])
                        break;
                    memcpy(civic, pos, s_len);
                    break;
                }
            }

            pos += s_len;
        }

        ESP_LOGI(TAG, "Roaming: RMM neighbor report bssid=" MACSTR " info=0x%04lx op_class=%u chan=%u phy_type=%u%s%s%s%s",
                 MAC2STR(nr), WPA_GET_LE32(nr + ETH_ALEN),
                 nr[ETH_ALEN + 4], nr[ETH_ALEN + 5],
                 nr[ETH_ALEN + 6],
                 lci[0] ? " lci=" : "", lci,
                 civic[0] ? " civic=" : "", civic);

        LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "Roaming: RMM neighbor report BSSID: " + bssid_to_string((char *)nr) + ", Channel: " + std::to_string(nr[ETH_ALEN + 5]));

        /* neighbor start */
        len += snprintf(buf + len, MAX_NEIGHBOR_LEN - len, " neighbor=");
        /* bssid */
        len += snprintf(buf + len, MAX_NEIGHBOR_LEN - len, MACSTR, MAC2STR(nr));
        /* , */
        len += snprintf(buf + len, MAX_NEIGHBOR_LEN - len, ",");
        /* bssid info */
        len += snprintf(buf + len, MAX_NEIGHBOR_LEN - len, "0x%04lx", WPA_GET_LE32(nr + ETH_ALEN));
        len += snprintf(buf + len, MAX_NEIGHBOR_LEN - len, ",");
        /* operating class */
        len += snprintf(buf + len, MAX_NEIGHBOR_LEN - len, "%u", nr[ETH_ALEN + 4]);
        len += snprintf(buf + len, MAX_NEIGHBOR_LEN - len, ",");
        /* channel number */
        len += snprintf(buf + len, MAX_NEIGHBOR_LEN - len, "%u", nr[ETH_ALEN + 5]);
        len += snprintf(buf + len, MAX_NEIGHBOR_LEN - len, ",");
        /* phy type */
        len += snprintf(buf + len, MAX_NEIGHBOR_LEN - len, "%u", nr[ETH_ALEN + 6]);
        /* optional elements, skip */

        data = end;
        report_len -= 2 + nr_len;
    }

cleanup:
    if (ret < 0)
    {
        free(buf);
        buf = NULL;
    }
    return buf;
}

void neighbor_report_recv_cb(void *ctx, const uint8_t *report, size_t report_len)
{
    int *val = (int *)ctx;
    uint8_t *pos = (uint8_t *)report;
    int cand_list = 0;
    int ret;

    if (!report)
    {
        ESP_LOGD(TAG, "Roaming: Neighbor report is null");
        return;
    }
    if (*val != rrm_ctx)
    {
        ESP_LOGE(TAG, "Roaming: rrm_ctx value didn't match, not initiated by us");
        return;
    }
    /* dump report info */
    ESP_LOGD(TAG, "Roaming: RRM neighbor report len=%d", report_len);
    ESP_LOG_BUFFER_HEXDUMP(TAG, pos, report_len, ESP_LOG_DEBUG);

    /* create neighbor list */
    char *neighbor_list = get_btm_neighbor_list(pos + 1, report_len - 1);

    /* In case neighbor list is not present issue a scan and get the list from that */
    if (!neighbor_list)
    {
        /* issue scan */
        wifi_scan_config_t params;
        memset(&params, 0, sizeof(wifi_scan_config_t));
        if (esp_wifi_scan_start(&params, true) < 0)
        {
            goto cleanup;
        }
        /* cleanup from net802.11 */
        uint16_t number = 1;
        wifi_ap_record_t ap_records;
        esp_wifi_scan_get_ap_records(&number, &ap_records);
        cand_list = 1;
    }
    /* send AP btm query requesting to roam depending on candidate list of AP */
    // btm_query_reasons: https://github.com/espressif/esp-idf/blob/release/v4.4/components/wpa_supplicant/esp_supplicant/include/esp_wnm.h
    ret = esp_wnm_send_bss_transition_mgmt_query(REASON_FRAME_LOSS, neighbor_list, cand_list); // query reason 16 -> LOW RSSI --> (btm_query_reason)16
    ESP_LOGD(TAG, "neighbor_report_recv_cb retval - bss_transisition_query: %d", ret);

cleanup:
    if (neighbor_list)
    {
        free(neighbor_list);
    }
}

void esp_bss_rssi_low_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    int retval = -1;
    wifi_event_bss_rssi_low_t *event = (wifi_event_bss_rssi_low_t *)event_data;

    LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "Roaming Event: RSSI " + std::to_string(event->rssi) + " < RSSI_Threshold " + std::to_string(network_config.rssi_threshold));

    /* If RRM is supported, call RRM and then send BTM query to AP */
    if (esp_rrm_is_rrm_supported_connection() && esp_wnm_is_btm_supported_connection())
    {
        /* Lets check channel conditions */
        rrm_ctx++;

        retval = esp_rrm_send_neighbor_rep_request(neighbor_report_recv_cb, &rrm_ctx);
        ESP_LOGD(TAG, "esp_rrm_send_neighbor_rep_request retval: %d", retval);
        if (retval == 0)
        {
            LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "Roaming: RRM + BTM query sent");
        }
        else
        {
            ESP_LOGD(TAG, "esp_rrm_send_neighbor_rep_request retval: %d", retval);
        }
    }

    /* If RRM is not supported or RRM request failed, send directly BTM query to AP */
    if (retval < 0 && esp_wnm_is_btm_supported_connection())
    {
        // btm_query_reasons: https://github.com/espressif/esp-idf/blob/release/v4.4/components/wpa_supplicant/esp_supplicant/include/esp_wnm.h
        retval = esp_wnm_send_bss_transition_mgmt_query(REASON_FRAME_LOSS, NULL, 0); // query reason 16 -> LOW RSSI --> (btm_query_reason)16
        ESP_LOGD(TAG, "esp_wnm_send_bss_transition_mgmt_query retval: %d", retval);
        if (retval == 0)
        {
            LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "Roaming: BTM query sent");
        }
        else
        {
            ESP_LOGD(TAG, "esp_wnm_send_bss_transition_mgmt_query retval: %d", retval);
        }
    }
}

void printRoamingFeatureSupport(void)
{
    if (esp_rrm_is_rrm_supported_connection())
    {
        LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Roaming: RRM (802.11k) supported by AP");
    }
    else
    {
        LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Roaming: RRM (802.11k) NOT supported by AP");
    }

    if (esp_wnm_is_btm_supported_connection())
    {
        LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Roaming: BTM (802.11v) supported by AP");
    }
    else
    {
        LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Roaming: BTM (802.11v) NOT supported by AP");
    }
}

#ifdef WLAN_USE_MESH_ROAMING_ACTIVATE_CLIENT_TRIGGERED_QUERIES
void wifi_roaming_query(void)
{
    /* Query only if WIFI is connected and feature is supported by AP */
    if (WIFIConnected && (esp_rrm_is_rrm_supported_connection() || esp_wnm_is_btm_supported_connection()))
    {
        /* Client is allowed to send query to AP for roaming request if RSSI is lower than threshold */
        /* Note 1: Set RSSI threshold funtion needs to be called to trigger WIFI_EVENT_STA_BSS_RSSI_LOW */
        /* Note 2: Additional querys will be sent after flow round is finshed --> server_tflite.cpp - function "task_autodoFlow" */
        /* Note 3: RSSI_Threshold = 0 --> Disable client query by application (WebUI parameter) */

        if (network_config.rssi_threshold != 0 && get_WIFI_RSSI() != -127 && (get_WIFI_RSSI() < network_config.rssi_threshold))
        {
            esp_wifi_set_rssi_threshold(network_config.rssi_threshold);
        }
    }
}
#endif // WLAN_USE_MESH_ROAMING_ACTIVATE_CLIENT_TRIGGERED_QUERIES
#endif // WLAN_USE_MESH_ROAMING

#ifdef WLAN_USE_ROAMING_BY_SCANNING
void wifi_scan(void)
{
    wifi_scan_config_t wifi_scan_config;
    memset(&wifi_scan_config, 0, sizeof(wifi_scan_config));

    wifi_scan_config.ssid = (uint8_t *)network_config.ssid.c_str(); // only scan for configured SSID
    wifi_scan_config.show_hidden = true;                            // scan also hidden SSIDs
    wifi_scan_config.channel = 0;                                   // scan all channels

    esp_wifi_scan_start(&wifi_scan_config, true); // not using event handler SCAN_DONE by purpose to keep SYS_EVENT heap smaller
                                                  // and the calling task task_autodoFlow is after scan is finish in wait state anyway
                                                  // Scan duration: ca. (120ms + 30ms) * Number of channels -> ca. 1,5 - 2s

    uint16_t max_number_of_ap_found = 10;                                             // max. number of APs, value will be updated by function "esp_wifi_scan_get_ap_num"
    esp_wifi_scan_get_ap_num(&max_number_of_ap_found);                                // get actual found APs
    wifi_ap_record_t *wifi_ap_records = new wifi_ap_record_t[max_number_of_ap_found]; // Allocate necessary record datasets
    if (wifi_ap_records == NULL)
    {
        esp_wifi_scan_get_ap_records(0, NULL); // free internal heap
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "wifi_scan: Failed to allocate heap for wifi_ap_records");
        return;
    }
    else
    {
        if (esp_wifi_scan_get_ap_records(&max_number_of_ap_found, wifi_ap_records) != ESP_OK)
        {
            // Retrieve results (and free internal heap)
            LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "wifi_scan: esp_wifi_scan_get_ap_records: Error retrieving datasets");
            delete[] wifi_ap_records;
            return;
        }
    }

    wifi_ap_record_t currentAP;
    esp_wifi_sta_get_ap_info(&currentAP);

    LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "Roaming: Current AP BSSID=" + bssid_to_string((char *)currentAP.bssid));
    LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "Roaming: Scan completed, APs found with configured SSID: " + std::to_string(max_number_of_ap_found));
    for (int i = 0; i < max_number_of_ap_found; i++)
    {
        LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "Roaming: " + std::to_string(i + 1) + ": SSID=" + std::string((char *)wifi_ap_records[i].ssid) + ", BSSID=" + bssid_to_string((char *)wifi_ap_records[i].bssid) + ", RSSI=" + std::to_string(wifi_ap_records[i].rssi) + ", CH=" + std::to_string(wifi_ap_records[i].primary) + ", AUTH=" + get_auth_mode_name(wifi_ap_records[i].authmode));

        // RSSI is better than actual RSSI + 5 --> Avoid switching to AP with roughly same RSSI
        if (wifi_ap_records[i].rssi > (currentAP.rssi + 5) && (strcmp(bssid_to_string((char *)wifi_ap_records[i].bssid).c_str(), bssid_to_string((char *)currentAP.bssid).c_str()) != 0))
        {
            wifi_sta_with_better_rssi = true;
        }
    }

    delete[] wifi_ap_records;
}

void wifi_roaming_by_scanning(void)
{
#ifdef HEAP_TRACING_MAIN_WIFI
    ESP_LOGI(TAG, "---- HEAP_TRACING_MAIN_WIFI ----");
    ESP_ERROR_CHECK(heap_trace_start(HEAP_TRACE_LEAKS));
#endif

    if (network_config.rssi_threshold != 0 && get_wifi_rssi() != -127 && (get_wifi_rssi() < network_config.rssi_threshold))
    {
        LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "Roaming: Start scan of all channels for SSID " + network_config.ssid);
        wifi_scan();

        if (wifi_sta_with_better_rssi)
        {
            wifi_sta_with_better_rssi = false;
            LogFile.WriteToFile(ESP_LOG_WARN, TAG, "Roaming: AP with better RSSI in range, disconnecting to switch AP...");
            esp_wifi_disconnect();
        }
        else
        {
            LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "Roaming: Scan completed, stay on current AP");
        }
    }

#ifdef HEAP_TRACING_MAIN_WIFI
    ESP_ERROR_CHECK(heap_trace_stop());
    heap_trace_dump();
#endif
}
#endif // WLAN_USE_ROAMING_BY_SCANNING
