#include "defines.h"

#include "ClassFlowControll.h"

#include "connect_wifi_sta.h"
#include "read_network_config.h"

#include "freertos/task.h"

#include <sys/stat.h>
#include <dirent.h>

#include "ClassLogFile.h"
#include "time_sntp.h"
#include "Helper.h"
#include "server_ota.h"
#include "interface_mqtt.h"
#include "server_mqtt.h"

#include "server_help.h"
#include "MainFlowControl.h"
#include "basic_auth.h"

static const char *TAG = "FLOWCTRL";

std::string ClassFlowControll::doSingleStep(std::string _stepname, std::string _host)
{
    std::string _classname = "";
    std::string result = "";

    ESP_LOGD(TAG, "Step %s start", _stepname.c_str());

    if ((_stepname.compare("[TakeImage]") == 0) || (_stepname.compare(";[TakeImage]") == 0))
    {
        _classname = "ClassFlowTakeImage";
    }
    else if ((_stepname.compare("[Alignment]") == 0) || (_stepname.compare(";[Alignment]") == 0))
    {
        _classname = "ClassFlowAlignment";
    }
    else if ((_stepname.compare(0, 7, "[Digits") == 0) || (_stepname.compare(0, 8, ";[Digits") == 0))
    {
        _classname = "ClassFlowCNNGeneral";
    }
    else if ((_stepname.compare("[Analog]") == 0) || (_stepname.compare(";[Analog]") == 0))
    {
        _classname = "ClassFlowCNNGeneral";
    }
    else if ((_stepname.compare("[MQTT]") == 0) || (_stepname.compare(";[MQTT]") == 0))
    {
        _classname = "ClassFlowMQTT";
    }
    else if ((_stepname.compare("[InfluxDB]") == 0) || (_stepname.compare(";[InfluxDB]") == 0))
    {
        _classname = "ClassFlowInfluxDB";
    }
    else if ((_stepname.compare("[InfluxDBv2]") == 0) || (_stepname.compare(";[InfluxDBv2]") == 0))
    {
        _classname = "ClassFlowInfluxDBv2";
    }
    else if ((_stepname.compare("[Webhook]") == 0) || (_stepname.compare(";[Webhook]") == 0))
    {
        _classname = "ClassFlowWebhook";
    }

    for (int i = 0; i < FlowControll.size(); ++i)
    {
        if (FlowControll[i]->name().compare(_classname) == 0)
        {
            if (!(FlowControll[i]->name().compare("ClassFlowTakeImage") == 0))
            {
                // if it is a TakeImage, the image does not need to be included, this happens automatically with the html query.
                FlowControll[i]->doFlow("");
            }

            result = FlowControll[i]->getHTMLSingleStep(_host);
        }
    }

    ESP_LOGD(TAG, "Step %s end", _stepname.c_str());

    return result;
}

std::string ClassFlowControll::TranslateAktstatus(std::string _input)
{
    if (_input.compare("ClassFlowTakeImage") == 0)
    {
        return ("Take Image");
    }
    else if (_input.compare("ClassFlowAlignment") == 0)
    {
        return ("Aligning");
    }
    else if (_input.compare("ClassFlowCNNGeneral") == 0)
    {
        return ("Digitization of ROIs");
    }
    else if (_input.compare("ClassFlowMQTT") == 0)
    {
        return ("Sending MQTT");
    }
    else if (_input.compare("ClassFlowInfluxDB") == 0)
    {
        return ("Sending InfluxDB");
    }
    else if (_input.compare("ClassFlowInfluxDBv2") == 0)
    {
        return ("Sending InfluxDBv2");
    }
    else if (_input.compare("ClassFlowWebhook") == 0)
    {
        return ("Sending Webhook");
    }
    else if (_input.compare("ClassFlowPostProcessing") == 0)
    {
        return ("Post-Processing");
    }

    return "Unkown Status";
}

