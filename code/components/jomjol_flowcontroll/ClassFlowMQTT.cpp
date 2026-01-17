#include "defines.h"

#include "ClassFlowMQTT.h"

#include <sstream>
#include <iomanip>
#include <time.h>

#include "Helper.h"

#include "connect_wifi_sta.h"
#include "read_network_config.h"

#include "ClassLogFile.h"

#include "time_sntp.h"
#include "interface_mqtt.h"
#include "ClassFlowPostProcessing.h"
#include "ClassFlowControll.h"

#include "server_mqtt.h"

static const char *TAG = "MQTT";

mqtt_controll_config_t mqtt_controll_config;

extern const char *libfive_git_version(void);
extern const char *libfive_git_revision(void);
extern const char *libfive_git_branch(void);

void ClassFlowMQTT::SetInitialParameter(void)
{
    mqtt_controll_config.mqtt_enabled = false;
    mqtt_controll_config.mqtt_configOK = false;
    mqtt_controll_config.mqtt_initialized = false;
    mqtt_controll_config.mqtt_connected = false;

    mqtt_controll_config.HomeAssistantDiscovery = false;

    mqtt_controll_config.esp_mqtt_ID = MQTT_EVENT_ANY;

    mqtt_controll_config.uri = "";
    mqtt_controll_config.topic = "";
    mqtt_controll_config.topicError = "";
    mqtt_controll_config.topicRate = "";
    mqtt_controll_config.topicTimeStamp = "";
    mqtt_controll_config.maintopic = network_config.hostname;
    mqtt_controll_config.discoveryprefix = "homeassistant";

    mqtt_controll_config.topicUptime = "";
    mqtt_controll_config.topicFreeMem = "";

    mqtt_controll_config.caCertFilename = "";
    mqtt_controll_config.clientCertFilename = "";
    mqtt_controll_config.clientKeyFilename = "";
    mqtt_controll_config.validateServerCert = true;
    mqtt_controll_config.clientname = network_config.hostname;

    mqtt_controll_config.OldValue = "";

    mqtt_controll_config.user = "";
    mqtt_controll_config.password = "";

    mqtt_controll_config.lwt_topic = mqtt_controll_config.maintopic + "/" + LWT_TOPIC;
    mqtt_controll_config.lwt_connected = LWT_CONNECTED;
    mqtt_controll_config.lwt_disconnected = LWT_DISCONNECTED;

    mqtt_controll_config.meterType = "";
    mqtt_controll_config.valueUnit = "";
    mqtt_controll_config.timeUnit = "";
    mqtt_controll_config.rateUnit = "Unit/Minute";

    mqtt_controll_config.retainFlag = false;
    mqtt_controll_config.keepAlive = 25 * 60;
    mqtt_controll_config.domoticzintopic = "";

    flowpostprocessing = NULL;
    previousElement = NULL;
    ListFlowControll = NULL;

    disabled = false;
}

ClassFlowMQTT::ClassFlowMQTT(void)
{
    SetInitialParameter();
}

ClassFlowMQTT::ClassFlowMQTT(std::vector<ClassFlow *> *lfc)
{
    SetInitialParameter();

    ListFlowControll = lfc;
    for (int i = 0; i < ListFlowControll->size(); ++i)
    {
        if (((*ListFlowControll)[i])->name().compare("ClassFlowPostProcessing") == 0)
        {
            flowpostprocessing = (ClassFlowPostProcessing *)(*ListFlowControll)[i];
        }
    }
}

ClassFlowMQTT::ClassFlowMQTT(std::vector<ClassFlow *> *lfc, ClassFlow *_prev)
{
    SetInitialParameter();

    previousElement = _prev;
    ListFlowControll = lfc;

    for (int i = 0; i < ListFlowControll->size(); ++i)
    {
        if (((*ListFlowControll)[i])->name().compare("ClassFlowPostProcessing") == 0)
        {
            flowpostprocessing = (ClassFlowPostProcessing *)(*ListFlowControll)[i];
        }
    }
}

