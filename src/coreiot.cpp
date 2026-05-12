#include "coreiot.h"
#include "global.h"
#include "led_blinky.h"
#include "neo_blinky.h"

static WiFiClient espClient;
static PubSubClient client(espClient);
static const char device_id[] = "ESP32_001";
static String topic_rpc = String("devices/") + device_id + "/rpc";
static String iot_topic_rpc = String("v1/devices/me/rpc/request/+");
static String topic_telemetry = String("devices/") + device_id + "/telemetry";
static String iot_topic_telemetry = String("v1/devices/me/telemetry");
static String mqtt_server_url;
static uint16_t mqtt_server_port = 1883;
static String mqtt_token;
static String mqtt_target_mode = "coreiot";
static char temp_led_state[50] = "";
static char hum_led_state[50] = "";
static int last_alert_status = -1;

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

static String getCoreIotToken()
{
    String token;
    if (takeSystemContext(portMAX_DELAY))
    {
        token = systemContext.core_iot_token;
        giveSystemContext();
    }
    return token;
}

static String getMqttTargetMode()
{
    String targetMode = "coreiot";
    if (takeSystemContext(portMAX_DELAY))
    {
        targetMode = systemContext.mqtt_target;
        giveSystemContext();
    }
    targetMode.toLowerCase();
    if (targetMode != "broker")
    {
        targetMode = "coreiot";
    }
    return targetMode;
}

