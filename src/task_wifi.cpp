#include "task_wifi.h"
#include <WiFi.h>

// ================= AP MODE =================
void startAP()
{
    WiFi.mode(WIFI_AP);
    WiFi.softAP(String(SSID_AP), String(PASS_AP));

    Serial.print("📡 AP IP: ");
    Serial.println(WiFi.softAPIP());
}

// ================= STA MODE =================
void startSTA()
{
    if (WIFI_SSID.isEmpty())
    {
        Serial.println("⚠️ Chưa có WiFi config");
        return;
    }

    Serial.println("[WiFi] Connecting...");

    WiFi.mode(WIFI_STA);

    if (WIFI_PASS.isEmpty())
        WiFi.begin(WIFI_SSID.c_str());
    else
        WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
}

// ================= WIFI TASK =================
void wifi_task(void *pvParameters)
{
    static bool isAP = false;
    static bool isSTA = false;
    static bool internetNotified = false;

    while (1)
    {
        bool hasConfig = check_info_File(1);

        // ================= SWITCH TO STA =================
        if (hasConfig && !isSTA)
        {
            Serial.println("🔄 Switching to STA mode");

            Webserver_stop();
            WiFi.disconnect(true);

            startSTA();

            isSTA = true;
            isAP = false;
            internetNotified = false;
        }

        // ================= SWITCH TO AP =================
        if (!hasConfig && !isAP)
        {
            Serial.println("🔄 Switching to AP mode");

            Webserver_stop();
            WiFi.disconnect(true);

            startAP();

            isAP = true;
            isSTA = false;
            internetNotified = false;
        }

        if (isSTA && WiFi.status() == WL_CONNECTED)
        {
            if (!internetNotified)
            {
                Serial.println("🌍 INTERNET READY");
                xSemaphoreGive(xBinarySemaphoreInternet);
                internetNotified = true;
                Serial.print("STA IP: ");
                Serial.println(WiFi.localIP());
            }
        }

        if (WiFi.status() != WL_CONNECTED)
        {
            internetNotified = false;
        }

        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}