bool ClassFlowMQTT::ReadParameter(FILE *pFile, std::string &aktparamgraph)
{
    aktparamgraph = trim_string_left_right(aktparamgraph);
    if (aktparamgraph.size() == 0)
    {
        if (!GetNextParagraph(pFile, aktparamgraph))
        {
            return false;
        }
    }

    if ((to_upper(aktparamgraph).compare("[MQTT]") != 0) && (to_upper(aktparamgraph).compare(";[MQTT]") != 0))
    {
        // Paragraph does not fit MQTT
        return false;
    }

    if (aktparamgraph[0] == ';')
    {
        mqtt_controll_config.mqtt_enabled = false;
        while (getNextLine(pFile, &aktparamgraph) && !isNewParagraph(aktparamgraph));
        ESP_LOGD(TAG, "mqtt is disabled!");

        return true;
    }

    std::vector<std::string> splitted;

    while (getNextLine(pFile, &aktparamgraph) && !isNewParagraph(aktparamgraph))
    {
        splitted = split_line(aktparamgraph);

        if (splitted.size() > 1)
        {
            std::string _param = to_upper(GetParameterName(splitted[0]));

            if (_param == "CACERT")
            {
                mqtt_controll_config.caCertFilename = "/sdcard" + splitted[1];
            }
            else if (_param == "VALIDATESERVERCERT")
            {
                mqtt_controll_config.validateServerCert = alphanumeric_to_boolean(splitted[1]);
            }
            else if (_param == "CLIENTCERT")
            {
                mqtt_controll_config.clientCertFilename = "/sdcard" + splitted[1];
            }
            else if (_param == "CLIENTKEY")
            {
                mqtt_controll_config.clientKeyFilename = "/sdcard" + splitted[1];
            }
            else if (_param == "USER")
            {
                mqtt_controll_config.user = splitted[1];
            }
            else if (_param == "PASSWORD")
            {
                mqtt_controll_config.password = splitted[1];
            }
            else if (_param == "URI")
            {
                mqtt_controll_config.uri = splitted[1];
            }
            else if (_param == "RETAINMESSAGES")
            {
                mqtt_controll_config.retainFlag = alphanumeric_to_boolean(splitted[1]);
            }
            else if (_param == "HOMEASSISTANTDISCOVERY")
            {
                if (to_upper(splitted[1]) == "TRUE")
                {
                    mqtt_controll_config.HomeAssistantDiscovery = true;
                }
            }
            else if (_param == "METERTYPE")
            {
                std::string _value = to_upper(splitted[1]);

                /* Use meter type for the device class
                   Make sure it is a listed one on https://developers.home-assistant.io/docs/core/entity/sensor/#available-device-classes */
                if (_value == "WATER_M3")
                {
                    SetMeterType("water", "m³", "h", "m³/h");
                }
                else if (_value == "WATER_L")
                {
                    SetMeterType("water", "L", "h", "L/h");
                }
                else if (_value == "WATER_FT3")
                {
                    SetMeterType("water", "ft³", "min", "ft³/min"); // min = Minutes
                }
                else if (_value == "WATER_GAL")
                {
                    SetMeterType("water", "gal", "h", "gal/h");
                }
                else if (_value == "WATER_GAL_MIN")
                {
                    SetMeterType("water", "gal", "min", "gal/min"); // min = Minutes
                }
                else if (_value == "GAS_M3")
                {
                    SetMeterType("gas", "m³", "h", "m³/h");
                }
                else if (_value == "GAS_FT3")
                {
                    SetMeterType("gas", "ft³", "min", "ft³/min"); // min = Minutes
                }
                else if (_value == "ENERGY_WH")
                {
                    SetMeterType("energy", "Wh", "h", "W");
                }
                else if (_value == "ENERGY_KWH")
                {
                    SetMeterType("energy", "kWh", "h", "kW");
                }
                else if (_value == "ENERGY_MWH")
                {
                    SetMeterType("energy", "MWh", "h", "MW");
                }
                else if (_value == "ENERGY_GJ")
                {
                    SetMeterType("energy", "GJ", "h", "GJ/h");
                }
                else if (_value == "TEMPERATURE_C")
                {
                    SetMeterType("temperature", "°C", "min", "°C/min"); // min = Minutes
                }
                else if (_value == "TEMPERATURE_F")
                {
                    SetMeterType("temperature", "°F", "min", "°F/min"); // min = Minutes
                }
                else if (_value == "TEMPERATURE_K")
                {
                    SetMeterType("temperature", "K", "min", "K/m"); // min = Minutes
                }
            }
            else if (_param == "CLIENTID")
            {
                mqtt_controll_config.clientname = splitted[1];
            }
            else if ((_param == "TOPIC") || (_param == "MAINTOPIC"))
            {
                mqtt_controll_config.maintopic = splitted[1];
            }
            else if (_param == "DISCOVERYPREFIX")
            {
                mqtt_controll_config.discoveryprefix = splitted[1];
            }
            else if (_param == "DOMOTICZTOPICIN")
            {
                mqtt_controll_config.domoticzintopic = splitted[1];
            }
            else if (_param == "DOMOTICZIDX")
            {
                handleIdx(splitted[0], splitted[1]);
            }
        }
    }

    if ((mqtt_controll_config.uri.length() > 0) && (mqtt_controll_config.user.length() > 0))
    {
        /* Note:
         * Originally, we started the MQTT client here.
         * How ever we need the interval parameter from the ClassFlowControl, but that only gets started later.
         * To work around this, we delay the start and trigger it from ClassFlowControl::ReadParameter() */

        LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "Init MQTT with uri: " + mqtt_controll_config.uri + ", user: " + mqtt_controll_config.user);

        if (mqtt_controll_config.domoticzintopic.length() > 0)
        {
            LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "Init MQTT with domoticzintopic: " + mqtt_controll_config.domoticzintopic);
        }

        mqtt_controll_config.mqtt_enabled = true;
    }
    else
    {
        LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "MQTT init skipped as we are missing some parameters");
    }

    return true;
}

