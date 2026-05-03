#include "global.h"

#include "led_blinky.h"
#include "neo_blinky.h"
#include "temp_humi_monitor.h"
#include "mainserver.h"
#include "tinyml.h"
#include "coreiot.h"

// include task
#include "task_check_info.h"
#include "task_toogle_boot.h"

void setup()
{
  Serial.begin(115200);
  initSystemContext();
  check_info_File(0);

  xTaskCreate(led_blinky, "Task LED Blink", 2048, NULL, 2, NULL);
  xTaskCreate(neo_blinky, "Task NEO Blink", 2048, NULL, 2, NULL);
  xTaskCreate(temp_humi_monitor, "Task TEMP HUMI Monitor", 2048, NULL, 2, NULL);
  xTaskCreate(main_server_task, "WebServer Task", 8192, NULL, 2, NULL);
  xTaskCreate(tiny_ml_task, "Tiny ML Task", 12288, NULL, 2, NULL);
  xTaskCreate(coreiot_task, "CoreIOT Task", 4096, NULL, 2, NULL);
  xTaskCreate(Task_Toogle_BOOT, "Task_Toogle_BOOT", 4096, NULL, 2, NULL);
}

void loop()
{
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}