## What changed

**Firmware** (compiles for `esp32-s3-lcd-1_28` and `simulator`)

- **`lv_conf.h`**: enabled `LV_USE_QRCODE` (LVGL ships its own QR generator at `extra/libs/qrcode`; no new lib_deps).
- **NVS prefs**: added `paired` flag + `hidResetCount` int. `clearWifiCreds()` now also clears the paired flag.
- **`main.cpp`**: at boot, if NVS has no hid → derive `feedme-{12-hex-mac}` (or `feedme-{mac}-{n}` after `n` resets), persist, push to `WifiNetwork`. After all wiring, if `!paired` → `BootView` lands on `pairing` instead of `idle`.
- **`WifiCaptivePortal`**: dropped the Household-ID input + validation. Form now only collects SSID/pass/optional user. Replaced with a hint that says "after Save, the device shows a pairing QR".
- **`PairingView`** (new): QR canvas (140 px) with `lv_qrcode_create`, hid label below, "tap to skip" hint. Tap/Press → fires `onPaired` callback (sets NVS paired flag) → `idle`. Long-press/Long-touch → `resetPairConfirm`.
- **`ResetPairConfirmView`** (new): destructive confirm. Tap fires the `onConfirm` callback (bumps reset count, clears hid + paired, `ESP.restart()`). Any other gesture cancels back to `pairing`.
- **`BootView`**: gained a `setNext("pairing")` override so first-boot can route past idle.
- **`SettingsView`**: bumped `ITEM_COUNT` 8→9, added "Pair" entry that re-shows the QR (so users can re-scan whenever, and reach reset via long-press from there).

**Backend**
- **`DELETE /api/auth/household`** (auth required): wipes households + cats + users rows for the token's hid; events stay (orphaned, harmless).

**Webapp** (builds clean)
- **`/setup`** route + `SetupPage`: reads `?hid=` from the QR, probes `/api/auth/exists`, renders one of four phases:
  - `new` → "Set a PIN" form → `/api/auth/setup` → signed in
  - `exists` → "Already paired" → "Continue to sign in" handoff to `/login?hid=…`
  - `missing` → friendly explainer + link to `/login`
  - `error` → retry button
- **`LoginPage`**: now reads `?hid=` from URL, auto-fires `exists` probe, lands directly on PIN entry.
- **`SettingsPage`**: new "Forget this household" button → `api.forgetHousehold()` → clears local token → `/login`.
- **`api.ts`**: added `forgetHousehold()` helper.
- **README**: new "Pairing flow" section documenting QR, reset path, and remote forget.

## Two reset paths the user now has

1. **Device-side ("I forgot my PIN"):** Long-press the QR screen → confirm → device rotates hid + reboots → new QR. Old DB row orphaned (clean it up via Settings → Forget on a different paired device, or via wrangler).
2. **Webapp-side ("Clean exit"):** Settings → Forget this household → server deletes the household row → user signed out. Device still keeps its hid; if they re-pair, the same hid becomes available again.

## Deployment

- Backend: `cd backend && npm run deploy`
- Webapp: `cd webapp && npm run build && npx wrangler pages deploy dist --project-name feedme-webapp` (the `functions/api/[[path]].ts` proxy from the previous fix is still in place)
- Firmware: `cd firmware && pio run -e esp32-s3-lcd-1_28 -t upload`

After flashing a clean device (or one whose NVS you've wiped via `nvs_erase`), the new flow runs end-to-end: captive portal → reboot → boot splash → **pairing screen with QR** → scan with phone → set PIN → home.


## Local deploy script
Two flavors — pick whichever shell you prefer:

Bash (Git Bash, WSL, macOS, Linux): scripts/deploy.sh
```
./scripts/deploy.sh                # backend + webapp (default)
./scripts/deploy.sh --firmware     # all three
./scripts/deploy.sh --backend-only
./scripts/deploy.sh --webapp-only
./scripts/deploy.sh --firmware-only
./scripts/deploy.sh --skip-migrations
```

PowerShell (native Windows): scripts/deploy.ps1

```
.\scripts\deploy.ps1                  # backend + webapp
.\scripts\deploy.ps1 -Firmware        # all three
.\scripts\deploy.ps1 -BackendOnly
.\scripts\deploy.ps1 -WebappOnly
.\scripts\deploy.ps1 -FirmwareOnly
.\scripts\deploy.ps1 -SkipMigrations
```
What each step does: