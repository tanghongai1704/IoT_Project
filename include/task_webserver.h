
#ifndef __TASK_WEBSERVER_H__
#define __TASK_WEBSERVER_H__

#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <ElegantOTA.h>
#include <task_handler.h>

extern AsyncWebServer server;
extern AsyncWebSocket ws;

void webserver_init();
void webserver_task(void *pvParameters);

void Webserver_sendata(const String &data);

#endif