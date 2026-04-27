#include "task_wifi.h"

void startAP()
{
    WiFi.softAP(SSID_AP, PASS_AP);

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
}

bool Wifi_reconnect()
{
    if (WiFi.status() == WL_CONNECTED)
        return true;

    Serial.println("[WiFi] Reconnecting...");

    WiFi.disconnect(false); // ❗ KHÔNG erase config
    startSTA();

    return WiFi.status() == WL_CONNECTED;
}

// ================= STATE =================
void wifi_task(void *pvParameters)
{
    static bool hasConfig = false;

    static bool staStarted = false;
    static bool apStarted = false;

    static bool internetNotified = false;

    while (1)
    {
        hasConfig = !check_info_File(1);

        // ================= AP MODE =================
        if (!hasConfig)
        {
            if (!apStarted)
            {
                Serial.println("📡 AP MODE");

                WiFi.mode(WIFI_AP); // giữ web server
                startAP();

                apStarted = true;
                staStarted = false;
                internetNotified = false;
            }
        }

        // ================= STA MODE =================
        else
        {
            if (!staStarted)
            {
                Serial.println("🌐 STA MODE");

                WiFi.mode(WIFI_AP_STA); // giữ AP + STA
                startSTA();

                staStarted = true;
                apStarted = false;
            }
        }

        // ================= INTERNET READY =================
        if (staStarted && WiFi.status() == WL_CONNECTED)
        {
            if (!internetNotified)
            {
                Serial.println("✅ INTERNET READY");
                xSemaphoreGive(xBinarySemaphoreInternet);
                internetNotified = true;
            }
        }
        else
        {
            internetNotified = false;
        }

        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}