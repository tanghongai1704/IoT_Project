#include "task_wifi.h"
#include <WiFi.h>

// ================= GLOBAL GUARD =================
volatile bool wifiSwitching = false;

// ================= AP MODE =================
void startAP()
{
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

    WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());

    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 20)
    {
        delay(500);
        Serial.print(".");
        retry++;
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("✅ STA Connected");
        Serial.println(WiFi.localIP());
    }
    else
    {
        Serial.println("❌ STA Failed");
    }
}

// ================= WIFI TASK =================
void wifi_task(void *pvParameters)
{
    static bool lastHasConfig = false;

    static bool staStarted = false;
    static bool apStarted = false;

    static bool internetNotified = false;

    while (1)
    {
        bool hasConfig = !check_info_File(1);

        // ================= DETECT CHANGE =================
        if (hasConfig != lastHasConfig)
        {
            Serial.println("🔄 WIFI MODE SWITCH DETECTED");

            wifiSwitching = true;

            // 🛑 STOP NETWORK SAFE
            vTaskDelay(pdMS_TO_TICKS(200));

            WiFi.disconnect(true);
            delay(300);

            staStarted = false;
            apStarted = false;
            internetNotified = false;

            wifiSwitching = false;
        }

        lastHasConfig = hasConfig;

        // ================= AP MODE =================
        if (!hasConfig)
        {
            if (!apStarted)
            {
                Serial.println("📡 SWITCH TO AP MODE");

                WiFi.mode(WIFI_AP);
                startAP();

                apStarted = true;
                staStarted = false;
            }
        }

        // ================= STA MODE =================
        else
        {
            if (!staStarted)
            {
                Serial.println("🌐 SWITCH TO STA MODE");

                WiFi.mode(WIFI_AP_STA);
                startSTA();

                staStarted = true;
                apStarted = false;
            }
        }

        // ================= INTERNET READY =================
        if (!wifiSwitching && staStarted && WiFi.status() == WL_CONNECTED)
        {
            if (!internetNotified)
            {
                Serial.println("🌍 INTERNET READY");
                xSemaphoreGive(xBinarySemaphoreInternet);
                internetNotified = true;
            }
        }

        if (WiFi.status() != WL_CONNECTED)
        {
            internetNotified = false;
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}