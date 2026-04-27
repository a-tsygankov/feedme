# Session Handoff — 2026-04-27

> Snapshot of where the work stands at the end of this session so the next
> person (or next conversation) can pick up cleanly. The companion docs are
> [handoff.md](../handoff.md) (design + hardware reference) and
> [docs/architecture.md](architecture.md) (diagrams + tooling inventory).
> This file captures **the current engineering state**, not the spec.

---

## TL;DR

- **Branch:** `dev-2` (5 commits ahead of `8f217f9`).
- **Wokwi simulator:** ✅ working end-to-end. Cat face renders, mood arc walks Happy → Hungry in ~25 s.
- **Native domain tests:** ✅ `pio test -e native` still passes (16 tests on MoodCalculator + RingProgress).
- **Real hardware (CrowPanel 1.28-inch HMI ESP32 Rotary Display, USB COM4):** ✅ end-to-end working. Cat face renders on the GC9A01 panel. Board identified through process of elimination: not Waveshare, not Spotpear — actually the Elecrow CrowPanel smart-knob device. The single most important missing fact across the early handoff draft was **GPIO 1 = LCD 3 V3 enable** — without driving it HIGH the LCD has no power, regardless of any other pin or polarity setting.

---

## Branch state

```
2287a90  Boot real ESP32-S3-LCD-1.28 hardware end-to-end           ← latest
c152a08  Replace face placeholder with LVGL-primitive cat
0c34f9b  Move memory_type=qio_qspi to [common] for real R2 chip    ← misnamed; corrected by 2287a90
4243083  Make Wokwi simulator boot on ESP32-S3R2                   ← misnamed; chip is R8V not R2
8f217f9  Add feedme project scaffolding                            ← parent
```

The two misnamed commits are not worth rewriting — `2287a90`'s message documents the corrected understanding of the chip.

### Uncommitted working-tree changes

When the session ended, three files held diagnostic/probe code that should be cleaned up before merging anywhere:

| File | What's in it that shouldn't ship |
|---|---|
| `firmware/src/main.cpp` | A 22-pin backlight GPIO sweep (lines under "BACKLIGHT PIN SWEEP DIAGNOSTIC"), and a 5-iteration `loop beat` heartbeat. Useful to keep in a feature branch; remove before landing. |
| `firmware/src/adapters/LvglDisplay.cpp` | A blue→red→black smoke test inside `begin()` (lines under "Smoke test:"). Same — diagnostic, drop before landing. |
| `firmware/platformio.ini` | Carries the same `TFT_BACKLIGHT_ON=LOW` change that's still under investigation; treat as tentative until a board with confirmed pinout proves it. |

The untracked `.claude/settings.local.json` is local Claude Code config — don't commit.

---

## What's working (verified this session)

### Wokwi simulator (`pio run -e simulator` → VS Code Wokwi extension)

- Boot reaches the app, `[feedme] boot` prints over UART0
- Round face area renders the Simon's Cat-style face (LVGL primitives — see [adapters/CatFace.cpp](../firmware/src/adapters/CatFace.cpp))
- Mood label, time label ("48m ago"), three meal dots all draw
- 720× simulated clock walks the mood arc Happy → Neutral → Hungry in ~25 s wall clock
- BGR/RGB tint inside the ring is a known sim-only artifact (ILI9341 default ≠ GC9A01)

### Real hardware (USB COM4, MAC e8:f6:0a:88:70:90)

What the chip prints over USB-CDC (after our fixes):

```
ESP-ROM:esp32s3-20210327
Build:Mar 27 2021
rst:0x15 (USB_UART_CHIP_RESET),boot:0x18 (SPI_FAST_FLASH_BOOT)
...
load:0x403cc700,len:0x2a0c
entry 0x403c98d0
E (131) esp_core_dump_flash: Core dump data check failed:
Calculated checksum='efb27e14'
Image checksum='ffffffff'                    ← benign, partition just uninitialized
[feedme] boot
[feedme] display ready                        ← display.begin() returned without crashing
[feedme] setup complete
[feedme] loop beat 0
[feedme] loop beat 1
...
```

So the firmware reaches `loop()` and ticks. LVGL is being pumped. SPI commands are being sent.

---

## Resolution — the board is Elecrow CrowPanel, not Waveshare or Spotpear

