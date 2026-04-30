#include "mainserver.h"
#include "task_check_info.h"
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <cmath>

bool isAPMode = true;

WebServer server(80);

unsigned long connect_start_ms = 0;
bool connecting = false;

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
  StaticJsonDocument<128> doc;

  doc["ssid"] = SSID_AP;
  doc["ip"] = WiFi.localIP().toString();
  doc["ap_password"] = PASS_AP;

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
  float t = glob_temperature;
  float h = glob_humidity;

  float hx = glob_humidex;

  StaticJsonDocument<256> doc;

  doc["temperature"] = t;
  doc["humidity"] = h;
  doc["humidex"] = hx;
  doc["state_temp"] = getTempState(t);
  doc["state_hum"] = getHumState(h);
  doc["comfort"] = getComfort(hx);

  String out;
  serializeJson(doc, out);

  server.send(200, "application/json", out);
}

void handleDevices()
{
  StaticJsonDocument<256> doc;

  doc["mode"] = device_mode;

  JsonObject led = doc.createNestedObject("led");
  led["state"] = led_state;
  led["logic_state"] = getTempState(glob_temperature);

  JsonObject neo = doc.createNestedObject("neo");
  JsonArray color = neo.createNestedArray("color");
  color.add(neo_r);
  color.add(neo_g);
  color.add(neo_b);

  neo["brightness"] = neo_brightness;
  neo["logic_state"] = getHumState(glob_humidity);

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

  StaticJsonDocument<256> doc;
  deserializeJson(doc, server.arg("plain"));

  if (doc["mode"])
    device_mode = doc["mode"].as<String>();

  if (doc["led"].is<bool>())
    led_state = doc["led"];

  if (doc["neo"])
  {
    neo_r = doc["neo"]["r"];
    neo_g = doc["neo"]["g"];
    neo_b = doc["neo"]["b"];
    neo_brightness = doc["neo"]["brightness"];
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

  WIFI_SSID = doc["ssid"] | "";
  WIFI_PASS = doc["password"] | "";
  CORE_IOT_TOKEN = doc["token"] | "";
  CORE_IOT_SERVER = doc["server"] | "";
  CORE_IOT_PORT = doc["port"] | "";

  Save_info_File(WIFI_SSID, WIFI_PASS, CORE_IOT_TOKEN, CORE_IOT_SERVER, CORE_IOT_PORT);

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

  String body = server.arg("plain");

  // ví dụ update interval sensor
  Serial.println("Settings: " + body);

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

  // static files
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
                    { server.send(404, "text/plain", "Route not found"); });

  server.begin();
}

void startAP()
{
  WiFi.mode(WIFI_AP);
  WiFi.softAP(String(SSID_AP), String(PASS_AP));
  sleep(10);
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  isAPMode = true;
  connecting = false;
}

void connectToWiFi()
{
  WiFi.mode(WIFI_STA);
  if (WIFI_PASS.isEmpty())
  {
    WiFi.begin(WIFI_SSID.c_str());
  }
  else
  {
    WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
  }
  Serial.print("Connecting to: ");
  Serial.print(WIFI_SSID.c_str());

  Serial.print(" Password: ");
  Serial.print(WIFI_PASS.c_str());
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

    // BOOT Button to switch to AP Mode
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

    // STA Mode
    if (connecting)
    {
      if (WiFi.status() == WL_CONNECTED)
      {
        Serial.print("STA IP address: ");
        Serial.println(WiFi.localIP());
        isWifiConnected = true; // Internet access

        xSemaphoreGive(xBinarySemaphoreInternet);
        Serial.println("WiFi connected! Internet access granted.");

        isAPMode = false;
        connecting = false;
      }
      else if (millis() - connect_start_ms > 10000)
      { // timeout 10s
        Serial.println("WiFi connect failed! Back to AP.");
        startAP();
        setupServer();
        connecting = false;
        isWifiConnected = false;
      }
    }

    vTaskDelay(20); // avoid watchdog reset
  }
}