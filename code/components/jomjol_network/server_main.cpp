#include "defines.h"

#include "server_main.h"

#include <stdio.h>
#include <string>
#include <esp_log.h>
#include <esp_chip_info.h>
#include <esp_wifi.h>
#include <netdb.h>

#include "server_help.h"
#include "ClassLogFile.h"

#include "time_sntp.h"

#include "connect_wifi_sta.h"
#include "read_network_config.h"

#include "../main/version.h"

#include "MainFlowControl.h"
#include "basic_auth.h"

#include "Helper.h"

static const char *TAG = "MAIN SERVER";

std::string webserver_start_time = "";

esp_err_t main_handler(httpd_req_t *req)
{
    char filepath[50];
    ESP_LOGD(TAG, "uri: %s\n", req->uri);
    int _pos;
    esp_err_t res;

    char *base_path = (char *)req->user_ctx;
    std::string filetosend(base_path);

    const char *filename = get_path_from_uri(filepath, base_path, req->uri - 1, sizeof(filepath));
    ESP_LOGD(TAG, "1 uri: %s, filename: %s, filepath: %s", req->uri, filename, filepath);

    if ((strcmp(req->uri, "/") == 0))
    {
        filetosend = filetosend + "/html/index.html";
    }
    else
    {
        filetosend = filetosend + "/html" + std::string(req->uri);
        _pos = filetosend.find("?");
        if (_pos > -1)
        {
            filetosend = filetosend.substr(0, _pos);
        }
    }

    if (filetosend == "/sdcard/html/index.html")
    {
        // Initialization failed with crritical errors!
        if (is_set_system_statusflag(SYSTEM_STATUS_PSRAM_BAD) || is_set_system_statusflag(SYSTEM_STATUS_CAM_BAD) ||
            is_set_system_statusflag(SYSTEM_STATUS_SDCARD_CHECK_BAD) || is_set_system_statusflag(SYSTEM_STATUS_FOLDER_CHECK_BAD))
        {
            LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "We have a critical error, not serving main page!");

            char buf[20];
            std::string message = "<h1>AI on the Edge Device</h1><b>We have one or more critical errors:</b><br>";

            for (int i = 0; i < 32; i++)
            {
                if (is_set_system_statusflag((SystemStatusFlag_t)(1 << i)))
                {
                    snprintf(buf, sizeof(buf), "0x%08X", 1 << i);
                    message += std::string(buf) + "<br>";
                }
            }

            message += "<br>Please check logs with log viewer and/or <a href=\"https://jomjol.github.io/AI-on-the-edge-device-docs/Error-Codes\" target=_blank>jomjol.github.io/AI-on-the-edge-device-docs/Error-Codes</a> for more information!";
            message += "<br><br><button onclick=\"window.location.href='/reboot';\">Reboot</button>";
            message += "&nbsp;<button onclick=\"window.open('/ota_page.html');\">OTA Update</button>";
            message += "&nbsp;<button onclick=\"window.open('/log.html');\">Log Viewer</button>";
            message += "&nbsp;<button onclick=\"window.open('/info.html');\">Show System Info</button>";
            httpd_resp_send(req, message.c_str(), message.length());
            return ESP_OK;
        }
        else if (isSetupModusActive())
        {
            ESP_LOGD(TAG, "System is in setup mode --> index.html --> setup.html");
            filetosend = "/sdcard/html/setup.html";
        }
    }

    ESP_LOGD(TAG, "Filename: %s", filename);
    ESP_LOGD(TAG, "File requested: %s", filetosend.c_str());

    if (!filename)
    {
        ESP_LOGE(TAG, "Filename is too long");
        /* Respond with 414 Error */
        httpd_resp_send_err(req, HTTPD_414_URI_TOO_LONG, "Filename too long");
        return ESP_FAIL;
    }

    res = send_file(req, filetosend);
    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);

    if (res != ESP_OK)
    {
        return res;
    }

    return ESP_OK;
}

esp_err_t start_time_handler(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, webserver_start_time.c_str(), webserver_start_time.length());
    return ESP_OK;
}

