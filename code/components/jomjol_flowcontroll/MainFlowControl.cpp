#include "defines.h"

#include "MainFlowControl.h"

#include <string>
#include <vector>
#include <string.h>
#include <esp_log.h>
#include <esp_timer.h>

#include <iomanip>
#include <sstream>

#include "Helper.h"
#include "statusled.h"

#include "esp_camera.h"
#include "time_sntp.h"
#include "ClassControllCamera.h"

#include "ClassFlowControll.h"

#include "ClassLogFile.h"
#include "server_GPIO.h"

#include "server_file.h"

#include "read_network_config.h"
#include "connect_wifi_sta.h"
#include "connect_roaming.h"
#include "psram.h"
#include "basic_auth.h"

// support IDF 5.x
#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif

ClassFlowControll flowctrl;

TaskHandle_t xHandletask_autodoFlow = NULL;

bool bTaskAutoFlowCreated = false;
bool flowisrunning = false;

long auto_interval = 0;
bool autostartIsEnabled = false;

int countRounds = 0;
bool isPlannedReboot = false;

static const char *TAG = "MAINCTRL";

void CheckIsPlannedReboot(void)
{
    FILE *pfile;
    if ((pfile = fopen("/sdcard/reboot.txt", "r")) == NULL)
    {
        // LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "Initial boot or not a planned reboot");
        isPlannedReboot = false;
    }
    else
    {
        LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "Planned reboot");
        delete_file("/sdcard/reboot.txt"); // Prevent Boot Loop!!!
        isPlannedReboot = true;
    }
}

bool getIsPlannedReboot(void)
{
    return isPlannedReboot;
}

int getCountFlowRounds(void)
{
    return countRounds;
}

esp_err_t GetJPG(std::string _filename, httpd_req_t *req)
{
    return flowctrl.GetJPGStream(_filename, req);
}

esp_err_t GetRawJPG(httpd_req_t *req)
{
    return flowctrl.SendRawJPG(req);
}

bool isSetupModusActive(void)
{
    return flowctrl.getStatusSetupModus();
}

void DeleteMainFlowTask(void)
{
    if (xHandletask_autodoFlow != NULL)
    {
        vTaskDelete(xHandletask_autodoFlow);
        xHandletask_autodoFlow = NULL;
    }
}

void doInit(void)
{
    flowctrl.InitFlow(CONFIG_FILE);

    /* GPIO handler has to be initialized before MQTT init to ensure proper topic subscription */
    gpio_handler_init();

    flowctrl.StartMQTTService();
}

bool doflow(void)
{
    std::string zw_time = getCurrentTimeString(LOGFILE_TIME_FORMAT);
    ESP_LOGD(TAG, "doflow - start %s", zw_time.c_str());
    flowisrunning = true;
    flowctrl.doFlow(zw_time);
    flowisrunning = false;
    return true;
}

esp_err_t handler_get_heap(httpd_req_t *req)
{
    std::string zw = "Heap info:<br>" + get_heapinfo();

#ifdef TASK_ANALYSIS_ON
    char *pcTaskList = (char *)calloc_psram_heap(std::string(TAG) + "->pcTaskList", 1, sizeof(char) * 768, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    if (pcTaskList)
    {
        vTaskList(pcTaskList);
        zw = zw + "<br><br>Task info:<br><pre>Name | State | Prio | Lowest stacksize | Creation order | CPU (-1=NoAffinity)<br>" + std::string(pcTaskList) + "</pre>";
        free_psram_heap(std::string(TAG) + "->pcTaskList", pcTaskList);
    }
    else
    {
        zw = zw + "<br><br>Task info:<br>ERROR - Allocation of TaskList buffer in PSRAM failed";
    }
#endif

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    if (zw.length() > 0)
    {
        httpd_resp_send(req, zw.c_str(), zw.length());
    }
    else
    {
        httpd_resp_send(req, NULL, 0);
    }

    return ESP_OK;
}

esp_err_t handler_init(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    const char *resp_str = "Init started<br>";
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    doInit();

    resp_str = "Init done<br>";
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

esp_err_t handler_stream(httpd_req_t *req)
{
    char _query[50];
    char _value[10];
    bool flashlightOn = false;

    if (httpd_req_get_url_query_str(req, _query, 50) == ESP_OK)
    {
        // ESP_LOGD(TAG, "Query: %s", _query);
        if (httpd_query_key_value(_query, "flashlight", _value, 10) == ESP_OK)
        {
            if (strlen(_value) > 0)
            {
                flashlightOn = true;
            }
        }
    }

    Camera.capture_to_stream(req, flashlightOn);

    return ESP_OK;
}

esp_err_t handler_flow_start(httpd_req_t *req)
{
    ESP_LOGD(TAG, "handler_flow_start uri: %s", req->uri);

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    if (autostartIsEnabled)
    {
        xTaskAbortDelay(xHandletask_autodoFlow); // Delay will be aborted if task is in blocked (waiting) state. If task is already running, no action
        LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "Flow start triggered by REST API /flow_start");
        const char *resp_str = "The flow is going to be started immediately or is already running";
        httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    }
    else
    {
        LogFile.WriteToFile(ESP_LOG_WARN, TAG, "Flow start triggered by REST API, but flow is not active!");
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Flow start triggered by REST API, but flow is not active");
    }

    return ESP_OK;
}

esp_err_t MQTTCtrlFlowStart(std::string _topic)
{
    ESP_LOGD(TAG, "MQTTCtrlFlowStart: topic %s", _topic.c_str());

    if (autostartIsEnabled)
    {
        xTaskAbortDelay(xHandletask_autodoFlow); // Delay will be aborted if task is in blocked (waiting) state. If task is already running, no action
        LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "Flow start triggered by MQTT topic " + _topic);
    }
    else
    {
        LogFile.WriteToFile(ESP_LOG_WARN, TAG, "Flow start triggered by MQTT topic " + _topic + ", but flow is not active!");
    }

    return ESP_OK;
}

esp_err_t handler_json(httpd_req_t *req)
{
    ESP_LOGD(TAG, "handler_JSON uri: %s", req->uri);

    if (bTaskAutoFlowCreated)
    {
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_set_type(req, "application/json");

        std::string zw = flowctrl.getJSON();
        if (zw.length() > 0)
        {
            httpd_resp_send(req, zw.c_str(), zw.length());
        }
        else
        {
            httpd_resp_send(req, NULL, 0);
        }
    }
    else
    {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Flow not (yet) started: REST API /json not yet available!");
        return ESP_ERR_NOT_FOUND;
    }

    return ESP_OK;
}

