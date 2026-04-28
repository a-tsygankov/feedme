# feedme — Implementation Roadmap

> Companion to [handoff.md](../handoff.md) (design + hardware reference),
> [docs/architecture.md](architecture.md) (component diagrams + tools), and
> [docs/session-handoff.md](session-handoff.md) (engineering state log).
> Detailed plan for the FeedMeKnob redesign in
> [docs/feedmeknob-plan.md](feedmeknob-plan.md).
> This file is the **forward-looking work plan**.

## ⚠ Direction change (2026-04-28)

A formal design handoff arrived (`FeedMe-handoff.zip`) that pivots
the product UX from a single-screen mood tracker to a 12-screen
interactive feeder. The new direction reuses all our hardware, build
system, and adapter layer, but redesigns the LVGL scene system on
top. Detailed sequencing in
[feedmeknob-plan.md](feedmeknob-plan.md).

This roadmap is **kept as-is below** for the parts that still apply
(NTP, persistent prefs, backend sync, push, deep sleep) — the
redesign reorders Phase 3 (UX polish) and replaces a chunk of it,
but Phases 1, 2, 4, 5 are mostly orthogonal.

---

## Where we are

`dev-3` (5 commits past `8ad2fa7` "Real device, correct colors"):

- ✅ Wokwi simulator boots & renders cat face end-to-end (`pio run -e simulator` + VS Code Wokwi extension).
- ✅ Real CrowPanel hardware boots & renders correctly (chip identified, pinmap fixed, colours fixed).
- ✅ All 8 input gestures detected (Tap / DoubleTap / LongTouch / Press / DoublePress / LongPress / RotateCW / RotateCCW).
- ✅ Hungry-threshold tuning via knob rotation (±30 min per detent, clamped 30 min – 12 h).
- ✅ History overlay on LCD (LVGL panel; last 5 feeds shown as "Nm feed" lines).
- ✅ WS2812 LED ring with mood-coloured ambient breathing + event pulses.
- ✅ LittleFS persistence (`/pending.jsonl` + `/history.jsonl`, rotation at 100 entries, durable across reboots).
- ✅ Native domain tests passing on machines with a host `gcc`.

## What's still missing (gap analysis)

Walking the original [handoff.md "Build Phases"](../handoff.md):

| Spec item | Status |
|---|---|
| Phase 1 — MVP single-device offline | ✅ done |
| Phase 2 — Two-person sync (Wi-Fi, NTP, backend, OTA) | ❌ not started |
| Phase 3 — Polish (animations, push, deep sleep, second device) | ❌ not started |

The real gap right now is that **everything time-related is fake** — `ArduinoClock` returns whatever `time(NULL)` produces (zero, since no NTP), so timestamps in history persist correctly *relatively* across reboots only because the file order preserves them. Any actual epoch-time logic (mood progression, "Nm ago", pending-event freshness) isn't real on hardware. Once NTP works, every other time-driven feature gets sharper for free.

Also: there's no way for two devices (or even two people on one device) to share state, which is the entire **product premise** in the README.

---

## Phase 1 — Make it real (foundational)

Goal: by the end, a flashed device on Wi-Fi sees correct wall-clock time, persists user preferences, and walks first-time users through Wi-Fi config.

### 1.1 NTP time sync · ~2-3 h
- Add a `WifiClient` adapter that connects on boot using credentials stored in NVS.
- Sync time via SNTP (`configTime("pool.ntp.org", ...)`) once associated.
- `ArduinoClock::nowSec()` returns real epoch seconds after sync.
- Store the last-sync timestamp; if stale (> 12 h since sync), retry.
- Verify on bench: history overlay shows real age between events; "FEED ME" mood transitions truthfully on a 5 h timer.

**Dependency:** Wi-Fi credentials. For phase 1.1 it's fine to hard-code via build flags (`-DWIFI_SSID=...`); 1.3 graduates this to runtime.

### 1.2 Persistent preferences · ~1.5 h
- New `IPreferences` port (or extend `IStorage`) for small key/value items: `hungryThresholdSec`, `ownerName`, `householdId`.
- Back with NVS (Espressif `Preferences` library — simpler than LittleFS for tiny K/V).
- `DisplayCoordinator::adjustHungryThreshold()` writes through to prefs.
- On boot, load prefs into the in-memory state.
- Default values when preferences don't exist (matches current hard-coded constants).

### 1.3 First-time setup flow · ~4 h
The trickiest UI work before Wi-Fi can be product-grade.

