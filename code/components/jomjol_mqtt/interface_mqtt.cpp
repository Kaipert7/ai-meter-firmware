#include "defines.h"

#include "interface_mqtt.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <mqtt_client.h>
#include <cJSON.h>

#include "Helper.h"

#include "connect_wifi_sta.h"
#include "read_network_config.h"

#include "ClassLogFile.h"
#include "MainFlowControl.h"

static const char *TAG = "MQTT IF";

std::map<std::string, std::function<void()>> *connectFunktionMap = NULL;
std::map<std::string, std::function<bool(std::string, char *, int)>> *subscribeFunktionMap = NULL;

esp_mqtt_client_handle_t client = NULL;

std::string caCert = "";
std::string clientCert = "";
std::string clientKey = "";

int failedOnRound = -1;
int MQTTReconnectCnt = 0;

void (*callbackOnConnected)(std::string, bool) = NULL;

bool MQTTPublish(std::string _key, std::string _content, int qos, bool retained_flag)
{
    if (!mqtt_controll_config.mqtt_enabled)
    {
        // MQTT sevice not started / configured (MQTT_Init not called before)
        return false;
    }

    if (failedOnRound == getCountFlowRounds())
    {
        // we already failed in this round, do not retry until the next round
        return true; // Fail quietly
    }

    MQTT_Init(); // Re-Init client if not initialized yet/anymore

    if (mqtt_controll_config.mqtt_initialized && mqtt_controll_config.mqtt_connected)
    {
        int msg_id = esp_mqtt_client_publish(client, _key.c_str(), _content.c_str(), 0, qos, retained_flag);
        if (msg_id == -1)
        {
            LogFile.WriteToFile(ESP_LOG_WARN, TAG, "Failed to publish topic '" + _key + "', re-trying...");
            msg_id = esp_mqtt_client_publish(client, _key.c_str(), _content.c_str(), 0, qos, retained_flag);

            if (msg_id == -1)
            {
                LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Failed to publish topic '" + _key + "', skipping all MQTT publishings in this round!");
                failedOnRound = getCountFlowRounds();
                return false;
            }
        }

        if (_content.length() > 80)
        {
            // Truncate message if too long
            _content.resize(80);
            _content.append("..");
        }

        LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "Published topic: " + _key + ", content: " + _content);
        return true;
    }
    else
    {
        LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "Publish skipped. Client not initalized or not connected. (topic: " + _key + ")");
        return false;
    }
}

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    std::string topic = "";
    switch (event->event_id)
    {
    case MQTT_EVENT_BEFORE_CONNECT:
        mqtt_controll_config.mqtt_initialized = true;
        break;

    case MQTT_EVENT_CONNECTED:
        MQTTReconnectCnt = 0;
        mqtt_controll_config.mqtt_initialized = true;
        mqtt_controll_config.mqtt_connected = true;
        MQTTconnected();
        break;

    case MQTT_EVENT_DISCONNECTED:
        mqtt_controll_config.mqtt_connected = false;
        MQTTReconnectCnt++;
        LogFile.WriteToFile(ESP_LOG_WARN, TAG, "Disconnected, trying to reconnect");

        if (MQTTReconnectCnt >= 5)
        {
            MQTTReconnectCnt = 0;
            LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Disconnected, multiple reconnect attempts failed, still retrying...");
        }
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGD(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGD(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGD(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGD(TAG, "MQTT_EVENT_DATA");
        ESP_LOGD(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
        ESP_LOGD(TAG, "DATA=%.*s", event->data_len, event->data);
        topic.assign(event->topic, event->topic_len);
        if (subscribeFunktionMap != NULL)
        {
            if (subscribeFunktionMap->find(topic) != subscribeFunktionMap->end())
            {
                ESP_LOGD(TAG, "call subcribe function for topic %s", topic.c_str());
                (*subscribeFunktionMap)[topic](topic, event->data, event->data_len);
            }
        }
        else
        {
            ESP_LOGW(TAG, "no handler available\r\n");
        }
        break;

    case MQTT_EVENT_ERROR:
        // http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Toc398718033 --> chapter 3.2.2.3

        // The server does not support the level of the MQTT protocol requested by the client
        // NOTE: Only protocol 3.1.1 is supported (refer to setting in sdkconfig)
        if (event->error_handle->connect_return_code == MQTT_CONNECTION_REFUSE_PROTOCOL)
        {
            LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Connection refused, unacceptable protocol version (0x01)");
        }
        // The client identifier is correct UTF-8 but not allowed by the server
        // e.g. clientID empty (cannot be the case -> default set in firmware)
        else if (event->error_handle->connect_return_code == MQTT_CONNECTION_REFUSE_ID_REJECTED)
        {
            LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Connection refused, identifier rejected (0x02)");
        }
        // The network connection has been made but the MQTT service is unavailable
        else if (event->error_handle->connect_return_code == MQTT_CONNECTION_REFUSE_SERVER_UNAVAILABLE)
        {
            LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Connection refused, Server unavailable (0x03)");
        }
        // The data in the user name or password is malformed
        else if (event->error_handle->connect_return_code == MQTT_CONNECTION_REFUSE_BAD_USERNAME)
        {
            LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Connection refused, malformed data in username or password (0x04)");
        }
        // The client is not authorized to connect
        else if (event->error_handle->connect_return_code == MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED)
        {
            LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Connection refused, not authorized. Check username/password (0x05)");
        }
        else
        {
            LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Other event id:" + event->error_handle->connect_return_code);
            ESP_LOGE(TAG, "Other event id:%d", event->error_handle->connect_return_code);
        }
        break;

    default:
        ESP_LOGD(TAG, "Other event id:%d", event->event_id);
        break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, (int)event_id);
    mqtt_event_handler_cb((esp_mqtt_event_handle_t)event_data);
}

bool MQTT_Configure(void *_callbackOnConnected)
{
    if ((mqtt_controll_config.uri.length() == 0) || (mqtt_controll_config.maintopic.length() == 0) || (mqtt_controll_config.clientname.length() == 0))
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Init aborted! Config error (URI, MainTopic or ClientID missing)");
        return false;
    }

    callbackOnConnected = (void (*)(std::string, bool))(_callbackOnConnected);

    if (mqtt_controll_config.clientCertFilename.length() && mqtt_controll_config.clientKeyFilename.length())
    {
        std::ifstream cert_ifs(mqtt_controll_config.clientCertFilename);
        if (cert_ifs.is_open())
        {
            std::string cert_content((std::istreambuf_iterator<char>(cert_ifs)), (std::istreambuf_iterator<char>()));
            clientCert = cert_content;
            cert_ifs.close();
            LogFile.WriteToFile(ESP_LOG_INFO, TAG, "using clientCert: " + mqtt_controll_config.clientCertFilename);
        }
        else
        {
            LogFile.WriteToFile(ESP_LOG_INFO, TAG, "could not open clientCert: " + mqtt_controll_config.clientCertFilename);
        }

        std::ifstream key_ifs(mqtt_controll_config.clientKeyFilename);
        if (key_ifs.is_open())
        {
            std::string key_content((std::istreambuf_iterator<char>(key_ifs)), (std::istreambuf_iterator<char>()));
            clientKey = key_content;
            key_ifs.close();
            LogFile.WriteToFile(ESP_LOG_INFO, TAG, "using clientKey: " + mqtt_controll_config.clientKeyFilename);
        }
        else
        {
            LogFile.WriteToFile(ESP_LOG_INFO, TAG, "could not open clientKey: " + mqtt_controll_config.clientKeyFilename);
        }
    }

    if (mqtt_controll_config.caCertFilename.length())
    {
        std::ifstream ca_ifs(mqtt_controll_config.caCertFilename);
        if (ca_ifs.is_open())
        {
            std::string content((std::istreambuf_iterator<char>(ca_ifs)), (std::istreambuf_iterator<char>()));
            caCert = content;
            ca_ifs.close();
            LogFile.WriteToFile(ESP_LOG_INFO, TAG, "using caCert: " + mqtt_controll_config.caCertFilename);
        }
        else
        {
            LogFile.WriteToFile(ESP_LOG_INFO, TAG, "could not open caCert: " + mqtt_controll_config.caCertFilename);
        }
    }

#ifdef __HIDE_PASSWORD
    LogFile.WriteToFile(ESP_LOG_DEBUG, TAG,
                        "URI: " + mqtt_controll_config.uri + ", clientname: " + mqtt_controll_config.clientname +
                            ", user: " + mqtt_controll_config.user + ", password: XXXXXXXX, maintopic: " + mqtt_controll_config.maintopic +
                            ", last-will-topic: " + mqtt_controll_config.lwt_topic +
                            ", keepAlive: " + std::to_string(mqtt_controll_config.keepAlive) +
                            ", RetainFlag: " + std::to_string(mqtt_controll_config.retainFlag));
#else
    LogFile.WriteToFile(ESP_LOG_DEBUG, TAG,
                        "URI: " + mqtt_controll_config.uri + ", clientname: " + mqtt_controll_config.clientname +
                            ", user: " + mqtt_controll_config.user + ", password: " + mqtt_controll_config.password +
                            ", maintopic: " + mqtt_controll_config.maintopic + ", last-will-topic: " + mqtt_controll_config.lwt_topic +
                            ", keepAlive: " + std::to_string(mqtt_controll_config.keepAlive) +
                            ", RetainFlag: " + std::to_string(mqtt_controll_config.retainFlag));
#endif

    mqtt_controll_config.mqtt_configOK = true;
    return true;
}

int MQTT_Init()
{
    if (mqtt_controll_config.mqtt_initialized)
    {
        return 0;
    }

    if (mqtt_controll_config.mqtt_configOK)
    {
        mqtt_controll_config.mqtt_enabled = true;
    }
    else
    {
        LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "Init called, but client is not yet configured.");
        return 0;
    }

    if (!get_wifi_sta_is_connected())
    {
        LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "Init called, but WIFI is not yet connected.");
        return 0;
    }

    LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Init");
    MQTTdestroy_client(false);

    esp_mqtt_client_config_t mqtt_cfg = {};

    mqtt_cfg.broker.address.uri = mqtt_controll_config.uri.c_str();
    mqtt_cfg.credentials.client_id = mqtt_controll_config.clientname.c_str();
    mqtt_cfg.network.disable_auto_reconnect = false;    // Reconnection routine active (Default: false)
    mqtt_cfg.network.reconnect_timeout_ms = 15000;      // Try to reconnect to broker (Default: 10000ms)
    mqtt_cfg.network.timeout_ms = 20000;                // Network Timeout (Default: 10000ms)
    mqtt_cfg.session.message_retransmit_timeout = 3000; // Time after message resent when broker not acknowledged (QoS1, QoS2)
    mqtt_cfg.session.last_will.topic = mqtt_controll_config.lwt_topic.c_str();
    mqtt_cfg.session.last_will.retain = 1;
    mqtt_cfg.session.last_will.msg = mqtt_controll_config.lwt_disconnected.c_str();
    mqtt_cfg.session.last_will.msg_len = (int)(mqtt_controll_config.lwt_disconnected.length());
    mqtt_cfg.session.keepalive = mqtt_controll_config.keepAlive;
    mqtt_cfg.buffer.size = 2048; // size of MQTT send/receive buffer

    if (caCert.length())
    {
        mqtt_cfg.broker.verification.certificate = caCert.c_str();
        mqtt_cfg.broker.verification.certificate_len = caCert.length() + 1;

        // Skip any validation of server certificate CN field, this reduces the
        // security of TLS and makes the *MQTT* client susceptible to MITM attacks
        mqtt_cfg.broker.verification.skip_cert_common_name_check = !mqtt_controll_config.validateServerCert;
    }

    if (clientCert.length() && clientKey.length())
    {
        mqtt_cfg.credentials.authentication.certificate = clientCert.c_str();
        mqtt_cfg.credentials.authentication.certificate_len = clientCert.length() + 1;

        mqtt_cfg.credentials.authentication.key = clientKey.c_str();
        mqtt_cfg.credentials.authentication.key_len = clientKey.length() + 1;
    }

    if (mqtt_controll_config.user.length() && mqtt_controll_config.password.length())
    {
        mqtt_cfg.credentials.username = mqtt_controll_config.user.c_str();
        mqtt_cfg.credentials.authentication.password = mqtt_controll_config.password.c_str();
    }

    client = esp_mqtt_client_init(&mqtt_cfg);
    if (client)
    {
        esp_err_t ret = esp_mqtt_client_register_event(client, mqtt_controll_config.esp_mqtt_ID, mqtt_event_handler, client);
        if (ret != ESP_OK)
        {
            LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Could not register event (ret=" + std::to_string(ret) + ")!");
            mqtt_controll_config.mqtt_initialized = false;
            return -1;
        }

        ret = esp_mqtt_client_start(client);
        if (ret != ESP_OK)
        {
            LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Client start failed (retval=" + std::to_string(ret) + ")!");
            mqtt_controll_config.mqtt_initialized = false;
            return -1;
        }
        else
        {
            LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Client started, waiting for established connection...");
            mqtt_controll_config.mqtt_initialized = true;
            return 1;
        }
    }
    else
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Init failed, no handle created!");
        mqtt_controll_config.mqtt_initialized = false;
        return -1;
    }
}

