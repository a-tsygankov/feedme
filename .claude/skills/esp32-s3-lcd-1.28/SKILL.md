---
name: esp32-s3-lcd-1.28
description: >
  Development skill for the Waveshare ESP32-S3-LCD-1.28 board — a compact ESP32-S3 board
  with a 1.28-inch round GC9A01A IPS LCD (240×240), QMI8658 6-axis IMU, Li-ion battery
  management, Wi-Fi/BLE, 16MB Flash, and 2MB PSRAM. Use this skill whenever the user
  mentions ESP32-S3-LCD-1.28, GC9A01 round display on ESP32-S3, Waveshare round LCD dev
  board, or wants to write Arduino/ESP-IDF/MicroPython code for this hardware. Covers
  pin mappings, TFT_eSPI & LVGL configuration, IMU reading, battery ADC, Wi-Fi/BLE, and
  project scaffolding. Always consult this skill before generating any code targeting this
  board — wrong pin assignments are a common failure mode.
---

# Waveshare ESP32-S3-LCD-1.28 Development Skill

## Hardware Summary

| Component | Details |
|-----------|---------|
| MCU | ESP32-S3R2, dual-core Xtensa LX7 @ up to 240 MHz |
| Flash | 16 MB (external) |
| PSRAM | 2 MB |
| Display | 1.28″ round IPS, 240×240, 65K color, driver **GC9A01A** |
| Display interface | 4-wire SPI, up to 80 MHz |
| IMU | **QMI8658** — 3-axis accel (16-bit, ±2/4/8/16 g) + 3-axis gyro (16-bit, ±16…2048 °/s) |
| IMU interface | I2C (hardware) |
| USB-UART | CH343P chip → UART_TXD=GPIO43, UART_RXD=GPIO44 |
| Battery | MX1.25 connector, ETA6096 charger (1 A), 3.7 V Li-ion |
| Programming | Arduino IDE, ESP-IDF (VSCode), MicroPython (Thonny) |
| Wireless | Wi-Fi 802.11 b/g/n 2.4 GHz + BLE 5.0 |

---

## Pin Reference (MEMORIZE THESE)

### LCD — SPI (GC9A01A)

| Signal | GPIO |
|--------|------|
| MOSI (SDA) | **11** |
| CLK (SCK) | **10** |
| CS | **9** |
| DC | **8** |
| RST | **12** |
| BL (backlight, PWM) | **40** |

> ⚠️ **Critical**: backlight is GPIO **40**, NOT GPIO 2. GPIO 2 appears in some Touch variant docs but is incorrect for the non-touch LCD-1.28.

### IMU — I2C (QMI8658)

| Signal | GPIO |
|--------|------|
| SDA | **6** |
| SCL | **7** |
| INT1 | **47** |
| INT2 | **48** |

### Other

| Function | GPIO |
|----------|------|
| Battery ADC | **1** (voltage divider 200 kΩ + 100 kΩ → formula below) |
| UART TX | 43 |
| UART RX | 44 |
| BOOT button | 0 |
| Touch INT (Touch variant only) | 5 |
| MOS switch 1 | 4 |
| MOS switch 2 | 5 |

**Battery voltage formula:**
```cpp
float voltage = 3.3f / (1 << 12) * 3.0f * analogRead(1);
```

---

## Arduino IDE Setup

### Board Configuration (Tools menu)

```
Board:       ESP32S3 Dev Module
Flash Size:  16MB (128Mb)
PSRAM:       QSPI PSRAM
Upload Mode: UART0 / Hardware CDC
USB Mode:    Hardware CDC and JTAG
```

> Add ESP32 boards URL: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`

### Required Libraries

Install by copying the Waveshare-provided archives to `<Arduino>/libraries/`:
- **TFT_eSPI** (pre-configured version from Waveshare demo zip)
- **lvgl** (pre-configured version from Waveshare demo zip)

Download links:
- Demo + lib zip: `https://files.waveshare.com/wiki/ESP32-S3-LCD-1.28/ESP32-S3-LCD-1.28-code.zip`
- TFT_eSPI + LVGL lib: `https://files.waveshare.com/wiki/ESP32-S3-LCD-1.28/Esp32-s3-lcd-1.28-lib.zip`

