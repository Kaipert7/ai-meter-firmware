#include "defines.h"

#include "server_remote.h"
#include "connect_wifi_ap.h"

#include <string.h>
#include <string>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_mac.h>
#include <esp_wifi.h>

#include <esp_log.h>
#include <nvs_flash.h>

#include <stdio.h>

#include <lwip/err.h>
#include <lwip/sys.h>

#include "time_sntp.h"

#include "ClassLogFile.h"
#include "server_help.h"
#include "Helper.h"
#include "statusled.h"
#include "server_ota.h"
#include "basic_auth.h"

static const char *TAG = "REMOTE SERVER";

std::string remote_webserver_start_time = "";

bool is_config_ini = false;
bool is_wlan_ini = false;

void remote_send_http_response(httpd_req_t *req)
{
    std::string message = "<h1>AI-on-the-edge - BASIC SETUP</h1><p>This is an access point with a minimal server to setup the minimum required files and information on the device and the SD-card. ";
    message += "This mode is always started if one of the following files is missing: /wlan.ini or the /config/config.ini.<p>";
    message += "The setup is done in 3 steps: 1. upload full inital configuration (sd-card content), 2. store WLAN access information, 3. reboot (and connect to WLANs)<p><p>";
    message += "Please follow the below instructions.<p>";
    httpd_resp_send_chunk(req, message.c_str(), strlen(message.c_str()));

    is_wlan_ini = (file_exists(WLAN_CONFIG_FILE) || file_exists(NETWORK_CONFIG_FILE));

    if (!is_config_ini)
    {
        message = "<h3>1. Upload initial configuration to sd-card</h3><p>";
        message += "The configuration file config.ini is missing and most propably the full configuration and html folder on the sd-card. ";
        message += "This is normal after the first flashing of the firmware and an empty sd-card. Please upload \"remote_setup.zip\", which contains a full inital configuration.<p>";
        message += "<input id=\"newfile\" type=\"file\"><br>";
        message += "<button class=\"button\" style=\"width:300px\" id=\"doUpdate\" type=\"button\" onclick=\"upload()\">Upload File</button><p>";
        message += "The upload might take up to 60s. After a succesfull upload the page will be updated.";
        httpd_resp_send_chunk(req, message.c_str(), strlen(message.c_str()));

        message = "<script language=\"JavaScript\">";
        message += "function upload() {";
        message += "var xhttp = new XMLHttpRequest();";
        message += "xhttp.onreadystatechange = function() {if (xhttp.readyState == 4) {if (xhttp.status == 200) {location.reload();}}};";
        message += "var filePath = document.getElementById(\"newfile\").value.split(/[\\\\/]/).pop();";
        message += "var file = document.getElementById(\"newfile\").files[0];";
        message += "if (!file.name.includes(\"remote-setup\")){if (!confirm(\"The zip file name should contain '...remote-setup...'. Are you sure that you have downloaded the correct file?\"))return;};";
        message += "var upload_path = \"/upload/firmware/\" + filePath; xhttp.open(\"POST\", upload_path, true); xhttp.send(file);document.reload();";
        message += "document.getElementById(\"doUpdate\").disabled = true;}";
        message += "</script>";
        httpd_resp_send_chunk(req, message.c_str(), strlen(message.c_str()));

        return;
    }

    if (!is_wlan_ini)
    {
        message = "<h3>2. WLAN access credentials</h3><p>";
        message += "<table>";
        message += "<tr><td>WLAN-SSID</td><td><input type=\"text\" name=\"ssid\" id=\"ssid\"></td><td>SSID of the WLAN</td></tr>";
        message += "<tr><td>WLAN-Password</td><td><input type=\"text\" name=\"password\" id=\"password\"></td><td>ATTENTION: the password will not be encrypted during the sending.</td><tr>";
        message += "</table><p>";
        message += "<h4>ATTENTION:<h4>Be sure about the WLAN settings. They cannot be reset afterwards. If ssid or password is wrong, you need to take out the sd-card and manually change them in \"wlan.ini\"!<p>";
        httpd_resp_send_chunk(req, message.c_str(), strlen(message.c_str()));

        message = "<button class=\"button\" type=\"button\" onclick=\"wr()\">Write wlan.ini</button>";
        message += "<script language=\"JavaScript\">async function wr(){";
        message += "api = \"/config?\"+\"ssid=\"+document.getElementById(\"ssid\").value+\"&pwd=\"+document.getElementById(\"password\").value;";
        message += "fetch(api);await new Promise(resolve => setTimeout(resolve, 1000));location.reload();}</script>";
        httpd_resp_send_chunk(req, message.c_str(), strlen(message.c_str()));

        return;
    }

    message = "<h3>3. Reboot</h3><p>";
    message += "After triggering the reboot, the zip-files gets extracted and written to the sd-card.<br>The ESP32 will restart two times and then connect to your access point. Please find the IP in your router settings and access it with the new ip-address.<p>";
    message += "The first update and initialization process can take up to 3 minutes before you find it in the wlan. Error logs can be found on the console / serial logout.<p>Have fun!<p>";
    message += "<button class=\"button\" type=\"button\" onclick=\"rb()\")>Reboot to first setup.</button>";
    message += "<script language=\"JavaScript\">async function rb(){";
    message += "api = \"/reboot\";";
    message += "fetch(api);await new Promise(resolve => setTimeout(resolve, 1000));location.reload();}</script>";
    httpd_resp_send_chunk(req, message.c_str(), strlen(message.c_str()));
}