void reconnect()
{
    while (!client.connected())
    {
        mqtt_server_url = getCoreIotServerUrl();
        mqtt_server_port = getCoreIotPort().toInt();
        mqtt_token = getCoreIotToken();
        mqtt_target_mode = getMqttTargetMode();
        const bool isCoreIotTarget = (mqtt_target_mode == "coreiot");

        client.setServer(mqtt_server_url.c_str(), mqtt_server_port);

        Serial.print("Infomation debuffing: ");
        Serial.print("Device ID: ");
        Serial.println(device_id);
        Serial.print("MQTT Target: ");
        Serial.println(mqtt_target_mode);
        Serial.print("MQTT Server: ");
        Serial.println(mqtt_server_url);
        Serial.print("MQTT Port: ");
        Serial.println(mqtt_server_port);
        Serial.println("Attempting MQTT connection...");

        if (isCoreIotTarget && mqtt_token.isEmpty())
        {
            Serial.println("failed, token is empty for CoreIOT target");
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        bool isConnected = false;
        if (isCoreIotTarget)
        {
            Serial.println("Using CoreIOT authentication: " + mqtt_token);
            isConnected = client.connect(device_id, mqtt_token.c_str(), "");
        }
        else
        {
            isConnected = client.connect(device_id);
        }

        if (isConnected && isCoreIotTarget)
        {
            Serial.println("connected to CoreIOT Server!");
            client.subscribe(iot_topic_rpc.c_str());
            Serial.println("Subscribed to " + iot_topic_rpc);
        }
        else if (isConnected)
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

void handle_temp_led_command()
{
    pinMode(LED_GPIO, OUTPUT);
    int t_on = 150;
    int t_off = 1000;
    while (1)
    {
        if (strcmp(temp_led_state, "cold") == 0)
        {
            // Serial.println("Setting TEMP_LED to COLD pattern");
            // Nháy một lần
            digitalWrite(LED_GPIO, HIGH);
            vTaskDelay(pdMS_TO_TICKS(t_on));
            digitalWrite(LED_GPIO, LOW);
            vTaskDelay(pdMS_TO_TICKS(t_off));
        }
        else if (strcmp(temp_led_state, "normal") == 0)
        {
            // Serial.println("Setting TEMP_LED to NORMAL pattern");
            // Nháy hai lần
            digitalWrite(LED_GPIO, HIGH);
            vTaskDelay(pdMS_TO_TICKS(t_on));
            digitalWrite(LED_GPIO, LOW);
            vTaskDelay(pdMS_TO_TICKS(t_on));
            digitalWrite(LED_GPIO, HIGH);
            vTaskDelay(pdMS_TO_TICKS(t_on));
            digitalWrite(LED_GPIO, LOW);
            vTaskDelay(pdMS_TO_TICKS(t_off));
        }
        else if (strcmp(temp_led_state, "hot") == 0)
        {
            // Serial.println("Setting TEMP_LED to HOT pattern");
            // Nháy ba lần
            for (int i = 0; i < 3; i++)
            {
                digitalWrite(LED_GPIO, HIGH);
                vTaskDelay(pdMS_TO_TICKS(t_on));
                digitalWrite(LED_GPIO, LOW);
                vTaskDelay(pdMS_TO_TICKS(t_on));
            }
            vTaskDelay(pdMS_TO_TICKS(t_off));
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
    }
}

void handle_humi_neo_command()
{
    Adafruit_NeoPixel strip(LED_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);
    strip.begin();
    strip.clear();
    strip.show();

    int r = 0, g = 0, b = 0;
    float T = 1000;
    float B = 0.5;
    int half = T / 2;

    while (1)
    {
        if (strcmp(hum_led_state, "dry") == 0)
        {
            r = 255;
            g = 200;
            b = 0;
        }
        else if (strcmp(hum_led_state, "normal") == 0)
        {
            r = 0;
            g = 255;
            b = 0;
        }
        else if (strcmp(hum_led_state, "humid") == 0)
        {
            r = 0;
            g = 100;
            b = 255;
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        uint32_t color = strip.Color(r, g, b);

        strip.setPixelColor(0, color);
        strip.show();
        vTaskDelay(pdMS_TO_TICKS(half));

        strip.setPixelColor(0, 0);
        strip.show();
        vTaskDelay(pdMS_TO_TICKS(half));
    }
}

void callback(char *topic, byte *payload, unsigned int length)
{
    // Serial.print("Message arrived [");
    // Serial.print(topic);
    // Serial.println("] ");

    char message[length + 1];
    memcpy(message, payload, length);
    message[length] = '\0';
    // Serial.print("Payload: ");
    // Serial.println(message);

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, message);
    if (error)
    {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return;
    }

    const char *method = doc["method"] | "";
    /*
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
    */

    if (strcmp(method, "TEMP_LED") == 0)
    {
        Serial.print("Received TEMP_LED command with params: ");
        Serial.println(doc["params"] | "");
        const char *params = doc["params"] | "";
        strncpy(temp_led_state, params, sizeof(temp_led_state) - 1);
        temp_led_state[sizeof(temp_led_state) - 1] = '\0';
    }
    else if (strcmp(method, "HUMI_NEO") == 0)
    {
        Serial.print("Received HUMI_NEO command with params: ");
        Serial.println(doc["params"] | "");
        const char *params = doc["params"] | "";
        strncpy(hum_led_state, params, sizeof(hum_led_state) - 1);
        hum_led_state[sizeof(hum_led_state) - 1] = '\0';
    }
    else
    {
        // Serial.print("Unknown method: ");
        // Serial.println(method);
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

    mqtt_server_url = serverUrl;
    mqtt_server_port = portString.toInt();

    Serial.println("Connected to WiFi! Now connecting to CoreIOT Server...");
    Serial.print("Connecting to CoreIOT Server: ");
    Serial.println(serverUrl);
    Serial.print(" on port ");
    Serial.println(portString);

    client.setServer(mqtt_server_url.c_str(), mqtt_server_port);
    client.setCallback(callback);
}

void coreiot_task(void *pvParameters)
{
    setup_coreiot();

    unsigned long lastPublish = 0;

    // Start handling LED commands in separate tasks to avoid blocking the main loop
    xTaskCreate([](void *)
                { handle_temp_led_command(); }, "TEMP_LED_Command_Task", 4096, NULL, 1, NULL);
    xTaskCreate([](void *)
                { handle_humi_neo_command(); }, "HUMI_NEO_Command_Task", 4096, NULL, 1, NULL);

    while (1)
    {
        if (!client.connected())
        {
            reconnect();
        }
        client.loop();

        unsigned long now = millis();

        int publish_interval = 10000;
        if (takeSystemContext(portMAX_DELAY))
        {
            publish_interval = systemContext.publish_interval;
            giveSystemContext();
        }

        if (now - lastPublish >= publish_interval)
        {
            lastPublish = now;

            float temperature = 0;
            float humidity = 0;
            int alert_status = 0;

            if (takeSystemContext(portMAX_DELAY))
            {
                temperature = systemContext.temperature;
                humidity = systemContext.humidity;
                alert_status = systemContext.alert_status;
                giveSystemContext();
            }

            String alert_changed = (last_alert_status != alert_status) ? "true" : "false";
            last_alert_status = alert_status;

            String payload = "{\"device\":\"" + String(device_id) + "\",\"temperature\":" + String(temperature) + ",\"humidity\":" + String(humidity) + ",\"alert_status\":\"" + get_alert_status(alert_status) + "\",\"alert_changed\":" + alert_changed + "}";

            if (getMqttTargetMode() == "coreiot")
            {
                client.publish(iot_topic_telemetry.c_str(), payload.c_str());
            }
            else
            {
                client.publish(topic_telemetry.c_str(), payload.c_str());
            }
            // Serial.println("Published payload: " + payload);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}