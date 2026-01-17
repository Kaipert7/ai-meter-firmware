#include "defines.h"

#include <sstream>
#include <time.h>

#include "ClassFlowInfluxDB.h"

#include "Helper.h"
#include "connect_wifi_sta.h"

#include "time_sntp.h"
#include "interface_influxdb.h"

#include "ClassFlowPostProcessing.h"
#include "esp_log.h"
#include "ClassLogFile.h"

static const char *TAG = "INFLUXDB";

influxDBv1_controll_config_t influxDBv1_controll_config;

void ClassFlowInfluxDB::SetInitialParameter(void)
{
    influxDBv1_controll_config.enabled = false;
    influxDBv1_controll_config.uri = "";
    influxDBv1_controll_config.database = "";
    influxDBv1_controll_config.user = "";
    influxDBv1_controll_config.password = "";
    influxDBv1_controll_config.oldValue = "";

    flowpostprocessing = NULL;
    previousElement = NULL;
    ListFlowControll = NULL;

    disabled = false;
}

ClassFlowInfluxDB::ClassFlowInfluxDB()
{
    SetInitialParameter();
}

ClassFlowInfluxDB::ClassFlowInfluxDB(std::vector<ClassFlow *> *lfc)
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

ClassFlowInfluxDB::ClassFlowInfluxDB(std::vector<ClassFlow *> *lfc, ClassFlow *_prev)
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

bool ClassFlowInfluxDB::ReadParameter(FILE *pFile, std::string &aktparamgraph)
{
    aktparamgraph = trim_string_left_right(aktparamgraph);
    if (aktparamgraph.size() == 0)
    {
        if (!GetNextParagraph(pFile, aktparamgraph))
        {
            return false;
        }
    }

    if ((to_upper(aktparamgraph).compare("[INFLUXDB]") != 0) && (to_upper(aktparamgraph).compare(";[INFLUXDB]") != 0))
    {
        return false;
    }

    if (aktparamgraph[0] == ';')
    {
        influxDBv1_controll_config.enabled = false;
        while (getNextLine(pFile, &aktparamgraph) && !isNewParagraph(aktparamgraph));
        ESP_LOGD(TAG, "InfluxDBv1 is disabled!");

        return true;
    }

    std::vector<std::string> splitted;

    while (getNextLine(pFile, &aktparamgraph) && !isNewParagraph(aktparamgraph))
    {
        splitted = split_line(aktparamgraph);

        if (splitted.size() > 1)
        {
            std::string _param = to_upper(GetParameterName(splitted[0]));

            if (_param == "USER")
            {
                influxDBv1_controll_config.user = splitted[1];
            }
            else if (_param == "PASSWORD")
            {
                influxDBv1_controll_config.password = splitted[1];
            }
            else if (_param == "URI")
            {
                influxDBv1_controll_config.uri = splitted[1];
            }
            else if (_param == "DATABASE")
            {
                influxDBv1_controll_config.database = splitted[1];
            }
            else if (_param == "MEASUREMENT")
            {
                handleMeasurement(splitted[0], splitted[1]);
            }
            else if (_param == "FIELD")
            {
                handleFieldname(splitted[0], splitted[1]);
            }
        }
    }

    if ((influxDBv1_controll_config.uri.length() > 0) && (influxDBv1_controll_config.database.length() > 0))
    {
        LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "Init InfluxDBv1 with uri: " + influxDBv1_controll_config.uri + ", user: " + influxDBv1_controll_config.user + ", password: " + influxDBv1_controll_config.password);
        influxDB.InfluxDBInitV1(influxDBv1_controll_config.uri, influxDBv1_controll_config.database, influxDBv1_controll_config.user, influxDBv1_controll_config.password);
        influxDBv1_controll_config.enabled = true;
    }
    else
    {
        LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "InfluxDBv1 init skipped as we are missing some parameters");
    }

    return true;
}

bool ClassFlowInfluxDB::doFlow(std::string temp_time)
{
    if (!influxDBv1_controll_config.enabled)
    {
        return true;
    }

    std::string result = "";
    std::string measurement = "";
    long int result_timeutc = 0;
    std::string name_number = "";

    if (flowpostprocessing)
    {
        std::vector<NumberPost *> *NUMBERS = flowpostprocessing->GetNumbers();

        for (int i = 0; i < (*NUMBERS).size(); ++i)
        {
            measurement = (*NUMBERS)[i]->MeasurementV1;
            result = (*NUMBERS)[i]->ReturnValue;
            result_timeutc = (*NUMBERS)[i]->timeStampTimeUTC;

            if ((*NUMBERS)[i]->FieldV1.length() > 0)
            {
                name_number = (*NUMBERS)[i]->FieldV1;
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

    influxDBv1_controll_config.oldValue = result;

    return true;
}

void ClassFlowInfluxDB::handleMeasurement(std::string _decsep, std::string _value)
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
        // Set to default first (if nothing else is set)
        if ((_digit == "default") || (flowpostprocessing->NUMBERS[j]->name == _digit))
        {
            flowpostprocessing->NUMBERS[j]->MeasurementV1 = _value;
        }
    }
}

void ClassFlowInfluxDB::handleFieldname(std::string _decsep, std::string _value)
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
        // Set to default first (if nothing else is set)
        if ((_digit == "default") || (flowpostprocessing->NUMBERS[j]->name == _digit))
        {
            flowpostprocessing->NUMBERS[j]->FieldV1 = _value;
        }
    }
}