esp_err_t remote_test_handler(httpd_req_t *req)
{
    remote_send_http_response(req);
    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

esp_err_t remote_reboot_handler(httpd_req_t *req)
{
    LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Trigger reboot due to firmware update.");
    doRebootOTA();

    return ESP_OK;
}

esp_err_t remote_config_ini_handler(httpd_req_t *req)
{
    LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "config_ini_handler");

    char _query[400];
    char _valuechar[100];

    std::string _ssid = "";
    std::string _pwd = "";
    std::string _hostname = "";
    std::string _ip = "";
    std::string _gateway = "";
    std::string _netmask = "";
    std::string _dns = "";
    std::string _rssithreshold = ""; // rssi threshold for WIFI roaming

    if (httpd_req_get_url_query_str(req, _query, 400) == ESP_OK)
    {
        ESP_LOGD(TAG, "Query: %s", _query);

        if (httpd_query_key_value(_query, "ssid", _valuechar, 100) == ESP_OK)
        {
            ESP_LOGD(TAG, "ssid is found: %s", _valuechar);
            _ssid = url_decode(std::string(_valuechar));
        }

        if (httpd_query_key_value(_query, "pwd", _valuechar, 100) == ESP_OK)
        {
            ESP_LOGD(TAG, "pwd is found: %s", _valuechar);
            _pwd = url_decode(std::string(_valuechar));
        }

        if (httpd_query_key_value(_query, "ssid", _valuechar, 100) == ESP_OK)
        {
            ESP_LOGD(TAG, "ssid is found: %s", _valuechar);
            _ssid = url_decode(std::string(_valuechar));
        }

        if (httpd_query_key_value(_query, "hn", _valuechar, 100) == ESP_OK)
        {
            ESP_LOGD(TAG, "hostname is found: %s", _valuechar);
            _hostname = url_decode(std::string(_valuechar));
        }

        if (httpd_query_key_value(_query, "ip", _valuechar, 100) == ESP_OK)
        {
            ESP_LOGD(TAG, "ip is found: %s", _valuechar);
            _ip = url_decode(std::string(_valuechar));
        }

        if (httpd_query_key_value(_query, "gw", _valuechar, 100) == ESP_OK)
        {
            ESP_LOGD(TAG, "gateway is found: %s", _valuechar);
            _gateway = url_decode(std::string(_valuechar));
        }

        if (httpd_query_key_value(_query, "nm", _valuechar, 100) == ESP_OK)
        {
            ESP_LOGD(TAG, "netmask is found: %s", _valuechar);
            _netmask = url_decode(std::string(_valuechar));
        }

        if (httpd_query_key_value(_query, "dns", _valuechar, 100) == ESP_OK)
        {
            ESP_LOGD(TAG, "dns is found: %s", _valuechar);
            _dns = url_decode(std::string(_valuechar));
        }

        if (httpd_query_key_value(_query, "rssithreshold", _valuechar, 100) == ESP_OK)
        {
            ESP_LOGD(TAG, "rssithreshold is found: %s", _valuechar);
            _rssithreshold = url_decode(std::string(_valuechar));
        }
    }

    FILE *pFile = fopen(NETWORK_CONFIG_FILE, "w");

    std::string text = ";++++++++++++++++++++++++++++++++++\n";
    text += "; AI on the edge - WLAN configuration\n";
    text += "; ssid: Name of WLAN network (mandatory), e.g. \"WLAN-SSID\"\n";
    text += "; password: Password of WLAN network (mandatory), e.g. \"PASSWORD\"\n\n";
    fputs(text.c_str(), pFile);

    if (_ssid.length())
    {
        _ssid = "ssid = \"" + _ssid + "\"\n";
    }
    else
    {
        _ssid = "ssid = \"\"\n";
    }
    fputs(_ssid.c_str(), pFile);

    if (_pwd.length())
    {
        _pwd = "password = \"" + _pwd + "\"\n";
    }
    else
    {
        _pwd = "password = \"\"\n";
    }
    fputs(_pwd.c_str(), pFile);

    text = "\n;++++++++++++++++++++++++++++++++++\n";
    text += "; Hostname: Name of device in network\n";
    text += "; This parameter can be configured via WebUI configuration\n";
    text += "; Default: \"watermeter\", if nothing is configured\n\n";
    fputs(text.c_str(), pFile);

    if (_hostname.length())
    {
        _hostname = "hostname = \"" + _hostname + "\"\n";
    }
    else
    {
        _hostname = ";hostname = \"watermeter\"\n";
    }
    fputs(_hostname.c_str(), pFile);

    text = "\n;++++++++++++++++++++++++++++++++++\n";
    text += "; Fixed IP: If you like to use fixed IP instead of DHCP (default), the following\n";
    text += "; parameters needs to be configured: ip, gateway, netmask are mandatory, dns optional\n\n";
    fputs(text.c_str(), pFile);

    if (_ip.length())
    {
        _ip = "ip = \"" + _ip + "\"\n";
    }
    else
    {
        _ip = ";ip = \"xxx.xxx.xxx.xxx\"\n";
    }
    fputs(_ip.c_str(), pFile);

    if (_gateway.length())
    {
        _gateway = "gateway = \"" + _gateway + "\"\n";
    }
    else
    {
        _gateway = ";gateway = \"xxx.xxx.xxx.xxx\"\n";
    }
    fputs(_gateway.c_str(), pFile);

    if (_netmask.length())
    {
        _netmask = "netmask = \"" + _netmask + "\"\n";
    }
    else
    {
        _netmask = ";netmask = \"xxx.xxx.xxx.xxx\"\n";
    }
    fputs(_netmask.c_str(), pFile);

    text = "\n;++++++++++++++++++++++++++++++++++\n";
    text += "; DNS server (optional, if no DNS is configured, gateway address will be used)\n\n";
    fputs(text.c_str(), pFile);

    if (_dns.length())
    {
        _dns = "dns = \"" + _dns + "\"\n";
    }
    else
    {
        _dns = ";dns = \"xxx.xxx.xxx.xxx\"\n";
    }
    fputs(_dns.c_str(), pFile);

    text = "\n;++++++++++++++++++++++++++++++++++\n";
    text += "; WIFI Roaming:\n";
    text += "; Network assisted roaming protocol is activated by default\n";
    text += "; AP / mesh system needs to support roaming protocol 802.11k/v\n";
    text += ";\n";
    text += "; Optional feature (usually not neccessary):\n";
    text += "; RSSI Threshold for client requested roaming query (RSSI < RSSIThreshold)\n";
    text += "; Note: This parameter can be configured via WebUI configuration\n";
    text += "; Default: 0 = Disable client requested roaming query\n\n";
    fputs(text.c_str(), pFile);

    if (_rssithreshold.length())
    {
        _rssithreshold = "RSSIThreshold = " + _rssithreshold + "\n";
    }
    else
    {
        _rssithreshold = "RSSIThreshold = 0\n";
    }
    fputs(_rssithreshold.c_str(), pFile);

    fflush(pFile);
    fclose(pFile);

    std::string zw = "ota without parameter - should not be the case!";
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, zw.c_str(), zw.length());

    ESP_LOGD(TAG, "end config.ini");

    return ESP_OK;
}