void MQTTdestroy_client(bool _disable = false)
{
    if (client)
    {
        if (mqtt_controll_config.mqtt_connected)
        {
            MQTTdestroySubscribeFunction();
            esp_mqtt_client_disconnect(client);
            mqtt_controll_config.mqtt_connected = false;
        }
        esp_mqtt_client_stop(client);
        esp_mqtt_client_destroy(client);
        client = NULL;
        mqtt_controll_config.mqtt_initialized = false;
    }

    if (_disable)
    {
        // Disable MQTT service, avoid restart with MQTTPublish
        mqtt_controll_config.mqtt_configOK = false;
    }
}

bool getMQTTisEnabled()
{
    return mqtt_controll_config.mqtt_enabled;
}

bool getMQTTisConnected()
{
    return mqtt_controll_config.mqtt_connected;
}

bool mqtt_handler_flow_start(std::string _topic, char *_data, int _data_len)
{
    ESP_LOGD(TAG, "Handler called: topic %s, data %.*s", _topic.c_str(), _data_len, _data);

    MQTTCtrlFlowStart(_topic);
    return ESP_OK;
}

bool mqtt_handler_set_prevalue(std::string _topic, char *_data, int _data_len)
{
    // ESP_LOGD(TAG, "Handler called: topic %s, data %.*s", _topic.c_str(), _data_len, _data);
    // example: {"numbersname": "main", "value": 12345.1234567}

    if (_data_len > 0)
    {
        // Check if data length > 0
        cJSON *jsonData = cJSON_Parse(_data);
        cJSON *numbersname = cJSON_GetObjectItemCaseSensitive(jsonData, "numbersname");
        cJSON *value = cJSON_GetObjectItemCaseSensitive(jsonData, "value");

        if (cJSON_IsString(numbersname) && (numbersname->valuestring != NULL))
        {
            // Check if numbersname is valid
            if (cJSON_IsNumber(value))
            {
                // Check if value is a number
                LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "handler_set_prevalue called: numbersname: " + std::string(numbersname->valuestring) + ", value: " + std::to_string(value->valuedouble));
                if (flowctrl.UpdatePrevalue(std::to_string(value->valuedouble), std::string(numbersname->valuestring), true))
                {
                    cJSON_Delete(jsonData);
                    return ESP_OK;
                }
            }
            else
            {
                LogFile.WriteToFile(ESP_LOG_WARN, TAG, "handler_set_prevalue: value not a valid number (\"value\": 12345.12345)");
            }
        }
        else
        {
            LogFile.WriteToFile(ESP_LOG_WARN, TAG, "handler_set_prevalue: numbersname not a valid string (\"numbersname\": \"main\")");
        }
        cJSON_Delete(jsonData);
    }
    else
    {
        LogFile.WriteToFile(ESP_LOG_WARN, TAG, "handler_set_prevalue: handler called, but no data received");
    }

    return ESP_FAIL;
}

