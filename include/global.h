#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

extern float glob_temperature;
extern float glob_humidity;
extern float glob_humidex;
extern int glob_weather_status; // 0: Sunny, 1: Cloudy, 2: Rainy, 3: Stormy

extern String WIFI_SSID;
extern String WIFI_PASS;
extern String CORE_IOT_TOKEN;
extern String CORE_IOT_SERVER;
extern String CORE_IOT_PORT;
extern String AP_SSID;
extern String AP_PASS;
extern int READ_INTERVAL;

extern boolean isWifiConnected;
extern SemaphoreHandle_t xBinarySemaphoreInternet;

// ================= DEVICE STATE =================
extern bool led_state;

extern String device_mode; // AUTO / MANUAL

// NeoPixel state
extern int neo_r;
extern int neo_g;
extern int neo_b;
extern int neo_brightness;

// ===== LABEL =====
const char *get_weather_label(int label);

#endif