# feedme

Fridge-mounted cat-feeder tracker. See [handoff.md](handoff.md) for the full design.

## Open in VS Code

```
File → Open Workspace from File… → feedme.code-workspace
```

Accept the prompt to install recommended extensions (PlatformIO, Wokwi, ESLint,
Prettier, Cloudflare Workers, Even Better TOML, C/C++).

## Layout

| Folder | Purpose | Tooling |
|--------|---------|---------|
| `firmware/` | ESP32-S3-LCD-1.28 sketch (TFT_eSPI + LVGL) | PlatformIO |
| `backend/`  | Cloudflare Worker + D1 (`/api/state`, `/api/feed`, `/api/history`) | Wrangler |
| `mockup/`   | Vite + React preview of the UI states | Vite |

## Quick start

```bash
# UI mockup
cd mockup && npm run dev

# Backend (local)
cd backend && npm install && npx wrangler d1 create feedme   # paste id into wrangler.toml
npm run db:apply:local
npm run dev

# Firmware — real board
cd firmware && pio run -e esp32-s3-lcd-1_28      # build
pio run -e esp32-s3-lcd-1_28 --target upload     # flash (USB-CDC, 921600)
pio device monitor                               # serial monitor
```

VS Code task palette (`Ctrl+Shift+P → Tasks: Run Task`) covers all of the
above plus the simulator below.

## Local simulator (Wokwi)

The Wokwi VS Code extension runs the actual ESP32-S3 firmware against an
emulated MCU, so Wi-Fi, I2C, SPI, and the LVGL state machine all execute for
real — no hardware needed.

**Caveat:** Wokwi doesn't have a native GC9A01 part. The `[env:simulator]`
PlatformIO env compiles with `ILI9341_DRIVER` instead of `GC9A01_DRIVER`, on
the same SPI pins. The simulated display appears rectangular 240×320; the real
board is round 240×240. Logic is identical.

```bash
# 1. Build the simulator firmware (already builds clean: 517 KB flash / 79 KB RAM)
cd firmware && pio run -e simulator
```

Then in VS Code:
1. Open `firmware/diagram.json` (or the Workspace Explorer ► firmware ► diagram.json)
2. `Ctrl+Shift+P` → `Wokwi: Start Simulator`
3. First run prompts for a **free hobbyist license** — sign in at [wokwi.com](https://wokwi.com) (one-time)

The firmware boots with a `SimulatedClock` running at **720× real time**, so
the mood arc (Happy → Neutral → Warning → Hungry) cycles through in about
**25 seconds** of wall clock time — easy to verify the state machine.

## Tests

Domain-layer logic (mood calculation, ring progress) runs as host-side unit
tests via `pio test -e native` — no flash, no simulator. Currently 16 tests.
Run from VS Code: `Ctrl+Shift+P → Tasks: Run Task → firmware: test (native)`.

The pin map and gotchas live in [handoff.md](handoff.md).
`firmware/platformio.ini` wires the TFT_eSPI defines so you don't need to
edit `User_Setup.h`.

## Notes

- PlatformIO Core CLI is at
  `C:\Users\tsyga\AppData\Roaming\Python\Python313\Scripts\platformio.exe`.
  To call `pio` directly from a shell, add that directory to your user PATH.
- Node 23.7 throws a benign `EBADENGINE` warning against Vite's `>=24` hint;
  builds work regardless.