esp_err_t remote_upload_post_handler(httpd_req_t *req)
{
    printf("Starting the post handler\n");
    make_dir("/sdcard/config");
    make_dir("/sdcard/firmware");
    make_dir("/sdcard/html");
    make_dir("/sdcard/img_tmp");
    make_dir("/sdcard/log");
    make_dir("/sdcard/demo");
    printf("After starting the post handler\n");

    LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "remote_upload_post_handler");
    char filepath[FILE_PATH_MAX];

    const char *filename = get_path_from_uri(filepath, "/sdcard", req->uri + sizeof("/upload") - 1, sizeof(filepath));
    if (!filename)
    {
        httpd_resp_send_err(req, HTTPD_414_URI_TOO_LONG, "Filename too long");
        return ESP_FAIL;
    }

    printf("filepath: %s, filename: %s\n", filepath, filename);

    delete_file(std::string(filepath));

    FILE *pFile = fopen(filepath, "w");
    if (!pFile)
    {
        ESP_LOGE(TAG, "Failed to create file: %s", filepath);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Receiving file: %s...", filename);

    char buf[1024];
    int received;

    int remaining = req->content_len;

    printf("remaining: %d\n", remaining);

    while (remaining > 0)
    {
        ESP_LOGI(TAG, "Remaining size: %d", remaining);
        if ((received = httpd_req_recv(req, buf, MIN(remaining, 1024))) <= 0)
        {
            if (received == HTTPD_SOCK_ERR_TIMEOUT)
            {
                continue;
            }

            fclose(pFile);
            unlink(filepath);

            ESP_LOGE(TAG, "File reception failed!");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
            return ESP_FAIL;
        }

        if (received && (received != fwrite(buf, 1, received, pFile)))
        {
            fclose(pFile);
            unlink(filepath);

            ESP_LOGE(TAG, "File write failed!");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file to storage");
            return ESP_FAIL;
        }

        remaining -= received;
    }

    fclose(pFile);
    is_config_ini = true;

    pFile = fopen("/sdcard/update.txt", "w");
    std::string temp_string = "/sdcard" + std::string(filename);
    fwrite(temp_string.c_str(), strlen(temp_string.c_str()), 1, pFile);
    fclose(pFile);

    ESP_LOGI(TAG, "File reception complete");
    httpd_resp_set_hdr(req, "Location", "/test");
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/test");
    httpd_resp_send_chunk(req, NULL, 0);

    ESP_LOGI(TAG, "Update page send out");

    return ESP_OK;
}

