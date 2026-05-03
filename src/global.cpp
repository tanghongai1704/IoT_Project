#include "global.h"

SystemContext systemContext;

static StaticSemaphore_t xContextMutexBuffer;
static StaticSemaphore_t xInternetSemaphoreBuffer;

void initSystemContext()
{
    systemContext.temperature = 0;
    systemContext.humidity = 0;
    systemContext.humidex = 0;
    systemContext.weather_status = 0;

    systemContext.wifi_ssid = "";
    systemContext.wifi_pass = "";
    systemContext.core_iot_token = "";
    systemContext.core_iot_server = "";
    systemContext.core_iot_port = "";
    systemContext.ap_ssid = SSID_AP;
    systemContext.ap_pass = String(PASS_AP);
    systemContext.read_interval = 5000;

    systemContext.is_wifi_connected = false;
    systemContext.led_state = false;
    systemContext.device_mode = "AUTO";

    systemContext.neo_r = 255;
    systemContext.neo_g = 107;
    systemContext.neo_b = 107;
    systemContext.neo_brightness = 120;

    systemContext.mutex = xSemaphoreCreateMutexStatic(&xContextMutexBuffer);
    systemContext.internet_semaphore = xSemaphoreCreateBinaryStatic(&xInternetSemaphoreBuffer);

    if (systemContext.mutex == NULL || systemContext.internet_semaphore == NULL)
    {
        Serial.println("❌ System context initialization failed");
        while (true)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

// ===== LABEL =====
const char *get_weather_label(int label)
{
    switch (label)
    {
    case 0:
        return "Sunny";
    case 1:
        return "Cloudy";
    case 2:
        return "Rain";
    case 3:
        return "Storm";
    default:
        return "Unknown";
    }
}