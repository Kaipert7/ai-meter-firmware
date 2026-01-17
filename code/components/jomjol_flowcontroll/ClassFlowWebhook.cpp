#include "defines.h"
#include "Helper.h"

#include <time.h>
#include <sstream>

#include "ClassFlowWebhook.h"
#include "connect_wifi_sta.h"

#include "time_sntp.h"
#include "interface_webhook.h"

#include "ClassFlowPostProcessing.h"
#include "ClassFlowAlignment.h"
#include "esp_log.h"

#include "ClassLogFile.h"

static const char *TAG = "WEBHOOK";

Webhook_controll_config_t Webhook_controll_config;

void ClassFlowWebhook::SetInitialParameter(void)
{
    Webhook_controll_config.enabled = false;
    Webhook_controll_config.uri = "";
    Webhook_controll_config.apikey = "";
    Webhook_controll_config.uploadImg = 0;
    Webhook_controll_config.oldValue = "";

    flowpostprocessing = NULL;
    flowAlignment = NULL;
    previousElement = NULL;
    ListFlowControll = NULL;

    disabled = false;
}

ClassFlowWebhook::ClassFlowWebhook()
{
    SetInitialParameter();
}

ClassFlowWebhook::ClassFlowWebhook(std::vector<ClassFlow *> *lfc)
{
    SetInitialParameter();

    ListFlowControll = lfc;
    for (int i = 0; i < ListFlowControll->size(); ++i)
    {
        if (((*ListFlowControll)[i])->name().compare("ClassFlowPostProcessing") == 0)
        {
            flowpostprocessing = (ClassFlowPostProcessing *)(*ListFlowControll)[i];
        }

        if (((*ListFlowControll)[i])->name().compare("ClassFlowAlignment") == 0)
        {
            flowAlignment = (ClassFlowAlignment *)(*ListFlowControll)[i];
        }
    }
}

ClassFlowWebhook::ClassFlowWebhook(std::vector<ClassFlow *> *lfc, ClassFlow *_prev)
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

        if (((*ListFlowControll)[i])->name().compare("ClassFlowAlignment") == 0)
        {
            flowAlignment = (ClassFlowAlignment *)(*ListFlowControll)[i];
        }
    }
}

bool ClassFlowWebhook::ReadParameter(FILE *pFile, std::string &aktparamgraph)
{
    aktparamgraph = trim_string_left_right(aktparamgraph);
    if (aktparamgraph.size() == 0)
    {
        if (!GetNextParagraph(pFile, aktparamgraph))
        {
            return false;
        }
    }

    if ((to_upper(aktparamgraph).compare("[WEBHOOK]") != 0) && (to_upper(aktparamgraph).compare(";[WEBHOOK]") != 0))
    {
        return false;
    }

    if (aktparamgraph[0] == ';')
    {
        Webhook_controll_config.enabled = false;
        while (getNextLine(pFile, &aktparamgraph) && !isNewParagraph(aktparamgraph));
        ESP_LOGD(TAG, "Webhook is disabled!");

        return true;
    }

    std::vector<std::string> splitted;

    while (getNextLine(pFile, &aktparamgraph) && !isNewParagraph(aktparamgraph))
    {
        splitted = split_line(aktparamgraph);

        if (splitted.size() > 1)
        {
            std::string _param = to_upper(GetParameterName(splitted[0]));

            if (_param == "URI")
            {
                Webhook_controll_config.uri = splitted[1];
            }
            else if (_param == "APIKEY")
            {
                Webhook_controll_config.apikey = splitted[1];
            }
            else if (_param == "UPLOADIMG")
            {
                if (to_upper(splitted[1]) == "1")
                {
                    Webhook_controll_config.uploadImg = 1;
                }
                else if (to_upper(splitted[1]) == "2")
                {
                    Webhook_controll_config.uploadImg = 2;
                }
            }
        }
    }

    if ((Webhook_controll_config.uri.length() > 0) && (Webhook_controll_config.apikey.length() > 0))
    {
        LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "Init Webhook with uri: " + Webhook_controll_config.uri + ", apikey: *****");
        WebhookInit(Webhook_controll_config.uri, Webhook_controll_config.apikey);
        Webhook_controll_config.enabled = true;
    }
    else
    {
        LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "Webhook init skipped as we are missing some parameters");
        Webhook_controll_config.enabled = false;
    }

    return true;
}

void ClassFlowWebhook::handleMeasurement(std::string _decsep, std::string _value)
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
            flowpostprocessing->NUMBERS[j]->MeasurementV2 = _value;
        }
    }
}

bool ClassFlowWebhook::doFlow(std::string temp_time)
{
    if (!Webhook_controll_config.enabled)
    {
        return true;
    }

    if (flowpostprocessing)
    {
        printf("vor sende WebHook");
        bool numbersWithError = WebhookPublish(flowpostprocessing->GetNumbers());

#ifdef ALGROI_LOAD_FROM_MEM_AS_JPG
        if ((Webhook_controll_config.uploadImg == 1 || (Webhook_controll_config.uploadImg != 0 && numbersWithError)) && flowAlignment && flowAlignment->AlgROI)
        {
            WebhookUploadPic(flowAlignment->AlgROI);
        }
#endif
    }

    return true;
}