httpd_handle_t start_remote_webserver(void)
{
    httpd_handle_t my_remote_webserver_httpd_handle = NULL;
    httpd_config_t my_remote_webserver_httpd_config = HTTPD_DEFAULT_CONFIG();

    remote_webserver_start_time = getCurrentTimeString("%Y%m%d-%H%M%S");

    my_remote_webserver_httpd_config.uri_match_fn = httpd_uri_match_wildcard;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", my_remote_webserver_httpd_config.server_port);
    if (httpd_start(&my_remote_webserver_httpd_handle, &my_remote_webserver_httpd_config) == ESP_OK)
    {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        return my_remote_webserver_httpd_handle;
    }

    ESP_LOGI(TAG, "Error starting ap server!");

    return NULL;
}

void stop_remote_webserver(httpd_handle_t server)
{
    httpd_stop(server);
}

httpd_handle_t remote_webserver_register_uri(httpd_handle_t server)
{
    httpd_uri_t reboot_handle = {
        .uri = "/reboot",
        .method = HTTP_GET,
        .handler = APPLY_BASIC_AUTH_FILTER(remote_reboot_handler),
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &reboot_handle);

    httpd_uri_t config_ini_handle = {
        .uri = "/config",
        .method = HTTP_GET,
        .handler = APPLY_BASIC_AUTH_FILTER(remote_config_ini_handler),
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &config_ini_handle);

    /* URI handler for uploading files to server */
    httpd_uri_t file_uploadAP = {
        .uri = "/upload/*",
        .method = HTTP_POST,
        .handler = APPLY_BASIC_AUTH_FILTER(remote_upload_post_handler),
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &file_uploadAP);

    httpd_uri_t test_uri = {
        .uri = "*",
        .method = HTTP_GET,
        .handler = APPLY_BASIC_AUTH_FILTER(remote_test_handler),
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &test_uri);

    return NULL;
}
