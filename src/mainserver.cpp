#include "mainserver.h"
#include "task_check_info.h"
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <cmath>
#include <DNSServer.h>
#include "esp_system.h"
#include "esp_spi_flash.h"

static bool isAPMode = true;
static WebServer server(80);
static unsigned long connect_start_ms = 0;
static bool connecting = false;
static DNSServer dnsServer;
const byte DNS_PORT = 53;

struct GPIOConfig
{
  int pin;
  String mode;
  int value;
};

// static const int ALLOWED_GPIO_PINS[] = {2, 4, 5, 12, 13, 14, 15, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33};
static const int ALLOWED_GPIO_PINS[] = {
    1, 2, 4, 5, 6, 7, 8, 9,
    10, 11, 12, 13, 14, 15, 16, 17,
    18, 21, 33, 34, 35, 36, 37, 38,
    39, 40, 41, 42, 43, 44, 45, 47, 48};
static const int ALLOWED_GPIO_PIN_COUNT = sizeof(ALLOWED_GPIO_PINS) / sizeof(ALLOWED_GPIO_PINS[0]);
static const int GPIO_MAX_CONFIGS = ALLOWED_GPIO_PIN_COUNT;
static const int PWM_CHANNEL_COUNT = 8;
static const int PWM_FREQ = 5000;
static const int PWM_RESOLUTION = 8;

static GPIOConfig gpioConfigs[GPIO_MAX_CONFIGS];
static int gpioConfigCount = 0;
static int pwmPinMap[PWM_CHANNEL_COUNT];
static bool gpioRuntimeInitialized = false;

bool isAllowedGPIOPin(int pin)
{
  for (int i = 0; i < ALLOWED_GPIO_PIN_COUNT; ++i)
  {
    if (ALLOWED_GPIO_PINS[i] == pin)
    {
      return true;
    }
  }
  return false;
}

int findGPIOConfigIndex(int pin)
{
  for (int i = 0; i < gpioConfigCount; ++i)
  {
    if (gpioConfigs[i].pin == pin)
    {
      return i;
    }
  }
  return -1;
}

void releasePWMChannelForPin(int pin)
{
  for (int channel = 0; channel < PWM_CHANNEL_COUNT; ++channel)
  {
    if (pwmPinMap[channel] == pin)
    {
      ledcDetachPin(pin);
      pwmPinMap[channel] = -1;
      return;
    }
  }
}

int getOrAssignPWMChannel(int pin)
{
  for (int channel = 0; channel < PWM_CHANNEL_COUNT; ++channel)
  {
    if (pwmPinMap[channel] == pin)
    {
      return channel;
    }
  }

  for (int channel = 0; channel < PWM_CHANNEL_COUNT; ++channel)
  {
    if (pwmPinMap[channel] == -1)
    {
      pwmPinMap[channel] = pin;
      return channel;
    }
  }

  return -1;
}

