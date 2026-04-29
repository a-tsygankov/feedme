// Minimal LVGL 8.x config for ESP32-S3 + GC9A01 round display.
// Tune sizes/features as the firmware grows.
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

#define LV_COLOR_DEPTH            16
// 0 = LVGL packs RGB565 as standard {red:5, green:6, blue:5}. Our flush
// callback in adapters/LvglDisplay.cpp does the R↔B channel swap that
// the CrowPanel GC9A01 needs, then pushes with TFT_eSPI's swap_bytes
// to get big-endian byte order for the panel.
#define LV_COLOR_16_SWAP          0
// Delegate widget + decode allocations to the Arduino-ESP32 heap so
// PNG decoder buffers (130x130 ARGB8888 = 67 KB each) can land in
// the 8 MB PSRAM. The previous internal 48 KB pool was too small to
// hold even one decoded cat — lodepng failed with error 83 (alloc).
// With BOARD_HAS_PSRAM + qio_opi the ESP-IDF heap routes large
// allocations to PSRAM automatically.
#define LV_MEM_CUSTOM             1
#define LV_MEM_CUSTOM_INCLUDE     <stdlib.h>
#define LV_MEM_CUSTOM_ALLOC       malloc
#define LV_MEM_CUSTOM_FREE        free
#define LV_MEM_CUSTOM_REALLOC     realloc
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
#define LV_FONT_MONTSERRAT_48     1
#define LV_FONT_DEFAULT           &lv_font_montserrat_14

#define LV_USE_ARC                1
#define LV_USE_LABEL              1
#define LV_USE_BTN                1
#define LV_USE_IMG                1
#define LV_USE_LINE               1
#define LV_USE_CANVAS             1
#define LV_USE_ANIMIMG            1

// PNG decoder for filesystem-backed cat silhouettes. Trades a one-time
// ~50-150 ms decode per image (cached after) for ~870 KB of flash savings
// vs the previous compiled CF_TRUE_COLOR_ALPHA C arrays. PSRAM holds the
// decoded pixel buffers; cache size below keeps all 12 cats × 2 sizes
// live after first display so subsequent renders are instant.
#define LV_USE_PNG                1
#define LV_IMG_CACHE_DEF_SIZE     24

// Use the standard libc malloc/free family for image cache + decoded
// buffers. With LV_MEM_CUSTOM=0 LVGL has its own ~48 KB pool for
// widgets but image decode buffers go through malloc — which lands in
// PSRAM via the Arduino-ESP32 heap_caps wiring.
#define LV_MEMCPY_MEMSET_STD      1

#endif
