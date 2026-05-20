#include "led_blinky.h"

#define T_COLD_MAX 28
#define T_HOT_MIN 32

bool last_led_state = false;

static int clamp(int val, int minVal, int maxVal)
{
  if (val < minVal)
    return minVal;
  if (val > maxVal)
    return maxVal;
  return val;
}

void led_blinky(void *pvParameters)
{
  pinMode(LED_GPIO, OUTPUT);

  while (1)
  {
    String mode;
    bool led_state = false;
    float temperature = 0;

    if (takeSystemContext(portMAX_DELAY))
    {
      mode = systemContext.device_mode;
      led_state = systemContext.led_state;
      temperature = systemContext.temperature;
      giveSystemContext();
    }

    if (mode == "MANUAL")
    {
      if (led_state != last_led_state)
      {
        digitalWrite(LED_GPIO, led_state ? HIGH : LOW);
        last_led_state = led_state;
      }
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    float T = temperature;

    if (T < T_COLD_MAX)
    {
      int t_on = 150;
      int t_off = 1500 - 120 * (T - 15);
      t_off = clamp(t_off, 200, 2000);

      digitalWrite(LED_GPIO, HIGH);
      vTaskDelay(pdMS_TO_TICKS(t_on));
      digitalWrite(LED_GPIO, LOW);
      vTaskDelay(pdMS_TO_TICKS(t_off));
    }
    else if (T <= T_HOT_MIN)
    {
      int t_on = 120;
      int t_gap = 120;
      int t_rest = 800 - 50 * (T - 25);
      t_rest = clamp(t_rest, 200, 1000);

      digitalWrite(LED_GPIO, HIGH);
      vTaskDelay(pdMS_TO_TICKS(t_on));
      digitalWrite(LED_GPIO, LOW);
      vTaskDelay(pdMS_TO_TICKS(t_gap));
      digitalWrite(LED_GPIO, HIGH);
      vTaskDelay(pdMS_TO_TICKS(t_on));
      digitalWrite(LED_GPIO, LOW);
      vTaskDelay(pdMS_TO_TICKS(t_rest));
    }
    else
    {
      int N = 3 + (T - 32) / 2;
      N = clamp(N, 3, 8);

      int t_blink = 150 - 11 * (T - 32);
      t_blink = clamp(t_blink, 40, 150);

      for (int i = 0; i < N; i++)
      {
        digitalWrite(LED_GPIO, HIGH);
        vTaskDelay(pdMS_TO_TICKS(t_blink));
        digitalWrite(LED_GPIO, LOW);
        vTaskDelay(pdMS_TO_TICKS(t_blink));
      }

      vTaskDelay(pdMS_TO_TICKS(500));
    }
  }
}