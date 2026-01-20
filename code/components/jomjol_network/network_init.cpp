#include "defines.h"
#include "Helper.h"

#include "network_init.h"

#include <string>
#include <stdio.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <esp_netif.h>
#include <esp_eth.h>
#include <netdb.h>
#include <esp_http_server.h>

#include "time_sntp.h"

#include "server_main.h"
#include "server_remote.h"
#include "server_file.h"
#include "server_ota.h"
#include "server_camera.h"
#include "server_mqtt.h"

#include "read_network_config.h"

#include "connect_wifi_ap.h"
#include "connect_wifi_sta.h"
#if (CONFIG_ETH_ENABLED && CONFIG_ETH_USE_SPI_ETHERNET && CONFIG_ETH_SPI_ETHERNET_W5500)
#include "connect_eth.h"
#endif

#include "MainFlowControl.h"
#include "ClassLogFile.h"

#include "basic_auth.h"

#include "statusled.h"

static const char *TAG = "NETWORK INIT";

// httpd_handle_t my_httpd_server = NULL;

esp_err_t init_network(void)
{
    esp_err_t retVal = ESP_OK;
    TickType_t xDelay = 500 / portTICK_PERIOD_MS;

    // network_config.connection_type = NETWORK_CONNECTION_WIFI_AP_SETUP;
    // network_config.connection_type = NETWORK_CONNECTION_WIFI_AP;
    network_config.connection_type = NETWORK_CONNECTION_WIFI_STA;
    // network_config.connection_type = NETWORK_CONNECTION_DISCONNECT;

    // Read Network parameter and start it
    // ********************************************
    int iNetworkStatus = -1;
    if (file_exists(WLAN_CONFIG_FILE))
    {
        iNetworkStatus = LoadNetworkFromFile(WLAN_CONFIG_FILE);
    }
    else
    {
        iNetworkStatus = LoadNetworkFromFile(NETWORK_CONFIG_FILE);
    }

    // Network config available (0) or SSID/password not configured (-2)
    if (file_exists(CONFIG_FILE) && ((iNetworkStatus == 0) || (iNetworkStatus == -2)))
    {
        LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Network config loaded, init Network...");

        if (network_config.connection_type == NETWORK_CONNECTION_WIFI_AP_SETUP)
        {
            retVal = wifi_init_ap();
            if (retVal != ESP_OK)
            {
                LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Wifi AP init failed. Device init aborted!");
                set_status_led(WLAN_INIT, 3, true);
                return retVal;
            }

            vTaskDelay(xDelay);

            retVal = init_remote_server();
            if (retVal != ESP_OK)
            {
                LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Remote Server init failed. Device init aborted!");
                set_status_led(WLAN_INIT, 3, true);
                return retVal;
            }

            LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Wifi AP Setup Initialized");
        }
        else if (network_config.connection_type == NETWORK_CONNECTION_WIFI_AP)
        {
            retVal = wifi_init_ap();
            if (retVal != ESP_OK)
            {
                LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Wifi AP init failed. Device init aborted!");
                set_status_led(WLAN_INIT, 3, true);
                return retVal;
            }

            vTaskDelay(xDelay);

            retVal = init_webserver();
            if (retVal != ESP_OK)
            {
                LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Wifi AP Webserver init failed. Device init aborted!");
                set_status_led(WLAN_INIT, 3, true);
                return retVal;
            }

            LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Wifi AP Webserver Initialized");
        }
        else if (network_config.connection_type == NETWORK_CONNECTION_WIFI_STA)
        {
            if (iNetworkStatus == -2)
            {
                network_config.connection_type = NETWORK_CONNECTION_WIFI_AP_SETUP;

                LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "SSID or password not configured!");
                set_status_led(WLAN_INIT, 2, true);

                retVal = wifi_init_ap();
                if (retVal != ESP_OK)
                {
                    LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Wifi AP init failed. Device init aborted!");
                    set_status_led(WLAN_INIT, 3, true);
                    return retVal;
                }

                vTaskDelay(xDelay);

                retVal = init_remote_server();
                if (retVal != ESP_OK)
                {
                    LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Remote Server init failed. Device init aborted!");
                    set_status_led(WLAN_INIT, 3, true);
                    return retVal;
                }

                LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Wifi AP Setup Initialized");
            }
            else
            {
                retVal = wifi_init_sta();
                if (retVal != ESP_OK)
                {
                    LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Wifi STA init failed. Device init aborted!");
                    set_status_led(WLAN_INIT, 3, true);
                    return retVal;
                }

                vTaskDelay(xDelay);

                retVal = init_webserver();
                if (retVal != ESP_OK)
                {
                    LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Wifi STA Webserver init failed. Device init aborted!");
                    set_status_led(WLAN_INIT, 3, true);
                    return retVal;
                }

                LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Wifi STA Webserver Initialized");
            }
        }
#if (CONFIG_ETH_ENABLED && CONFIG_ETH_USE_SPI_ETHERNET && CONFIG_ETH_SPI_ETHERNET_W5500)
        else if (network_config.connection_type == NETWORK_CONNECTION_ETH)
        {
            retVal = eth_init_W5500();
            if (retVal != ESP_OK)
            {
                LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Ethernet init failed. Device init aborted!");
                set_status_led(WLAN_INIT, 3, true);
                return retVal;
            }

            vTaskDelay(xDelay);

            retVal = init_webserver();
            if (retVal != ESP_OK)
            {
                LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Ethernet Webserver init failed. Device init aborted!");
                set_status_led(WLAN_INIT, 3, true);
                return retVal;
            }

            LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Ethernet Webserver Initialized");
        }
#endif // (CONFIG_ETH_ENABLED && CONFIG_ETH_USE_SPI_ETHERNET && CONFIG_ETH_SPI_ETHERNET_W5500)
        else if (network_config.connection_type == NETWORK_CONNECTION_DISCONNECT)
        {
            // esp_wifi_deinit();
            // esp_wifi_set_mode(WIFI_MODE_NULL);
            // esp_wifi_stop();
            return ESP_OK;
        }
    }
    // wlan.ini not available (-1) and config.ini not available
    else
    {
        network_config.connection_type = NETWORK_CONNECTION_WIFI_AP_SETUP;

        retVal = wifi_init_ap();
        if (retVal != ESP_OK)
        {
            LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Wifi AP init failed. Device init aborted!");
            set_status_led(WLAN_INIT, 3, true);
            return retVal;
        }

        vTaskDelay(xDelay);

        retVal = init_remote_server();
        if (retVal != ESP_OK)
        {
            LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Remote Server init failed. Device init aborted!");
            set_status_led(WLAN_INIT, 3, true);
            return retVal;
        }

        LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Wifi AP Setup Initialized");
    }

    init_basic_auth();

    ESP_LOGD(TAG, "main: sleep for: %ldms", (long)xDelay * CONFIG_FREERTOS_HZ / portTICK_PERIOD_MS);
    vTaskDelay(xDelay);

    return retVal;
}

esp_err_t init_webserver(void)
{
    // Start webserver + register handler
    // ********************************************
    ESP_LOGD(TAG, "starting servers");
    // my_httpd_server = start_webserver();
    httpd_handle_t my_httpd_server = start_webserver();
    if (my_httpd_server == NULL)
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "start webserver failed");
        return ESP_FAIL;
    }

    camera_register_uri(my_httpd_server);
    main_flow_register_uri(my_httpd_server);
    file_server_register_uri(my_httpd_server, "/sdcard");
    ota_register_uri(my_httpd_server);
    mqtt_register_uri(my_httpd_server);

    gpio_handler_create(my_httpd_server);

    ESP_LOGD(TAG, "Before reg main server uri");
    webserver_register_uri(my_httpd_server, "/sdcard");

    return ESP_OK;
}

esp_err_t init_remote_server(void)
{
    // Start ap server + register handler
    // ********************************************
    ESP_LOGD(TAG, "starting ap servers");
    // my_httpd_server = start_remote_webserver();
    httpd_handle_t my_httpd_server = start_webserver();
    if (my_httpd_server == NULL)
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "start remote server failed");
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Before reg ap server uri");
    remote_webserver_register_uri(my_httpd_server);

    return ESP_OK;
}
