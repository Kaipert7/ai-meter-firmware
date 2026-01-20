#include "defines.h"

#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <string.h>

#include "read_network_config.h"
#include "Helper.h"
#include "connect_wifi_sta.h"

#include "esp_log.h"
#include "ClassLogFile.h"

static const char *TAG = "NETWORK CONFIG";

struct network_config_t network_config = {};
struct network_config_t network_config_temp = {};

int LoadNetworkFromFile(std::string filename)
{
    filename = format_filename(filename);
    FILE *pFile = fopen(filename.c_str(), "r");
    if (pFile == NULL)
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "unable to open wlan.ini (read)!");
        return -1;
    }

    ESP_LOGD(TAG, "LoadNetworkFromFile: wlan.ini opened");

    std::string line = "";
    char temp_bufer[256];
    if (fgets(temp_bufer, sizeof(temp_bufer), pFile) == NULL)
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "wlan.ini opened, but empty or content not readable!");
        fclose(pFile);
        return -1;
    }
    else
    {
        line = std::string(temp_bufer);
    }

    int _fix_ipaddress = 3;

    std::vector<std::string> splitted;

    while ((line.size() > 0) || !(feof(pFile)))
    {
        if (line[0] != ';')
        {
            splitted = split_line(line);

            if (splitted.size() > 1)
            {
                std::string _param = to_upper(splitted[0]);

                if (_param == "CONNECTIONTYPE")
                {
                    network_config.connection_type = splitted[1];
                    LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Connection Type: " + network_config.connection_type);
                }

                else if (_param == "SSID")
                {
                    network_config.ssid = splitted[1];
                    LogFile.WriteToFile(ESP_LOG_INFO, TAG, "SSID: " + network_config.ssid);
                }

                else if (_param == "EAPID")
                {
                    network_config.eapid = decrypt_pw_string(splitted[1]);
                    if (network_config.eapid.empty())
                    {
                        LogFile.WriteToFile(ESP_LOG_INFO, TAG, "EPA-ID: eapid_not_set");
                    }
#ifndef __HIDE_PASSWORD
                    else
                    {
                        LogFile.WriteToFile(ESP_LOG_INFO, TAG, "EPA-ID: " + network_config.eapid);
                    }
#else
                    else
                    {
                        LogFile.WriteToFile(ESP_LOG_INFO, TAG, "EPA-ID: eapid_hiden");
                    }
#endif
                }

                else if (_param == "PASSWORD")
                {
                    network_config.password = decrypt_pw_string(splitted[1]);
                    if (network_config.password.empty())
                    {
                        LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Password: wifi_pw_not_set");
                    }
#ifndef __HIDE_PASSWORD
                    else
                    {
                        LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Password: " + network_config.password);
                    }
#else
                    else
                    {
                        LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Password: wifi_pw_hiden");
                    }
#endif
                }

                else if (_param == "HOSTNAME")
                {
                    network_config.hostname = splitted[1];
                    LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Hostname: " + network_config.hostname);
                }

                else if (_param == "IP")
                {
                    network_config.ipaddress = splitted[1];
                    LogFile.WriteToFile(ESP_LOG_INFO, TAG, "IP-Address: " + network_config.ipaddress);

                    _fix_ipaddress--;
                }

                else if (_param == "GATEWAY")
                {
                    network_config.gateway = splitted[1];
                    LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Gateway: " + network_config.gateway);

                    _fix_ipaddress--;
                }

                else if (_param == "NETMASK")
                {
                    network_config.netmask = splitted[1];
                    LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Netmask: " + network_config.netmask);

                    _fix_ipaddress--;
                }

                else if (_param == "DNS")
                {
                    network_config.dns = splitted[1];
                    LogFile.WriteToFile(ESP_LOG_INFO, TAG, "DNS: " + network_config.dns);
                }

                else if (_param == "HTTP_AUTH")
                {
                    network_config.http_auth = alphanumeric_to_boolean(splitted[1]);
                    LogFile.WriteToFile(ESP_LOG_INFO, TAG, "HTTP_AUTH: " + std::to_string(network_config.http_auth));
                }

                else if (_param == "HTTP_USERNAME")
                {
                    network_config.http_username = splitted[1];
                    LogFile.WriteToFile(ESP_LOG_INFO, TAG, "HTTP_USERNAME: " + network_config.http_username);
                }

                else if (_param == "HTTP_PASSWORD")
                {
                    network_config.http_password = decrypt_pw_string(splitted[1]);
                    if (network_config.http_password.empty())
                    {
                        LogFile.WriteToFile(ESP_LOG_INFO, TAG, "HTTP_Password: http_pw_not_set");
                    }
#ifndef __HIDE_PASSWORD
                    else
                    {
                        LogFile.WriteToFile(ESP_LOG_INFO, TAG, "HTTP_PASSWORD: " + network_config.http_password);
                    }
#else
                    else
                    {
                        LogFile.WriteToFile(ESP_LOG_INFO, TAG, "HTTP_Password: http_pw_hiden");
                    }
#endif
                }

#if (defined WLAN_USE_ROAMING_BY_SCANNING || (defined WLAN_USE_MESH_ROAMING && defined WLAN_USE_MESH_ROAMING_ACTIVATE_CLIENT_TRIGGERED_QUERIES))
                else if (_param == "RSSITHRESHOLD")
                {
                    network_config.rssi_threshold = atoi(splitted[1].c_str());
                    LogFile.WriteToFile(ESP_LOG_INFO, TAG, "RSSIThreshold: " + std::to_string(network_config.rssi_threshold));
                }
#endif
            }
        }

        /* read next line */
        if (fgets(temp_bufer, sizeof(temp_bufer), pFile) == NULL)
        {
            line = "";
        }
        else
        {
            line = std::string(temp_bufer);
        }
    }
    fclose(pFile);

    memcpy(&network_config_temp, &network_config, sizeof(network_config));

    /* Check if SSID is empty (mandatory parameter) */
