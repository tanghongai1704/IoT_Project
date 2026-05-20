#ifndef LCD_MONITOR_H
#define LCD_MONITOR_H

#include "global.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

void lcd_monitor_task(void *pvParameters);

#endif // LCD_MONITOR_H