std::vector<HTMLInfo *> ClassFlowControll::GetAllDigit()
{
    if (flowdigit)
    {
        ESP_LOGD(TAG, "ClassFlowControll::GetAllDigit - flowdigit != NULL");
        return flowdigit->GetHTMLInfo();
    }

    std::vector<HTMLInfo *> empty;
    return empty;
}

std::vector<HTMLInfo *> ClassFlowControll::GetAllAnalog()
{
    if (flowanalog)
    {
        return flowanalog->GetHTMLInfo();
    }

    std::vector<HTMLInfo *> empty;
    return empty;
}

t_CNNType ClassFlowControll::GetTypeDigit()
{
    if (flowdigit)
    {
        return flowdigit->getCNNType();
    }

    return t_CNNType::None;
}

t_CNNType ClassFlowControll::GetTypeAnalog()
{
    if (flowanalog)
    {
        return flowanalog->getCNNType();
    }

    return t_CNNType::None;
}

#ifdef ALGROI_LOAD_FROM_MEM_AS_JPG
void ClassFlowControll::DigitDrawROI(CImageBasis *TempImage)
{
    if (flowdigit)
    {
        flowdigit->DrawROI(TempImage);
    }
}

void ClassFlowControll::AnalogDrawROI(CImageBasis *TempImage)
{
    if (flowanalog)
    {
        flowanalog->DrawROI(TempImage);
    }
}
#endif

bool ClassFlowControll::StartMQTTService()
{
    /* Start the MQTT service */
    for (int i = 0; i < FlowControll.size(); ++i)
    {
        if (FlowControll[i]->name().compare("ClassFlowMQTT") == 0)
        {
            return ((ClassFlowMQTT *)(FlowControll[i]))->Start(AutoInterval);
        }
    }
    return false;
}

void ClassFlowControll::SetInitialParameter(void)
{
    AutoStart = true;
    SetupModeActive = false;
    AutoInterval = 10; // Minutes
    flowdigit = NULL;
    flowanalog = NULL;
    flowpostprocessing = NULL;

    aktRunNr = 0;
    aktstatus = "Flow task not yet created";
    aktstatusWithTime = aktstatus;

    disabled = false;
}

bool ClassFlowControll::getIsAutoStart(void)
{
    // return AutoStart;
    return true; // Flow must always be enabled, else the manual trigger (REST, MQTT) will not work!
}

void ClassFlowControll::setAutoStartInterval(long &_interval)
{
    _interval = AutoInterval * 60 * 1000; // AutoInterval: minutes -> ms
}

ClassFlow *ClassFlowControll::CreateClassFlow(std::string _type)
{
    ClassFlow *cfc = NULL;
    _type = to_upper(trim_string_left_right(_type));

    if (_type.compare("[TAKEIMAGE]") == 0)
    {
        cfc = new ClassFlowTakeImage(&FlowControll);
        flowtakeimage = (ClassFlowTakeImage *)cfc;
    }

    else if (_type.compare("[ALIGNMENT]") == 0)
    {
        cfc = new ClassFlowAlignment(&FlowControll);
        flowalignment = (ClassFlowAlignment *)cfc;
    }

    else if (_type.compare("[ANALOG]") == 0)
    {
        cfc = new ClassFlowCNNGeneral(flowalignment);
        flowanalog = (ClassFlowCNNGeneral *)cfc;
    }

    else if (_type.compare(0, 7, "[DIGITS") == 0)
    {
        cfc = new ClassFlowCNNGeneral(flowalignment);
        flowdigit = (ClassFlowCNNGeneral *)cfc;
    }

    else if (_type.compare("[MQTT]") == 0)
    {
        cfc = new ClassFlowMQTT(&FlowControll);
    }

    else if (_type.compare("[INFLUXDB]") == 0)
    {
        cfc = new ClassFlowInfluxDB(&FlowControll);
    }

    else if (_type.compare("[INFLUXDBV2]") == 0)
    {
        cfc = new ClassFlowInfluxDBv2(&FlowControll);
    }

    else if (_type.compare("[WEBHOOK]") == 0)
    {
        cfc = new ClassFlowWebhook(&FlowControll);
    }

    else if (_type.compare("[POSTPROCESSING]") == 0)
    {
        cfc = new ClassFlowPostProcessing(&FlowControll, flowanalog, flowdigit);
        flowpostprocessing = (ClassFlowPostProcessing *)cfc;
    }

    if (cfc)
    {
        // Attached only if it is not [AutoTimer], because this is for FlowControll
        FlowControll.push_back(cfc);
    }

    if (_type.compare("[AUTOTIMER]") == 0)
    {
        cfc = this;
    }

    else if (_type.compare("[DATALOGGING]") == 0)
    {
        cfc = this;
    }

    else if (_type.compare("[DEBUG]") == 0)
    {
        cfc = this;
    }

    else if (_type.compare("[SYSTEM]") == 0)
    {
        cfc = this;
    }

    return cfc;
}