Detection: on boot, if no `wifiSsid` in prefs → enter setup mode, show config screen instead of cat.

Steps:
1. **Wi-Fi SSID picker** — scan nearby APs, list on LCD; rotate knob to highlight, press to select. Or skip this if it's too hairy and just take the SSID via app/pairing-mode QR (deferred).
2. **Wi-Fi password entry** — alphabet/digits/symbols selector via rotation, press to commit each character, long-press to finish. Slow but works without a phone.
3. **Owner name** — same character picker.
4. **Household ID** — auto-generate `home-XXXX` (4 hex chars) and display, or accept manual.

Output: persist to NVS, reboot into normal flow, attach to Wi-Fi, sync NTP.

> **Risk**: full alphanumeric entry on a knob is slow and tedious. Acceptable v0 trade-off. v1 escape hatch: ESP32 SoftAP captive-portal so you connect with a phone — see *2.4* below.

---

## Phase 2 — Two-person sync (the product premise)

Goal: two CrowPanels in two kitchens, both seeing the same "last fed" state within 30 s.

### 2.1 `WifiNetwork` adapter · ~3 h

Replaces `NoopNetwork`. Implements `INetwork` against the existing [backend/src/index.ts](../backend/src/index.ts):

- `isOnline()` — bare `WiFi.isConnected()`.
- `fetchState(hid)` — `GET https://<worker>/api/state?hid=<hid>`. Parse with ArduinoJson into `RemoteState{lastFeedTs, todayCount, snoozeUntilTs}`.
- `postFeed(by, ts)` / `postSnooze(by, ts, durationSec)` — `POST /api/feed` with the body shape the worker expects. Carries `clientEventId` (UUID) for idempotency.
- HTTPS with the Cloudflare cert pinned (or `setInsecure()` for v0 — accepted risk).

### 2.2 Pending-queue drain · ~1 h
- `FeedingService::tick()` already calls `network_.fetchState()` every 30 s. Wire it to *also* call `storage_.drainPending()` once after `network_.isOnline()` flips to true, replaying each event via `postFeed`/`postSnooze`.
- Fail-soft: if a replay fails, leave it in pending and try again next cycle.

### 2.3 Conflict / clock-skew handling · ~1 h
- `tick()` already merges remote state only if `fresh->lastFeedTs >= state_.lastFeedTs`. That handles the simple case where the other device fed more recently.
- Edge cases worth a unit test: two simultaneous feeds (both within 1 s) — last-write-wins is acceptable. Snooze + feed at the same time → feed should win (snooze cleared in `logFeeding` already, just verify).

### 2.4 Captive-portal Wi-Fi setup · ~3 h *(replaces or augments 1.3 password entry)*
- On no-credentials boot, start an AP `feedme-XXXX` with a captive portal page.
- LCD shows: "connect to feedme-XXXX, then open http://192.168.4.1".
- Web page lists scanned SSIDs, password field, owner-name field. Submit → save → reboot.
- Library: `WifiManager` (tzapu) or hand-rolled with `WebServer` — hand-rolled is ~50 LOC and avoids a dep.

**Dependency:** none beyond Phase 1.1; can land before or after 1.3. If it lands first, 1.3's character-picker UI becomes optional/deletable.

### 2.5 Two-device bench verification · ~1 h
- Flash a second board (need to source another CrowPanel — *gating dep*).
- Same `householdId`. Feed on A; verify within 30 s the cat on B has reset its ring + green-flashed.
- Document the exact procedure in [docs/session-handoff.md](session-handoff.md) so the next person can repro.

---

## Phase 3 — UX redesign (per FeedMeKnob handoff)

**This phase is largely replaced** by the structured rebuild in
[feedmeknob-plan.md](feedmeknob-plan.md): visual baseline + asset
pipeline, screen manager + state machine, twelve individual screens,
settings sub-editors. The sub-items below stay as backlog items that
can fold into the redesign as they touch the same widgets:

### 3.1 Auto-dismiss history overlay · ~30 min
- LvglDisplay tracks `historyShownAtMs_`; in `tick()`, if visible and `now - historyShownAtMs_ > 10 000`, hide.

### 3.2 "fed by X" line · ~30 min
- Add a fourth label below `timeLbl_`. Source the latest entry's `by` from `feeding.state()` or the history ring.

### 3.3 Threshold-change feedback · ~45 min
- On `RotateCW/CCW`, briefly overlay the new threshold ("5h 30m") on the cat scene for 2 s.
- Could reuse the history-overlay scaffolding with a different layout.

