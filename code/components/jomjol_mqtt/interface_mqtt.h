#pragma once

#ifndef INTERFACE_MQTT_H
#define INTERFACE_MQTT_H

#include <string>
#include <map>
#include <functional>

bool MQTT_Configure(void *callbackOnConnected);
int MQTT_Init();
void MQTTdestroy_client(bool _disable);

bool MQTTPublish(std::string _key, std::string _content, int qos, bool retained_flag = 1); // retained Flag as Standart

bool getMQTTisEnabled();
bool getMQTTisConnected();

void MQTTregisterConnectFunction(std::string name, std::function<void()> func);
void MQTTunregisterConnectFunction(std::string name);
void MQTTregisterSubscribeFunction(std::string topic, std::function<bool(std::string, char *, int)> func);
void MQTTdestroySubscribeFunction();
void MQTTconnected();

#endif // INTERFACE_MQTT_H
