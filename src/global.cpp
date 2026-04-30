#include "global.h"
float glob_temperature = 0;
float glob_humidity = 0;
float glob_humidex = 0;

String WIFI_SSID;
String WIFI_PASS;
String CORE_IOT_TOKEN;
String CORE_IOT_SERVER;
String CORE_IOT_PORT;

boolean isWifiConnected = false;
SemaphoreHandle_t xBinarySemaphoreInternet = xSemaphoreCreateBinary();

bool led_state = false;

String device_mode = "AUTO";

int neo_r = 255;
int neo_g = 107;
int neo_b = 107;
int neo_brightness = 120;