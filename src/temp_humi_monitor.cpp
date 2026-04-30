#include "temp_humi_monitor.h"
DHT20 dht20;
LiquidCrystal_I2C lcd(33, 16, 2);

float calcHumidex(float T_air, float HR)
{
    // 1. Tính áp suất hơi nước (e)
    // Công thức: e = 6.112 * 10^((7.5 * T_air) / (237.7 + T_air)) * (HR / 100)
    float exponent = (7.5 * T_air) / (237.7 + T_air);
    float e = 6.112 * std::pow(10, exponent) * (HR / 100.0);

    // 2. Tính chỉ số Humidex
    // Công thức: Humidex = T_air + 5/9 * (e - 10)
    float humidex = T_air + (5.0 / 9.0) * (e - 10.0);

    return humidex;
}

void temp_humi_monitor(void *pvParameters)
{

    Wire.begin(11, 12);
    Serial.begin(115200);
    dht20.begin();

    while (1)
    {
        /* code */

        dht20.read();
        // Reading temperature in Celsius
        float temperature = dht20.getTemperature();
        // Reading humidity
        float humidity = dht20.getHumidity();

        // Check if any reads failed and exit early
        if (isnan(temperature) || isnan(humidity))
        {
            Serial.println("Failed to read from DHT sensor!");
            temperature = humidity = -1;
            // return;
        }

        // Update global variables for temperature and humidity
        glob_temperature = temperature;
        glob_humidity = humidity;
        glob_humidex = calcHumidex(temperature, humidity);

        // Print the results

        Serial.print("Humidity: ");
        Serial.print(humidity);
        Serial.print("%  Temperature: ");
        Serial.print(temperature);
        Serial.println("°C");
        Serial.print("Humidex: ");
        Serial.println(glob_humidex);

        vTaskDelay(5000);
    }
}