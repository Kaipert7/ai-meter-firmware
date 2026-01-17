#pragma once

#ifndef CLASSFINFLUXDBv2_H
#define CLASSFINFLUXDBv2_H

#include <string>

#include "ClassFlow.h"
#include "ClassFlowPostProcessing.h"
#include "interface_influxdb.h"

typedef struct
{
    bool enabled;
    std::string uri;
    std::string bucket;
    std::string dborg;
    std::string dbtoken;
    std::string dbfield;
    std::string oldValue;

} influxDBv2_controll_config_t;

extern influxDBv2_controll_config_t influxDBv2_controll_config;

class ClassFlowInfluxDBv2 : public ClassFlow
{
protected:
    InfluxDB influxDB;
    ClassFlowPostProcessing *flowpostprocessing;

    void SetInitialParameter(void);
    void handleFieldname(std::string _decsep, std::string _value);
    void handleMeasurement(std::string _decsep, std::string _value);

public:
    ClassFlowInfluxDBv2();
    ClassFlowInfluxDBv2(std::vector<ClassFlow *> *lfc);
    ClassFlowInfluxDBv2(std::vector<ClassFlow *> *lfc, ClassFlow *_prev);

    bool ReadParameter(FILE *pFile, std::string &aktparamgraph);
    bool doFlow(std::string temp_time);
    std::string name() { return "ClassFlowInfluxDBv2"; };
};

#endif // CLASSFINFLUXDBv2_H