/**
 * Generates a http response containing the OpenMetrics (https://openmetrics.io/) text wire format
 * according to https://github.com/OpenObservability/OpenMetrics/blob/main/specification/OpenMetrics.md#text-format.
 *
 * A MetricFamily with a Metric for each Sequence is provided. If no valid value is available, the metric is not provided.
 * MetricPoints are provided without a timestamp. Additional metrics with some device information is also provided.
 *
 * The metric name prefix is 'ai_on_the_edge_device_'.
 *
 * example configuration for Prometheus (`prometheus.yml`):
 *
 *    - job_name: watermeter
 *      static_configs:
 *        - targets: ['watermeter.fritz.box']
 *
 */
esp_err_t handler_openmetrics(httpd_req_t *req)
{
    ESP_LOGD(TAG, "handler_openmetrics uri: %s", req->uri);

    if (bTaskAutoFlowCreated)
    {
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_set_type(req, "text/plain"); // application/openmetrics-text is not yet supported by prometheus so we use text/plain for now

        const string metricNamePrefix = "ai_on_the_edge_device";

        // get current measurement (flow)
        string response = createSequenceMetrics(metricNamePrefix, flowctrl.getNumbers());

        // CPU Temperature
        response += createMetric(metricNamePrefix + "_cpu_temperature_celsius", "current cpu temperature in celsius", "gauge", std::to_string((int)read_tempsensor()));

        // WiFi signal strength
        response += createMetric(metricNamePrefix + "_rssi_dbm", "current WiFi signal strength in dBm", "gauge", std::to_string(get_wifi_rssi()));

        // memory info
        response += createMetric(metricNamePrefix + "_memory_heap_free_bytes", "available heap memory", "gauge", std::to_string(get_heapsize()));

        // device uptime
        response += createMetric(metricNamePrefix + "_uptime_seconds", "device uptime in seconds", "gauge", std::to_string((long)get_uptime()));

        // data aquisition round
        response += createMetric(metricNamePrefix + "_rounds_total", "data aquisition rounds since device startup", "counter", std::to_string(countRounds));

        // the response always contains at least the metadata (HELP, TYPE) for the MetricFamily so no length check is needed
        httpd_resp_send(req, response.c_str(), response.length());
    }
    else
    {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Flow not (yet) started: REST API /metrics not yet available!");
        return ESP_ERR_NOT_FOUND;
    }

    return ESP_OK;
}

esp_err_t handler_value(httpd_req_t *req)
{
    if (bTaskAutoFlowCreated)
    {
        bool _rawValue = false;
        bool _noerror = false;
        bool _all = false;
        std::string _type = "value";

        ESP_LOGD(TAG, "handler water counter uri: %s", req->uri);

        char _query[100];
        char _size[10];

        if (httpd_req_get_url_query_str(req, _query, 100) == ESP_OK)
        {
            // ESP_LOGD(TAG, "Query: %s", _query);
            if (httpd_query_key_value(_query, "all", _size, 10) == ESP_OK)
            {
                _all = true;
            }

            if (httpd_query_key_value(_query, "type", _size, 10) == ESP_OK)
            {
                _type = std::string(_size);
            }

            if (httpd_query_key_value(_query, "rawvalue", _size, 10) == ESP_OK)
            {
                _rawValue = true;
            }

            if (httpd_query_key_value(_query, "noerror", _size, 10) == ESP_OK)
            {
                _noerror = true;
            }
        }

        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

        if (_all)
        {
            httpd_resp_set_type(req, "text/plain");
            ESP_LOGD(TAG, "TYPE: %s", _type.c_str());
            int _intype = READOUT_TYPE_VALUE;

            if (_type == "prevalue")
            {
                _intype = READOUT_TYPE_PREVALUE;
            }
            else if (_type == "raw")
            {
                _intype = READOUT_TYPE_RAWVALUE;
            }
            else if (_type == "error")
            {
                _intype = READOUT_TYPE_ERROR;
            }

            std::string temp_value = flowctrl.getReadoutAll(_intype);
            ESP_LOGD(TAG, "temp_value: %s", temp_value.c_str());

            if (temp_value.length() > 0)
            {
                httpd_resp_send(req, temp_value.c_str(), temp_value.length());
            }

            return ESP_OK;
        }

        std::string *status = flowctrl.getActStatus();
        std::string query = std::string(_query);
        // ESP_LOGD(TAG, "Query: %s, query.c_str());

        if (query.find("full") != std::string::npos)
        {
            std::string txt;
            txt = "<body style=\"font-family: arial\">";

            if ((countRounds <= 1) && (*status != std::string("Flow finished")))
            {
                // First round not completed yet
                txt += "<h3>Please wait for the first round to complete!</h3><h3>Current state: " + *status + "</h3>\n";
            }
            else
            {
                txt += "<h3>Value</h3>";
            }

            httpd_resp_sendstr_chunk(req, txt.c_str());
        }

        std::string temp_value = flowctrl.getReadout(_rawValue, _noerror, 0);

        if (temp_value.length() > 0)
        {
            httpd_resp_sendstr_chunk(req, temp_value.c_str());
        }

        if (query.find("full") != std::string::npos)
        {
            std::string output_string, temp_string;

            if ((countRounds <= 1) && (*status != std::string("Flow finished")))
            {
                // First round not completed yet
                // Nothing to do
            }
            else
            {
                /* Digit ROIs */
                output_string = "<body style=\"font-family: arial\">";
                output_string += "<hr><h3>Recognized Digit ROIs (previous round)</h3>\n";
                output_string += "<table style=\"border-spacing: 5px\"><tr style=\"text-align: center; vertical-align: top;\">\n";

                std::vector<HTMLInfo *> htmlinfo_dig;
                htmlinfo_dig = flowctrl.GetAllDigit();

                for (int i = 0; i < htmlinfo_dig.size(); ++i)
                {
                    if (flowctrl.GetTypeDigit() == Digit)
                    {
                        // Numbers greater than 10 and less than 0 indicate NaN, since a Roi can only have values ​​from 0 to 9.
                        if ((htmlinfo_dig[i]->val >= 10.0) || (htmlinfo_dig[i]->val < 0.0))
                        {
                            temp_string = "NaN";
                        }
                        else
                        {
                            temp_string = std::to_string((int)htmlinfo_dig[i]->val);
                        }

                        output_string += "<td style=\"width: 100px\"><h4>" + temp_string + "</h4><p><img src=\"/img_tmp/" + htmlinfo_dig[i]->filename + "\"></p></td>\n";
                    }
                    else
                    {
                        std::stringstream stream;
                        stream << std::fixed << std::setprecision(2) << htmlinfo_dig[i]->val;
                        temp_string = stream.str();

                        // Numbers greater than 10 and less than 0 indicate NaN, since a Roi can only have values ​​from 0 to 9.
                        if ((std::stof(temp_string) >= 10.00) || (std::stof(temp_string) < 0.00))
                        {
                            temp_string = "NaN";
                        }

                        output_string += "<td style=\"width: 100px\"><h4>" + temp_string + "</h4><p><img src=\"/img_tmp/" + htmlinfo_dig[i]->filename + "\"></p></td>\n";
                    }
                    
                    delete htmlinfo_dig[i];
                }

                htmlinfo_dig.clear();

                output_string += "</tr></table>\n";
                httpd_resp_sendstr_chunk(req, output_string.c_str());

                /* Analog ROIs */
                output_string = "<hr><h3>Recognized Analog ROIs (previous round)</h3>\n";
                output_string += "<table style=\"border-spacing: 5px\"><tr style=\"text-align: center; vertical-align: top;\">\n";

                std::vector<HTMLInfo *> htmlinfo_ana;
                htmlinfo_ana = flowctrl.GetAllAnalog();

                for (int i = 0; i < htmlinfo_ana.size(); ++i)
                {
                    std::stringstream stream;
                    stream << std::fixed << std::setprecision(2) << htmlinfo_ana[i]->val;
                    temp_string = stream.str();

                    // Numbers greater than 10 and less than 0 indicate NaN, since a Roi can only have values ​​from 0 to 9.
                    if ((std::stof(temp_string) >= 10.00) || (std::stof(temp_string) < 0.00))
                    {
                        temp_string = "NaN";
                    }

                    output_string += "<td style=\"width: 150px;\"><h4>" + temp_string + "</h4><p><img src=\"/img_tmp/" + htmlinfo_ana[i]->filename + "\"></p></td>\n";
                    delete htmlinfo_ana[i];
                }

                htmlinfo_ana.clear();

                output_string += "</tr>\n</table>\n";
                httpd_resp_sendstr_chunk(req, output_string.c_str());

                /* Full Image
                 * Only show it after the image got taken */
                output_string = "<hr><h3>Full Image (current round)</h3>\n";

                if ((*status == std::string("Initialization")) || (*status == std::string("Initialization (delayed)")) || (*status == std::string("Take Image")))
                {
                    output_string += "<p>Current state: " + *status + "</p>\n";
                }
                else
                {
                    output_string += "<img src=\"/img_tmp/alg_roi.jpg\">\n";
                }
                httpd_resp_sendstr_chunk(req, output_string.c_str());
            }
        }

        /* Respond with an empty chunk to signal HTTP response completion */
        httpd_resp_sendstr_chunk(req, NULL);
    }
    else
    {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Flow not (yet) started: REST API /value not available!");
        return ESP_ERR_NOT_FOUND;
    }

    return ESP_OK;
}