void MQTTconnected()
{
    if (mqtt_controll_config.mqtt_connected)
    {
        LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Connected to broker");

        if (connectFunktionMap != NULL)
        {
            for (std::map<std::string, std::function<void()>>::iterator it = connectFunktionMap->begin(); it != connectFunktionMap->end(); ++it)
            {
                it->second();
                ESP_LOGD(TAG, "call connect function %s", it->first.c_str());
            }
        }

        // Subcribe to topics
        // Note: Further subsriptions are handled in GPIO class
        //*****************************************
        std::function<bool(std::string topic, char *data, int data_len)> subHandler1 = mqtt_handler_flow_start;
        MQTTregisterSubscribeFunction(mqtt_controll_config.maintopic + "/ctrl/flow_start", subHandler1); // subcribe to maintopic/ctrl/flow_start

        std::function<bool(std::string topic, char *data, int data_len)> subHandler2 = mqtt_handler_set_prevalue;
        MQTTregisterSubscribeFunction(mqtt_controll_config.maintopic + "/ctrl/set_prevalue", subHandler2); // subcribe to maintopic/ctrl/set_prevalue

        if (subscribeFunktionMap != NULL)
        {
            for (std::map<std::string, std::function<bool(std::string, char *, int)>>::iterator it = subscribeFunktionMap->begin(); it != subscribeFunktionMap->end(); ++it)
            {
                int msg_id = esp_mqtt_client_subscribe(client, it->first.c_str(), 0);
                LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "topic " + it->first + " subscribe successful");
            }
        }

        /* Send Static Topics and Homeassistant Discovery */
        if (callbackOnConnected)
        {
            // Call onConnected callback routine --> mqtt_server
            callbackOnConnected(mqtt_controll_config.maintopic, mqtt_controll_config.retainFlag);
        }
    }
}

