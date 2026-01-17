#include "defines.h"
#include "Helper.h"

#include "connect_wifi_sta.h"

#include <string.h>
#include <stdlib.h>
#include <fstream>
#include <vector>
#include <sstream>
#include <iostream>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_eap_client.h"
#include "esp_wnm.h"
#include "esp_rrm.h"
#include "esp_mbo.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include <netdb.h>
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include <esp_netif_sntp.h>
#include "time_sntp.h"

#include "interface_mqtt.h"

#include "ClassLogFile.h"
#include "connect_roaming.h"
#include "read_network_config.h"
#include "statusled.h"

#include "../include/mdns.h"

static const char *TAG = "WIFI STA";

esp_netif_t *my_sta_netif = NULL;
wifi_init_config_t my_sta_wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
wifi_config_t my_sta_wifi_config = {};

static bool wifi_sta_initialized = false;
static bool wifi_sta_connected = false;
static bool wifi_sta_connection_successful = false;
static int wifi_sta_reconnect_cnt = 0;

static void wifi_sta_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        LogFile.WriteToFile(ESP_LOG_INFO, TAG, "WIFI Started");
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_STOP)
    {
        LogFile.WriteToFile(ESP_LOG_INFO, TAG, "WIFI Stopped");
        wifi_sta_connected = false;
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
    {
        LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Connected to: " + network_config.ssid + ", RSSI: " + std::to_string(get_wifi_rssi()));

#ifdef WLAN_USE_MESH_ROAMING
        printRoamingFeatureSupport();

#ifdef WLAN_USE_MESH_ROAMING_ACTIVATE_CLIENT_TRIGGERED_QUERIES
        // Avoid client triggered query during processing flow (reduce risk of heap shortage).
        // Request will be triggered at the end of every round anyway
        // wifi_roaming_query();
#endif // WLAN_USE_MESH_ROAMING_ACTIVATE_CLIENT_TRIGGERED_QUERIES

#endif // WLAN_USE_MESH_ROAMING

        wifi_sta_connected = true;
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        /* Disconnect reason:
         * https://github.com/espressif/esp-idf/blob/d825753387c1a64463779bbd2369e177e5d59a79/components/esp_wifi/include/esp_wifi_types.h
         */
        wifi_event_sta_disconnected_t *disconn = (wifi_event_sta_disconnected_t *)event_data;
        if (disconn->reason == WIFI_REASON_ROAMING)
        {
            LogFile.WriteToFile(ESP_LOG_WARN, TAG, "Disconnected (" + std::to_string(disconn->reason) + ", Roaming 802.11kv)");
            // --> no reconnect neccessary, it should automatically reconnect to new AP
        }
        else
        {
            if (disconn->reason == WIFI_REASON_NO_AP_FOUND)
            {
                LogFile.WriteToFile(ESP_LOG_WARN, TAG, "Disconnected (" + std::to_string(disconn->reason) + ", No AP)");
                set_status_led(WLAN_CONN, 1, false);
            }
            else if (disconn->reason == WIFI_REASON_AUTH_EXPIRE || disconn->reason == WIFI_REASON_AUTH_FAIL || disconn->reason == WIFI_REASON_NOT_AUTHED || disconn->reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
                     disconn->reason == WIFI_REASON_HANDSHAKE_TIMEOUT)
            {
                LogFile.WriteToFile(ESP_LOG_WARN, TAG, "Disconnected (" + std::to_string(disconn->reason) + ", Auth fail)");
                set_status_led(WLAN_CONN, 2, false);
            }
            else if (disconn->reason == WIFI_REASON_BEACON_TIMEOUT)
            {
                LogFile.WriteToFile(ESP_LOG_WARN, TAG, "Disconnected (" + std::to_string(disconn->reason) + ", Timeout)");
                set_status_led(WLAN_CONN, 3, false);
            }
            else
            {
                LogFile.WriteToFile(ESP_LOG_WARN, TAG, "Disconnected (" + std::to_string(disconn->reason) + ")");
                set_status_led(WLAN_CONN, 4, false);
            }
            wifi_sta_reconnect_cnt++;
            esp_wifi_connect(); // Try to connect again
        }

        if (wifi_sta_reconnect_cnt >= 10)
        {
            wifi_sta_reconnect_cnt = 0;
            LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Disconnected, multiple reconnect attempts failed (" + std::to_string(disconn->reason) + "), retrying after 5s");
            vTaskDelay(5000 / portTICK_PERIOD_MS); // Delay between the reconnections
        }

        wifi_sta_connected = false;
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        wifi_sta_connection_successful = true;
        wifi_sta_connected = true;
        wifi_sta_reconnect_cnt = 0;

        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

        network_config.ipaddress = std::string(ip4addr_ntoa((const ip4_addr *)&event->ip_info.ip));
        LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Assigned IP: " + network_config.ipaddress);

        network_config.netmask = std::string(ip4addr_ntoa((const ip4_addr *)&event->ip_info.netmask));
        network_config.gateway = std::string(ip4addr_ntoa((const ip4_addr *)&event->ip_info.gw));

        esp_netif_dns_info_t dnsInfo;
        ESP_ERROR_CHECK(esp_netif_get_dns_info(event->esp_netif, ESP_NETIF_DNS_MAIN, &dnsInfo));
        network_config.dns = std::string(ip4addr_ntoa((const ip4_addr *)&dnsInfo.ip));

        LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Assigned IP: " + network_config.ipaddress + ", Subnet: " + network_config.netmask + ", Gateway: " + network_config.gateway + ", DNS: " + network_config.dns);

        if (getMQTTisEnabled())
        {
            vTaskDelay(500 / portTICK_PERIOD_MS);
            MQTT_Init(); // Init when WIFI is getting connected
        }
    }
}