### TFT_eSPI User_Setup.h (minimal correct config)

```cpp
#define GC9A01_DRIVER
#define TFT_WIDTH  240
#define TFT_HEIGHT 240

#define TFT_MOSI  11
#define TFT_SCLK  10
#define TFT_CS     9
#define TFT_DC     8
#define TFT_RST   12
#define TFT_BL    40

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

#define SPI_FREQUENCY       80000000
#define SPI_READ_FREQUENCY  20000000

// Required to prevent ESP32-S3 crash with TFT_eSPI
#define USE_HSPI_PORT
```

> ⚠️ **`#define USE_HSPI_PORT` is mandatory** — without it, TFT_eSPI crashes on ESP32-S3 due to SPI bus selection issues.

---

## Minimal Arduino Sketch Template

```cpp
#include <TFT_eSPI.h>
#include <Wire.h>

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);

  // Backlight ON
  pinMode(40, OUTPUT);
  digitalWrite(40, HIGH);

  // LCD init
  tft.init();
  tft.setRotation(0);        // 0–3; round display, usually 0
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("Hello!", 80, 110);

  // I2C for IMU
  Wire.setPins(6, 7);        // SDA=6, SCL=7 — MUST call before Wire.begin()
  Wire.begin();
}

void loop() {
  // Battery voltage
  float vbat = 3.3f / 4096.0f * 3.0f * analogRead(1);
  Serial.printf("Battery: %.2f V\n", vbat);
  delay(1000);
}
```

---

## QMI8658 IMU Usage

QMI8658 I2C address: **0x6B** (default), can be 0x6A.

### Basic register-level read (no external library)

```cpp
#include <Wire.h>

#define QMI8658_ADDR  0x6B
#define REG_ACC_X_L   0x35

void qmi_write(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(QMI8658_ADDR);
  Wire.write(reg); Wire.write(val);
  Wire.endTransmission();
}

uint8_t qmi_read(uint8_t reg) {
  Wire.beginTransmission(QMI8658_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(QMI8658_ADDR, 1);
  return Wire.read();
}

void readIMU(int16_t &ax, int16_t &ay, int16_t &az,
             int16_t &gx, int16_t &gy, int16_t &gz) {
  Wire.beginTransmission(QMI8658_ADDR);
  Wire.write(REG_ACC_X_L);
  Wire.endTransmission(false);
  Wire.requestFrom(QMI8658_ADDR, 12);
  uint8_t buf[12];
  for (int i = 0; i < 12; i++) buf[i] = Wire.read();
  ax = (int16_t)(buf[1]<<8|buf[0]);
  ay = (int16_t)(buf[3]<<8|buf[2]);
  az = (int16_t)(buf[5]<<8|buf[4]);
  gx = (int16_t)(buf[7]<<8|buf[6]);
  gy = (int16_t)(buf[9]<<8|buf[8]);
  gz = (int16_t)(buf[11]<<8|buf[10]);
}

// In setup():
void imuInit() {
  Wire.setPins(6, 7);
  Wire.begin();
  qmi_write(0x02, 0x60);  // CTRL1: enable accel+gyro, 896 Hz ODR
  qmi_write(0x08, 0x03);  // CTRL7: enable accel+gyro output
}
```

### Using the Waveshare QMI8658 library (from demo zip)

```cpp
#include "QMI8658.h"   // from demo zip

float acc[3], gyro[3];
QMI8658_init();        // handles Wire.setPins internally
QMI8658_read_acc_xyz(acc);
QMI8658_read_gyro_xyz(gyro);
```

---

## LVGL Integration (Arduino)