void MQTTregisterConnectFunction(std::string name, std::function<void()> func)
{
    ESP_LOGD(TAG, "MQTTregisteronnectFunction %s\r\n", name.c_str());
    if (connectFunktionMap == NULL)
    {
        connectFunktionMap = new std::map<std::string, std::function<void()>>();
    }

    if ((*connectFunktionMap)[name] != NULL)
    {
        ESP_LOGW(TAG, "connect function %s already registred", name.c_str());
        return;
    }

    (*connectFunktionMap)[name] = func;

    if (mqtt_controll_config.mqtt_connected)
    {
        func();
    }
}

void MQTTunregisterConnectFunction(std::string name)
{
    ESP_LOGD(TAG, "unregisterConnnectFunction %s\r\n", name.c_str());
    if ((connectFunktionMap != NULL) && (connectFunktionMap->find(name) != connectFunktionMap->end()))
    {
        connectFunktionMap->erase(name);
    }
}

void MQTTregisterSubscribeFunction(std::string topic, std::function<bool(std::string, char *, int)> func)
{
    ESP_LOGD(TAG, "registerSubscribeFunction %s", topic.c_str());
    if (subscribeFunktionMap == NULL)
    {
        subscribeFunktionMap = new std::map<std::string, std::function<bool(std::string, char *, int)>>();
    }

    if ((*subscribeFunktionMap)[topic] != NULL)
    {
        ESP_LOGW(TAG, "topic %s already registered for subscription", topic.c_str());
        return;
    }

    (*subscribeFunktionMap)[topic] = func;
}

void MQTTdestroySubscribeFunction()
{
    if (subscribeFunktionMap != NULL)
    {
        if (mqtt_controll_config.mqtt_connected)
        {
            for (std::map<std::string, std::function<bool(std::string, char *, int)>>::iterator it = subscribeFunktionMap->begin(); it != subscribeFunktionMap->end(); ++it)
            {
                int msg_id = esp_mqtt_client_unsubscribe(client, it->first.c_str());
                ESP_LOGD(TAG, "topic %s unsubscribe successful, msg_id=%d", it->first.c_str(), msg_id);
            }
        }

        subscribeFunktionMap->clear();
        delete subscribeFunktionMap;
        subscribeFunktionMap = NULL;
    }
}