esp_err_t handler_editflow(httpd_req_t *req)
{
    ESP_LOGD(TAG, "handler_editflow uri: %s", req->uri);

    char _query[512];
    char _valuechar[30];
    std::string _task;

    if (httpd_req_get_url_query_str(req, _query, 512) == ESP_OK)
    {
        if (httpd_query_key_value(_query, "task", _valuechar, 30) == ESP_OK)
        {
            _task = std::string(_valuechar);
        }
    }

    if (_task.compare("namenumbers") == 0)
    {
        ESP_LOGD(TAG, "Get NUMBER list");
        return get_numbers_file_handler(req);
    }

    else if (_task.compare("data") == 0)
    {
        ESP_LOGD(TAG, "Get data list");
        return get_data_file_handler(req);
    }

    else if (_task.compare("tflite") == 0)
    {
        ESP_LOGD(TAG, "Get tflite list");
        return get_tflite_file_handler(req);
    }

    if (_task.compare("copy") == 0)
    {
        std::string in, out, zw;

        httpd_query_key_value(_query, "in", _valuechar, 30);
        in = std::string(_valuechar);
        httpd_query_key_value(_query, "out", _valuechar, 30);
        out = std::string(_valuechar);

        in = "/sdcard" + in;
        out = "/sdcard" + out;

        copy_file(in, out);
        zw = "Copy Done";
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req, zw.c_str(), zw.length());
    }

    if (_task.compare("cutref") == 0)
    {
        std::string in, out, zw;
        int x = 0, y = 0, dx = 20, dy = 20;
        bool enhance = false;

        httpd_query_key_value(_query, "in", _valuechar, 30);
        in = std::string(_valuechar);

        httpd_query_key_value(_query, "out", _valuechar, 30);
        out = std::string(_valuechar);

        httpd_query_key_value(_query, "x", _valuechar, 30);
        std::string _x = std::string(_valuechar);
        if (is_string_numeric(_x))
        {
            x = std::stoi(_x);
        }

        httpd_query_key_value(_query, "y", _valuechar, 30);
        std::string _y = std::string(_valuechar);
        if (is_string_numeric(_y))
        {
            y = std::stoi(_y);
        }

        httpd_query_key_value(_query, "dx", _valuechar, 30);
        std::string _dx = std::string(_valuechar);
        if (is_string_numeric(_dx))
        {
            dx = std::stoi(_dx);
        }

        httpd_query_key_value(_query, "dy", _valuechar, 30);
        std::string _dy = std::string(_valuechar);
        if (is_string_numeric(_dy))
        {
            dy = std::stoi(_dy);
        }

        if (httpd_query_key_value(_query, "enhance", _valuechar, 10) == ESP_OK)
        {
            string _enhance = std::string(_valuechar);

            if (_enhance.compare("true") == 0)
            {
                enhance = true;
            }
        }

        in = "/sdcard" + in;
        out = "/sdcard" + out;

        std::string out2 = out.substr(0, out.length() - 4) + "_org.jpg";

        // if ((flowctrl.SetupModeActive || (*flowctrl.getActStatus() == std::string("Flow finished"))) && psram_init_shared_memory_for_take_image_step())
        if ((flowctrl.SetupModeActive || (*flowctrl.getActStatus() != std::string("Take Image"))) && psram_init_shared_memory_for_take_image_step())
        {
            LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Taking image for Alignment Mark Update...");

            CAlignAndCutImage *caic = new CAlignAndCutImage("cutref", in);
            caic->CutAndSave(out2, x, y, dx, dy);
            delete caic;

            CImageBasis *cim = new CImageBasis("cutref", out2);

            if (enhance)
            {
                cim->Contrast(90);
            }

            cim->SaveToFile(out);
            delete cim;

            psram_deinit_shared_memory_for_take_image_step();
            zw = "CutImage Done";
        }
        else
        {
            LogFile.WriteToFile(ESP_LOG_WARN, TAG, std::string("Taking image for Alignment Mark not possible while device") + " is busy with a round (Current State: '" + *flowctrl.getActStatus() + "')!");
            zw = "Device Busy";
        }

        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req, zw.c_str(), zw.length());
    }

    // wird beim Erstellen eines neuen Referenzbildes aufgerufen
    std::string *sys_status = flowctrl.getActStatus();

    // if ((sys_status->c_str() != std::string("Take Image")) && (sys_status->c_str() != std::string("Aligning")))
    if (sys_status->c_str() != std::string("Take Image"))
    {
        if ((_task.compare("test_take") == 0) || (_task.compare("cam_settings") == 0))
        {
            std::string _host = "";

            // laden der aktuellen Kameraeinstellungen(CCstatus) in den Zwischenspeicher(CFstatus)
            Camera.set_camera_config_from_to(&CCstatus, &CFstatus); // CCstatus >>> CFstatus

            if (httpd_query_key_value(_query, "host", _valuechar, 30) == ESP_OK)
            {
                _host = std::string(_valuechar);
            }

            if (httpd_query_key_value(_query, "waitb", _valuechar, 30) == ESP_OK)
            {
                std::string _waitb = std::string(_valuechar);
                if (is_string_numeric(_waitb))
                {
                    CFstatus.WaitBeforePicture = std::stoi(_valuechar);
                }
            }

            if (httpd_query_key_value(_query, "xclk", _valuechar, sizeof(_valuechar)) == ESP_OK)
            {
                std::string temp_xclk = std::string(_valuechar);
                if (is_string_numeric(temp_xclk))
                {
                    int temp_xclk_ = std::stoi(_valuechar);
                    CFstatus.CamXclkFreqMhz = clip_int(temp_xclk_, 20, 1);
                }
            }

            if (httpd_query_key_value(_query, "aecgc", _valuechar, 30) == ESP_OK)
            {
                std::string _aecgc = std::string(_valuechar);
                if (is_string_numeric(_aecgc))
                {
                    int _aecgc_ = std::stoi(_valuechar);
                    switch (_aecgc_)
                    {
                    case 1:
                        CFstatus.ImageGainceiling = GAINCEILING_4X;
                        break;
                    case 2:
                        CFstatus.ImageGainceiling = GAINCEILING_8X;
                        break;
                    case 3:
                        CFstatus.ImageGainceiling = GAINCEILING_16X;
                        break;
                    case 4:
                        CFstatus.ImageGainceiling = GAINCEILING_32X;
                        break;
                    case 5:
                        CFstatus.ImageGainceiling = GAINCEILING_64X;
                        break;
                    case 6:
                        CFstatus.ImageGainceiling = GAINCEILING_128X;
                        break;
                    default:
                        CFstatus.ImageGainceiling = GAINCEILING_2X;
                    }
                }
                else
                {
                    if (_aecgc == "X4")
                    {
                        CFstatus.ImageGainceiling = GAINCEILING_4X;
                    }
                    else if (_aecgc == "X8")
                    {
                        CFstatus.ImageGainceiling = GAINCEILING_8X;
                    }
                    else if (_aecgc == "X16")
                    {
                        CFstatus.ImageGainceiling = GAINCEILING_16X;
                    }
                    else if (_aecgc == "X32")
                    {
                        CFstatus.ImageGainceiling = GAINCEILING_32X;
                    }
                    else if (_aecgc == "X64")
                    {
                        CFstatus.ImageGainceiling = GAINCEILING_64X;
                    }
                    else if (_aecgc == "X128")
                    {
                        CFstatus.ImageGainceiling = GAINCEILING_128X;
                    }
                    else
                    {
                        CFstatus.ImageGainceiling = GAINCEILING_2X;
                    }
                }
            }

            if (httpd_query_key_value(_query, "qual", _valuechar, 30) == ESP_OK)
            {
                std::string _qual = std::string(_valuechar);
                if (is_string_numeric(_qual))
                {
                    int _qual_ = std::stoi(_valuechar);
                    CFstatus.ImageQuality = clip_int(_qual_, 63, 6);
                }
            }

            if (httpd_query_key_value(_query, "bri", _valuechar, 30) == ESP_OK)
            {
                std::string _bri = std::string(_valuechar);
                if (is_string_numeric(_bri))
                {
                    int _bri_ = std::stoi(_valuechar);
                    CFstatus.ImageBrightness = clip_int(_bri_, 2, -2);
                }
            }

            if (httpd_query_key_value(_query, "con", _valuechar, 30) == ESP_OK)
            {
                std::string _con = std::string(_valuechar);
                if (is_string_numeric(_con))
                {
                    int _con_ = std::stoi(_valuechar);
                    CFstatus.ImageContrast = clip_int(_con_, 2, -2);
                }
            }

            if (httpd_query_key_value(_query, "sat", _valuechar, 30) == ESP_OK)
            {
                std::string _sat = std::string(_valuechar);
                if (is_string_numeric(_sat))
                {
                    int _sat_ = std::stoi(_valuechar);
                    CFstatus.ImageSaturation = clip_int(_sat_, 2, -2);
                }
            }

            if (httpd_query_key_value(_query, "shp", _valuechar, 30) == ESP_OK)
            {
                std::string _shp = std::string(_valuechar);
                if (is_string_numeric(_shp))
                {
                    int _shp_ = std::stoi(_valuechar);
                    if (Camera.CamSensorId == OV2640_PID)
                    {
                        CFstatus.ImageSharpness = clip_int(_shp_, 2, -2);
                    }
                    else
                    {
                        CFstatus.ImageSharpness = clip_int(_shp_, 3, -3);
                    }
                }
            }

            if (httpd_query_key_value(_query, "ashp", _valuechar, 30) == ESP_OK)
            {
                std::string _ashp = std::string(_valuechar);
                CFstatus.ImageAutoSharpness = alphanumeric_to_boolean(_ashp);
            }

            if (httpd_query_key_value(_query, "spe", _valuechar, 30) == ESP_OK)
            {
                std::string _spe = std::string(_valuechar);
                if (is_string_numeric(_spe))
                {
                    int _spe_ = std::stoi(_valuechar);
                    CFstatus.ImageSpecialEffect = clip_int(_spe_, 6, 0);
                }
                else
                {
                    if (_spe == "negative")
                    {
                        CFstatus.ImageSpecialEffect = 1;
                    }
                    else if (_spe == "grayscale")
                    {
                        CFstatus.ImageSpecialEffect = 2;
                    }
                    else if (_spe == "red")
                    {
                        CFstatus.ImageSpecialEffect = 3;
                    }
                    else if (_spe == "green")
                    {
                        CFstatus.ImageSpecialEffect = 4;
                    }
                    else if (_spe == "blue")
                    {
                        CFstatus.ImageSpecialEffect = 5;
                    }
                    else if (_spe == "retro")
                    {
                        CFstatus.ImageSpecialEffect = 6;
                    }
                    else
                    {
                        CFstatus.ImageSpecialEffect = 0;
                    }
                }
            }

            if (httpd_query_key_value(_query, "wbm", _valuechar, 30) == ESP_OK)
            {
                std::string _wbm = std::string(_valuechar);
                if (is_string_numeric(_wbm))
                {
                    int _wbm_ = std::stoi(_valuechar);
                    CFstatus.ImageWbMode = clip_int(_wbm_, 4, 0);
                }
                else
                {
                    if (_wbm == "sunny")
                    {
                        CFstatus.ImageWbMode = 1;
                    }
                    else if (_wbm == "cloudy")
                    {
                        CFstatus.ImageWbMode = 2;
                    }
                    else if (_wbm == "office")
                    {
                        CFstatus.ImageWbMode = 3;
                    }
                    else if (_wbm == "home")
                    {
                        CFstatus.ImageWbMode = 4;
                    }
                    else
                    {
                        CFstatus.ImageWbMode = 0;
                    }
                }
            }

            if (httpd_query_key_value(_query, "awb", _valuechar, 30) == ESP_OK)
            {
                std::string _awb = std::string(_valuechar);
                CFstatus.ImageAwb = alphanumeric_to_boolean(_awb);
            }

            if (httpd_query_key_value(_query, "awbg", _valuechar, 30) == ESP_OK)
            {
                std::string _awbg = std::string(_valuechar);
                CFstatus.ImageAwbGain = alphanumeric_to_boolean(_awbg);
            }

            if (httpd_query_key_value(_query, "aec", _valuechar, 30) == ESP_OK)
            {
                std::string _aec = std::string(_valuechar);
                CFstatus.ImageAec = alphanumeric_to_boolean(_aec);
            }

            if (httpd_query_key_value(_query, "aec2", _valuechar, 30) == ESP_OK)
            {
                std::string _aec2 = std::string(_valuechar);
                CFstatus.ImageAec2 = alphanumeric_to_boolean(_aec2);
            }

            if (httpd_query_key_value(_query, "ael", _valuechar, 30) == ESP_OK)
            {
                std::string _ael = std::string(_valuechar);
                if (is_string_numeric(_ael))
                {
                    int _ael_ = std::stoi(_valuechar);
                    if (Camera.CamSensorId == OV2640_PID)
                    {
                        CFstatus.ImageAeLevel = clip_int(_ael_, 2, -2);
                    }
                    else
                    {
                        CFstatus.ImageAeLevel = clip_int(_ael_, 5, -5);
                    }
                }
            }

            if (httpd_query_key_value(_query, "aecv", _valuechar, 30) == ESP_OK)
            {
                std::string _aecv = std::string(_valuechar);
                if (is_string_numeric(_aecv))
                {
                    int _aecv_ = std::stoi(_valuechar);
                    CFstatus.ImageAecValue = clip_int(_aecv_, 1200, 0);
                }
            }

            if (httpd_query_key_value(_query, "agc", _valuechar, 30) == ESP_OK)
            {
                std::string _agc = std::string(_valuechar);
                CFstatus.ImageAgc = alphanumeric_to_boolean(_agc);
            }

            if (httpd_query_key_value(_query, "agcg", _valuechar, 30) == ESP_OK)
            {
                std::string _agcg = std::string(_valuechar);
                if (is_string_numeric(_agcg))
                {
                    int _agcg_ = std::stoi(_valuechar);
                    CFstatus.ImageAgcGain = clip_int(_agcg_, 30, 0);
                }
            }

            if (httpd_query_key_value(_query, "bpc", _valuechar, 30) == ESP_OK)
            {
                std::string _bpc = std::string(_valuechar);
                CFstatus.ImageBpc = alphanumeric_to_boolean(_bpc);
            }

            if (httpd_query_key_value(_query, "wpc", _valuechar, 30) == ESP_OK)
            {
                std::string _wpc = std::string(_valuechar);
                CFstatus.ImageWpc = alphanumeric_to_boolean(_wpc);
            }

            if (httpd_query_key_value(_query, "rgma", _valuechar, 30) == ESP_OK)
            {
                std::string _rgma = std::string(_valuechar);
                CFstatus.ImageRawGma = alphanumeric_to_boolean(_rgma);
            }

            if (httpd_query_key_value(_query, "lenc", _valuechar, 30) == ESP_OK)
            {
                std::string _lenc = std::string(_valuechar);
                CFstatus.ImageLenc = alphanumeric_to_boolean(_lenc);
            }

            if (httpd_query_key_value(_query, "mirror", _valuechar, 30) == ESP_OK)
            {
                std::string _mirror = std::string(_valuechar);
                CFstatus.ImageHmirror = alphanumeric_to_boolean(_mirror);
            }

            if (httpd_query_key_value(_query, "flip", _valuechar, 30) == ESP_OK)
            {
                std::string _flip = std::string(_valuechar);
                CFstatus.ImageVflip = alphanumeric_to_boolean(_flip);
            }

            if (httpd_query_key_value(_query, "dcw", _valuechar, 30) == ESP_OK)
            {
                std::string _dcw = std::string(_valuechar);
                CFstatus.ImageDcw = alphanumeric_to_boolean(_dcw);
            }

            if (httpd_query_key_value(_query, "den", _valuechar, 30) == ESP_OK)
            {
                std::string _idlv = std::string(_valuechar);
                if (is_string_numeric(_idlv))
                {
                    int _ImageDenoiseLevel = std::stoi(_valuechar);
                    if (Camera.CamSensorId == OV2640_PID)
                    {
                        CFstatus.ImageDenoiseLevel = 0;
                    }
                    else
                    {
                        CFstatus.ImageDenoiseLevel = clip_int(_ImageDenoiseLevel, 8, 0);
                    }
                }
            }

            if (httpd_query_key_value(_query, "zoom", _valuechar, 30) == ESP_OK)
            {
                std::string _zoom = std::string(_valuechar);
                CFstatus.ImageZoomEnabled = alphanumeric_to_boolean(_zoom);
            }

            if (httpd_query_key_value(_query, "zoomx", _valuechar, 30) == ESP_OK)
            {
                std::string _zoomx = std::string(_valuechar);
                if (is_string_numeric(_zoomx))
                {
                    int _ImageZoomOffsetX = std::stoi(_valuechar);
                    if (Camera.CamSensorId == OV2640_PID)
                    {
                        CFstatus.ImageZoomOffsetX = clip_int(_ImageZoomOffsetX, 480, -480);
                    }
                    else if (Camera.CamSensorId == OV3660_PID)
                    {
                        CFstatus.ImageZoomOffsetX = clip_int(_ImageZoomOffsetX, 704, -704);
                    }
                    else if (Camera.CamSensorId == OV5640_PID)
                    {
                        CFstatus.ImageZoomOffsetX = clip_int(_ImageZoomOffsetX, 960, -960);
                    }
                }
            }

            if (httpd_query_key_value(_query, "zoomy", _valuechar, 30) == ESP_OK)
            {
                std::string _zoomy = std::string(_valuechar);
                if (is_string_numeric(_zoomy))
                {
                    int _ImageZoomOffsetY = std::stoi(_valuechar);
                    if (Camera.CamSensorId == OV2640_PID)
                    {
                        CFstatus.ImageZoomOffsetY = clip_int(_ImageZoomOffsetY, 360, -360);
                    }
                    else if (Camera.CamSensorId == OV3660_PID)
                    {
                        CFstatus.ImageZoomOffsetY = clip_int(_ImageZoomOffsetY, 528, -528);
                    }
                    else if (Camera.CamSensorId == OV5640_PID)
                    {
                        CFstatus.ImageZoomOffsetY = clip_int(_ImageZoomOffsetY, 720, -720);
                    }
                }
            }

            if (httpd_query_key_value(_query, "zooms", _valuechar, 30) == ESP_OK)
            {
                std::string _zooms = std::string(_valuechar);
                if (is_string_numeric(_zooms))
                {
                    int _ImageZoomSize = std::stoi(_valuechar);
                    if (Camera.CamSensorId == OV2640_PID)
                    {
                        CFstatus.ImageZoomSize = clip_int(_ImageZoomSize, 29, 0);
                    }
                    else if (Camera.CamSensorId == OV3660_PID)
                    {
                        CFstatus.ImageZoomSize = clip_int(_ImageZoomSize, 43, 0);
                    }
                    else if (Camera.CamSensorId == OV5640_PID)
                    {
                        CFstatus.ImageZoomSize = clip_int(_ImageZoomSize, 59, 0);
                    }
                }
            }

            if (httpd_query_key_value(_query, "ledi", _valuechar, 30) == ESP_OK)
            {
                std::string _ledi = std::string(_valuechar);
                if (is_string_numeric(_ledi))
                {
                    int _ImageLedIntensity = std::stoi(_valuechar);
                    CFstatus.ImageLedIntensity = Camera.set_led_intensity(_ImageLedIntensity);
                }
            }

            if (_task.compare("cam_settings") == 0)
            {
                // wird aufgerufen, wenn das Referenzbild + Kameraeinstellungen gespeichert wurden
                Camera.set_camera_config_from_to(&CFstatus, &CCstatus); // CFstatus >>> CCstatus

                // Kameraeinstellungen wurden verädert
                Camera.changedCameraSettings = true;
                Camera.CamTempImage = false;

                ESP_LOGD(TAG, "Cam Settings set");
                std::string _zw = "CamSettingsSet";
                httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
                httpd_resp_send(req, _zw.c_str(), _zw.length());
            }
            else
            {
                // wird aufgerufen, wenn ein neues Referenzbild erstellt oder aktualisiert wurde
                // Kameraeinstellungen wurden verädert
                Camera.changedCameraSettings = true;
                Camera.CamTempImage = true;

                ESP_LOGD(TAG, "test_take - vor TakeImage");
                std::string image_temp = flowctrl.doSingleStep("[TakeImage]", _host);
                httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
                httpd_resp_send(req, image_temp.c_str(), image_temp.length());
            }
        }

        if (_task.compare("test_align") == 0)
        {
            std::string _host = "";

            if (httpd_query_key_value(_query, "host", _valuechar, 30) == ESP_OK)
            {
                _host = std::string(_valuechar);
            }

            std::string zw = flowctrl.doSingleStep("[Alignment]", _host);
            httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
            httpd_resp_send(req, zw.c_str(), zw.length());
        }
    }
    else
    {
        std::string _zw = "DeviceIsBusy";
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req, _zw.c_str(), _zw.length());
    }

    return ESP_OK;
}

