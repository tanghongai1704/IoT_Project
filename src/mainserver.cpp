#include "mainserver.h"
#include "task_check_info.h"
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <cmath>

static bool isAPMode = true;
static WebServer server(80);
static unsigned long connect_start_ms = 0;
static bool connecting = false;

// ========== Handlers ==========
void handleRoot()
{
  serveFile("/index.html", "text/html");
}

void handleJS()
{
  serveFile("/script.js", "application/javascript");
}

void handleCSS()
{
  serveFile("/styles.css", "text/css");
}

String jsonResponse(String body)
{
  return "{" + body + "}";
}

void handleSystem()
{
  String ap_ssid;
  String ap_pass;
  String wifi_ssid;
  String wifi_pass;
  String device_token;
  String mqtt_server;
  int mqtt_port = 1883;
  int read_interval = 5000;

  if (takeSystemContext(portMAX_DELAY))
  {
    ap_ssid = systemContext.ap_ssid;
    ap_pass = systemContext.ap_pass;
    wifi_ssid = systemContext.wifi_ssid;
    wifi_pass = systemContext.wifi_pass;
    device_token = systemContext.core_iot_token;
    mqtt_server = systemContext.core_iot_server;
    mqtt_port = systemContext.core_iot_port.toInt();
    read_interval = systemContext.read_interval;
    giveSystemContext();
  }

  StaticJsonDocument<256> doc;
  doc["ssid"] = ap_ssid;
  doc["ip"] = WiFi.localIP().toString();
  doc["ap_password"] = ap_pass;
  doc["wifi_ssid"] = wifi_ssid;
  doc["wifi_password"] = wifi_pass;
  doc["device_token"] = device_token;
  doc["mqtt_server"] = mqtt_server;
  doc["mqtt_port"] = mqtt_port;
  doc["ap_name"] = ap_ssid;
  doc["read_interval"] = read_interval;

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

String getTempState(float t)
{
  if (t < 20)
    return "COLD";
  if (t < 35)
    return "NORMAL";
  return "HOT";
}

String getHumState(float h)
{
  if (h < 40)
    return "DRY";
  if (h < 70)
    return "COMFORT";
  return "HUMID";
}

String getComfort(float hx)
{
  if (hx < 30)
    return "EASY";
  if (hx < 35)
    return "STICKY";
  if (hx < 40)
    return "UNCOMFY";
  return "RISKY";
}

void handleSensorsAPI()
{
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

  StaticJsonDocument<256> doc;
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  doc["humidex"] = humidex;
  doc["state_temp"] = getTempState(temperature);
  doc["state_hum"] = getHumState(humidity);
  doc["comfort"] = getComfort(humidex);
  doc["weather"] = get_weather_label(weather_status);

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleDevices()
{
  String mode;
  bool led_state = false;
  int neo_r = 0;
  int neo_g = 0;
  int neo_b = 0;
  int neo_brightness = 0;
  float temperature = 0;
  float humidity = 0;

  if (takeSystemContext(portMAX_DELAY))
  {
    mode = systemContext.device_mode;
    led_state = systemContext.led_state;
    neo_r = systemContext.neo_r;
    neo_g = systemContext.neo_g;
    neo_b = systemContext.neo_b;
    neo_brightness = systemContext.neo_brightness;
    temperature = systemContext.temperature;
    humidity = systemContext.humidity;
    giveSystemContext();
  }

  StaticJsonDocument<256> doc;
  doc["mode"] = mode;

  JsonObject led = doc.createNestedObject("led");
  led["state"] = led_state;
  led["logic_state"] = getTempState(temperature);

  JsonObject neo = doc.createNestedObject("neo");
  JsonArray color = neo.createNestedArray("color");
  color.add(neo_r);
  color.add(neo_g);
  color.add(neo_b);
  neo["brightness"] = neo_brightness;
  neo["logic_state"] = getHumState(humidity);

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleControl()
{
  if (!server.hasArg("plain"))
  {
    server.send(400, "application/json", "{\"error\":\"no body\"}");
    return;
  }

  StaticJsonDocument<384> doc;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err)
  {
    server.send(400, "application/json", "{\"error\":\"json parse failed\"}");
    return;
  }

  if (takeSystemContext(portMAX_DELAY))
  {
    if (doc["mode"])
      systemContext.device_mode = doc["mode"].as<String>();

    if (doc["led"].is<bool>())
      systemContext.led_state = doc["led"];

    if (doc["neo"])
    {
      systemContext.neo_r = doc["neo"]["r"];
      systemContext.neo_g = doc["neo"]["g"];
      systemContext.neo_b = doc["neo"]["b"];
      systemContext.neo_brightness = doc["neo"]["brightness"];
    }
    giveSystemContext();
  }

  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void handleConfig()
{
  if (!server.hasArg("plain"))
  {
    server.send(400, "application/json", "{\"error\":\"no body\"}");
    return;
  }

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err)
  {
    server.send(400, "application/json", "{\"error\":\"json parse failed\"}");
    return;
  }

  String wifi_ssid = doc["ssid"] | "";
  String wifi_pass = doc["password"] | "";
  String core_iot_token = doc["token"] | "";
  String core_iot_server = doc["server"] | "";
  int port = doc["port"] | 1883;
  String ap_ssid = doc["ap_ssid"] | String();
  String ap_pass = doc["ap_password"] | String();
  int read_interval = doc["read_interval"] | 0;

  String saved_ap_ssid;
  String saved_ap_pass;
  int saved_read_interval = read_interval;

  if (takeSystemContext(portMAX_DELAY))
  {
    systemContext.wifi_ssid = wifi_ssid;
    systemContext.wifi_pass = wifi_pass;
    systemContext.core_iot_token = core_iot_token;
    systemContext.core_iot_server = core_iot_server;
    systemContext.core_iot_port = String(port);
    if (!ap_ssid.isEmpty())
      systemContext.ap_ssid = ap_ssid;
    if (!ap_pass.isEmpty())
      systemContext.ap_pass = ap_pass;
    if (read_interval > 0)
      systemContext.read_interval = read_interval;

    saved_ap_ssid = systemContext.ap_ssid;
    saved_ap_pass = systemContext.ap_pass;
    saved_read_interval = systemContext.read_interval;
    giveSystemContext();
  }

  Save_info_File(wifi_ssid, wifi_pass, core_iot_token, core_iot_server, String(port), saved_ap_ssid, saved_ap_pass, saved_read_interval);

  isAPMode = false;
  connecting = true;
  connect_start_ms = millis();
  connectToWiFi();

  server.send(200, "application/json", "{\"status\":\"saved\"}");
}

void handleSettingsAPI()
{
  if (!server.hasArg("plain"))
  {
    server.send(400, "application/json", "{\"error\":\"no body\"}");
    return;
  }

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err)
  {
    server.send(400, "application/json", "{\"error\":\"json parse failed\"}");
    return;
  }

  if (takeSystemContext(portMAX_DELAY))
  {
    if (doc["ap_ssid"])
      systemContext.ap_ssid = doc["ap_ssid"].as<String>();
    if (doc["ap_password"])
      systemContext.ap_pass = doc["ap_password"].as<String>();
    if (doc["sensor_interval"])
      systemContext.read_interval = max(1, doc["sensor_interval"].as<int>());
    giveSystemContext();
  }

  String wifi_ssid;
  String wifi_pass;
  String core_iot_token;
  String core_iot_server;
  String core_iot_port;
  String ap_ssid;
  String ap_pass;
  int read_interval;

  if (takeSystemContext(portMAX_DELAY))
  {
    wifi_ssid = systemContext.wifi_ssid;
    wifi_pass = systemContext.wifi_pass;
    core_iot_token = systemContext.core_iot_token;
    core_iot_server = systemContext.core_iot_server;
    core_iot_port = systemContext.core_iot_port;
    ap_ssid = systemContext.ap_ssid;
    ap_pass = systemContext.ap_pass;
    read_interval = systemContext.read_interval;
    giveSystemContext();
  }

  Save_info_File(wifi_ssid, wifi_pass, core_iot_token, core_iot_server, core_iot_port, ap_ssid, ap_pass, read_interval);

  Serial.println("Settings updated:");
  Serial.println(ap_ssid);
  Serial.println(ap_pass);
  Serial.println(read_interval);

  server.send(200, "application/json", "{\"status\":\"updated\"}");
}

void handleReset()
{
  server.send(200, "application/json", "{\"status\":\"restarting\"}");
  delay(1000);
  ESP.restart();
}

bool serveFile(const char *path, const char *type)
{
  if (!LittleFS.exists(path))
  {
    server.send(404, "text/plain", "File not found");
    return false;
  }

  File file = LittleFS.open(path, "r");
  if (!file)
  {
    server.send(500, "text/plain", "Open failed");
    return false;
  }

  server.streamFile(file, type);
  file.close();
  return true;
}

// ========== WiFi ==========
void setupServer()
{
  server.on("/", HTTP_GET, handleRoot);

  server.on("/script.js", HTTP_GET, handleJS);
  server.on("/styles.css", HTTP_GET, handleCSS);

  server.on("/api/system", HTTP_GET, handleSystem);
  server.on("/api/sensors", HTTP_GET, handleSensorsAPI);
  server.on("/api/devices", HTTP_GET, handleDevices);
  server.on("/api/control", HTTP_POST, handleControl);
  server.on("/api/config", HTTP_POST, handleConfig);
  server.on("/api/settings", HTTP_POST, handleSettingsAPI);
  server.on("/api/reset", HTTP_POST, handleReset);

  server.onNotFound([]()
  {
    server.send(404, "text/plain", "Route not found");
  });

  server.begin();
}

void startAP()
{
  String ap_ssid;
  String ap_pass;
  if (takeSystemContext(portMAX_DELAY))
  {
    ap_ssid = systemContext.ap_ssid;
    ap_pass = systemContext.ap_pass;
    giveSystemContext();
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid.c_str(), ap_pass.c_str());
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  isAPMode = true;
  connecting = false;
}

void connectToWiFi()
{
  String wifi_ssid;
  String wifi_pass;
  if (takeSystemContext(portMAX_DELAY))
  {
    wifi_ssid = systemContext.wifi_ssid;
    wifi_pass = systemContext.wifi_pass;
    giveSystemContext();
  }

  WiFi.mode(WIFI_STA);
  if (wifi_pass.isEmpty())
  {
    WiFi.begin(wifi_ssid.c_str());
  }
  else
  {
    WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
  }
  Serial.print("Connecting to: ");
  Serial.print(wifi_ssid.c_str());
  Serial.print(" Password: ");
  Serial.print(wifi_pass.c_str());
}

// ========== Main task ==========
void main_server_task(void *pvParameters)
{
  pinMode(BOOT_PIN, INPUT_PULLUP);

  startAP();
  setupServer();

  while (1)
  {
    server.handleClient();

    if (digitalRead(BOOT_PIN) == LOW)
    {
      vTaskDelay(100);
      if (digitalRead(BOOT_PIN) == LOW)
      {
        if (!isAPMode)
        {
          startAP();
          setupServer();
        }
      }
    }

    if (connecting)
    {
      if (WiFi.status() == WL_CONNECTED)
      {
        Serial.print("STA IP address: ");
        Serial.println(WiFi.localIP());

        if (takeSystemContext(portMAX_DELAY))
        {
          systemContext.is_wifi_connected = true;
          giveSystemContext();
        }

        xSemaphoreGive(systemContext.internet_semaphore);
        Serial.println("WiFi connected! Internet access granted.");

        isAPMode = false;
        connecting = false;
      }
      else if (millis() - connect_start_ms > 10000)
      {
        Serial.println("WiFi connect failed! Back to AP.");
        startAP();
        setupServer();
        connecting = false;

        if (takeSystemContext(portMAX_DELAY))
        {
          systemContext.is_wifi_connected = false;
          giveSystemContext();
        }
      }
    }

    vTaskDelay(20);
  }
}