bool upsertGPIOConfig(int pin, const String &mode, int value, String &error)
{
  if (!isAllowedGPIOPin(pin))
  {
    error = "invalid pin";
    return false;
  }

  String normalizedMode = mode;
  normalizedMode.toUpperCase();

  int normalizedValue = value;

  if (normalizedMode == "INPUT")
  {
    releasePWMChannelForPin(pin);
    pinMode(pin, INPUT);
    normalizedValue = digitalRead(pin);
  }
  else if (normalizedMode == "OUTPUT")
  {
    releasePWMChannelForPin(pin);
    pinMode(pin, OUTPUT);
    normalizedValue = value > 0 ? HIGH : LOW;
    digitalWrite(pin, normalizedValue);
  }
  else if (normalizedMode == "PWM")
  {
    normalizedValue = constrain(value, 0, 255);

    int channel = getOrAssignPWMChannel(pin);
    if (channel < 0)
    {
      error = "no free pwm channel";
      return false;
    }

    // Only initialize PWM hardware if this is a new channel assignment
    int existingIndex = findGPIOConfigIndex(pin);
    if (existingIndex < 0 || gpioConfigs[existingIndex].mode != "PWM")
    {
      Serial.print("Assigning pin ");
      Serial.print(pin);
      Serial.print(" to PWM channel ");
      Serial.print(channel);
      Serial.print(" with value ");
      Serial.println(normalizedValue);

      pinMode(pin, OUTPUT);
      ledcSetup(channel, PWM_FREQ, PWM_RESOLUTION);
      ledcAttachPin(pin, channel);
    }

    // Always write the new PWM value
    ledcWrite(channel, normalizedValue);
  }
  else
  {
    error = "invalid mode";
    return false;
  }

  int index = findGPIOConfigIndex(pin);
  if (index < 0)
  {
    if (gpioConfigCount >= GPIO_MAX_CONFIGS)
    {
      error = "gpio config full";
      return false;
    }

    index = gpioConfigCount++;
    gpioConfigs[index].pin = pin;
  }

  gpioConfigs[index].mode = normalizedMode;
  gpioConfigs[index].value = normalizedValue;
  return true;
}

bool removeGPIOConfig(int pin)
{
  int index = findGPIOConfigIndex(pin);
  if (index < 0)
  {
    return false;
  }

  releasePWMChannelForPin(pin);
  pinMode(pin, INPUT);

  for (int i = index; i < gpioConfigCount - 1; ++i)
  {
    gpioConfigs[i] = gpioConfigs[i + 1];
  }
  --gpioConfigCount;

  return true;
}

// ========== Handlers ==========
void handleRoot()
{
  serveFile("/index.html", "text/html");
}

void handleJS()
{
  serveFile("/script.js", "application/javascript");
}

void handleChartJS()
{
  serveFile("/chart.js", "application/javascript");
}

void handleCSS()
{
  serveFile("/styles.css", "text/css");
}

String jsonResponse(String body)
{
  return "{" + body + "}";
}

String formatUptime(unsigned long ms)
{
  unsigned long seconds = ms / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;

  seconds %= 60;
  minutes %= 60;

  char buffer[32];
  sprintf(buffer, "%luh %lum %lus", hours, minutes, seconds);

  return String(buffer);
}

void handleDiagnostics()
{
  StaticJsonDocument<512> doc;

  // ===== Heap =====
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t totalHeap = ESP.getHeapSize();

  // ===== WiFi RSSI =====
  int wifiRSSI = WiFi.RSSI();

  // ===== Filesystem =====
  size_t totalBytes = LittleFS.totalBytes();
  size_t usedBytes = LittleFS.usedBytes();

  float fsUsage = 0;
  if (totalBytes > 0)
  {
    fsUsage = ((float)usedBytes / (float)totalBytes) * 100.0f;
  }

  // ===== CPU Temp =====
  float cpuTemp = temperatureRead();

  // ===== Uptime =====
  String uptime = formatUptime(millis());

  // ===== MQTT =====
  bool mqttConnected = false;

  if (takeSystemContext(portMAX_DELAY))
  {
    mqttConnected = systemContext.is_wifi_connected;
    giveSystemContext();
  }

  // ===== JSON =====
  doc["free_heap_kb"] = freeHeap / 1024;
  doc["total_heap_kb"] = totalHeap / 1024;
  doc["wifi_rssi"] = wifiRSSI;
  doc["mqtt"] = mqttConnected ? "Connected" : "Disconnected";
  doc["fs_usage"] = round(fsUsage);
  doc["cpu_temp"] = round(cpuTemp);
  doc["uptime"] = uptime;

  String out;
  serializeJson(doc, out);

  server.send(200, "application/json", out);
}