The crucial reveal came from a photo of the front of the device: a knurled rotary knob ring around the LCD with a 5-LED RGB ring under the bezel. That form factor — round 1.28″ display + physical rotary encoder + LED ring — is the **[Elecrow CrowPanel 1.28-inch HMI ESP32 Rotary Display](https://www.elecrow.com/wiki/CrowPanel_1.28inch-HMI_ESP32_Rotary_Display.html)**, not any of the boards earlier handoff drafts assumed.

Per the [Elecrow wiki](https://www.elecrow.com/wiki/CrowPanel_1.28inch-HMI_ESP32_Rotary_Display.html), confirmed by [makerguides.com](https://www.makerguides.com/getting-started-crowpanel-1-28inch-hmi-esp32-rotary-display/), the actual pin map is:

| Function | GPIO |
|---|---|
| **LCD 3V3 enable** | **1** ← MUST be driven HIGH or the LCD has no power |
| LED-ring 3V3 enable | 2 |
| LCD MOSI | 11 |
| LCD MISO | -1 |
| LCD SCLK | 10 |
| LCD CS | 9 |
| **LCD DC** | **3** (was wrong as 8 in every earlier draft) |
| LCD RST | 14 |
| **LCD BL** | **46** (was wrong as 40, then 2, in earlier drafts) |
| Touch (CST816D) SDA / SCL / INT / RST | 6 / 7 / 5 / 13 |
| Rotary encoder A / B / SW | 45 / 42 / 41 |
| WS2812 LED ring (5 LEDs) | 48 |
| Power-indicator LED | 40 |

**The single hardest-to-diagnose miss was GPIO 1.** It's a load-switch enable for the LCD's 3.3 V rail — without driving it HIGH, the LCD chip is completely unpowered. No backlight pin, no SPI command, no reset toggle, and no diagnostic colour cycle could ever produce a visible result. Driving GPIO 2 HIGH (which we'd been doing as a guessed backlight) inadvertently powered the LED ring instead, which is why the user saw 5 LEDs glow with random colours retained from the seller's pre-loaded demo firmware.

## Hypotheses explored (chronological)

| Hypothesis | Tested | Outcome |
|---|---|---|
| Boot loop / `bootloader_reset()` | esptool + serial | Resolved by [`2287a90`](../firmware/platformio.ini) |
| 8 MB flash header on a 16 MB chip | `image_info` | Fixed by `board_upload.flash_size = 16MB` |
| Wrong PSRAM mode (QSPI on actual OPI hardware) | `esp_psram` serial error | Fixed by `memory_type = qio_opi` for `[env:esp32-s3-lcd-1_28]` |
| USB-CDC silent (firmware seemed dead but was running) | pyserial direct read with DTR/RTS off | Host auto-reset on port open was eating the boot banner |
| Backlight = GPIO 40 (per original handoff) | drove HIGH, then LOW | Wrong pin entirely — GPIO 40 is the power-indicator LED on the CrowPanel |
| Backlight = GPIO 2 (per Waveshare/Spotpear docs) | drove HIGH | Drove the LED-ring power rail (which is why those LEDs lit up), not the LCD backlight |
| Board is Spotpear ESP32 Display 1.28 | tried Spotpear pin map (RST=14, MISO=12) | Closer (RST=14 was right!) but still dark — not the actual board |
| 22-pin GPIO sweep / 9-pin re-sweep | drove every plausible GPIO | None of those pins is the BL on the CrowPanel — BL is 46, only 47 and 48 were tested in the second sweep, *46 was the unlucky gap* |
| **Board is CrowPanel — needs GPIO 1 LCD-power enable, BL on 46, DC on 3** | applied full pin map per Elecrow wiki | ✅ Cat face renders |

---

## Repro / continue from here

### Build and flash

```bash
cd C:/Workspaces/feedme/firmware

# Build for real hardware
& "$env:APPDATA\Python\Python313\Scripts\platformio.exe" run -e esp32-s3-lcd-1_28

# Flash (replace COM4 with whatever `pio device list` shows)
& "$env:APPDATA\Python\Python313\Scripts\platformio.exe" run -e esp32-s3-lcd-1_28 --target upload --upload-port COM4
```

### Read serial reliably

PIO's `pio device monitor` and Arduino's serial monitor both auto-trigger DTR/RTS pulses that reset the chip on connect, which loses the boot banner. The reliable way to read is direct pyserial **with DTR/RTS forced false**:

```bash
C:/Python313/python.exe -c "
import serial, time, sys
p = serial.Serial('COM4', 115200, timeout=0.2, dsrdtr=False, rtscts=False)
p.dtr = False; p.rts = False
end = time.time() + 8
while time.time() < end:
    chunk = p.read(512)
    if chunk:
        sys.stdout.buffer.write(chunk); sys.stdout.flush()
p.close()
"
```

### Inspect the chip identity

```bash
C:/Python313/python.exe C:/Users/tsyga/.platformio/packages/tool-esptoolpy/esptool.py \
    --chip esp32s3 --port COM4 chip_id
```

Expected output line:
```
Features: WiFi, BLE, Embedded PSRAM 8MB (AP_3v3)
```
That `(AP_3v3)` confirms **OPI** PSRAM (AP Memory APS6408 family). If a future board reports `(AP_3v3)` differently, double-check the chip variant before assuming pin maps from this handoff.

---

## Things that need to happen next, in order

1. **Identify the actual board** — silkscreen, vendor docs, or schematic. Until then, real-hardware work is parked. The seller's Amazon listing may have a manual / pinout PDF; if not, ask them or open an inquiry. Common candidates that match "ESP32-S3 + 1.28″ round + knob + touch":
   - LilyGO T-Encoder-Pro (BL=9, RST=14, SCK=12 — *different* from our pin map)
   - Various AliExpress generic clones (no consistent pinout)
2. **Once pinout is known**, update `firmware/platformio.ini` build flags (`TFT_MOSI/SCLK/CS/DC/RST/BL`) and `firmware/src/main.cpp`'s GPIO 40 backlight reference. The `LvglDisplay::begin()` implementation is otherwise hardware-agnostic.
3. **Strip the diagnostic code** from `main.cpp` (pin sweep, loop heartbeat) and `LvglDisplay.cpp` (blue→red smoke test) once the board lights up. Keep the per-stage breadcrumbs in setup() — they're cheap and useful for future field debug.
4. **Update [handoff.md](../handoff.md)** sections that referenced "ESP32-S3R2 / 2 MB PSRAM / pin 40 backlight" — replace with the verified-real values once they're known. The same content also lives in [docs/architecture.md](architecture.md) and [README.md](../README.md).
5. **Implement the deferred adapters** that were always going to be next:
   - `Qmi8658TapSensor` (real IMU adapter; currently `StubTapSensor` — single tap, long-press, double tap all dead on real hardware)
   - Wi-Fi `INetwork` adapter (currently `NoopNetwork`, so `time(NULL)` returns 0 → ring/mood don't progress on real hw)
   - LittleFS-backed `IStorage` (currently `NoopStorage`; reboots forget everything)

---

## Files of interest, with one-line purpose

| Path | What it does |
|---|---|
| [firmware/platformio.ini](../firmware/platformio.ini) | 3 envs: `esp32-s3-lcd-1_28` (real), `simulator` (Wokwi), `native` (host tests). Carries the OPI/QSPI memory_type split and the 16 MB flash override. |
| [firmware/wokwi.toml](../firmware/wokwi.toml) | Points VS Code Wokwi extension at `feedme-merged.bin` (post-build hook output). |
| [firmware/diagram.json](../firmware/diagram.json) | Wokwi wiring with `flashSize:"16", psramSize:"2", psramType:"quad"` (Wokwi can't do octal flash mode). |
| [firmware/scripts/merge_simulator_bin.py](../firmware/scripts/merge_simulator_bin.py) | Post-build merge for simulator: writes 40 MHz flash freq into image header (Wokwi's S3 emulator wedges at 80 MHz). |
| [firmware/src/main.cpp](../firmware/src/main.cpp) | Composition root + setup() with breadcrumbs + (currently) the backlight pin sweep diagnostic. |
| [firmware/src/adapters/LvglDisplay.cpp](../firmware/src/adapters/LvglDisplay.cpp) | LVGL + TFT_eSPI display adapter, owns the cat face. |
| [firmware/src/adapters/CatFace.h](../firmware/src/adapters/CatFace.h) / [.cpp](../firmware/src/adapters/CatFace.cpp) | LVGL-primitive cat face — one of 5 mood layouts (Happy / Neutral=Warning / Hungry / Fed / Sleepy). |
| [docs/architecture.md](architecture.md) | Mermaid diagrams of the system + per-module rationale + tooling inventory with free alternatives. |
| [handoff.md](../handoff.md) | Hardware reference — pin map, design intent, build phases. **The chip identification at the top is now corrected** (R8V, 8 MB OPI PSRAM, AP_3v3) but the rest still assumes the canonical Waveshare pinout. |

---

## Quick-reference: confirmed chip identity

From `esptool chip_id` against this specific physical board:

```
Chip:     ESP32-S3 (QFN56) revision v0.2
Crystal:  40 MHz
USB mode: USB-Serial/JTAG
MAC:      e8:f6:0a:88:70:90
Features: WiFi, BLE, Embedded PSRAM 8MB (AP_3v3)
Flash:    GD25Q128 (manuf c8 / dev 4018), 16 MB, eFuse says quad
```

So the chip is effectively an ESP32-S3R8V with external 16 MB QIO flash. That's the substrate any future pin-map work has to assume.