#if (CONFIG_ETH_ENABLED && CONFIG_ETH_USE_SPI_ETHERNET && CONFIG_ETH_SPI_ETHERNET_W5500)
    if ((network_config.connection_type.empty()) || ((network_config.connection_type != NETWORK_CONNECTION_WIFI_AP_SETUP) && (network_config.connection_type != NETWORK_CONNECTION_WIFI_AP) &&
                                                     (network_config.connection_type != NETWORK_CONNECTION_WIFI_STA) && (network_config.connection_type != NETWORK_CONNECTION_ETH) && (network_config.connection_type != NETWORK_CONNECTION_DISCONNECT)))
#else
    if ((network_config.connection_type.empty()) || ((network_config.connection_type != NETWORK_CONNECTION_WIFI_AP_SETUP) && (network_config.connection_type != NETWORK_CONNECTION_WIFI_AP) &&
                                                     (network_config.connection_type != NETWORK_CONNECTION_WIFI_STA) && (network_config.connection_type != NETWORK_CONNECTION_DISCONNECT)))
#endif
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Connection Type empty. It set to WiFi AP setup!");
        network_config.connection_type = NETWORK_CONNECTION_WIFI_AP_SETUP;
    }

    /* Check if SSID is empty (mandatory parameter) */
    if (network_config.ssid.empty() && network_config.connection_type == NETWORK_CONNECTION_WIFI_STA)
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "SSID empty!");
        return -2;
    }

    if (_fix_ipaddress == 0)
    {
        network_config.fix_ipaddress_used = true;
        LogFile.WriteToFile(ESP_LOG_INFO, TAG, "fix ipaddress used");
    }
    else
    {
        network_config.fix_ipaddress_used = false;
    }

    return 0;
}

bool ChangeHostName(std::string filename, std::string _newhostname)
{
    if (_newhostname == network_config.hostname)
    {
        return false;
    }

    filename = format_filename(filename);
    FILE *pFile = fopen(filename.c_str(), "r");
    if (pFile == NULL)
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "ChangeHostName: unable to open wlan.ini (read)");
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "ChangeHostName: wlan.ini opened");

    char temp_bufer[256];
    std::string line = "";

    if (fgets(temp_bufer, sizeof(temp_bufer), pFile) == NULL)
    {
        line = "";
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "ChangeHostName: wlan.ini opened, but empty or content not readable");
        fclose(pFile);
        return ESP_FAIL;
    }
    else
    {
        line = std::string(temp_bufer);
    }

    bool found = false;
    std::vector<string> splitted;
    std::vector<string> new_file;

    while ((line.size() > 0) || !(feof(pFile)))
    {
        splitted = split_line(line);

        if (splitted.size() > 1)
        {
            std::string _param = to_upper(splitted[0]);

            if (to_upper(splitted[0]) == "HOSTNAME" || to_upper(splitted[0]) == ";HOSTNAME")
            {
                line = "hostname = \"" + _newhostname + "\"\n";
                found = true;
            }

            new_file.push_back(line);

            if (fgets(temp_bufer, sizeof(temp_bufer), pFile) == NULL)
            {
                line = "";
            }
            else
            {
                line = std::string(temp_bufer);
            }
        }
    }

    if (!found)
    {
        line = "\n;++++++++++++++++++++++++++++++++++\n";
        line += "; Hostname: Name of device in network\n";
        line += "; This parameter can be configured via WebUI configuration\n";
        line += "; Default: \"watermeter\", if nothing is configured\n\n";
        line = "hostname = \"" + _newhostname + "\"\n";
        new_file.push_back(line);
    }
    fclose(pFile);

    pFile = fopen(filename.c_str(), "w+");
    if (pFile == NULL)
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "ChangeHostName: unable to open wlan.ini (write)");
        return false;
    }

    for (int i = 0; i < new_file.size(); ++i)
    {
        fputs(new_file[i].c_str(), pFile);
    }
    fclose(pFile);

    ESP_LOGD(TAG, "ChangeHostName done");

    return true;
}