void handleSystem()
{
  String ap_ssid;
  String ap_pass;
  String wifi_ssid;
  String wifi_pass;
  String device_token;
  String mqtt_server;
  String mqtt_target;
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
    mqtt_target = systemContext.mqtt_target;
    mqtt_port = systemContext.core_iot_port.toInt();
    read_interval = systemContext.read_interval;
    giveSystemContext();
  }

  StaticJsonDocument<512> doc;
  doc["ssid"] = ap_ssid;
  doc["ip"] = isAPMode
                  ? WiFi.softAPIP().toString()
                  : WiFi.localIP().toString();
  doc["ap_password"] = ap_pass;
  doc["wifi_ssid"] = wifi_ssid;
  doc["wifi_password"] = wifi_pass;
  doc["device_token"] = device_token;
  doc["mqtt_server"] = mqtt_server;
  doc["mqtt_port"] = mqtt_port;
  doc["mqtt_target"] = mqtt_target;
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
  int alert_status = 0;

  if (takeSystemContext(portMAX_DELAY))
  {
    temperature = systemContext.temperature;
    humidity = systemContext.humidity;
    alert_status = systemContext.alert_status;
    giveSystemContext();
  }

  StaticJsonDocument<512> doc;
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  doc["state_temp"] = getTempState(temperature);
  doc["state_hum"] = getHumState(humidity);
  doc["alert_status"] = get_alert_status(alert_status);

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
  String mqtt_target = doc["target"] | "coreiot";
  String ap_ssid = doc["ap_ssid"] | String();
  String ap_pass = doc["ap_password"] | String();
  int read_interval = doc["read_interval"] | 0;

  mqtt_target.toLowerCase();
  if (mqtt_target != "coreiot" && mqtt_target != "broker")
  {
    mqtt_target = "coreiot";
  }

  if (mqtt_target == "broker")
  {
    core_iot_token = "";
  }
  else if (core_iot_token.isEmpty())
  {
    server.send(400, "application/json", "{\"error\":\"device token is required for coreiot target\"}");
    return;
  }

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
    systemContext.mqtt_target = mqtt_target;
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

  Save_info_File(wifi_ssid, wifi_pass, core_iot_token, core_iot_server, String(port), mqtt_target, saved_ap_ssid, saved_ap_pass, saved_read_interval);

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
  String mqtt_target;
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
    mqtt_target = systemContext.mqtt_target;
    ap_ssid = systemContext.ap_ssid;
    ap_pass = systemContext.ap_pass;
    read_interval = systemContext.read_interval;
    giveSystemContext();
  }

  Save_info_File(wifi_ssid, wifi_pass, core_iot_token, core_iot_server, core_iot_port, mqtt_target, ap_ssid, ap_pass, read_interval);

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

void handleGPIOGet()
{
  StaticJsonDocument<1536> doc;
  JsonArray pins = doc.createNestedArray("pins");

  for (int i = 0; i < gpioConfigCount; ++i)
  {
    if (gpioConfigs[i].mode == "INPUT")
    {
      gpioConfigs[i].value = digitalRead(gpioConfigs[i].pin);
    }

    JsonObject item = pins.createNestedObject();
    item["pin"] = gpioConfigs[i].pin;
    item["mode"] = gpioConfigs[i].mode;
    item["value"] = gpioConfigs[i].value;
  }

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleGPIOPost()
{
  if (!server.hasArg("plain"))
  {
    server.send(400, "application/json", "{\"error\":\"no body\"}");
    return;
  }

  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err)
  {
    server.send(400, "application/json", "{\"error\":\"json parse failed\"}");
    return;
  }

  if (!doc["pin"].is<int>())
  {
    server.send(400, "application/json", "{\"error\":\"pin required\"}");
    return;
  }

  int pin = doc["pin"].as<int>();

  if (!isAllowedGPIOPin(pin))
  {
    server.send(400, "application/json", "{\"error\":\"invalid pin\"}");
    return;
  }

  if (doc["remove"].is<bool>() && doc["remove"].as<bool>())
  {
    if (!removeGPIOConfig(pin))
    {
      server.send(404, "application/json", "{\"error\":\"pin not configured\"}");
      return;
    }

    server.send(200, "application/json", "{\"status\":\"removed\"}");
    return;
  }

  String mode = doc["mode"] | "";
  int value = doc["value"] | 0;

  String error;
  if (!upsertGPIOConfig(pin, mode, value, error))
  {
    String out = "{\"error\":\"" + error + "\"}";
    server.send(400, "application/json", out);
    return;
  }

  int index = findGPIOConfigIndex(pin);

  StaticJsonDocument<256> outDoc;
  outDoc["status"] = "ok";
  outDoc["pin"] = pin;
  outDoc["mode"] = gpioConfigs[index].mode;
  outDoc["value"] = gpioConfigs[index].value;

  String out;
  serializeJson(outDoc, out);
  server.send(200, "application/json", out);
}

