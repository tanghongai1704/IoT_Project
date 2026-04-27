#include "task_wifi.h"

void startAP()
{
    WiFi.mode(WIFI_AP);
    WiFi.softAP(String(SSID_AP), String(PASS_AP));
    sleep(10);
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
}

void startSTA()
{
    if (WIFI_SSID.isEmpty())
    {
        Serial.println("⚠️ Chưa có WiFi config");
        return;
    }

    WiFi.mode(WIFI_STA);

    Serial.println("[WiFi] Connecting...");

    if (WIFI_PASS.isEmpty())
        WiFi.begin(WIFI_SSID.c_str());
    else
        WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());

    int retry = 0;

    while (WiFi.status() != WL_CONNECTED && retry < 20)
    {
        vTaskDelay(500 / portTICK_PERIOD_MS);
        Serial.print(".");
        retry++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("\n✅ Connected!");
        Serial.println(WiFi.localIP());

        xSemaphoreGive(xBinarySemaphoreInternet);
    }
    else
    {
        Serial.println("\n❌ Connect fail!");
    }
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