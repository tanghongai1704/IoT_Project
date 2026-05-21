#include "global.h"

SystemContext systemContext;

static StaticSemaphore_t xContextMutexBuffer;
static StaticSemaphore_t xInternetSemaphoreBuffer;
static StaticSemaphore_t xSensorUpdateSemaphoreBuffer;

void initSystemContext()
{
    // Initialize all shared state to known defaults before tasks start.
    systemContext.temperature = 0;
    systemContext.humidity = 0;
    systemContext.alert_status = 0;

    systemContext.wifi_ssid = "";
    systemContext.wifi_pass = "";
    systemContext.core_iot_token = "";
    systemContext.core_iot_server = "";
    systemContext.core_iot_port = "";
    systemContext.mqtt_target = "coreiot";
    systemContext.ap_ssid = SSID_AP;
    systemContext.ap_pass = String(PASS_AP);
    systemContext.read_interval = 5000;
    systemContext.publish_interval = 10000;

    systemContext.is_wifi_connected = false;
    systemContext.led_state = false;
    systemContext.device_mode = "AUTO";

    systemContext.neo_r = 255;
    systemContext.neo_g = 107;
    systemContext.neo_b = 107;
    systemContext.neo_brightness = 120;

    systemContext.mutex = xSemaphoreCreateMutexStatic(&xContextMutexBuffer);
    systemContext.internet_semaphore = xSemaphoreCreateBinaryStatic(&xInternetSemaphoreBuffer);
    systemContext.sensor_update_semaphore = xSemaphoreCreateBinaryStatic(&xSensorUpdateSemaphoreBuffer);

    if (systemContext.mutex == NULL || systemContext.internet_semaphore == NULL || systemContext.sensor_update_semaphore == NULL)
    {
        Serial.println("❌ System context initialization failed");
        // Stop here because the application cannot safely run without sync primitives.
        while (true)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

// Human-readable alert labels for the TinyML output classes.
const char *get_alert_status(int label)
{
    switch (label)
    {
    case 0:
        return "Safe";
    case 1:
        return "Caution";
    case 2:
        return "Extreme Caution";
    case 3:
        return "Danger";
    case 4:
        return "Extreme Danger";
    default:
        return "Unknown";
    }
}