void ClassFlowControll::InitFlow(std::string config)
{
    aktstatus = "Initialization";
    aktstatusWithTime = aktstatus;
    flowpostprocessing = NULL;

    ClassFlow *cfc;
    config = format_filename(config);
    FILE *pFile = fopen(config.c_str(), "r");

    std::string line = "";

    char temp_char[1024];

    if (pFile != NULL)
    {
        fgets(temp_char, 1024, pFile);
        ESP_LOGD(TAG, "%s", temp_char);
        line = std::string(temp_char);
    }

    while ((line.size() > 0) && !(feof(pFile)))
    {
        cfc = CreateClassFlow(line);

        if (cfc)
        {
            ESP_LOGE(TAG, "Start ReadParameter (%s)", line.c_str());
            cfc->ReadParameter(pFile, line);
        }
        else
        {
            line = "";

            if (fgets(temp_char, 1024, pFile) && !feof(pFile))
            {
                ESP_LOGD(TAG, "Read: %s", temp_char);
                line = std::string(temp_char);
            }
        }
    }

    fclose(pFile);
}

std::string *ClassFlowControll::getActStatusWithTime()
{
    return &aktstatusWithTime;
}

std::string *ClassFlowControll::getActStatus()
{
    return &aktstatus;
}

void ClassFlowControll::setActStatus(std::string _aktstatus)
{
    aktstatus = _aktstatus;
    aktstatusWithTime = aktstatus;
}

void ClassFlowControll::doFlowTakeImageOnly(std::string time)
{
    std::string temp_time;

    for (int i = 0; i < FlowControll.size(); ++i)
    {
        if (FlowControll[i]->name() == "ClassFlowTakeImage")
        {
            temp_time = getCurrentTimeString("%H:%M:%S");
            aktstatus = TranslateAktstatus(FlowControll[i]->name());
            aktstatusWithTime = aktstatus + " (" + temp_time + ")";
            MQTTPublish(mqttServer_getMainTopic() + "/" + "status", aktstatus, 1, false);
            FlowControll[i]->doFlow(time);
        }
    }
}

bool ClassFlowControll::doFlow(std::string time)
{
    bool result = true;
    std::string temp_time;
    int repeat = 0;
    int qos = 1;

    for (int i = 0; i < FlowControll.size(); ++i)
    {
        temp_time = getCurrentTimeString("%H:%M:%S");
        aktstatus = TranslateAktstatus(FlowControll[i]->name());
        aktstatusWithTime = aktstatus + " (" + temp_time + ")";
        LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "Status: " + aktstatusWithTime);

        MQTTPublish(mqttServer_getMainTopic() + "/" + "status", aktstatus, qos, false);

        if (!FlowControll[i]->doFlow(time))
        {
            repeat++;
            LogFile.WriteToFile(ESP_LOG_WARN, TAG, "Fehler beim " + aktstatus + " Schritt - wird zum " + std::to_string(repeat) + ". Mal wiederholt");

            if (i)
            {
                i -= 1;
            } // vPrevious step must be repeated (probably take pictures)

            result = false;

            if (repeat > 5)
            {
                LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Wiederholung 5x nicht erfolgreich --> reboot");
                doReboot();
                // Step was repeated 5x --> reboot
            }
        }
        else
        {
            result = true;
        }
    }

    temp_time = getCurrentTimeString("%H:%M:%S");
    aktstatus = "Flow finished";
    aktstatusWithTime = aktstatus + " (" + temp_time + ")";

    // LogFile.WriteToFile(ESP_LOG_INFO, TAG, aktstatusWithTime);
    MQTTPublish(mqttServer_getMainTopic() + "/" + "status", aktstatus, qos, false);

    return result;
}

