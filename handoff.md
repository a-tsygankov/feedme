# Cat Feeder Tracker — Session Handoff

> Generated from conversation on 2026-04-26. Pick up from here.

---

## What This Is

A fridge-mounted IoT device using the **Waveshare ESP32-S3-LCD-1.28** (round 1.28″ 240×240 display, Wi-Fi, BLE, IMU) that tracks when a cat was last fed. Two people share state via Cloudflare Workers + D1. Glanceable ring + cat face = no interaction needed to check status.

---

## Entities: household, devices, users, cats

The product serves a **single household**. Inside the household:

| Entity | Cardinality | Notes |
|---|---|---|
| **Household** | exactly 1 | The whole system serves one home. There is no concept of multi-household — that's a separate deployment. |
| **Devices** | 1..N (today: 1) | Each physical knob. Today only one is built, but every event the firmware emits and every API call carries a `deviceId` so the backend can already tell devices apart when a second one ships. Don't optimise the field away. |
| **Users** | 1..N | A user is a person who feeds the cat(s) and records it. Stamped on each event as `by`. No upper bound. |
| **Cats** | 1..N | Each cat is what the events record feedings *of*. No upper bound. |

### Multiple users may use the same device at once

Devices are shared. Two flatmates may both feed the same cat from the same fridge knob — no per-device "signed in" user, no mutual exclusion. The user model is just a roster of names; attribution happens at feed-time, not at sign-in-time. (Clarified 2026-04-28; supersedes earlier drafts that described a per-device "signed-in user".)

### Adaptive UI: don't offer choices that don't exist

A core UX rule: **never present a selector for an entity that has only one option.**

- 1 cat configured → no cat picker on Idle, no cat field on Feed Confirm. The single cat is implicit.
- 1 user configured → no "by whom?" prompt in the Feed flow. The single user is implicit; the firmware stamps that name on the event.
- 2+ cats → cat selector appears (probably on Idle as a long-rotate-or-edge gesture; specifics TBD when the second cat is real).
- 2+ users → an explicit "fed by …" attribution picker appears in the Feed flow (probably between Feed Confirm and Pouring, so the cat-feeder picks who they are before logging). No persistent "current user" — the picker fires every time.

This rule is more important than feature symmetry — a household with 1 cat and 1 user should never see a screen that asks "which one?". The cardinality is queried at render time, not baked in.

### Scope per screen / state

Every piece of state belongs to exactly one of these scopes. New features must declare their scope before any code is written.

| Screen / state | Scope | Notes |
|---|---|---|
| Idle (cat face, "fed Xm ago") | per-cat | shows the **active cat**. With 1 cat, "active" is the cat. With 2+, a cat-selection gesture chooses. |
| Feed Confirm / Pouring / Fed | per-cat | logs `feed` event with `cat` and `by` populated. The selectors for cat / by appear only when their cardinality > 1. |
| Schedule (4 meal slots) | per-cat | each cat keeps its own slot times. |
| Quiet hours | household | one quiet window for the home, applies to all cats. |
| Portion default | per-cat | each cat has its own default portion. With 1 cat, the device's "default portion" *is* that cat's. |
| Settings (Wi-Fi, wake, calibrate) | per-device | local to the physical knob; not synced. Each device runs its own. |
| Per-feed `by` attribution | per-event | captured at the moment of feeding (silent when the household has 1 user; explicit picker when N≥2). Devices are shared — there is no "signed-in user" stored per device. |
| `hid` (household ID) | household | constant for the whole deployment. Doesn't need to be runtime-configurable in the single-household model — but is still passed on every API call so the backend isn't coupled to the assumption. |
| `deviceId` | per-device | UUID baked into the device on first boot. Never reused. |

### Backward-compatible backend

The MVP D1 schema in this doc has no `cat`, `deviceId`, or per-cat config tables. The migration path to N cats / N devices / N users:

- Add `cat TEXT NOT NULL DEFAULT 'primary'` and `device TEXT NOT NULL DEFAULT 'd0'` columns to `events`. Pre-existing rows fall into the primary cat / first device.
- `GET /api/state?hid=…[&cat=…]` filters by cat; omitting `cat` returns the primary cat's state for back-compat (and is the correct call when a household has only one cat).
- A new `cats` table holds per-cat config (name, slug, schedule, portion, threshold). For a 1-cat household this table has 1 row.
- A new `devices` table holds per-device metadata (name, signed-in user, last seen). For a 1-device household this table has 1 row.
- No `users` table — `by` is free-form text. The active user list per household = the distinct `by` values seen in the last N days. The 2+ user prompt UI uses this set.

### What "Mochi", "Andrey", "Masha" mean in this doc