### 3.4 Settings menu via LongTouch · ~3 h
- LongTouch (capacitive hold) enters a menu mode.
- Items: Threshold · Snooze duration · Brightness · Owner name · Wi-Fi reset.
- Knob rotates between items, Press selects, LongPress (knob) exits.
- Each item has its own sub-screen (inherit from Setting + value editor pattern).

### 3.5 Better cat artwork · ~half a day
The current cat is recognizable but blocky. Two paths:

1. **LVGL canvas + bezier draw** — write paths matching the React mockup more closely. Fidelity: ~70 % of mockup. Cost: stays "no bitmaps" per [handoff.md:80](../handoff.md:80).
2. **PNG bitmaps via `lv_img_dsc_t`** — convert React SVGs through `lv_img_conv` (CLI) into C arrays. Fidelity: 100 %. Cost: relaxes the "no bitmaps" constraint and bloats flash by ~50 KB across 5 mood frames.

Recommend (2) — handoff's "no bitmaps" was a guess at design intent; fidelity wins.

### 3.6 Animations · ~3 h
- LVGL `lv_anim_t` for ring colour transitions (200 ms cross-fade between mood colours).
- Cat blink animation (every 4 s, swap eye widget for a closed-eye line for 100 ms).
- Mood-change cat crossfade.

---

## Phase 4 — Reach beyond the device

### 4.1 Push notifications (overdue alerts) · ~2 h backend, ~30 min device
- Cloudflare Worker cron: every 5 min, look at `/api/state` per household; if `now - lastFeedTs > threshold + 30 min` AND no feed since last alert, POST to https://ntfy.sh/<topic>.
- Topic = `feedme-<householdId>`. Owners subscribe on phone (any ntfy app).
- No device-side change required. The threshold value comes from the most recent `/api/feed` body's optional `threshold` field, or stays default.

### 4.2 OTA firmware updates · ~3-4 h
- ESP-IDF / Arduino `Update` API.
- Backend serves `/firmware/<version>.bin`.
- Settings menu adds "Check for update".
- Defer signing for v1; for v0, accept HTTPS-only as the integrity gate.

---

## Phase 5 — Power (only if going battery)

The CrowPanel **doesn't have an on-board battery**, so this phase is gated on either: (a) deciding to keep the device USB-powered forever, or (b) sourcing a different board variant.

If pursued:

### 5.1 Deep sleep · ~3 h
- After 30 s idle (no input), enter light sleep with display off.
- Wake on: touch INT (GPIO 5), encoder switch (GPIO 41), or knob A/B (45/42).
- LED ring off in sleep.
- Resume preserves state; doesn't re-NTP unless the wake was > 12 h since last sync.

### 5.2 Backlight dimming · ~1 h
- Fade backlight to 20 % after 60 s no-input.
- Brighten on any input.

---

## Current state of branches

- `main` ← `47f9bae` Enhance CrowPanel input handling with multi-sensor support (#2 squash)
- `dev-4` ← Phase 1 NTP + persistent prefs, **PR #3 open and mergeable**
- *(next)* `dev-5` — FeedMeKnob redesign, sequenced in [feedmeknob-plan.md](feedmeknob-plan.md)

## Recommended next session

1. **Merge PR #3** (dev-4 → main, squash). Ships honest wall-clock time and persistent threshold.
2. **Branch `dev-5` from updated `main`.**
3. **Execute [feedmeknob-plan.md](feedmeknob-plan.md) Phase A** — visual baseline + asset pipeline. Smallest cohesive ship of the redesign: device boots into the new Aubergine Idle screen with a real cat PNG, but inputs still drive dev-4 logic.
4. **Phase B onward** as that lands cleanly.

Phase 2 (backend sync) stays as a parallel track and folds into Phase E of the redesign — wire the `WifiNetwork` adapter once Schedule + Quiet Hours screens have something worth syncing.

---

## Cross-cutting concerns to keep in mind

- **No secrets in commits.** Wi-Fi passwords / Cloudflare tokens go in `.gitignore`'d build flags or NVS. The `.claude/settings.local.json` exclusion pattern already established is a precedent.
- **`.pio/` is already ignored.** Build artefacts shouldn't drift into commits.
- **`pio test -e native` won't run on this Windows machine** until host gcc is on PATH. Land that fix (or use WSL) before serious domain-test work; otherwise pretest changes only via simulator.
- **Branch hygiene:** keep one feature per branch (`dev-N`). Squash on merge to `main` is the established pattern.
