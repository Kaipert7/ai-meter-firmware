#pragma once

#ifndef SERVER_MQTT_H
#define SERVER_MQTT_H

#include "ClassFlowDefineTypes.h"

void mqttServer_setParameter(std::vector<NumberPost *> *_NUMBERS);
std::string mqttServer_getMainTopic();

bool publishSystemData(int qos);
void GotConnected(std::string maintopic, bool SetRetainFlag);
esp_err_t sendDiscovery_and_static_Topics(void);

std::string createNodeId(std::string &topic);

void mqtt_register_uri(httpd_handle_t server);

#endif // SERVER_MQTT_H
