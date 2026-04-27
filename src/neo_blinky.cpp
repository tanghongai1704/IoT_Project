#include "neo_blinky.h"

void neo_blinky(void *pvParameters)
{

    Adafruit_NeoPixel strip(LED_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);
    strip.begin();
    // Set all pixels to off to start
    strip.clear();
    strip.show();

    while (1)
    {
        strip.setPixelColor(0, strip.Color(0, 0, 255)); // Set pixel 0 to blue
        strip.show();                                   // Update the strip

        // Wait for 500 milliseconds
        vTaskDelay(500);

        // Set the pixel to off
        strip.setPixelColor(0, strip.Color(0, 0, 0)); // Turn pixel 0 off
        strip.show();                                 // Update the strip

        // Wait for another 500 milliseconds
        vTaskDelay(500);
    }
}