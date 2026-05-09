#ifndef __TASK_CHECK_INFO_H__
#define __TASK_CHECK_INFO_H__

#include <ArduinoJson.h>
#include "LittleFS.h"
#include "global.h"

bool check_info_File(bool check);
void Load_info_File();
void Delete_info_File();
void Save_info_File(String wifi_ssid, String wifi_pass, String CORE_IOT_TOKEN, String CORE_IOT_SERVER, String CORE_IOT_PORT, String MQTT_TARGET, String ap_ssid, String ap_pass, int read_interval);

#endif