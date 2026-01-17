#include "defines.h"

#include <sstream>
#include <time.h>

#include "ClassFlowInfluxDBv2.h"

#include "Helper.h"
#include "connect_wifi_sta.h"

#include "time_sntp.h"
#include "interface_influxdb.h"

#include "ClassFlowPostProcessing.h"
#include "esp_log.h"
#include "ClassLogFile.h"

static const char *TAG = "INFLUXDBV2";

influxDBv2_controll_config_t influxDBv2_controll_config;

void ClassFlowInfluxDBv2::SetInitialParameter(void)
{
    influxDBv2_controll_config.enabled = false;
    influxDBv2_controll_config.uri = "";
    influxDBv2_controll_config.bucket = "";
    influxDBv2_controll_config.dborg = "";
    influxDBv2_controll_config.dbtoken = "";
    influxDBv2_controll_config.oldValue = "";

    flowpostprocessing = NULL;
    previousElement = NULL;
    ListFlowControll = NULL;

    disabled = false;
}

ClassFlowInfluxDBv2::ClassFlowInfluxDBv2()
{
    SetInitialParameter();
}

ClassFlowInfluxDBv2::ClassFlowInfluxDBv2(std::vector<ClassFlow *> *lfc)
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

ClassFlowInfluxDBv2::ClassFlowInfluxDBv2(std::vector<ClassFlow *> *lfc, ClassFlow *_prev)
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

bool ClassFlowInfluxDBv2::ReadParameter(FILE *pFile, std::string &aktparamgraph)
{
    aktparamgraph = trim_string_left_right(aktparamgraph);
    if (aktparamgraph.size() == 0)
    {
        if (!GetNextParagraph(pFile, aktparamgraph))
        {
            return false;
        }
    }

    if ((to_upper(aktparamgraph).compare("[INFLUXDBV2]") != 0) && (to_upper(aktparamgraph).compare(";[INFLUXDBV2]") != 0))
    {
        return false;
    }

    if (aktparamgraph[0] == ';')
    {
        influxDBv2_controll_config.enabled = false;
        while (getNextLine(pFile, &aktparamgraph) && !isNewParagraph(aktparamgraph));
        ESP_LOGD(TAG, "InfluxDBv2 is disabled!");

        return true;
    }

    std::vector<std::string> splitted;

    while (getNextLine(pFile, &aktparamgraph) && !isNewParagraph(aktparamgraph))
    {
        splitted = split_line(aktparamgraph);

        if (splitted.size() > 1)
        {
            std::string _param = to_upper(GetParameterName(splitted[0]));

            if (_param == "ORG")
            {
                influxDBv2_controll_config.dborg = splitted[1];
            }
            else if (_param == "TOKEN")
            {
                influxDBv2_controll_config.dbtoken = splitted[1];
            }
            else if (_param == "URI")
            {
                influxDBv2_controll_config.uri = splitted[1];
            }
            else if (_param == "FIELD")
            {
                handleFieldname(splitted[0], splitted[1]);
            }
            else if (_param == "MEASUREMENT")
            {
                handleMeasurement(splitted[0], splitted[1]);
            }
            else if (_param == "BUCKET")
            {
                influxDBv2_controll_config.bucket = splitted[1];
            }
        }
    }

    if ((influxDBv2_controll_config.uri.length() > 0) && (influxDBv2_controll_config.bucket.length() > 0) && (influxDBv2_controll_config.dbtoken.length() > 0) && (influxDBv2_controll_config.dborg.length() > 0))
    {
        LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "Init InfluxDBv2 with uri: " + influxDBv2_controll_config.uri + ", org: " + influxDBv2_controll_config.dborg + ", token: *****");
        influxDB.InfluxDBInitV2(influxDBv2_controll_config.uri, influxDBv2_controll_config.bucket, influxDBv2_controll_config.dborg, influxDBv2_controll_config.dbtoken);
        influxDBv2_controll_config.enabled = true;
    }
    else
    {
        LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "InfluxDBv2 init skipped as we are missing some parameters");
    }

    return true;
}

bool ClassFlowInfluxDBv2::doFlow(std::string temp_time)
{
    if (!influxDBv2_controll_config.enabled)
    {
        return true;
    }

    std::string measurement = "";
    std::string result = "";
    long int result_timeutc = 0;
    std::string name_number = "";

    if (flowpostprocessing)
    {
        std::vector<NumberPost *> *NUMBERS = flowpostprocessing->GetNumbers();

        for (int i = 0; i < (*NUMBERS).size(); ++i)
        {
            measurement = (*NUMBERS)[i]->MeasurementV2;
            result = (*NUMBERS)[i]->ReturnValue;
            result_timeutc = (*NUMBERS)[i]->timeStampTimeUTC;

            if ((*NUMBERS)[i]->FieldV2.length() > 0)
            {
                name_number = (*NUMBERS)[i]->FieldV2;
            }
            else
            {
                name_number = (*NUMBERS)[i]->name;
                if (name_number == "default")
                {
                    name_number = "value";
                }
                else
                {
                    name_number = name_number + "/value";
                }
            }

            if (result.length() > 0)
            {
                influxDB.InfluxDBPublish(measurement, name_number, result, result_timeutc);
            }
        }
    }

    influxDBv2_controll_config.oldValue = result;

    return true;
}

void ClassFlowInfluxDBv2::handleFieldname(std::string _decsep, std::string _value)
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
            flowpostprocessing->NUMBERS[j]->FieldV2 = _value;
        }
    }
}

void ClassFlowInfluxDBv2::handleMeasurement(std::string _decsep, std::string _value)
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
            flowpostprocessing->NUMBERS[j]->MeasurementV2 = _value;
        }
    }
}
