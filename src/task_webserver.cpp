#include "task_webserver.h"

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

void Webserver_sendata(const String &data)
{
    if (ws.count() > 0)
    {
        ws.textAll(data); // Gửi đến tất cả client đang kết nối
        Serial.println("📤 Đã gửi dữ liệu qua WebSocket: " + data);
    }
    else
    {
        Serial.println("⚠️ Không có client WebSocket nào đang kết nối!");
    }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    if (type == WS_EVT_CONNECT)
    {
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    }
    else if (type == WS_EVT_DISCONNECT)
    {
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
    }
    else if (type == WS_EVT_DATA)
    {
        AwsFrameInfo *info = (AwsFrameInfo *)arg;

        if (info->opcode == WS_TEXT)
        {
            String message = String((char *)data).substring(0, len);
            // parseJson(message, true);
            handleWebSocketMessage(message);
        }
    }
}

void webserver_init()
{
    ws.onEvent(onEvent);
    server.addHandler(&ws);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/index.html", "text/html"); });

    server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/script.js", "application/javascript"); });

    server.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/styles.css", "text/css"); });

    server.begin();
    ElegantOTA.begin(&server);

    Serial.println("WebServer started");
}

// ==================== SEND SENSOR REALTIME ====================
void sendSensorData()
{
    float temp = glob_temperature;
    float humi = glob_humidity;

    String json = "{\"page\":\"home\",\"value\":{\"temp\":" +
                  String(temp) + ",\"humi\":" + String(humi) + "}}";

    Webserver_sendata(json);
}

// ==================== TASK ====================
void webserver_task(void *pvParameters)
{
    webserver_init();

    unsigned long lastSend = 0;

    while (1)
    {
        ws.cleanupClients();
        ElegantOTA.loop();

        // gửi sensor mỗi 2s
        if (millis() - lastSend > 2000)
        {
            sendSensorData();
            lastSend = millis();
        }

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}