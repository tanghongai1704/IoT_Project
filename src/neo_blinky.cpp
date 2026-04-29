#include "neo_blinky.h"
#include "global.h"

// clamp
float clampf(float v, float a, float b)
{
    if (v < a)
        return a;
    if (v > b)
        return b;
    return v;
}

uint32_t getColor(Adafruit_NeoPixel &strip, int r, int g, int b, float brightness)
{
    r = (int)(r * brightness);
    g = (int)(g * brightness);
    b = (int)(b * brightness);
    return strip.Color(r, g, b);
}

void neo_blinky(void *pvParameters)
{
    Adafruit_NeoPixel strip(LED_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);

    strip.begin();
    strip.clear();
    strip.show();

    while (1)
    {
        // =========================
        // ===== MANUAL MODE =======
        // =========================
        if (device_mode == "MANUAL")
        {
            float B = clampf(neo_brightness / 255.0f, 0.0, 1.0);

            uint32_t color = getColor(strip, neo_r, neo_g, neo_b, B);

            strip.setPixelColor(0, color);
            strip.show();

            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // =========================
        // ===== AUTO MODE =========
        // =========================
        float H = glob_humidity;

        int r = 0, g = 0, b = 0;
        float T = 1000;
        float B = 0.5;

        if (H < 40)
        {
            r = 255;
            g = 200;
            b = 0;
            T = 1500 - 20 * (40 - H);
            B = 0.4 + 0.01 * H;
        }
        else if (H <= 70)
        {
            r = 0;
            g = 255;
            b = 0;
            T = 1000 - 20 * abs(H - 60);
            B = 0.6 + 0.005 * abs(H - 60);
        }
        else
        {
            r = 0;
            g = 100;
            b = 255;
            T = 700 - 8 * (H - 70);
            B = 0.7 + 0.003 * (H - 70);
        }

        T = clampf(T, 20, 2000);
        B = clampf(B, 0.1, 1.0);

        uint32_t color = getColor(strip, r, g, b, B);

        int half = T / 2;

        strip.setPixelColor(0, color);
        strip.show();
        vTaskDelay(pdMS_TO_TICKS(half));

        strip.setPixelColor(0, 0);
        strip.show();
        vTaskDelay(pdMS_TO_TICKS(half));
    }
}