esp_err_t handler_statusflow(httpd_req_t *req)
{
    const char *resp_str;
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    if (bTaskAutoFlowCreated)
    {
        string *zw = flowctrl.getActStatusWithTime();
        resp_str = zw->c_str();

        httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    }
    else
    {
        resp_str = "Flow task not yet created";
        httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    }

    return ESP_OK;
}

esp_err_t handler_cputemp(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, std::to_string((int)read_tempsensor()).c_str(), HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

esp_err_t handler_rssi(httpd_req_t *req)
{
    if (get_wifi_sta_is_connected())
    {
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req, std::to_string(get_wifi_rssi()).c_str(), HTTPD_RESP_USE_STRLEN);
    }
    else
    {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "WIFI not (yet) connected: REST API /rssi not available!");
        return ESP_ERR_NOT_FOUND;
    }

    return ESP_OK;
}

esp_err_t handler_current_date(httpd_req_t *req)
{
    std::string formatedDateAndTime = getCurrentTimeString("%Y-%m-%d %H:%M:%S");
    // std::string formatedDate = getCurrentTimeString("%Y-%m-%d");

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, formatedDateAndTime.c_str(), formatedDateAndTime.length());

    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_sendstr_chunk(req, NULL);

    return ESP_OK;
}

esp_err_t handler_uptime(httpd_req_t *req)
{
    std::string formatedUptime = get_formated_uptime(false);

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, formatedUptime.c_str(), formatedUptime.length());

    return ESP_OK;
}