std::string ClassFlowControll::getReadoutAll(int _type)
{
    std::string out = "";

    if (flowpostprocessing)
    {
        std::vector<NumberPost *> *numbers = flowpostprocessing->GetNumbers();

        for (int i = 0; i < (*numbers).size(); ++i)
        {
            out = out + (*numbers)[i]->name + "\t";

            switch (_type)
            {
            case READOUT_TYPE_VALUE:
                out = out + (*numbers)[i]->ReturnValue;
                break;
            case READOUT_TYPE_PREVALUE:
                if (flowpostprocessing->PreValueUse)
                {
                    if ((*numbers)[i]->PreValueOkay)
                    {
                        out = out + (*numbers)[i]->ReturnPreValue;
                    }
                    else
                    {
                        out = out + "PreValue too old";
                    }
                }
                else
                {
                    out = out + "PreValue deactivated";
                }
                break;
            case READOUT_TYPE_RAWVALUE:
                out = out + (*numbers)[i]->ReturnRawValue;
                break;
            case READOUT_TYPE_ERROR:
                out = out + (*numbers)[i]->ErrorMessageText;
                break;
            }

            if (i < (*numbers).size() - 1)
            {
                out = out + "\r\n";
            }
        }
        // ESP_LOGD(TAG, "OUT: %s", out.c_str());
    }

    return out;
}

std::string ClassFlowControll::getReadout(bool _rawvalue = false, bool _noerror = false, int _number = 0)
{
    if (flowpostprocessing)
    {
        return flowpostprocessing->getReadoutParam(_rawvalue, _noerror, _number);
    }

    return std::string("");
}

std::string ClassFlowControll::GetPrevalue(std::string _number)
{
    if (flowpostprocessing)
    {
        return flowpostprocessing->GetPreValue(_number);
    }

    return std::string("");
}

bool ClassFlowControll::UpdatePrevalue(std::string _newvalue, std::string _numbers, bool _extern)
{
    double newvalueAsDouble;
    char *p;

    _newvalue = trim_string_left_right(_newvalue);
    // ESP_LOGD(TAG, "Input UpdatePreValue: %s", _newvalue.c_str());

    if (_newvalue.substr(0, 8).compare("0.000000") == 0 || _newvalue.compare("0.0") == 0 || _newvalue.compare("0") == 0)
    {
        newvalueAsDouble = 0; // preset to value = 0
    }
    else
    {
        newvalueAsDouble = strtod(_newvalue.c_str(), &p);
        if (newvalueAsDouble == 0)
        {
            LogFile.WriteToFile(ESP_LOG_WARN, TAG, "UpdatePrevalue: No valid value for processing: " + _newvalue);
            return false;
        }
    }

    if (flowpostprocessing)
    {
        if (flowpostprocessing->SetPreValue(newvalueAsDouble, _numbers, _extern))
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "UpdatePrevalue: ERROR - Class Post-Processing not initialized");
        return false;
    }
}

