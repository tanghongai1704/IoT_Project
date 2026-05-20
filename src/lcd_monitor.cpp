#include "lcd_monitor.h"
#include "global.h"

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

static LiquidCrystal_I2C lcd(33, 16, 2);

String showAlert(int label)
{
    switch (label)
    {
    case 0:
        return "SAFE";

    case 1:
        return "CAUTION";

    case 2:
        return "X-CAUTION";

    case 3:
        return "DANGER";

    case 4:
        return "X-DANGER";

    default:
        return "UNKNOWN";
    }
}

void lcd_monitor_task(void *pvParameters)
{
    Wire.begin(11, 12);

    lcd.begin();
    lcd.backlight();
    lcd.clear();

    bool blinkState = true;

    while (1)
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

        String alert = showAlert(alert_status);

        // =========================
        // ROW 0 (always visible)
        // =========================
        lcd.setCursor(0, 0);

        String line1 =
            "T:" + String(temperature, 1) +
            "C H:" + String(humidity, 1) + "%";

        while (line1.length() < 16)
            line1 += " ";

        lcd.print(line1);

        // =========================
        // ROW 1 (blinking alert)
        // =========================
        lcd.setCursor(0, 1);

        String line2;

        if (blinkState)
        {
            line2 = "ALERT:" + alert;
        }
        else
        {
            line2 = "";
        }

        while (line2.length() < 16)
            line2 += " ";

        lcd.print(line2);

        blinkState = !blinkState;

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}