esp_err_t handler_prevalue(httpd_req_t *req)
{
    // Default usage message when handler gets called without any parameter
    const std::string RESTUsageInfo =
        "00: Handler usage:<br>"
        "- To retrieve actual PreValue, please provide only a numbersname, e.g. /setPreValue?numbers=main<br>"
        "- To set PreValue to a new value, please provide a numbersname and a value, e.g. /setPreValue?numbers=main&value=1234.5678<br>"
        "NOTE:<br>"
        "value >= 0.0: Set PreValue to provided value<br>"
        "value <  0.0: Set PreValue to actual RAW value (as long RAW value is a valid number, without N)";

    // Default return error message when no return is programmed
    std::string sReturnMessage = "E90: Uninitialized";

    char _query[100];
    char _numbersname[50] = "default";
    char _value[20] = "";

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    if (httpd_req_get_url_query_str(req, _query, 100) == ESP_OK)
    {
        if (httpd_query_key_value(_query, "numbers", _numbersname, 50) != ESP_OK)
        {
            // If request is incomplete
            sReturnMessage = "E91: Query parameter incomplete or not valid!<br> "
                             "Call /setPreValue to show REST API usage info and/or check documentation";
            httpd_resp_send(req, sReturnMessage.c_str(), sReturnMessage.length());
            return ESP_FAIL;
        }

        if (httpd_query_key_value(_query, "value", _value, 20) == ESP_OK)
        {
            ESP_LOGD(TAG, "handler_prevalue() - Value: %s", _value);
        }
    }
    else
    {
        // if no parameter is provided, print handler usage
        httpd_resp_send(req, RESTUsageInfo.c_str(), RESTUsageInfo.length());
        return ESP_OK;
    }

    if (strlen(_value) == 0)
    {
        // If no value is povided --> return actual PreValue
        sReturnMessage = flowctrl.GetPrevalue(std::string(_numbersname));

        if (sReturnMessage.empty())
        {
            sReturnMessage = "E92: Numbers name not found";
            httpd_resp_send(req, sReturnMessage.c_str(), sReturnMessage.length());
            return ESP_FAIL;
        }
    }
    else
    {
        // New value is positive: Set PreValue to provided value and return value
        // New value is negative and actual RAW value is a valid number: Set PreValue to RAW value and return value
        LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "REST API handler_prevalue called: numbersname: " + std::string(_numbersname) + ", value: " + std::string(_value));

        if (!flowctrl.UpdatePrevalue(_value, _numbersname, true))
        {
            sReturnMessage = "E93: Update request rejected. Please check device logs for more details";
            httpd_resp_send(req, sReturnMessage.c_str(), sReturnMessage.length());
            return ESP_FAIL;
        }

        sReturnMessage = flowctrl.GetPrevalue(std::string(_numbersname));

        if (sReturnMessage.empty())
        {
            sReturnMessage = "E94: Numbers name not found";
            httpd_resp_send(req, sReturnMessage.c_str(), sReturnMessage.length());
            return ESP_FAIL;
        }
    }

    httpd_resp_send(req, sReturnMessage.c_str(), sReturnMessage.length());

    return ESP_OK;
}

