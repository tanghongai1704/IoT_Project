#include "coreiot.h"

static WiFiClient espClient;
static PubSubClient client(espClient);
static const char device_id[] = "ESP32_002";
static String topic_rpc = String("devices/") + device_id + "/rpc";
static String topic_telemetry = String("devices/") + device_id + "/telemetry";

static String getCoreIotServerUrl()
{
    String serverUrl;
    if (takeSystemContext(portMAX_DELAY))
    {
        serverUrl = systemContext.core_iot_server;
        giveSystemContext();
    }
    return serverUrl;
}

static String getCoreIotPort()
{
    String portString;
    if (takeSystemContext(portMAX_DELAY))
    {
        portString = systemContext.core_iot_port;
        giveSystemContext();
    }
    return portString;
}

void reconnect()
{
    while (!client.connected())
    {
        const String serverUrl = getCoreIotServerUrl();
        const String portString = getCoreIotPort();

        Serial.print("Infomation debuffing: ");
        Serial.print("Device ID: ");
        Serial.println(device_id);
        Serial.print("MQTT Server: ");
        Serial.print(serverUrl);
        Serial.print("MQTT Port: ");
        Serial.println(portString);
        Serial.print("Attempting MQTT connection...");

        if (client.connect(device_id))
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

    char message[length + 1];
    memcpy(message, payload, length);
    message[length] = '\0';
    Serial.print("Payload: ");
    Serial.println(message);

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
        const char *params = doc["params"] | "";
        if (takeSystemContext(portMAX_DELAY))
        {
            systemContext.device_mode = "MANUAL";
            systemContext.led_state = (strcmp(params, "ON") == 0);
            giveSystemContext();
        }

        if (strcmp(params, "ON") == 0)
        {
            Serial.println("Device turned ON.");
        }
        else
        {
            Serial.println("Device turned OFF.");
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
    Serial.println("Waiting for internet access before CoreIOT startup...");
    if (xSemaphoreTake(systemContext.internet_semaphore, portMAX_DELAY) == pdTRUE)
    {
        Serial.println("✅ Internet access granted to CoreIOT Task.");
    }

    String serverUrl;
    String portString;
    if (takeSystemContext(portMAX_DELAY))
    {
        serverUrl = systemContext.core_iot_server;
        portString = systemContext.core_iot_port;
        giveSystemContext();
    }

    Serial.println("Connected to WiFi! Now connecting to CoreIOT Server...");
    Serial.print("Connecting to CoreIOT Server: ");
    Serial.print(serverUrl);
    Serial.print(" on port ");
    Serial.println(portString);

    client.setServer(serverUrl.c_str(), portString.toInt());
    client.setCallback(callback);
}

void coreiot_task(void *pvParameters)
{
    setup_coreiot();

    while (1)
    {
        if (!client.connected())
        {
            reconnect();
        }
        client.loop();

        float temperature = 0;
        float humidity = 0;
        float humidex = 0;
        int weather_status = 0;

        if (takeSystemContext(portMAX_DELAY))
        {
            temperature = systemContext.temperature;
            humidity = systemContext.humidity;
            humidex = systemContext.humidex;
            weather_status = systemContext.weather_status;
            giveSystemContext();
        }

        String payload = "{\"device\":\"" + String(device_id) + "\",\"temperature\":" + String(temperature) + ",\"humidity\":" + String(humidity) + ",\"humidex\":" + String(humidex) + ",\"weather_status\":\"" + get_weather_label(weather_status) + "\"}";

        client.publish(topic_telemetry.c_str(), payload.c_str());
        Serial.println("Published payload: " + payload);
        vTaskDelay(10000);
    }
}