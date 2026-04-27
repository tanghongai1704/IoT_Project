#include "task_wifi.h"

enum WifiState
{
    WIFI_IDLE,
    WIFI_AP_MODE,
    WIFI_CONNECTING,
    WIFI_CONNECTED
};

WifiState wifiState = WIFI_IDLE;

void startAP()
{
    WiFi.mode(WIFI_AP);
    WiFi.softAP(String(SSID_AP), String(PASS_AP));
    // sleep(10);
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
}

void startSTA()
{
    if (WIFI_SSID.isEmpty())
    {
        Serial.println("⚠️ No WiFi config");
        return;
    }

    WiFi.mode(WIFI_STA);

    if (WIFI_PASS.isEmpty())
        WiFi.begin(WIFI_SSID.c_str());
    else
        WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());

    Serial.println("[WiFi] Connecting...");
}

bool Wifi_reconnect()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        return true;
    }

    Serial.println("[WiFi] Reconnecting...");
    WiFi.disconnect();
    startSTA();

    return WiFi.status() == WL_CONNECTED;
}

void wifi_task(void *pvParameters)
{
    while (1)
    {
        bool hasConfig = !check_info_File(1);

        switch (wifiState)
        {
        case WIFI_IDLE:
            if (hasConfig)
            {
                Serial.println("➡️ Start STA");
                startSTA();
                wifiState = WIFI_CONNECTING;
            }
            else
            {
                Serial.println("➡️ Start AP");
                startAP();
                wifiState = WIFI_AP_MODE;
            }
            break;

        case WIFI_CONNECTING:
            if (WiFi.status() == WL_CONNECTED)
            {
                Serial.println("✅ STA Connected");
                xSemaphoreGive(xBinarySemaphoreInternet);
                wifiState = WIFI_CONNECTED;
            }
            break;

        case WIFI_CONNECTED:
            if (WiFi.status() != WL_CONNECTED)
            {
                Serial.println("⚠️ Lost WiFi → reconnect");
                startSTA();
                wifiState = WIFI_CONNECTING;
            }
            break;

        case WIFI_AP_MODE:
            if (hasConfig)
            {
                Serial.println("➡️ Switch AP → STA");
                WiFi.softAPdisconnect(true);
                startSTA();
                wifiState = WIFI_CONNECTING;
            }
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}