void task_autodoFlow(void *pvParameter)
{
    int64_t fr_start, fr_delta_ms;

    bTaskAutoFlowCreated = true;

    if (!isPlannedReboot && (esp_reset_reason() == ESP_RST_PANIC))
    {
        flowctrl.setActStatus("Initialization (delayed)");
        vTaskDelay(60 * 5000 / portTICK_PERIOD_MS); // Wait 5 minutes to give time to do an OTA update or fetch the log
    }

    ESP_LOGD(TAG, "task_autodoFlow(): start");
    doInit();

    flowctrl.setAutoStartInterval(auto_interval);
    autostartIsEnabled = flowctrl.getIsAutoStart();

    if (isSetupModusActive())
    {
        LogFile.WriteToFile(ESP_LOG_INFO, TAG, "task_autodoFlow(): We are in Setup Mode -> Not starting Auto Flow!");
        autostartIsEnabled = false;
    }

    if (autostartIsEnabled)
    {
        LogFile.WriteToFile(ESP_LOG_INFO, TAG, "task_autodoFlow(): Starting Flow...");
    }
    else
    {
        LogFile.WriteToFile(ESP_LOG_INFO, TAG, "task_autodoFlow(): Autostart is not enabled -> Not starting Flow");
    }

    while (autostartIsEnabled)
    {
        LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "----------------------------------------------------------------"); // Clear separation between runs
        time_t roundStartTime = get_uptime();

        std::string _zw = "Round #" + std::to_string(++countRounds) + " started";
        LogFile.WriteToFile(ESP_LOG_INFO, TAG, _zw);

        fr_start = esp_timer_get_time();

        if (flowisrunning)
        {
            ESP_LOGD(TAG, "task_autodoFlow(): doFlow is already running!");
        }
        else
        {
            ESP_LOGD(TAG, "task_autodoFlow(): doFlow is started");
            flowisrunning = true;
            doflow();

            ESP_LOGD(TAG, "task_autodoFlow(): Remove older log files");
            LogFile.RemoveOldLogFile();
            LogFile.RemoveOldDataLog();
        }

        // Round finished -> Logfile
        LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Round #" + std::to_string(countRounds) + " completed (" + std::to_string(get_uptime() - roundStartTime) + " seconds)");

        // CPU Temp -> Logfile
        LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "CPU Temperature: " + std::to_string((int)read_tempsensor()) + "°C");

        if (network_config.connection_type != NETWORK_CONNECTION_DISCONNECT)
        {
            // WIFI Signal Strength (RSSI) -> Logfile
            LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "WIFI Signal (RSSI): " + std::to_string(get_wifi_rssi()) + "dBm");

            // Check if time is synchronized (if NTP is configured)
            if (getUseNtp() && !getTimeIsSet())
            {
                LogFile.WriteToFile(ESP_LOG_WARN, TAG, "task_autodoFlow(): Time server is configured, but time is not yet set!");
                set_status_led(TIME_CHECK, 1, false);
            }