esp_err_t wifi_init_sta(void)
{
    LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Wifi STA init...");

    // Set log level for wifi component to WARN level (default: INFO; only relevant for serial console)
    // ********************************************
    esp_log_level_set("wifi", ESP_LOG_WARN);

    esp_err_t retval = esp_netif_init();
    if (retval != ESP_OK)
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "esp_netif_init: Error: " + std::to_string(retval));
        return retval;
    }

    retval = esp_event_loop_create_default();
    if (retval != ESP_OK)
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "esp_event_loop_create_default: Error: " + std::to_string(retval));
        return retval;
    }

    my_sta_netif = esp_netif_create_default_wifi_sta();
    assert(my_sta_netif);

    if (!network_config.ipaddress.empty() && !network_config.gateway.empty() && !network_config.netmask.empty())
    {
        LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Manual interface config -> IP: " + network_config.ipaddress + ", Gateway: " + std::string(network_config.gateway) + ", Netmask: " + std::string(network_config.netmask));
        esp_netif_dhcpc_stop(my_sta_netif); // Stop DHCP service

        esp_netif_ip_info_t ip_info;
        int a, b, c, d;
        string_to_ip4(network_config.ipaddress.c_str(), a, b, c, d);
        IP4_ADDR(&ip_info.ip, a, b, c, d); // Set static IP address

        string_to_ip4(network_config.gateway.c_str(), a, b, c, d);
        IP4_ADDR(&ip_info.gw, a, b, c, d); // Set gateway

        string_to_ip4(network_config.netmask.c_str(), a, b, c, d);
        IP4_ADDR(&ip_info.netmask, a, b, c, d); // Set netmask

        esp_netif_set_ip_info(my_sta_netif, &ip_info); // Set static IP configuration

        if (network_config.dns.empty())
        {
            LogFile.WriteToFile(ESP_LOG_INFO, TAG, "No DNS server, use gateway");
            network_config.dns = network_config.gateway;
        }
        else
        {
            LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Manual interface config -> DNS: " + network_config.dns);
        }

        esp_netif_dns_info_t dns_info;
        ip4_addr_t ip;
        ip.addr = esp_ip4addr_aton(network_config.dns.c_str());
        ip_addr_set_ip4_u32(&dns_info.ip, ip.addr);

        retval = esp_netif_set_dns_info(my_sta_netif, ESP_NETIF_DNS_MAIN, &dns_info);
        if (retval != ESP_OK)
        {
            LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "esp_netif_set_dns_info: Error: " + std::to_string(retval));
            return retval;
        }
    }
    else
    {
        LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Automatic interface config --> Use DHCP service");
    }

    retval = esp_wifi_init(&my_sta_wifi_init_config);
    if (retval != ESP_OK)
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "esp_wifi_init: Error: " + std::to_string(retval));
        return retval;
    }

    // Register an instance of event handler to the default loop.
    retval = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_sta_event_handler, NULL, NULL);
    if (retval != ESP_OK)
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "esp_event_handler_instance_register - WIFI_ANY: Error: " + std::to_string(retval));
        return retval;
    }

    retval = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_sta_event_handler, NULL, NULL);
    if (retval != ESP_OK)
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "esp_event_handler_instance_register - GOT_IP: Error: " + std::to_string(retval));
        return retval;
    }

#ifdef WLAN_USE_MESH_ROAMING
    // Register an instance of event handler to the default loop.
    retval = esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_BSS_RSSI_LOW, &esp_bss_rssi_low_handler, NULL, NULL);
    if (retval != ESP_OK)
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "esp_event_handler_instance_register - BSS_RSSI_LOW: Error: " + std::to_string(retval));
        return retval;
    }
