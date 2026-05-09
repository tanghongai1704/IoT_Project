#include "task_check_info.h"
#include "mainserver.h"

void Load_info_File()
{
  File file = LittleFS.open("/info.dat", "r");
  if (!file)
  {
    return;
  }

  DynamicJsonDocument doc(4096);
  DeserializationError error = deserializeJson(doc, file);
  if (error)
  {
    Serial.print(F("deserializeJson() failed: "));
  }
  else
  {
    if (takeSystemContext(portMAX_DELAY))
    {
      systemContext.wifi_ssid = doc["WIFI_SSID"] | "";
      systemContext.wifi_pass = doc["WIFI_PASS"] | "";
      systemContext.core_iot_token = doc["CORE_IOT_TOKEN"] | "";
      systemContext.core_iot_server = doc["CORE_IOT_SERVER"] | "";
      systemContext.core_iot_port = doc["CORE_IOT_PORT"] | "";
      systemContext.mqtt_target = doc["MQTT_TARGET"] | "coreiot";
      systemContext.ap_ssid = doc["AP_SSID"] | String(SSID_AP);
      systemContext.ap_pass = doc["AP_PASS"] | String(PASS_AP);
      systemContext.read_interval = doc["READ_INTERVAL"] | 5000;
      giveSystemContext();
    }

    Serial.println("✅ Info loaded from file:");
    Serial.println(systemContext.wifi_ssid);
    Serial.println(systemContext.wifi_pass);
    Serial.println(systemContext.core_iot_token);
    Serial.println(systemContext.core_iot_server);
    Serial.println(systemContext.core_iot_port);
    Serial.println(systemContext.mqtt_target);
    Serial.println(systemContext.ap_ssid);
    Serial.println(systemContext.ap_pass);
    Serial.println(systemContext.read_interval);
  }
  file.close();
}

void Delete_info_File()
{
  if (LittleFS.exists("/info.dat"))
  {
    LittleFS.remove("/info.dat");
  }
  ESP.restart();
}

void Save_info_File(String wifi_ssid, String wifi_pass, String CORE_IOT_TOKEN, String CORE_IOT_SERVER, String CORE_IOT_PORT, String MQTT_TARGET, String ap_ssid, String ap_pass, int read_interval)
{
  DynamicJsonDocument doc(4096);
  doc["WIFI_SSID"] = wifi_ssid;
  doc["WIFI_PASS"] = wifi_pass;
  doc["CORE_IOT_TOKEN"] = CORE_IOT_TOKEN;
  doc["CORE_IOT_SERVER"] = CORE_IOT_SERVER;
  doc["CORE_IOT_PORT"] = CORE_IOT_PORT;
  doc["MQTT_TARGET"] = MQTT_TARGET;
  doc["AP_SSID"] = ap_ssid;
  doc["AP_PASS"] = ap_pass;
  doc["READ_INTERVAL"] = read_interval;

  File configFile = LittleFS.open("/info.dat", "w");
  if (configFile)
  {
    serializeJson(doc, configFile);
    configFile.close();
  }
  else
  {
    Serial.println("Unable to save the configuration.");
  }
}

bool check_info_File(bool check)
{
  if (!check)
  {
    if (!LittleFS.begin(true))
    {
      Serial.println("❌ Lỗi khởi động LittleFS!");
      return false;
    }
    Load_info_File();
  }

  String wifi_ssid;
  String wifi_pass;
  if (takeSystemContext(portMAX_DELAY))
  {
    wifi_ssid = systemContext.wifi_ssid;
    wifi_pass = systemContext.wifi_pass;
    giveSystemContext();
  }

  if (wifi_ssid.isEmpty() && wifi_pass.isEmpty())
  {
    if (!check)
    {
      startAP();
    }
    return false;
  }
  return true;
}