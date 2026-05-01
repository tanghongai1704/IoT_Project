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
    WIFI_SSID = doc["WIFI_SSID"] | "";
    WIFI_PASS = doc["WIFI_PASS"] | "";
    CORE_IOT_TOKEN = doc["CORE_IOT_TOKEN"] | "";
    CORE_IOT_SERVER = doc["CORE_IOT_SERVER"] | "";
    CORE_IOT_PORT = doc["CORE_IOT_PORT"] | "";
    AP_SSID = doc["AP_SSID"] | String(SSID_AP);
    AP_PASS = doc["AP_PASS"] | String(PASS_AP);
    READ_INTERVAL = doc["READ_INTERVAL"] | 5000;

    Serial.println("✅ Info loaded from file:");
    Serial.println(WIFI_SSID);
    Serial.println(WIFI_PASS);
    Serial.println(CORE_IOT_TOKEN);
    Serial.println(CORE_IOT_SERVER);
    Serial.println(CORE_IOT_PORT);
    Serial.println(AP_SSID);
    Serial.println(AP_PASS);
    Serial.println(READ_INTERVAL);
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

void Save_info_File(String wifi_ssid, String wifi_pass, String CORE_IOT_TOKEN, String CORE_IOT_SERVER, String CORE_IOT_PORT, String ap_ssid, String ap_pass, int read_interval)
{
  DynamicJsonDocument doc(4096);
  doc["WIFI_SSID"] = wifi_ssid;
  doc["WIFI_PASS"] = wifi_pass;
  doc["CORE_IOT_TOKEN"] = CORE_IOT_TOKEN;
  doc["CORE_IOT_SERVER"] = CORE_IOT_SERVER;
  doc["CORE_IOT_PORT"] = CORE_IOT_PORT;
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
  // ESP.restart();
};

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

  if (WIFI_SSID.isEmpty() && WIFI_PASS.isEmpty())
  {
    if (!check)
    {
      startAP();
    }
    return false;
  }
  return true;
}