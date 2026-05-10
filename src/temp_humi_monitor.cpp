#include "temp_humi_monitor.h"

static DHT20 dht20;

float calcHumidex(float T_air, float HR)
{
    float exponent = (7.5 * T_air) / (237.7 + T_air);
    float e = 6.112 * std::pow(10, exponent) * (HR / 100.0);
    return T_air + (5.0 / 9.0) * (e - 10.0);
}

void temp_humi_monitor(void *pvParameters)
{
    Wire.begin(11, 12);
    Serial.begin(115200);
    dht20.begin();

    while (1)
    {
        dht20.read();
        float temperature = dht20.getTemperature();
        float humidity = dht20.getHumidity();

        if (isnan(temperature) || isnan(humidity))
        {
            Serial.println("Failed to read from DHT sensor!");
            temperature = humidity = -1;
        }

        if (takeSystemContext(portMAX_DELAY))
        {
            systemContext.temperature = temperature;
            systemContext.humidity = humidity;
            giveSystemContext();
        }

        // Serial.print("Humidity: ");
        // Serial.print(humidity);
        // Serial.print("%  Temperature: ");
        // Serial.print(temperature);
        // Serial.println("°C");
        // Serial.print("Humidex: ");
        // Serial.println(humidex);

        int interval = 5000;
        if (takeSystemContext(pdMS_TO_TICKS(100)))
        {
            interval = systemContext.read_interval;
            giveSystemContext();
        }

        vTaskDelay(pdMS_TO_TICKS(interval));
    }
}