esp_err_t sysinfo_handler(httpd_req_t *req)
{
    std::string zw;
    std::string cputemp = std::to_string((int)read_tempsensor());
    std::string gitversion = libfive_git_version();
    std::string buildtime = build_time();
    std::string gitbranch = libfive_git_branch();
    std::string gittag = libfive_git_version();
    std::string gitrevision = libfive_git_revision();
    std::string htmlversion = getHTMLversion();
    char freeheapmem[11];
    sprintf(freeheapmem, "%lu", (long)get_heapsize());

    zw = string("[{") +
         "\"firmware\": \"" + gitversion + "\"," +
         "\"buildtime\": \"" + buildtime + "\"," +
         "\"gitbranch\": \"" + gitbranch + "\"," +
         "\"gittag\": \"" + gittag + "\"," +
         "\"gitrevision\": \"" + gitrevision + "\"," +
         "\"html\": \"" + htmlversion + "\"," +
         "\"cputemp\": \"" + cputemp + "\"," +
         "\"hostname\": \"" + network_config.hostname + "\"," +
         "\"IPv4\": \"" + network_config.ipaddress + "\"," +
         "\"freeHeapMem\": \"" + freeheapmem + "\"" +
         "}]";

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, zw.c_str(), zw.length());

    return ESP_OK;
}

/* An HTTP GET handler */
esp_err_t info_handler(httpd_req_t *req)
{
    LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "info_handler");
    char _query[200];
    char _valuechar[30];
    std::string _task;

    if (httpd_req_get_url_query_str(req, _query, 200) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid query string");
    }

    ESP_LOGD(TAG, "Query: %s", _query);

    if (httpd_query_key_value(_query, "type", _valuechar, 30) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing or invalid 'type' query parameter (too long value?)");
    }

    ESP_LOGD(TAG, "type is found: %s", _valuechar);
    _task = std::string(_valuechar);

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    if (_task.compare("GitBranch") == 0)
    {
        httpd_resp_sendstr(req, libfive_git_branch());
        return ESP_OK;
    }
    else if (_task.compare("GitTag") == 0)
    {
        httpd_resp_sendstr(req, libfive_git_version());
        return ESP_OK;
    }
    else if (_task.compare("GitRevision") == 0)
    {
        httpd_resp_sendstr(req, libfive_git_revision());
        return ESP_OK;
    }
    else if (_task.compare("BuildTime") == 0)
    {
        httpd_resp_sendstr(req, build_time());
        return ESP_OK;
    }
    else if (_task.compare("FirmwareVersion") == 0)
    {
        httpd_resp_sendstr(req, getFwVersion().c_str());
        return ESP_OK;
    }
    else if (_task.compare("HTMLVersion") == 0)
    {
        httpd_resp_sendstr(req, getHTMLversion().c_str());
        return ESP_OK;
    }
    else if (_task.compare("Hostname") == 0)
    {
        std::string zw = std::string(network_config.hostname);
        httpd_resp_sendstr(req, zw.c_str());
        return ESP_OK;
    }
    else if (_task.compare("IP") == 0)
    {
        std::string zw = std::string(network_config.ipaddress);
        httpd_resp_sendstr(req, zw.c_str());
        return ESP_OK;
    }
    else if (_task.compare("SSID") == 0)
    {
        std::string zw = std::string(network_config.ssid);
        httpd_resp_sendstr(req, zw.c_str());
        return ESP_OK;
    }
    else if (_task.compare("FlowStatus") == 0)
    {
        std::string zw = std::string("FlowStatus");
        httpd_resp_sendstr(req, zw.c_str());
        return ESP_OK;
    }
    else if (_task.compare("Round") == 0)
    {
        char formated[10] = "";
        snprintf(formated, sizeof(formated), "%d", getCountFlowRounds());
        httpd_resp_sendstr(req, formated);
        return ESP_OK;
    }
    else if (_task.compare("SDCardPartitionSize") == 0)
    {
        std::string zw;
        zw = get_sd_card_partition_size();
        httpd_resp_sendstr(req, zw.c_str());
        return ESP_OK;
    }
    else if (_task.compare("SDCardFreePartitionSpace") == 0)
    {
        std::string zw;
        zw = get_sd_card_free_partition_space();
        httpd_resp_sendstr(req, zw.c_str());
        return ESP_OK;
    }
    else if (_task.compare("SDCardPartitionAllocationSize") == 0)
    {
        std::string zw;
        zw = get_sd_card_partition_allocation_size();
        httpd_resp_sendstr(req, zw.c_str());
        return ESP_OK;
    }
    else if (_task.compare("SDCardManufacturer") == 0)
    {
        std::string zw;
        zw = get_sd_card_manufacturer();
        httpd_resp_sendstr(req, zw.c_str());
        return ESP_OK;
    }
    else if (_task.compare("SDCardName") == 0)
    {
        std::string zw;
        zw = get_sd_card_name();
        httpd_resp_sendstr(req, zw.c_str());
        return ESP_OK;
    }
    else if (_task.compare("SDCardCapacity") == 0)
    {
        std::string zw;
        zw = get_sd_card_capacity();
        httpd_resp_sendstr(req, zw.c_str());
        return ESP_OK;
    }
    else if (_task.compare("SDCardSectorSize") == 0)
    {
        std::string zw;
        zw = get_sd_card_sector_size();
        httpd_resp_sendstr(req, zw.c_str());
        return ESP_OK;
    }
    else if (_task.compare("ChipCores") == 0)
    {
        esp_chip_info_t chipInfo;
        esp_chip_info(&chipInfo);
        httpd_resp_sendstr(req, to_string(chipInfo.cores).c_str());
        return ESP_OK;
    }
    else if (_task.compare("ChipRevision") == 0)
    {
        esp_chip_info_t chipInfo;
        esp_chip_info(&chipInfo);
        httpd_resp_sendstr(req, to_string(chipInfo.revision).c_str());
        return ESP_OK;
    }
    else if (_task.compare("ChipFeatures") == 0)
    {
        esp_chip_info_t chipInfo;
        esp_chip_info(&chipInfo);
        httpd_resp_sendstr(req, to_string(chipInfo.features).c_str());
        return ESP_OK;
    }
    else
    {
        char formatted[256];
        snprintf(formatted, sizeof(formatted), "Unknown value for parameter info 'type': '%s'\n", _task.c_str());
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, formatted);
    }

    return ESP_OK;
}

esp_err_t img_tmp_handler(httpd_req_t *req)
{
    char filepath[50];
    ESP_LOGD(TAG, "uri: %s", req->uri);

    char *base_path = (char *)req->user_ctx;
    std::string filetosend(base_path);

    const char *filename = get_path_from_uri(filepath, base_path, req->uri + sizeof("/img_tmp/") - 1, sizeof(filepath));
    ESP_LOGD(TAG, "1 uri: %s, filename: %s, filepath: %s", req->uri, filename, filepath);

    filetosend = filetosend + "/img_tmp/" + std::string(filename);
    ESP_LOGD(TAG, "File to upload: %s", filetosend.c_str());

    esp_err_t res = send_file(req, filetosend);
    if (res != ESP_OK)
    {
        return res;
    }

    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

esp_err_t img_tmp_virtual_handler(httpd_req_t *req)
{
    char filepath[50];
    ESP_LOGD(TAG, "uri: %s", req->uri);

    char *base_path = (char *)req->user_ctx;
    std::string filetosend(base_path);

    const char *filename = get_path_from_uri(filepath, base_path, req->uri + sizeof("/img_tmp/") - 1, sizeof(filepath));
    ESP_LOGD(TAG, "1 uri: %s, filename: %s, filepath: %s", req->uri, filename, filepath);

    filetosend = std::string(filename);
    ESP_LOGD(TAG, "File to upload: %s", filetosend.c_str());

    // Serve raw.jpg
    if (filetosend == "raw.jpg")
    {
        return GetRawJPG(req);
    }

    // Serve alg.jpg, alg_roi.jpg or digit and analog ROIs
    if (ESP_OK == GetJPG(filetosend, req))
    {
        return ESP_OK;
    }

    // File was not served already --> serve with img_tmp_handler
    return img_tmp_handler(req);
}

esp_err_t wifi_scan_handler(httpd_req_t *req)
{
    std::string data = string("{\n");
    data = data + wifi_scan_ap() + "\n";
    data = data + string("}");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, data.c_str(), data.length());

    // Respond with an empty chunk to signal HTTP response completion
    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

httpd_handle_t start_webserver(void)
{
    httpd_handle_t my_webserver_httpd_handle = NULL;
    httpd_config_t my_webserver_httpd_config = HTTPD_DEFAULT_CONFIG();

    my_webserver_httpd_config.task_priority = tskIDLE_PRIORITY + 3; // previously -> 2022-12-11: tskIDLE_PRIORITY+1; 2021-09-24: tskIDLE_PRIORITY+5
    my_webserver_httpd_config.stack_size = 12288;                   // previously -> 2023-01-02: 32768
    my_webserver_httpd_config.core_id = 1;                          // previously -> 2023-01-02: 0, 2022-12-11: tskNO_AFFINITY;
    my_webserver_httpd_config.server_port = 80;
    my_webserver_httpd_config.ctrl_port = 32768;
    my_webserver_httpd_config.max_open_sockets = 5;  // 20210921 --> previously 7
    my_webserver_httpd_config.max_uri_handlers = 42; // Make sure this fits all URI handlers. Memory usage in bytes: 6*max_uri_handlers
    my_webserver_httpd_config.max_resp_headers = 8;
    my_webserver_httpd_config.backlog_conn = 5;
    my_webserver_httpd_config.lru_purge_enable = true; // this cuts old connections if new ones are needed.
    my_webserver_httpd_config.recv_wait_timeout = 5;   // default: 5 20210924 --> previously 30
    my_webserver_httpd_config.send_wait_timeout = 5;   // default: 5 20210924 --> previously 30
    my_webserver_httpd_config.global_user_ctx = NULL;
    my_webserver_httpd_config.global_user_ctx_free_fn = NULL;
    my_webserver_httpd_config.global_transport_ctx = NULL;
    my_webserver_httpd_config.global_transport_ctx_free_fn = NULL;
    my_webserver_httpd_config.open_fn = NULL;
    my_webserver_httpd_config.close_fn = NULL;
    // my_webserver_httpd_config.uri_match_fn = NULL;
    my_webserver_httpd_config.uri_match_fn = httpd_uri_match_wildcard;

    webserver_start_time = getCurrentTimeString("%Y%m%d-%H%M%S");

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", my_webserver_httpd_config.server_port);
    if (httpd_start(&my_webserver_httpd_handle, &my_webserver_httpd_config) == ESP_OK)
    {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        return my_webserver_httpd_handle;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

void stop_webserver(httpd_handle_t server)
{
    httpd_stop(server);
}

void webserver_register_uri(httpd_handle_t server, const char *base_path)
{
    httpd_uri_t info_handle = {
        .uri = "/info", // Match all URIs of type /path/to/file
        .method = HTTP_GET,
        .handler = APPLY_BASIC_AUTH_FILTER(info_handler),
        .user_ctx = (void *)base_path // Pass server data as context
    };
    httpd_register_uri_handler(server, &info_handle);

    httpd_uri_t sysinfo_handle = {
        .uri = "/sysinfo", // Match all URIs of type /path/to/file
        .method = HTTP_GET,
        .handler = APPLY_BASIC_AUTH_FILTER(sysinfo_handler),
        .user_ctx = (void *)base_path // Pass server data as context
    };
    httpd_register_uri_handler(server, &sysinfo_handle);

    httpd_uri_t wifi_scan_handle = {
        .uri = "/wifiscan", // Match all URIs of type /path/to/file
        .method = HTTP_GET,
        .handler = APPLY_BASIC_AUTH_FILTER(wifi_scan_handler),
        .user_ctx = (void *)base_path // Pass server data as context
    };
    httpd_register_uri_handler(server, &wifi_scan_handle);

    httpd_uri_t start_time_handle = {
        .uri = "/starttime", // Match all URIs of type /path/to/file
        .method = HTTP_GET,
        .handler = APPLY_BASIC_AUTH_FILTER(start_time_handler),
        .user_ctx = NULL // Pass server data as context
    };
    httpd_register_uri_handler(server, &start_time_handle);

    httpd_uri_t img_tmp_handle = {
        .uri = "/img_tmp/*", // Match all URIs of type /path/to/file
        .method = HTTP_GET,
        .handler = APPLY_BASIC_AUTH_FILTER(img_tmp_virtual_handler),
        .user_ctx = (void *)base_path // Pass server data as context
    };
    httpd_register_uri_handler(server, &img_tmp_handle);

    httpd_uri_t main_rest_handle = {
        .uri = "/*", // Match all URIs of type /path/to/file
        .method = HTTP_GET,
        .handler = APPLY_BASIC_AUTH_FILTER(main_handler),
        .user_ctx = (void *)base_path // Pass server data as context
    };
    httpd_register_uri_handler(server, &main_rest_handle);
}
