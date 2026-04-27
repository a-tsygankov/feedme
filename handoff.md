# Cat Feeder Tracker — Session Handoff

> Generated from conversation on 2026-04-26. Pick up from here.

---

## What This Is

A fridge-mounted IoT device using the **Waveshare ESP32-S3-LCD-1.28** (round 1.28″ 240×240 display, Wi-Fi, BLE, IMU) that tracks when a cat was last fed. Two people share state via Cloudflare Workers + D1. Glanceable ring + cat face = no interaction needed to check status.

---

## Hardware

### Board
**Waveshare ESP32-S3-LCD-1.28** (bare) or **ESP32-S3-LCD-1.28-B** (with CNC metal case — recommended)
- Amazon: search `waveshare ESP32-S3-LCD-1.28-B`
- ESP32-S3 chip rev v0.2, **16 MB QIO flash** (GD25Q128), **8 MB OPI PSRAM** (AP Memory APS6408, "AP_3v3" in esptool output)
- Wi-Fi + BLE 5, USB-Serial-JTAG on-chip

> ⚠️ Earlier handoff drafts said "ESP32-S3R2, 2 MB PSRAM" — that turned
> out to be wrong. esptool's `chip_id` reports `Embedded PSRAM 8MB
> (AP_3v3)` and the bootloader's QSPI probe fails (`PSRAM ID read
> error: 0x00ffffff, … wrong PSRAM line mode`). The chip actually has
> **OPI** PSRAM, so the PlatformIO `memory_type` for the real-board env
> must be `qio_opi` (selects `bootloader_opi_80m.elf`, drives PSRAM in
> octal mode). The simulator env overrides this to `qio_qspi` because
> Wokwi's emulator can't service OPI flash bus mode.
>
> Symptom of a wrong setting: `bootloader_reset()` loop with `Saved PC`
> pointing into bootloader IRAM, no `[feedme] boot` line ever printed.

### Key Pin Assignments (memorize these)
| Function | GPIO |
|----------|------|
| LCD MOSI | 11 |
| LCD CLK  | 10 |
| LCD CS   | 9  |
| LCD DC   | 8  |
| LCD RST  | 12 |
| Backlight (BL) | **40** ← not 2 |
| IMU SDA  | 6  |
| IMU SCL  | 7  |
| Battery ADC | 1 |

Battery voltage formula: `3.3 / 4096.0 * 3.0 * analogRead(1)`

### Shopping List (to buy)
| Item | Search term | Price |
|------|-------------|-------|
| Battery (good) | `Qimoo 802525 JST 1.25 400mAh` | ~$7 |
| Battery (better) | `Qimoo 603450 JST 1.25 1200mAh` | ~$11 |
| Magnets | `TRYMAG 20x3mm adhesive neodymium` | ~$9 |
| Case (if bare board) | Thingiverse thing:7038776 (free STL) | free |

**⚠️ Connector warning:** board uses MX1.25 2-pin. Always verify polarity before plugging — no reverse-polarity protection on-board.

**Battery life estimate:**
- 400 mAh + Modem Sleep → ~7–10h between charges
- 1200 mAh + Modem Sleep → ~20–30h between charges

---

## App Design

### Core Concept
The outer color ring depletes over 5 hours (configurable). Cat face expression changes with urgency. At a glance: ring color + cat mood + dot count = full picture.

### Screens / States
1. **Happy (green)** — fed < 2h ago, calm cat, full ring
2. **Neutral (yellow)** — 2–3h, half-lid cat, partial ring
3. **Warning (orange)** — 3–4h, neutral cat, ring nearly empty
4. **Hungry (red, pulsing)** — 4h+, desperate cat with raised paw + open yowl, "FEED ME!" pulse

### Interactions
| Gesture | Action |
|---------|--------|
| Single tap | Log feeding (timestamp + owner name) |
| Long press 2s | Snooze 30 min ("just begging") |
| Double tap | View last 5 feeding events |
| Hold BOOT button | First-time setup / settings |

### Meal Dots
Three dots at bottom = today's meals. Filled = fed, empty = pending. 3 meals/day target.

---

## Tech Stack

| Layer | Choice |
|-------|--------|
| Firmware | Arduino + TFT_eSPI + LVGL |
| Cat character | Custom SVG LVGL drawing (no bitmaps) |
| Wi-Fi sync | HTTPS poll every 30s to Cloudflare Worker |
| Backend | Cloudflare Workers + D1 (already have this infra from FC26 app) |
| Real-time | Cloudflare Durable Objects (optional v2) |
| Offline buffer | LittleFS on ESP32 (last 24h events) |
| Time | NTP on boot |

### TFT_eSPI User_Setup.h critical settings
```cpp
#define GC9A01_DRIVER
#define TFT_MOSI 11
#define TFT_SCLK 10
#define TFT_CS    9
#define TFT_DC    8
#define TFT_RST  12
#define TFT_BL   40
#define USE_HSPI_PORT          // ← MANDATORY, crashes without it on ESP32-S3
#define SPI_FREQUENCY 80000000
```

### I2C (IMU)
```cpp
Wire.setPins(6, 7);   // MUST call before Wire.begin()
Wire.begin();
// QMI8658 at I2C address 0x6B
```

---

## Backend API (Cloudflare Workers + D1)

### D1 Schema
```sql
CREATE TABLE events (
  id    INTEGER PRIMARY KEY AUTOINCREMENT,
  hid   TEXT NOT NULL,          -- household ID (e.g. "home-4a7f")
  ts    INTEGER NOT NULL,       -- unix timestamp
  type  TEXT NOT NULL,          -- 'feed' | 'snooze'
  by    TEXT NOT NULL,          -- owner name ("Masha" / "Andrey")
  note  TEXT
);
CREATE INDEX idx_hid_ts ON events(hid, ts DESC);
```

### Endpoints needed
```
GET  /api/state?hid=xxx          → last event + time since + today's count
POST /api/feed   {hid, by, type} → log event, return new state
GET  /api/history?hid=xxx&n=5    → last N events
```

### ESP32 polling pattern
```cpp
// Every 30s in loop:
HTTPClient http;
http.begin("https://your-worker.workers.dev/api/state?hid=home-4a7f");
int code = http.GET();
// parse JSON, update display state
```

---

## Cat Character (SVG / LVGL)

Drawn in Simon's Cat style — big round head, chunky outlines, pink inner ears, expressive oval eyes with shine spots, horizontal whiskers, V-nose.

Five moods implemented as SVG primitives (viewBox 0 0 120 118):

| Mood | Trigger | Eyes | Mouth |
|------|---------|------|-------|
| `happy` | < 2h | Round full + shine | Wide upward curve |
| `neutral` | 2–4h | Half-lidded (top mask) | Flat line |
| `hungry` | > 4h | HUGE + tears + angled brows | Wide open yowl + tongue |
| `fed` | Just tapped | ^_^ squint arcs | Big filled grin + heart |
| `sleepy` | Snooze active | Droopy heavy-lid | Small curve + Zzz |

**Hungry cat key detail:** left arm raised high (path sweeping wide left of head), paw with single raised toe-finger pointing upward, three side toe beans visible. The Simon's Cat signature gesture. Head shifted right (cx=68) to leave arm clear.

The full React/SVG implementation is in `cat-feeder-mockups.jsx`.

---

## Mockup File

`cat-feeder-mockups.jsx` — interactive React component showing all 6 workflows:
1. At-a-glance status (4 states, live animation)
2. Logging a meal (tap → confirm → idle)
3. Just begging (long press → snooze)
4. Two-person sync (Cloudflare ⇄ two devices)
5. Feeding history (double-tap)
6. First-time setup (name → threshold → Wi-Fi → ready)

Run it as a Claude artifact or drop into a Vite React project.

---

## SKILL.md

`esp32-s3-lcd-1.28.skill` — Claude Code skill covering:
- Full pin reference
- TFT_eSPI User_Setup.h with all gotchas
- QMI8658 IMU usage (register-level + Waveshare library)
- LVGL integration boilerplate
- MicroPython notes
- Common failure modes table
- Links to all datasheets and demo zips

Install in Claude Code skills folder before starting firmware work.

---

## Build Phases

### Phase 1 — MVP (single device, offline)
- [ ] Arduino setup with TFT_eSPI + LVGL
- [ ] Cat face SVG rendered via LVGL custom draw
- [ ] Arc ring widget (LVGL arc or custom)
- [ ] Tap detection (capacitive touch or IMU tap interrupt)
- [ ] LittleFS for local event storage
- [ ] Battery voltage display

### Phase 2 — Two-person sync
- [ ] Cloudflare Worker + D1 deployed
- [ ] ESP32 Wi-Fi connect + NTP
- [ ] 30s poll loop for state
- [ ] POST on tap/snooze
- [ ] First-time setup flow (owner name, household ID)
- [ ] OTA firmware update support

### Phase 3 — Polish
- [ ] LVGL animations (ring color transition, cat face crossfade)
- [ ] Phone push notification when cat overdue (Cloudflare + ntfy.sh or Pushover)
- [ ] Deep sleep between polls (IMU tap wakes display)
- [ ] Second device confirmed working

---

## Open Questions / Decisions

- **Touch vs tap**: bare ESP32-S3-LCD-1.28 has no touch — interaction is physical tap detected via IMU (QMI8658 tap interrupt on GPIO47/48) or BOOT button. Touch variant (`-Touch-LCD-1.28`) has CST816S chip on I2C. Decide which board variant to use.
- **Household pairing**: simplest = shared hardcoded `hid` string baked into firmware. Fancier = QR code setup on first boot.
- **Display always-on vs sleep**: always-on drains battery; IMU wake-on-tap is the middle ground (screen off after 30s, tap to wake, still polls in background).
- **Notification**: ntfy.sh is free and works well for push to Android/iOS. One Cloudflare Worker cron job checks if last feed > threshold and sends notification.
