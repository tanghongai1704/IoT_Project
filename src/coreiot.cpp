#include "coreiot.h"

WiFiClient espClient;
PubSubClient client(espClient);

String device_id = "ESP32_002";
String topic_rpc = "devices/" + device_id + "/rpc";
String topic_telemetry = "devices/" + device_id + "/telemetry";

void reconnect()
{
    // Loop until we're reconnected
    while (!client.connected())
    {
        Serial.print("Infomation debuffing: ");
        Serial.print("Device ID: ");
        Serial.println(device_id);
        Serial.print("MQTT Server: ");
        Serial.print(CORE_IOT_SERVER);
        Serial.print("MQTT Port: ");
        Serial.println(CORE_IOT_PORT);
        Serial.print("Attempting MQTT connection...");
        // Attempt to connect (username=token, password=empty)
        Serial.print("Connecting to CoreIOT Server... with token: ");
        Serial.println(CORE_IOT_TOKEN);
        if (client.connect(device_id.c_str()))
        {
            Serial.println("connected to CoreIOT Server!");
            client.subscribe(topic_rpc.c_str());
            Serial.println("Subscribed to " + topic_rpc);
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }
}

void callback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.println("] ");

    // Allocate a temporary buffer for the message
    char message[length + 1];
    memcpy(message, payload, length);
    message[length] = '\0';
    Serial.print("Payload: ");
    Serial.println(message);

    // Parse JSON
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, message);

    if (error)
    {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return;
    }

    const char *method = doc["method"] | "";
    if (strcmp(method, "POWER") == 0)
    {
        device_mode = "MANUAL";
        // Check params type (could be boolean, int, or string according to your RPC)
        // Example: {"method": "setValueLED", "params": "ON"}
        const char *params = doc["params"] | "";

        if (strcmp(params, "ON") == 0)
        {
            Serial.println("Device turned ON.");
            // TODO: Implement LED ON logic
            led_state = true;
        }
        else
        {
            Serial.println("Device turned OFF.");
            // TODO: Implement LED OFF logic
            led_state = false;
        }
    }
    else
    {
        Serial.print("Unknown method: ");
        Serial.println(method);
    }
}

void setup_coreiot()
{

    // Serial.print("Connecting to WiFi...");
    // WiFi.begin(wifi_ssid, wifi_password);
    // while (WiFi.status() != WL_CONNECTED) {

    // while (isWifiConnected == false) {
    //   delay(500);
    //   Serial.print(".");
    // }

    while (1)
    {
        if (xSemaphoreTake(xBinarySemaphoreInternet, portMAX_DELAY))
        {
            Serial.println("✅ Internet access granted to CoreIOT Task.");
            break;
        }
        delay(500);
        Serial.print(".........................");
    }

    Serial.println("Connected to WiFi! Now connecting to CoreIOT Server...");

    Serial.print("Connecting to CoreIOT Server: ");
    Serial.print(CORE_IOT_SERVER);
    Serial.print(" on port ");
    Serial.println(CORE_IOT_PORT);
    client.setServer(CORE_IOT_SERVER.c_str(), CORE_IOT_PORT.toInt());
    client.setCallback(callback);
}

void coreiot_task(void *pvParameters)
{
    // WIFI_SSID = "DUY AN";
    // WIFI_PASS = "DUYAN2019";
    // CORE_IOT_TOKEN = "";
    // CORE_IOT_SERVER = "127.0.0.1";
    // CORE_IOT_PORT = "1884";
    setup_coreiot();

    while (1)
    {

        if (!client.connected())
        {
            reconnect();
        }
        client.loop();

        // Sample payload, publish to 'v1/devices/me/telemetry'
        String payload = "{\"device\":\"" + String(device_id) + "\",\"temperature\":" + String(glob_temperature) + ",\"humidity\":" + String(glob_humidity) + ",\"humidex\":" + String(glob_humidex) + ",\"weather_status\":\"" + get_weather_label(glob_weather_status) + "\"}";

        client.publish(topic_telemetry.c_str(), payload.c_str());

        Serial.println("Published payload: " + payload);
        vTaskDelay(10000); // Publish every 10 seconds
    }
}