void ClassFlowMQTT::SetMeterType(std::string _meterType, std::string _valueUnit, std::string _timeUnit, std::string _rateUnit)
{
    mqtt_controll_config.meterType = _meterType;
    mqtt_controll_config.valueUnit = _valueUnit;
    mqtt_controll_config.timeUnit = _timeUnit;
    mqtt_controll_config.rateUnit = _rateUnit;
}

bool ClassFlowMQTT::Start(float AutoInterval)
{
    mqtt_controll_config.roundInterval = AutoInterval;                              // Minutes
    mqtt_controll_config.keepAlive = mqtt_controll_config.roundInterval * 60 * 2.5; // Seconds, make sure it is greater thatn 2 rounds!

    mqttServer_setParameter(flowpostprocessing->GetNumbers());

    if (!MQTT_Configure((void *)&GotConnected))
    {
        return false;
    }

    return (MQTT_Init() == 1);
}

bool ClassFlowMQTT::doFlow(std::string time)
{
    int qos = 1;

    std::string value_temp = "";
    std::string error_message_text_temp = "";
    std::string raw_value_temp = "";
    // std::string pre_value_temp = "";
    std::string rate_value_temp = ""; // Always Unit / Minute
    std::string time_stamp_temp = "";
    std::string change_absolute_temp = "";

    /* Send the the Homeassistant Discovery and the Static Topics in case they where scheduled */
    sendDiscovery_and_static_Topics();
    bool success = publishSystemData(qos);

    if (flowpostprocessing && getMQTTisConnected())
    {
        std::vector<NumberPost *> *NUMBERS = flowpostprocessing->GetNumbers();
        LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "Publishing MQTT topics...");

        for (int i = 0; i < (*NUMBERS).size(); ++i)
        {
            value_temp = (*NUMBERS)[i]->ReturnValue;
            raw_value_temp = (*NUMBERS)[i]->ReturnRawValue;
            // pre_value_temp = (*NUMBERS)[i]->ReturnPreValue;
            error_message_text_temp = (*NUMBERS)[i]->ErrorMessageText;
            rate_value_temp = (*NUMBERS)[i]->ReturnRateValue;           // Unit per minutes
            change_absolute_temp = (*NUMBERS)[i]->ReturnChangeAbsolute; // Units per round
            time_stamp_temp = (*NUMBERS)[i]->timeStamp;

            std::string DomoticzIdx = (*NUMBERS)[i]->DomoticzIdx;
            std::string domoticzpayload = "{\"command\":\"udevice\",\"idx\":" + DomoticzIdx + ",\"svalue\":\"" + value_temp + "\"}";

            std::string name_temp = (*NUMBERS)[i]->name;

            if (name_temp == "default")
            {
                name_temp = mqtt_controll_config.maintopic + "/";
            }
            else
            {
                name_temp = mqtt_controll_config.maintopic + "/" + name_temp + "/";
            }

            if ((mqtt_controll_config.domoticzintopic.length() > 0) && (value_temp.length() > 0))
            {
                success |= MQTTPublish(mqtt_controll_config.domoticzintopic, domoticzpayload, qos, mqtt_controll_config.retainFlag);
            }

            if (value_temp.length() > 0)
            {
                success |= MQTTPublish(name_temp + "value", value_temp, qos, mqtt_controll_config.retainFlag);
            }

            if (error_message_text_temp.length() > 0)
            {
                success |= MQTTPublish(name_temp + "error", error_message_text_temp, qos, mqtt_controll_config.retainFlag);
            }

            if (rate_value_temp.length() > 0)
            {
                success |= MQTTPublish(name_temp + "rate", rate_value_temp, qos, mqtt_controll_config.retainFlag);

                std::string resultRatePerTimeUnit;
                if (mqtt_controll_config.timeUnit == "h")
                {
                    // Need conversion to be per hour
                    resultRatePerTimeUnit = resultRatePerTimeUnit = to_string((*NUMBERS)[i]->FlowRateAct * 60); // per minutes => per hour
                }
                else
                {
                    // Keep per minute
                    resultRatePerTimeUnit = rate_value_temp;
                }
                success |= MQTTPublish(name_temp + "rate_per_time_unit", resultRatePerTimeUnit, qos, mqtt_controll_config.retainFlag);
            }

            if (change_absolute_temp.length() > 0)
            {
                success |= MQTTPublish(name_temp + "changeabsolut", change_absolute_temp, qos, mqtt_controll_config.retainFlag); // Legacy API
                success |= MQTTPublish(name_temp + "rate_per_digitization_round", change_absolute_temp, qos, mqtt_controll_config.retainFlag);
            }

            if (raw_value_temp.length() > 0)
            {
                success |= MQTTPublish(name_temp + "raw", raw_value_temp, qos, mqtt_controll_config.retainFlag);
            }

            if (time_stamp_temp.length() > 0)
            {
                success |= MQTTPublish(name_temp + "timestamp", time_stamp_temp, qos, mqtt_controll_config.retainFlag);
            }

            std::string json = flowpostprocessing->getJsonFromNumber(i, "\n");
            success |= MQTTPublish(name_temp + "json", json, qos, mqtt_controll_config.retainFlag);
        }
    }

    mqtt_controll_config.OldValue = value_temp;

    if (!success)
    {
        LogFile.WriteToFile(ESP_LOG_WARN, TAG, "One or more MQTT topics failed to be published!");
    }

    return true;
}

void ClassFlowMQTT::handleIdx(std::string _decsep, std::string _value)
{
    std::string _digit;
    int _pospunkt = _decsep.find_first_of(".");

    if (_pospunkt > -1)
    {
        _digit = _decsep.substr(0, _pospunkt);
    }
    else
    {
        _digit = "default";
    }

    for (int j = 0; j < flowpostprocessing->NUMBERS.size(); ++j)
    {
        //  Set to default first (if nothing else is set)
        if ((_digit == "default") || (flowpostprocessing->NUMBERS[j]->name == _digit))
        {
            flowpostprocessing->NUMBERS[j]->DomoticzIdx = _value;
        }
    }
}