bool ClassFlowControll::ReadParameter(FILE *pFile, std::string &aktparamgraph)
{
    aktparamgraph = trim_string_left_right(aktparamgraph);
    if (aktparamgraph.size() == 0)
    {
        if (!GetNextParagraph(pFile, aktparamgraph))
        {
            return false;
        }
    }

    if ((to_upper(aktparamgraph).compare("[AUTOTIMER]") != 0) && (to_upper(aktparamgraph).compare("[DEBUG]") != 0) &&
        (to_upper(aktparamgraph).compare("[SYSTEM]") != 0 && (to_upper(aktparamgraph).compare("[DATALOGGING]") != 0)))
    {
        // Paragraph passt nicht zu Debug oder DataLogging
        return false;
    }

    std::vector<std::string> splitted;

    while (getNextLine(pFile, &aktparamgraph) && !isNewParagraph(aktparamgraph))
    {
        splitted = split_line(aktparamgraph);

        if (splitted.size() > 1)
        {
            std::string _param = to_upper(splitted[0]);

            if (_param == "INTERVAL")
            {
                if (is_string_numeric(splitted[1]))
                {
                    AutoInterval = std::stof(splitted[1]);
                }
            }
            else if (_param == "DATALOGACTIVE")
            {
                LogFile.SetDataLogToSD(alphanumeric_to_boolean(splitted[1]));
            }
            else if (_param == "DATAFILESRETENTION")
            {
                if (is_string_numeric(splitted[1]))
                {
                    LogFile.SetDataLogRetention(std::stoi(splitted[1]));
                }
            }
            else if (_param == "LOGLEVEL")
            {
                /* matches esp_log_level_t */
                if ((to_upper(splitted[1]) == "TRUE") || (to_upper(splitted[1]) == "2"))
                {
                    LogFile.setLogLevel(ESP_LOG_WARN);
                }
                else if ((to_upper(splitted[1]) == "FALSE") || (to_upper(splitted[1]) == "0") || (to_upper(splitted[1]) == "1"))
                {
                    LogFile.setLogLevel(ESP_LOG_ERROR);
                }
                else if (to_upper(splitted[1]) == "3")
                {
                    LogFile.setLogLevel(ESP_LOG_INFO);
                }
                else if (to_upper(splitted[1]) == "4")
                {
                    LogFile.setLogLevel(ESP_LOG_DEBUG);
                }

                /* If system reboot was not triggered by user and reboot was caused by execption -> keep log level to DEBUG */
                if (!getIsPlannedReboot() && (esp_reset_reason() == ESP_RST_PANIC))
                {
                    LogFile.setLogLevel(ESP_LOG_DEBUG);
                }
            }
            else if (_param == "LOGFILESRETENTION")
            {
                if (is_string_numeric(splitted[1]))
                {
                    LogFile.SetLogFileRetention(std::stoi(splitted[1]));
                }
            }
#if (defined WLAN_USE_ROAMING_BY_SCANNING || (defined WLAN_USE_MESH_ROAMING && defined WLAN_USE_MESH_ROAMING_ACTIVATE_CLIENT_TRIGGERED_QUERIES))
            else if (_param == "RSSITHRESHOLD")
            {
                int RSSIThresholdTMP = atoi(splitted[1].c_str());
                RSSIThresholdTMP = min(0, max(-100, RSSIThresholdTMP)); // Verify input limits (-100 - 0)

                if (ChangeRSSIThreshold(NETWORK_CONFIG_FILE, RSSIThresholdTMP))
                {
                    // reboot necessary so that the new wlan.ini is also used !!!
                    fclose(pFile);
                    LogFile.WriteToFile(ESP_LOG_WARN, TAG, "Rebooting to activate new RSSITHRESHOLD ...");
                    doReboot();
                }
            }
#endif
            else if (_param == "HOSTNAME")
            {
                if (ChangeHostName(NETWORK_CONFIG_FILE, splitted[1]))
                {
                    // reboot necessary so that the new wlan.ini is also used !!!
                    fclose(pFile);
                    LogFile.WriteToFile(ESP_LOG_WARN, TAG, "Rebooting to activate new HOSTNAME...");
                    doReboot();
                }
            }
            else if (_param == "SETUPMODE")
            {
                SetupModeActive = alphanumeric_to_boolean(splitted[1]);
            }
        }
    }

    return true;
}

