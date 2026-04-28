#include "led_blinky.h"

// ===== CONFIG =====
#define T_COLD_MAX 25
#define T_HOT_MIN 32

// clamp helper
int clamp(int val, int minVal, int maxVal)
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
    float T = glob_temperature;

    // =========================
    // ===== COLD REGION =======
    // =========================
    if (T < T_COLD_MAX)
    {
      // Single Blink
      int t_on = 150;

      // t_off = 1500 - 120*(T-15)
      int t_off = 1500 - 120 * (T - 15);

      // tránh âm hoặc quá nhỏ
      t_off = clamp(t_off, 200, 2000);

      // ON
      digitalWrite(LED_GPIO, HIGH);
      vTaskDelay(pdMS_TO_TICKS(t_on));

      // OFF
      digitalWrite(LED_GPIO, LOW);
      vTaskDelay(pdMS_TO_TICKS(t_off));
    }

    // =========================
    // ===== NORMAL REGION =====
    // =========================
    else if (T >= 25 && T <= 32)
    {
      int t_on = 120;
      int t_gap = 120;

      // t_rest = 800 - 50*(T-25)
      int t_rest = 800 - 50 * (T - 25);
      t_rest = clamp(t_rest, 200, 1000);

      // Blink 1
      digitalWrite(LED_GPIO, HIGH);
      vTaskDelay(pdMS_TO_TICKS(t_on));

      digitalWrite(LED_GPIO, LOW);
      vTaskDelay(pdMS_TO_TICKS(t_gap));

      // Blink 2
      digitalWrite(LED_GPIO, HIGH);
      vTaskDelay(pdMS_TO_TICKS(t_on));

      digitalWrite(LED_GPIO, LOW);
      vTaskDelay(pdMS_TO_TICKS(t_rest));
    }

    // =========================
    // ===== HOT REGION ========
    // =========================
    else // T > 32
    {
      // N = 3 + (T-32)/2
      int N = 3 + (T - 32) / 2;
      N = clamp(N, 3, 8);

      // t_blink = 150 - 11*(T-32)
      int t_blink = 150 - 11 * (T - 32);
      t_blink = clamp(t_blink, 40, 150);

      int t_rest = 500;

      for (int i = 0; i < N; i++)
      {
        digitalWrite(LED_GPIO, HIGH);
        vTaskDelay(pdMS_TO_TICKS(t_blink));

        digitalWrite(LED_GPIO, LOW);
        vTaskDelay(pdMS_TO_TICKS(t_blink));
      }

      // nghỉ sau burst
      vTaskDelay(pdMS_TO_TICKS(t_rest));
    }
  }
}