#endif

    // ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    my_sta_wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;     // Scan all channels instead of stopping after first match
    my_sta_wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL; // Sort by signal strength and keep up to 4 best APs
    my_sta_wifi_config.sta.failure_retry_cnt = 3;                   // IDF version 5.0 will support this

#ifdef WLAN_USE_MESH_ROAMING
    my_sta_wifi_config.sta.rm_enabled = 1;      // 802.11k (Radio Resource Management)
    my_sta_wifi_config.sta.btm_enabled = 1;     // 802.11v (BSS Transition Management)
    my_sta_wifi_config.sta.mbo_enabled = 1;     // Multiband Operation (better use of Wi-Fi network resources in roaming decisions) -> not activated to save heap
    my_sta_wifi_config.sta.pmf_cfg.capable = 1; // 802.11w (Protected Management Frame, activated by default if other device also advertizes PMF capability)
    my_sta_wifi_config.sta.ft_enabled = 1;      // 802.11r (BSS Fast Transition) -> Upcoming IDF version 5.0 will support 11r
#endif

    if (!network_config.username.empty())
    {
        strcpy((char *)my_sta_wifi_config.sta.ssid, (const char *)network_config.ssid.c_str());
    }
    else
    {
        strcpy((char *)my_sta_wifi_config.sta.ssid, (const char *)network_config.ssid.c_str());
        strcpy((char *)my_sta_wifi_config.sta.password, (const char *)network_config.password.c_str());
    }

    retval = esp_wifi_set_mode(WIFI_MODE_STA);
    if (retval != ESP_OK)
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "esp_wifi_set_mode: Error: " + std::to_string(retval));
        return retval;
    }

    retval = esp_wifi_set_config(WIFI_IF_STA, &my_sta_wifi_config);
    if (retval != ESP_OK)
    {
        if (retval == ESP_ERR_WIFI_PASSWORD)
        {
            LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "esp_wifi_set_config: SSID password invalid! Error: " + std::to_string(retval));
        }
        else
        {
            LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "esp_wifi_set_config: Error: " + std::to_string(retval));
        }
        return retval;
    }

    if ((!network_config.username.empty()) && (!network_config.eapid.empty()) && (!network_config.password.empty()))
    {
        retval = esp_eap_client_set_identity((const unsigned char *)network_config.eapid.c_str(), (int)strlen(network_config.eapid.c_str()));
        if (retval != ESP_OK)
        {
            if (retval == ESP_ERR_INVALID_ARG)
            {
                LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "esp_eap_client_set_identity: Invalid argument (len <= 0 or len >= 128)! Error: " + std::to_string(retval));
            }
            else
            {
                LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "esp_eap_client_set_identity: Error: " + std::to_string(retval));
            }
            return retval;
        }

        retval = esp_eap_client_set_username((const unsigned char *)network_config.username.c_str(), (int)strlen(network_config.username.c_str()));
        if (retval != ESP_OK)
        {
            if (retval == ESP_ERR_INVALID_ARG)
            {
                LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "esp_eap_client_set_username: Invalid argument (len <= 0 or len >= 128)! Error: " + std::to_string(retval));
            }
            else
            {
                LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "esp_eap_client_set_username: Error: " + std::to_string(retval));
            }
            return retval;
        }

        retval = esp_eap_client_set_password((const unsigned char *)network_config.password.c_str(), (int)strlen(network_config.password.c_str()));
        if (retval != ESP_OK)
        {
            if (retval == ESP_ERR_INVALID_ARG)
            {
                LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "esp_eap_client_set_password: Invalid argument (len <= 0)! Error: " + std::to_string(retval));
            }
            else
            {
                LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "esp_eap_client_set_password: Error: " + std::to_string(retval));
            }
            return retval;
        }

#ifdef WIFI_USE_DEFAULT_CERT_BUNDLE
        retval = esp_eap_client_use_default_cert_bundle(true);
        if (retval != ESP_OK)
        {
            LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "esp_eap_client_use_default_cert_bundle: Error: " + std::to_string(retval));
            return retval;
        }