#if (defined WLAN_USE_MESH_ROAMING && defined WLAN_USE_MESH_ROAMING_ACTIVATE_CLIENT_TRIGGERED_QUERIES)
            wifiRoamingQuery();
#endif

// Scan channels and check if an AP with better RSSI is available, then disconnect and try to reconnect to AP with better RSSI
// NOTE: Keep this direct before the following task delay, because scan is done in blocking mode and this takes ca. 1,5 - 2s.
#ifdef WLAN_USE_ROAMING_BY_SCANNING
            wifi_roaming_by_scanning();
#endif
        }

        fr_delta_ms = (esp_timer_get_time() - fr_start) / 1000;

        if (auto_interval > fr_delta_ms)
        {
            const TickType_t xDelay = (auto_interval - fr_delta_ms) / portTICK_PERIOD_MS;
            ESP_LOGD(TAG, "task_autodoFlow(): sleep for: %ldms", (long)xDelay);
            vTaskDelay(xDelay);
        }
    }

    while (1)
    {
        // Keep flow task running to handle necessary sub tasks like reboot handler, etc..
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL); // Delete this task if it exits from the loop above
    xHandletask_autodoFlow = NULL;

    ESP_LOGD(TAG, "task_autodoFlow(): end");
}

void InitializeFlowTask(void)
{
    BaseType_t xReturned;

    ESP_LOGD(TAG, "getESPHeapInfo: %s", get_heapinfo().c_str());

    uint32_t stackSize = 16 * 1024;
    xReturned = xTaskCreatePinnedToCore(&task_autodoFlow, "task_autodoFlow", stackSize, NULL, tskIDLE_PRIORITY + 2, &xHandletask_autodoFlow, 0);

    if (xReturned != pdPASS)
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Creation task_autodoFlow failed. Requested stack size:" + std::to_string(stackSize));
        LogFile.WriteHeapInfo("Creation task_autodoFlow failed");
    }

    ESP_LOGD(TAG, "getESPHeapInfo: %s", get_heapinfo().c_str());
}

