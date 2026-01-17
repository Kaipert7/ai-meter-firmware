#pragma once

#ifndef CLASSFFLOWMQTT_H
#define CLASSFFLOWMQTT_H

#include <string>
#include <mqtt_client.h>

#include "ClassFlow.h"
#include "ClassFlowPostProcessing.h"

typedef struct
{
    bool mqtt_enabled;
    bool mqtt_configOK;
    bool mqtt_initialized;
    bool mqtt_connected;

    bool HomeAssistantDiscovery;

    esp_mqtt_event_id_t esp_mqtt_ID;

    std::string uri;
    std::string topic;
    std::string topicError;
    std::string clientname;
    std::string topicRate;
    std::string topicTimeStamp;
    std::string topicUptime;
    std::string topicFreeMem;
    std::string OldValue;

    std::string user;
    std::string password;
    std::string caCertFilename;
    std::string clientCertFilename;
    std::string clientKeyFilename;
    bool validateServerCert;

    std::string maintopic;
    std::string discoveryprefix;
    std::string domoticzintopic;

    std::string lwt_topic;
    std::string lwt_connected;
    std::string lwt_disconnected;

    std::string meterType;
    std::string valueUnit;
    std::string timeUnit;
    std::string rateUnit;

    float roundInterval; // in Minutes
    bool retainFlag;
    int keepAlive; // in Seconds
} mqtt_controll_config_t;

extern mqtt_controll_config_t mqtt_controll_config;

class ClassFlowMQTT : public ClassFlow
{
protected:
    ClassFlowPostProcessing *flowpostprocessing;

    void SetInitialParameter(void);
    void SetMeterType(std::string _meterType, std::string _valueUnit, std::string _timeUnit, std::string _rateUnit);
    void handleIdx(std::string _decsep, std::string _value);

public:
    ClassFlowMQTT(void);
    ClassFlowMQTT(std::vector<ClassFlow *> *lfc);
    ClassFlowMQTT(std::vector<ClassFlow *> *lfc, ClassFlow *_prev);

    bool Start(float AutoInterval);

    bool ReadParameter(FILE *pFile, std::string &aktparamgraph);
    bool doFlow(std::string time);
    std::string name(void) { return "ClassFlowMQTT"; };
};
#endif // CLASSFFLOWMQTT_H