```cpp
#include <TFT_eSPI.h>
#include <lvgl.h>

#define LVGL_TICK_PERIOD_MS 2
#define BUF_SIZE (240 * 10)

TFT_eSPI tft;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[BUF_SIZE];

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t*)&color_p->full, w * h, true);
  tft.endWrite();
  lv_disp_flush_ready(disp);
}

hw_timer_t *lvgl_timer = NULL;
void IRAM_ATTR lvgl_tick_cb() { lv_tick_inc(LVGL_TICK_PERIOD_MS); }

void setup() {
  pinMode(40, OUTPUT); digitalWrite(40, HIGH);
  tft.init();
  tft.setRotation(0);

  lv_init();
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, BUF_SIZE);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = 240;
  disp_drv.ver_res = 240;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  lvgl_timer = timerBegin(0, 80, true);
  timerAttachInterrupt(lvgl_timer, &lvgl_tick_cb, true);
  timerAlarmWrite(lvgl_timer, LVGL_TICK_PERIOD_MS * 1000, true);
  timerAlarmEnable(lvgl_timer);

  // Create a simple label
  lv_obj_t *label = lv_label_create(lv_scr_act());
  lv_label_set_text(label, "Hello LVGL!");
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}

void loop() {
  lv_timer_handler();
  delay(5);
}
```

---

## Wi-Fi Example Snippet

```cpp
#include <WiFi.h>

void wifiConnect(const char* ssid, const char* pass) {
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nConnected: " + WiFi.localIP().toString());
}
```

---

## MicroPython Notes

The board ships with **custom MicroPython firmware** that includes a GC9A01 driver. Do NOT flash standard ESP32-S3 MicroPython — it won't have the display driver.

**Firmware source**: built from https://github.com/russhughes/gc9a01_mpy  
**Flash tool**: `flash_download_tool_3.9.5`, address **0x0**, chip: ESP32-S3, mode: USB

**Enter download mode**: hold BOOT → tap RESET → release BOOT

```python
# MicroPython display example (gc9a01_mpy firmware)
import gc9a01
from machine import Pin, SPI

spi = SPI(1, baudrate=40_000_000, sck=Pin(10), mosi=Pin(11))
tft = gc9a01.GC9A01(spi, dc=Pin(8), cs=Pin(9), reset=Pin(12), backlight=Pin(40))
tft.fill(gc9a01.WHITE)
tft.text("Hello!", 80, 110, gc9a01.BLACK)
```

---

## ESP-IDF Notes

- Use **ESP-IDF v5.x** (shipped with Arduino-esp32 3.x).
- Component path for LCD: use `esp_lcd` component with `GC9A01` driver or Waveshare's provided ESP-IDF demo.
- Partition table: use `16MB` custom partition; application firmware at `0x10000`, partition table at `0x8000`, bootloader at `0x0`.

---

## Common Gotchas

| Problem | Fix |
|---------|-----|
| TFT_eSPI crashes / blank screen | Add `#define USE_HSPI_PORT` to User_Setup.h |
| Wrong backlight pin | Use GPIO **40**, not 2 or 21 |
| I2C not working | Call `Wire.setPins(6, 7)` **before** `Wire.begin()` |
| White screen / no display | Verify CS=9, DC=8, RST=12, MOSI=11, SCK=10 |
| Flash size mismatch | Select **16MB Flash** + **QSPI PSRAM** in Arduino board config |
| MicroPython blank screen | Must use custom gc9a01_mpy firmware, not generic ESP32-S3 build |
| Battery ADC wrong value | Use `analogRead(1)` with formula `3.3/4096*3*raw` |

---

## Resources

| Resource | URL |
|----------|-----|
| Waveshare Wiki | https://www.waveshare.com/wiki/ESP32-S3-LCD-1.28 |
| Schematic | https://files.waveshare.com/wiki/ESP32-S3-LCD-1.28/Esp32-s3-lcd-.128-sch.pdf |
| Demo + MicroPython code | https://files.waveshare.com/wiki/ESP32-S3-LCD-1.28/ESP32-S3-LCD-1.28-code.zip |
| TFT_eSPI + LVGL libs | https://files.waveshare.com/wiki/ESP32-S3-LCD-1.28/Esp32-s3-lcd-1.28-lib.zip |
| GC9A01A datasheet | https://files.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.28/GC9A01A.pdf |
| QMI8658A datasheet | https://files.waveshare.com/upload/5/5f/QMI8658A_Datasheet_Rev_A.pdf |
| gc9a01_mpy firmware | https://github.com/russhughes/gc9a01_mpy |
| Arduino-esp32 | https://github.com/espressif/arduino-esp32 |