#endif

        retval = esp_wifi_sta_enterprise_enable();
        if (retval != ESP_OK)
        {
            LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "esp_wifi_sta_enterprise_enable: Error: " + std::to_string(retval));
            return retval;
        }
    }

    retval = esp_wifi_start();
    if (retval != ESP_OK)
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "esp_wifi_start: Error: " + std::to_string(retval));
        return retval;
    }

    if (!network_config.hostname.empty())
    {
        retval = esp_netif_set_hostname(my_sta_netif, network_config.hostname.c_str());
        if (retval != ESP_OK)
        {
            LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Failed to set hostname! Error: " + std::to_string(retval));
        }
        else
        {
            LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Set hostname to: " + network_config.hostname);
        }
        // initialize mDNS service
        retval = mdns_init();
        if (retval != ESP_OK)
        {
            LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "mdns_init failed! Error: " + std::to_string(retval));
        }
        else
        {
            // set mdns hostname
            mdns_hostname_set(network_config.hostname.c_str());
        }
    }

    wifi_sta_initialized = true;

    LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Init successful");
    return ESP_OK;
}

void wifi_deinit_sta(void)
{
    LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Wifi STA deinit...");

    wifi_sta_initialized = false;

    ESP_LOGD(TAG, "esp_wifi_disconnect()");
    ESP_ERROR_CHECK(esp_wifi_disconnect());

    ESP_LOGD(TAG, "esp_wifi_stop()");
    ESP_ERROR_CHECK(esp_wifi_stop());

    ESP_LOGD(TAG, "esp_netif_destroy(my_sta_netif)");
    esp_netif_destroy(my_sta_netif);

    ESP_LOGD(TAG, "esp_netif_deinit()");
    ESP_ERROR_CHECK(esp_netif_deinit());

    ESP_LOGD(TAG, "esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler)");
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_sta_event_handler));

    ESP_LOGD(TAG, "esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler)");
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_sta_event_handler));

#ifdef WLAN_USE_MESH_ROAMING
    ESP_LOGD(TAG, "esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_BSS_RSSI_LOW, esp_bss_rssi_low_handler)");
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_BSS_RSSI_LOW, esp_bss_rssi_low_handler));
#endif

    ESP_LOGD(TAG, "esp_wifi_deinit()");
    ESP_ERROR_CHECK(esp_wifi_deinit());

    LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Wifi STA deinit done");
}

std::string wifi_scan_ap(void)
{
    std::string data = "";
    wifi_scan_config_t my_wifi_scan_config = {};
    memset(&my_wifi_scan_config, 0, sizeof(my_wifi_scan_config));

    my_wifi_scan_config.show_hidden = true; // scan also hidden SSIDs
    my_wifi_scan_config.channel = 0;        // scan all channels

    esp_wifi_scan_start(&my_wifi_scan_config, true);

    uint16_t max_number_of_ap_found = 10;
    esp_wifi_scan_get_ap_num(&max_number_of_ap_found);
    wifi_ap_record_t *wifi_ap_records = new wifi_ap_record_t[max_number_of_ap_found];

    if (wifi_ap_records == NULL)
    {
        esp_wifi_scan_get_ap_records(0, NULL); // free internal heap
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "wifi_scan: Failed to allocate heap for wifi_ap_records");
        data = data + "\"wifi_scan\"" + string(": {");
        data = data + "}";
        return data;
    }
    else
    {
        if (esp_wifi_scan_get_ap_records(&max_number_of_ap_found, wifi_ap_records) != ESP_OK)
        {
            // Retrieve results (and free internal heap)
            LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "wifi_scan: esp_wifi_scan_get_ap_records: Error retrieving datasets");
            delete[] wifi_ap_records;
            data = data + "\"wifi_scan\"" + string(": {");
            data = data + "}";
            return data;
        }
    }

    wifi_ap_record_t currentAP;
    esp_wifi_sta_get_ap_info(&currentAP);

    for (int i = 0; i < max_number_of_ap_found; i++)
    {
        data = data + "\"wifi" + std::to_string(i) + string("\": {");

        data = data + "\"SSID\": \"" + std::string((char *)wifi_ap_records[i].ssid) + "\", " + "\"BSSID\": \"" + bssid_to_string((char *)wifi_ap_records[i].bssid) + "\", " + "\"RSSI\": \"" + std::to_string(wifi_ap_records[i].rssi) + "\", " + "\"CH\": \"" +
               std::to_string(wifi_ap_records[i].primary) + "\", " + "\"AUTH\": \"" + get_auth_mode_name(wifi_ap_records[i].authmode) + "\"";

        if (i == max_number_of_ap_found - 1)
        {
            data = data + "}";
        }
        else
        {
            data = data + "},\n";
        }
    }

    delete[] wifi_ap_records;
    return data;
}

bool get_wifi_sta_is_connected(void)
{
    return wifi_sta_connected;
}

int get_wifi_rssi(void)
{
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK)
    {
        return ap.rssi;
    }
    else
    {
        return -127; // Return -127 if no info available e.g. not connected
    }
}
