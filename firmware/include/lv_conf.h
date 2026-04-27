// Minimal LVGL 8.x config for ESP32-S3 + GC9A01 round display.
// Tune sizes/features as the firmware grows.
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

#define LV_COLOR_DEPTH            16
#define LV_COLOR_16_SWAP          1
#define LV_MEM_CUSTOM             0
#define LV_MEM_SIZE               (48U * 1024U)
#define LV_TICK_CUSTOM            1
#define LV_TICK_CUSTOM_INCLUDE    "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

#define LV_USE_PERF_MONITOR       0
#define LV_USE_LOG                0
#define LV_USE_ASSERT_NULL        1
#define LV_USE_ASSERT_MALLOC      1

#define LV_FONT_MONTSERRAT_14     1
#define LV_FONT_MONTSERRAT_18     1
#define LV_FONT_MONTSERRAT_24     1
#define LV_FONT_DEFAULT           &lv_font_montserrat_14

#define LV_USE_ARC                1
#define LV_USE_LABEL              1
#define LV_USE_BTN                1
#define LV_USE_IMG                1
#define LV_USE_LINE               1
#define LV_USE_CANVAS             1
#define LV_USE_ANIMIMG            1

#endif
