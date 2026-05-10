#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

typedef struct
{
    float temperature;
    float humidity;
    int alert_status;

    String wifi_ssid;
    String wifi_pass;
    String core_iot_token;
    String core_iot_server;
    String core_iot_port;
    String mqtt_target;
    String ap_ssid;
    String ap_pass;
    int read_interval;

    bool is_wifi_connected;
    bool led_state;
    String device_mode;

    int neo_r;
    int neo_g;
    int neo_b;
    int neo_brightness;

    SemaphoreHandle_t mutex;
    SemaphoreHandle_t internet_semaphore;
} SystemContext;

extern SystemContext systemContext;

void initSystemContext();

static inline bool takeSystemContext(TickType_t timeout = portMAX_DELAY)
{
    return xSemaphoreTake(systemContext.mutex, timeout) == pdTRUE;
}

static inline void giveSystemContext()
{
    xSemaphoreGive(systemContext.mutex);
}

const char *get_alert_status(int label);

#endif