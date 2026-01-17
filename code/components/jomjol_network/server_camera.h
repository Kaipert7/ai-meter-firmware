#pragma once

#ifndef SERVER_CAMERA_H
#define SERVER_CAMERA_H

#include <esp_log.h>
#include <esp_http_server.h>

void camera_register_uri(httpd_handle_t server);

#endif // SERVER_CAMERA_H