Throughout this repo and in the design JSX (`FeedMeKnobApp.jsx`, etc.) you'll see hardcoded names like **Mochi** (cat) and **Andrey** / **Masha** (users). These are **placeholders for whatever cat / user is active at runtime**, not literals — the firmware substitutes the real names captured at first-time-setup. Treat them as `<cat-name>` / `<user-name>` when reading the design.

---

## Hardware

### Board
**[CrowPanel 1.28-inch HMI ESP32 Rotary Display](https://www.elecrow.com/crowpanel-1-28inch-hmi-esp32-rotary-display-240-240-ips-round-touch-knob-screen.html)** (Elecrow). Sold on Amazon under various reseller brand names ("IoTeikXgo", etc.). Form factor is a **smart-knob device**: round 1.28″ IPS LCD, knurled rotary encoder ring, capacitive touch, and a 5-LED WS2812 RGB ring under the bezel.
- Wiki: <https://www.elecrow.com/wiki/CrowPanel_1.28inch-HMI_ESP32_Rotary_Display.html>
- ESP32-S3 chip rev v0.2 (QFN56), **16 MB QIO flash** (GD25Q128), **8 MB OPI PSRAM** (AP Memory APS6408, "AP_3v3" per esptool)
- Wi-Fi + BLE 5, USB-Serial-JTAG on-chip
- Display driver **GC9A01**, touch IC **CST816D**, no IMU on this variant
- 5V/1A USB-C input, 3.3V chip rail

> ⚠️ Earlier handoff drafts misidentified this as the
> Waveshare ESP32-S3-LCD-1.28 / Spotpear ESP32 Display 1.28. Those are
> different boards with different pin maps. The CrowPanel additionally
> gates the LCD's 3.3 V rail through GPIO 1 — the LCD is dead until that
> pin is driven HIGH. Several hours of dark-screen debugging traces back
> to this single fact.
>
> The chip variant fact (8 MB OPI PSRAM, AP_3v3) is unchanged — that
> drove the platformio.ini `memory_type = qio_opi` decision and remains
> correct. The simulator env overrides to `qio_qspi` because Wokwi's
> emulator can't service OPI flash bus mode.

### Key Pin Assignments (verified — CrowPanel 1.28 HMI)
| Function | GPIO | Notes |
|----------|------|-------|
| **LCD 3V3 enable** | **1** | Drive HIGH first thing in setup() — without this the LCD has no power and nothing else matters. |
| **LED-ring 3V3 enable** | **2** | Drive HIGH if you intend to use the LED ring. |
| LCD MOSI | 11 | |
| LCD MISO | -1 | GC9A01 is write-only; not connected. |
| LCD CLK  | 10 | |
| LCD CS   | 9  | |
| LCD DC   | **3** | NOT 8 — that was wrong on every earlier draft. |
| LCD RST  | 14 | |
| LCD Backlight (BL) | **46** | active-HIGH PWM. |
| Touch SDA | 6 | I²C bus 0. |
| Touch SCL | 7 | |
| Touch INT | 5 | CST816D interrupt. |
| Touch RST | 13 | |
| Rotary encoder A | 45 | |
| Rotary encoder B | 42 | |
| Rotary encoder switch (push) | 41 | |
| RGB LED ring data | 48 | WS2812, 5 LEDs. |
| Power-indicator LED | 40 | |
| Test / spare GPIOs | 4, 12 | exposed on FPC header. |

Authoritative source: the [Elecrow wiki](https://www.elecrow.com/wiki/CrowPanel_1.28inch-HMI_ESP32_Rotary_Display.html) and [makerguides.com getting-started page](https://www.makerguides.com/getting-started-crowpanel-1-28inch-hmi-esp32-rotary-display/). Both list identical pin maps.

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
#define TFT_MISO -1            // GC9A01 is write-only on this board
#define TFT_SCLK 10
#define TFT_CS    9
#define TFT_DC    3
#define TFT_RST  14
#define TFT_BL   46
#define TFT_BACKLIGHT_ON HIGH
#define USE_HSPI_PORT          // ← MANDATORY, crashes without it on ESP32-S3
#define SPI_FREQUENCY 80000000
```

Plus, **outside of TFT_eSPI**, you must drive GPIO 1 HIGH before
`tft.init()` to power the LCD itself:
```cpp
pinMode(1, OUTPUT);
digitalWrite(1, HIGH);   // LCD 3V3 enable — required, not optional
```

(In our `firmware/platformio.ini` these are passed as `-D` flags rather than
edited into `User_Setup.h`. `TFT_MISO=13` is needed even though the GC9A01
is write-only — without it the `spiAttachMISO()` call inside the Arduino-
ESP32 SPI HAL prints `HSPI Does not have default pins on ESP32S3!` errors.)

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
