#pragma once

#ifndef MAINFLOWCONTROL_H
#define MAINFLOWCONTROL_H

#include <string>
#include <esp_log.h>
#include <esp_http_server.h>

#include "CImageBasis.h"
#include "ClassFlowControll.h"
#include "openmetrics.h"

extern ClassFlowControll flowctrl;

void CheckIsPlannedReboot(void);
bool getIsPlannedReboot(void);

void InitializeFlowTask(void);
void DeleteMainFlowTask(void);
bool isSetupModusActive(void);

int getCountFlowRounds(void);

esp_err_t MQTTCtrlFlowStart(std::string _topic);

esp_err_t GetRawJPG(httpd_req_t *req);
esp_err_t GetJPG(std::string _filename, httpd_req_t *req);

void main_flow_register_uri(httpd_handle_t server);

#endif // MAINFLOWCONTROL_H
