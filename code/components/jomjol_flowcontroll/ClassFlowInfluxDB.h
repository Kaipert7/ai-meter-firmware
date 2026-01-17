#pragma once

#ifndef CLASSFINFLUXDB_H
#define CLASSFINFLUXDB_H

#include <string>

#include "ClassFlow.h"
#include "ClassFlowPostProcessing.h"
#include "interface_influxdb.h"

typedef struct
{
    bool enabled;
    std::string uri;
    std::string database;
    std::string measurement;
    std::string user;
    std::string password;
    std::string oldValue;

} influxDBv1_controll_config_t;

extern influxDBv1_controll_config_t influxDBv1_controll_config;

class ClassFlowInfluxDB : public ClassFlow
{
protected:
    InfluxDB influxDB;

    ClassFlowPostProcessing *flowpostprocessing;

    void SetInitialParameter(void);
    void handleFieldname(std::string _decsep, std::string _value);
    void handleMeasurement(std::string _decsep, std::string _value);

public:
    ClassFlowInfluxDB();
    ClassFlowInfluxDB(std::vector<ClassFlow *> *lfc);
    ClassFlowInfluxDB(std::vector<ClassFlow *> *lfc, ClassFlow *_prev);

    bool ReadParameter(FILE *pfile, std::string &aktparamgraph);
    bool doFlow(std::string temp_time);
    std::string name() { return "ClassFlowInfluxDB"; };
};

#endif // CLASSFINFLUXDB_H