void main_flow_register_uri(httpd_handle_t server)
{
    ESP_LOGI(TAG, "server_main_flow_task - Registering URI handlers");

    httpd_uri_t camuri = {};
    camuri.method = HTTP_GET;

    camuri.uri = "/doinit";
    camuri.handler = APPLY_BASIC_AUTH_FILTER(handler_init);
    camuri.user_ctx = (void *)"Light On";
    httpd_register_uri_handler(server, &camuri);

    // Legacy API => New: "/setPreValue"
    camuri.uri = "/setPreValue.html";
    camuri.handler = APPLY_BASIC_AUTH_FILTER(handler_prevalue);
    camuri.user_ctx = (void *)"Prevalue";
    httpd_register_uri_handler(server, &camuri);

    camuri.uri = "/setPreValue";
    camuri.handler = APPLY_BASIC_AUTH_FILTER(handler_prevalue);
    camuri.user_ctx = (void *)"Prevalue";
    httpd_register_uri_handler(server, &camuri);

    camuri.uri = "/flow_start";
    camuri.handler = APPLY_BASIC_AUTH_FILTER(handler_flow_start);
    camuri.user_ctx = (void *)"Flow Start";
    httpd_register_uri_handler(server, &camuri);

    camuri.uri = "/statusflow.html";
    camuri.handler = APPLY_BASIC_AUTH_FILTER(handler_statusflow);
    camuri.user_ctx = (void *)"Light Off";
    httpd_register_uri_handler(server, &camuri);

    camuri.uri = "/statusflow";
    camuri.handler = APPLY_BASIC_AUTH_FILTER(handler_statusflow);
    camuri.user_ctx = (void *)"Light Off";
    httpd_register_uri_handler(server, &camuri);

    // Legacy API => New: "/cpu_temperature"
    camuri.uri = "/cputemp.html";
    camuri.handler = APPLY_BASIC_AUTH_FILTER(handler_cputemp);
    camuri.user_ctx = (void *)"Light Off";
    httpd_register_uri_handler(server, &camuri);

    camuri.uri = "/cpu_temperature";
    camuri.handler = APPLY_BASIC_AUTH_FILTER(handler_cputemp);
    camuri.user_ctx = (void *)"Light Off";
    httpd_register_uri_handler(server, &camuri);

    // Legacy API => New: "/rssi"
    camuri.uri = "/rssi.html";
    camuri.handler = APPLY_BASIC_AUTH_FILTER(handler_rssi);
    camuri.user_ctx = (void *)"Light Off";
    httpd_register_uri_handler(server, &camuri);

    camuri.uri = "/rssi";
    camuri.handler = APPLY_BASIC_AUTH_FILTER(handler_rssi);
    camuri.user_ctx = (void *)"Light Off";
    httpd_register_uri_handler(server, &camuri);

    camuri.uri = "/date";
    camuri.handler = APPLY_BASIC_AUTH_FILTER(handler_current_date);
    camuri.user_ctx = (void *)"Light Off";
    httpd_register_uri_handler(server, &camuri);

    camuri.uri = "/uptime";
    camuri.handler = APPLY_BASIC_AUTH_FILTER(handler_uptime);
    camuri.user_ctx = (void *)"Light Off";
    httpd_register_uri_handler(server, &camuri);

    camuri.uri = "/editflow";
    camuri.handler = APPLY_BASIC_AUTH_FILTER(handler_editflow);
    camuri.user_ctx = (void *)"EditFlow";
    httpd_register_uri_handler(server, &camuri);

    // Legacy API => New: "/value"
    camuri.uri = "/value.html";
    camuri.handler = APPLY_BASIC_AUTH_FILTER(handler_value);
    camuri.user_ctx = (void *)"Value";
    httpd_register_uri_handler(server, &camuri);

    camuri.uri = "/value";
    camuri.handler = APPLY_BASIC_AUTH_FILTER(handler_value);
    camuri.user_ctx = (void *)"Value";
    httpd_register_uri_handler(server, &camuri);

    // Legacy API => New: "/value"
    camuri.uri = "/wasserzaehler.html";
    camuri.handler = APPLY_BASIC_AUTH_FILTER(handler_value);
    camuri.user_ctx = (void *)"Wasserzaehler";
    httpd_register_uri_handler(server, &camuri);

    camuri.uri = "/json";
    camuri.handler = APPLY_BASIC_AUTH_FILTER(handler_json);
    camuri.user_ctx = (void *)"JSON";
    httpd_register_uri_handler(server, &camuri);

    camuri.uri = "/heap";
    camuri.handler = APPLY_BASIC_AUTH_FILTER(handler_get_heap);
    camuri.user_ctx = (void *)"Heap";
    httpd_register_uri_handler(server, &camuri);

    camuri.uri = "/stream";
    camuri.handler = APPLY_BASIC_AUTH_FILTER(handler_stream);
    camuri.user_ctx = (void *)"stream";
    httpd_register_uri_handler(server, &camuri);

    /** will handle metrics requests */
    camuri.uri = "/metrics";
    camuri.handler = APPLY_BASIC_AUTH_FILTER(handler_openmetrics);
    camuri.user_ctx = (void *)"metrics";
    httpd_register_uri_handler(server, &camuri);

    /** when adding a new handler, make sure to increment the value for config.max_uri_handlers in `main/server_main.cpp` */
}
