#include "defines.h"
#include "Helper.h"

#include "connect_wifi_ap.h"

#include <string.h>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"

#include "esp_log.h"
#include "nvs_flash.h"

#include "stdio.h"
#include "time_sntp.h"

#include "ClassLogFile.h"
#include "server_help.h"

#include "statusled.h"
#include "server_ota.h"
#include "basic_auth.h"

#include "lwip/err.h"
#include "lwip/sys.h"

static const char *TAG = "WIFI AP";

esp_netif_t *my_ap_netif = NULL;
wifi_init_config_t my_ap_wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
wifi_config_t my_ap_wifi_config = {};

static bool wifi_ap_initialized = false;
static bool wifi_ap_connected = false;
static bool wifi_ap_connection_successful = false;
static int wifi_ap_reconnect_cnt = 0;

static void wifi_ap_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
    }
}

esp_err_t wifi_init_ap(void)
{
    LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Wifi AP init...");

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

    my_ap_netif = esp_netif_create_default_wifi_ap();
    assert(my_ap_netif);

    retval = esp_wifi_init(&my_ap_wifi_init_config);
    if (retval != ESP_OK)
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "esp_wifi_init: Error: " + std::to_string(retval));
        return retval;
    }

    // Register an instance of event handler to the default loop.
    retval = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_ap_event_handler, NULL, NULL);
    if (retval != ESP_OK)
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "esp_event_handler_instance_register - WIFI_ANY: Error: " + std::to_string(retval));
        return retval;
    }

    //////////////////////////////////////////////////////////////
    uint8_t ssid[32] = {};
    uint8_t password[64] = {};

    strcpy((char *)ssid, ESP_WIFI_AP_SSID);
    strcpy((char *)password, ESP_WIFI_AP_PASS);

    memcpy(&my_ap_wifi_config.ap.ssid, &ssid, sizeof(ssid));
    memcpy(&my_ap_wifi_config.ap.password, &password, sizeof(password));
    //////////////////////////////////////////////////////////////

    my_ap_wifi_config.ap.channel = ESP_WIFI_AP_CHANNEL;
    my_ap_wifi_config.ap.max_connection = ESP_WIFI_AP_MAX_STA_CONN;
    my_ap_wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

    if (strlen(ESP_WIFI_AP_PASS) == 0)
    {
        my_ap_wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    retval = esp_wifi_set_mode(WIFI_MODE_AP);
    if (retval != ESP_OK)
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "esp_wifi_set_mode: Error: " + std::to_string(retval));
        return retval;
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &my_ap_wifi_config));
    retval = esp_wifi_set_config(WIFI_IF_AP, &my_ap_wifi_config);
    if (retval != ESP_OK)
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "esp_wifi_set_config: Error: " + std::to_string(retval));
        return retval;
    }

    retval = esp_wifi_start();
    if (retval != ESP_OK)
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "esp_wifi_start: Error: " + std::to_string(retval));
        return retval;
    }

    ESP_LOGI(TAG, "started with SSID \"%s\", password: \"%s\", channel: %d. Connect to AP and open http://192.168.4.1", ESP_WIFI_AP_SSID, ESP_WIFI_AP_PASS, ESP_WIFI_AP_CHANNEL);

    return retval;
}

void wifi_deinit_ap(void)
{
    LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Wifi AP deinit...");

    wifi_ap_initialized = false;

    ESP_LOGD(TAG, "esp_wifi_disconnect()");
    ESP_ERROR_CHECK(esp_wifi_disconnect());

    ESP_LOGD(TAG, "esp_wifi_stop()");
    ESP_ERROR_CHECK(esp_wifi_stop());

    ESP_LOGD(TAG, "esp_netif_destroy(my_ap_netif)");
    esp_netif_destroy(my_ap_netif);

    ESP_LOGD(TAG, "esp_netif_deinit()");
    ESP_ERROR_CHECK(esp_netif_deinit());

    ESP_LOGD(TAG, "esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_ap_event_handler)");
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_ap_event_handler));

    ESP_LOGD(TAG, "esp_wifi_deinit()");
    ESP_ERROR_CHECK(esp_wifi_deinit());

    LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Wifi AP deinit done");
}

bool get_wifi_ap_is_connected(void)
{
    return wifi_ap_connected;
}