#if (defined WLAN_USE_ROAMING_BY_SCANNING || (defined WLAN_USE_MESH_ROAMING && defined WLAN_USE_MESH_ROAMING_ACTIVATE_CLIENT_TRIGGERED_QUERIES))
bool ChangeRSSIThreshold(std::string filename, int _newrssithreshold)
{
    if (network_config.rssi_threshold == _newrssithreshold)
    {
        return false;
    }

    filename = format_filename(filename);
    FILE *pFile = fopen(filename.c_str(), "r");
    if (pFile == NULL)
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "ChangeRSSIThreshold: unable to open wlan.ini (read)");
        fclose(pFile);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "ChangeRSSIThreshold: wlan.ini opened");

    char temp_bufer[256];
    std::string line = "";

    if (fgets(temp_bufer, sizeof(temp_bufer), pFile) == NULL)
    {
        line = "";
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "ChangeRSSIThreshold: wlan.ini opened, but empty or content not readable");
        fclose(pFile);
        return ESP_FAIL;
    }
    else
    {
        line = std::string(temp_bufer);
    }

    bool found = false;
    std::vector<string> splitted;
    std::vector<string> new_file;

    while ((line.size() > 0) || !(feof(pFile)))
    {
        splitted = split_line(line);

        if (splitted.size() > 1)
        {
            std::string _param = to_upper(splitted[0]);

            if (_param == "RSSITHRESHOLD" || _param == ";RSSITHRESHOLD")
            {
                line = "RSSIThreshold = " + to_string(_newrssithreshold) + "\n";
                found = true;
            }

            new_file.push_back(line);

            if (fgets(temp_bufer, sizeof(temp_bufer), pFile) == NULL)
            {
                line = "";
            }
            else
            {
                line = std::string(temp_bufer);
            }
        }
    }

    if (!found)
    {
        line = "\n;++++++++++++++++++++++++++++++++++\n";
        line += "; WIFI Roaming:\n";
        line += "; Network assisted roaming protocol is activated by default\n";
        line += "; AP / mesh system needs to support roaming protocol 802.11k/v\n";
        line += ";\n";
        line += "; Optional feature (usually not neccessary):\n";
        line += "; RSSI Threshold for client requested roaming query (RSSI < RSSIThreshold)\n";
        line += "; Note: This parameter can be configured via WebUI configuration\n";
        line += "; Default: 0 = Disable client requested roaming query\n\n";
        line += "RSSIThreshold = " + to_string(_newrssithreshold) + "\n";
        new_file.push_back(line);
    }

    fclose(pFile);

    pFile = fopen(filename.c_str(), "w+");
    if (pFile == NULL)
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "ChangeRSSIThreshold: unable to open wlan.ini (write)");
        return false;
    }

    for (int i = 0; i < new_file.size(); ++i)
    {
        fputs(new_file[i].c_str(), pFile);
    }

    fclose(pFile);

    ESP_LOGD(TAG, "ChangeRSSIThreshold done");

    return true;
}
#endif

esp_err_t GetAuthWebUIConfig(std::string filename)
{
    filename = format_filename(filename);
    FILE *pFile = fopen(filename.c_str(), "r");
    if (pFile == NULL)
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "GetAuthWebUIConfig: unable to open wlan.ini (read)");
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "GetAuthWebUIConfig: wlan.ini opened");

    char temp_bufer[256];
    std::string line = "";

    if (fgets(temp_bufer, sizeof(temp_bufer), pFile) == NULL)
    {
        line = "";
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "GetAuthWebUIConfig: wlan.ini opened, but empty or content not readable");
        fclose(pFile);
        return ESP_FAIL;
    }
    else
    {
        line = std::string(temp_bufer);
    }

    std::vector<string> splitted;

    while ((line.size() > 0) || !(feof(pFile)))
    {
        splitted = split_line(line);

        if (splitted.size() > 1)
        {
            std::string _param = to_upper(splitted[0]);

            if ((_param == "HTTP_AUTH") || (_param == ";HTTP_AUTH"))
            {
                network_config.http_auth = alphanumeric_to_boolean(splitted[1]);
            }

            else if ((_param == "HTTP_USERNAME") || (_param == ";HTTP_USERNAME"))
            {
                network_config.http_username = splitted[1];
            }

            else if ((_param == "HTTP_PASSWORD") || (_param == ";HTTP_PASSWORD"))
            {
                network_config.http_password = decrypt_pw_string(splitted[1]);
            }
        }

        /* read next line */
        if (fgets(temp_bufer, sizeof(temp_bufer), pFile) == NULL)
        {
            line = "";
        }
        else
        {
            line = std::string(temp_bufer);
        }
    }

    fclose(pFile);

    return ESP_OK;
}
