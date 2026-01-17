#pragma once

#ifndef CLASSFWEBHOOK_H
#define CLASSFWEBHOOK_H

#include <string>

#include "ClassFlow.h"
#include "ClassFlowPostProcessing.h"
#include "ClassFlowAlignment.h"

typedef struct
{
    bool enabled;
    std::string uri;
    std::string apikey;
    int uploadImg;
    std::string oldValue;
} Webhook_controll_config_t;

extern Webhook_controll_config_t Webhook_controll_config;

class ClassFlowWebhook : public ClassFlow
{
protected:
    ClassFlowPostProcessing *flowpostprocessing;
    ClassFlowAlignment *flowAlignment;

    void SetInitialParameter(void);
    void handleMeasurement(std::string _decsep, std::string _value);

public:
    ClassFlowWebhook();
    ClassFlowWebhook(std::vector<ClassFlow *> *lfc);
    ClassFlowWebhook(std::vector<ClassFlow *> *lfc, ClassFlow *_prev);

    bool ReadParameter(FILE *pFile, std::string &aktparamgraph);
    bool doFlow(std::string temp_time);
    std::string name() { return "ClassFlowWebhook"; };
};

#endif // CLASSFWEBHOOK_H
