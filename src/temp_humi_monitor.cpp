#include "temp_humi_monitor.h"

static DHT20 dht20;

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

            // Notify downstream tasks that fresh sensor data is available.
            xSemaphoreGive(systemContext.sensor_update_semaphore);
        }

        // Uncomment the block below to trace raw sensor readings locally.

        int interval = 5000;
        if (takeSystemContext(pdMS_TO_TICKS(100)))
        {
            interval = systemContext.read_interval;
            giveSystemContext();
        }

        vTaskDelay(pdMS_TO_TICKS(interval));
    }
}