int ClassFlowControll::CleanTempFolder()
{
    const char *folderPath = "/sdcard/img_tmp";

    ESP_LOGD(TAG, "Clean up temporary folder to avoid damage of sdcard sectors: %s", folderPath);
    DIR *dir = opendir(folderPath);

    if (!dir)
    {
        ESP_LOGE(TAG, "Failed to stat dir: %s", folderPath);
        return -1;
    }

    struct dirent *entry;
    int deleted = 0;

    while ((entry = readdir(dir)) != NULL)
    {
        std::string path = std::string(folderPath) + "/" + entry->d_name;
        if (entry->d_type == DT_REG)
        {
            if (unlink(path.c_str()) == 0)
            {
                deleted++;
            }
            else
            {
                ESP_LOGE(TAG, "can't delete file: %s", path.c_str());
            }
        }
        else if (entry->d_type == DT_DIR)
        {
            deleted += remove_folder(path.c_str(), TAG);
        }
    }

    closedir(dir);
    ESP_LOGD(TAG, "%d files deleted", deleted);

    return 0;
}

esp_err_t ClassFlowControll::SendRawJPG(httpd_req_t *req)
{
    return flowtakeimage != NULL ? flowtakeimage->SendRawJPG(req) : ESP_FAIL;
}

esp_err_t ClassFlowControll::GetJPGStream(std::string file_name, httpd_req_t *req)
{
    ESP_LOGD(TAG, "ClassFlowControl::GetJPGStream %s", file_name.c_str());

    CImageBasis *image_to_send = NULL;
    esp_err_t result = ESP_FAIL;
    bool delete_image_to_send = false;

    if (file_name == "alg.jpg")
    {
        if (flowalignment && flowalignment->ImageBasis->ImageOkay())
        {
            image_to_send = flowalignment->ImageBasis;
        }
        else
        {
            LogFile.WriteToFile(ESP_LOG_WARN, TAG, "ClassFlowControl::GetJPGStream: alg.jpg cannot be served");
            result = send_file(req, "/sdcard/html/Flowstate_at_work.jpg");
            httpd_resp_send_chunk(req, NULL, 0);
        }
    }
    else if (file_name == "alg_roi.jpg")
    {
#ifdef ALGROI_LOAD_FROM_MEM_AS_JPG // no CImageBasis needed to create alg_roi.jpg (ca. 790kB less RAM)
        if (aktstatus.find("Initialization (delayed)") != -1)
        {
            result = send_file(req, "/sdcard/html/Flowstate_init_delayed.jpg");
        }
        else if (aktstatus.find("Initialization") != -1)
        {
            result = send_file(req, "/sdcard/html/Flowstate_init.jpg");
        }
        else if (aktstatus.find("Take Image") != -1)
        {
            if (flowalignment && flowalignment->AlgROI)
            {
                result = send_file(req, "/sdcard/html/Flowstate_take_image.jpg");
            }
            else
            {
                if (flowalignment && flowalignment->ImageBasis->ImageOkay())
                {
                    image_to_send = flowalignment->ImageBasis;
                }
                else
                {
                    LogFile.WriteToFile(ESP_LOG_WARN, TAG, "ClassFlowControl::GetJPGStream: alg_roi.jpg cannot be served -> alg.jpg is going to be served!");
                    result = send_file(req, "/sdcard/html/Flowstate_at_work.jpg");
                    httpd_resp_send_chunk(req, NULL, 0);
                }
            }
        }
        else
        {
            if (flowalignment && flowalignment->AlgROI)
            {
                httpd_resp_set_type(req, "image/jpeg");
                result = httpd_resp_send(req, (const char *)flowalignment->AlgROI->data, flowalignment->AlgROI->size);
            }
            else
            {
                if (flowalignment && flowalignment->ImageBasis->ImageOkay())
                {
                    image_to_send = flowalignment->ImageBasis;
                }
                else
                {
                    LogFile.WriteToFile(ESP_LOG_WARN, TAG, "ClassFlowControl::GetJPGStream: alg_roi.jpg cannot be served -> alg.jpg is going to be served!");
                    result = send_file(req, "/sdcard/html/Flowstate_at_work.jpg");
                    httpd_resp_send_chunk(req, NULL, 0);
                }
            }
        }
#else
        if (!flowalignment)
        {
            ESP_LOGD(TAG, "ClassFloDControl::GetJPGStream: FlowAlignment is not (yet) initialized. Interrupt serving!");
            httpd_resp_send(req, NULL, 0);
            return ESP_FAIL;
        }

        image_to_send = new CImageBasis("alg_roi", flowalignment->ImageBasis);
        if (image_to_send->ImageOkay())
        {
            if (flowalignment)
            {
                flowalignment->DrawRef(image_to_send);
            }
            if (flowdigit)
            {
                flowdigit->DrawROI(image_to_send);
            }
            if (flowanalog)
            {
                flowanalog->DrawROI(image_to_send);
            }

            delete_image_to_send = true; // delete temporary image_to_send element after sending
        }
        else
        {
            if (flowalignment && flowalignment->ImageBasis->ImageOkay())
            {
                image_to_send = flowalignment->ImageBasis;
            }
            else
            {
                LogFile.WriteToFile(ESP_LOG_WARN, TAG, "ClassFlowControl::GetJPGStream: Not enough memory to create alg_roi.jpg -> alg.jpg is going to be served!");
                result = send_file(req, "/sdcard/html/Flowstate_at_work.jpg");
                httpd_resp_send_chunk(req, NULL, 0);
            }
        }
#endif
    }
    else
    {
        std::vector<HTMLInfo *> htmlinfo = GetAllDigit();
        ESP_LOGD(TAG, "After getClassFlowControl::GetAllDigit");

        for (int i = 0; i < htmlinfo.size(); ++i)
        {
            if (file_name == htmlinfo[i]->filename)
            {
                if (htmlinfo[i]->image)
                {
                    image_to_send = htmlinfo[i]->image;
                }
            }

            if (file_name == htmlinfo[i]->filename_org)
            {
                if (htmlinfo[i]->image_org)
                {
                    image_to_send = htmlinfo[i]->image_org;
                }
            }
            delete htmlinfo[i];
        }
        htmlinfo.clear();

        if (!image_to_send)
        {
            htmlinfo = GetAllAnalog();
            ESP_LOGD(TAG, "After getClassFlowControl::GetAllAnalog");

            for (int i = 0; i < htmlinfo.size(); ++i)
            {
                if (file_name == htmlinfo[i]->filename)
                {
                    if (htmlinfo[i]->image)
                    {
                        image_to_send = htmlinfo[i]->image;
                    }
                }

                if (file_name == htmlinfo[i]->filename_org)
                {
                    if (htmlinfo[i]->image_org)
                    {
                        image_to_send = htmlinfo[i]->image_org;
                    }
                }
                delete htmlinfo[i];
            }
            htmlinfo.clear();
        }
    }

    if (image_to_send)
    {
        ESP_LOGD(TAG, "Sending file: %s ...", file_name.c_str());
        set_content_type_from_file(req, file_name.c_str());
        result = image_to_send->SendJPGtoHTTP(req);

        /* Respond with an empty chunk to signal HTTP response completion */
        httpd_resp_send_chunk(req, NULL, 0);
        ESP_LOGD(TAG, "File sending complete");

        if (delete_image_to_send)
        {
            delete image_to_send;
        }

        image_to_send = NULL;
    }

    return result;
}

std::string ClassFlowControll::getNumbersName()
{
    return flowpostprocessing->getNumbersName();
}

std::string ClassFlowControll::getJSON()
{
    return flowpostprocessing->GetJSON();
}

/**
 * @returns a vector of all current sequences
 **/
const std::vector<NumberPost *> &ClassFlowControll::getNumbers()
{
    return *flowpostprocessing->GetNumbers();
}