void handleScanWiFi()
{
  int n = WiFi.scanNetworks();

  DynamicJsonDocument doc(4096);

  JsonArray arr = doc.to<JsonArray>();

  for (int i = 0; i < n; i++)
  {
    JsonObject obj =
        arr.createNestedObject();

    obj["ssid"] =
        WiFi.SSID(i);

    obj["rssi"] =
        WiFi.RSSI(i);

    obj["open"] =
        (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
  }

  String response;

  serializeJson(arr, response);

  server.send(
      200,
      "application/json",
      response);

  WiFi.scanDelete();
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
  if (!gpioRuntimeInitialized)
  {
    for (int i = 0; i < PWM_CHANNEL_COUNT; ++i)
    {
      pwmPinMap[i] = -1;
    }
    gpioRuntimeInitialized = true;
  }

  server.on("/", HTTP_GET, handleRoot);

  server.on("/script.js", HTTP_GET, handleJS);
  server.on("/chart.js", HTTP_GET, handleChartJS);
  server.on("/styles.css", HTTP_GET, handleCSS);
  server.on("/favicon.ico", HTTP_GET, []()
            { server.send(204, "image/x-icon", ""); });

  server.on("/api/system", HTTP_GET, handleSystem);
  server.on("/api/sensors", HTTP_GET, handleSensorsAPI);
  server.on("/api/devices", HTTP_GET, handleDevices);
  server.on("/api/control", HTTP_POST, handleControl);
  server.on("/api/config", HTTP_POST, handleConfig);
  server.on("/api/settings", HTTP_POST, handleSettingsAPI);
  server.on("/api/reset", HTTP_POST, handleReset);
  server.on("/api/diagnostics", HTTP_GET, handleDiagnostics);
  server.on("/api/gpio", HTTP_GET, handleGPIOGet);
  server.on("/api/gpio", HTTP_POST, handleGPIOPost);
  server.on("/scanwifi", HTTP_GET, handleScanWiFi);
  server.onNotFound([]()
                    {
    if (isAPMode)
    {
        server.sendHeader("Location", "/", true);
        server.send(302, "text/plain", "");
    }
    else
    {
        server.send(404, "text/plain", "Not found");
    } });

  server.begin();
}

// void startAP()
// {
//   String ap_ssid;
//   String ap_pass;
//   if (takeSystemContext(portMAX_DELAY))
//   {
//     ap_ssid = systemContext.ap_ssid;
//     ap_pass = systemContext.ap_pass;
//     giveSystemContext();
//   }

//   WiFi.mode(WIFI_AP);
//   WiFi.softAP(ap_ssid.c_str(), ap_pass.c_str());
//   Serial.print("AP IP address: ");
//   Serial.println(WiFi.softAPIP());
//   isAPMode = true;
//   connecting = false;
// }

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

  IPAddress local_ip(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);

  WiFi.mode(WIFI_AP);

  WiFi.softAPConfig(local_ip, gateway, subnet);

  WiFi.softAP(ap_ssid.c_str(), ap_pass.c_str());

  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

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

  WiFi.mode(WIFI_AP_STA);
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
    if (isAPMode)
    {
      dnsServer.processNextRequest();
    }
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