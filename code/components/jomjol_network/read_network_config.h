#pragma once

#ifndef READ_NETWORK_CONFIG_H
#define READ_NETWORK_CONFIG_H

#include <string>
#include <esp_err.h>

#include "defines.h"

struct network_config_t
{
    std::string connection_type = "";

    std::string ssid = "";
    std::string eapid = "";
    std::string username = "";
    std::string password = "";
    std::string hostname = "watermeter"; // Default: watermeter
    std::string ipaddress = "";
    std::string gateway = "";
    std::string netmask = "";
    std::string dns = "";
    int rssi_threshold = 0; // Default: 0 -> ROAMING disabled

    bool fix_ipaddress_used = false;

    bool http_auth = false;
    std::string http_username = "";
    std::string http_password = "";
};
extern struct network_config_t network_config;
extern struct network_config_t network_config_temp;

#define NETWORK_CONNECTION_WIFI_AP_SETUP "Wifi_AP_Setup"
#define NETWORK_CONNECTION_WIFI_AP "Wifi_AP"
#define NETWORK_CONNECTION_WIFI_STA "Wifi_STA"
#if (CONFIG_ETH_ENABLED && CONFIG_ETH_USE_SPI_ETHERNET && CONFIG_ETH_SPI_ETHERNET_W5500)
#define NETWORK_CONNECTION_ETH "Ethernet"
#endif // (CONFIG_ETH_ENABLED && CONFIG_ETH_USE_SPI_ETHERNET && CONFIG_ETH_SPI_ETHERNET_W5500)
#define NETWORK_CONNECTION_DISCONNECT "Disconnect"

int LoadNetworkFromFile(std::string filename);

bool ChangeHostName(std::string filename, std::string _newhostname);
bool ChangeRSSIThreshold(std::string filename, int _newrssithreshold);
esp_err_t GetAuthWebUIConfig(std::string filename);

#endif // READ_NETWORK_CONFIG_H
