# FeedMe — Handoff Notes

Two surfaces share one product brain:

| Surface | File | Status |
|---|---|---|
| **Phone app** (FeedMe.html) | `FeedMe.html` + `FeedMeApp.jsx` | Concept, three workflows |
| **Round IoT knob** (FeedMeKnob.html) | `FeedMeKnob.html` + `FeedMeKnobIcons.jsx` + `FeedMeKnobScreens.jsx` + `FeedMeKnobApp.jsx` | Concept, twelve LVGL screens |

Both pull from the same silhouette set in `cats4/` (twelve poses, each as a black PNG and a `-white.png` inverted PNG) and share the same Aubergine palette.

---

## 1. Mood → cat mapping (locked)

These five poses carry mood across both surfaces. Stored as the `MOOD_TO_CAT` / `KNOB_DEFAULTS` block at the top of each `*App.jsx` so users can swap from the Tweaks panel.

| Mood | Slug | Pose |
|---|---|---|
| `happy` | **C2** | Upright sit, ears forward, looking up & sideways — alert, content |
| `neutral` | **B1** | Three-quarter sit, calm baseline |
| `hungry` | **B2** | Front-facing, paw raised — the "feed me" gesture |
| `sleepy` | **B3** | Head bowed, settling / grooming |
| `fed` | **C4** | Chunky round body, looking back over shoulder, satisfied |

Any of the twelve in `cats4/` (`A1–A4`, `B1–B4`, `C1–C4`) is swappable into any slot via the in-page Tweaks panel — picks persist into the source file via the `EDITMODE-BEGIN/END` block.

---

## 2. Phone app — `FeedMe.html`

### Workflows
1. **Morning check-in** — Hungry cat states "mew", press *Feed Mochi · 40 g*, pour animation runs ~1.2 s, ends on Fed cat.
2. **Today's schedule** — Vertical timeline of four meal slots (07/13/18/22), each with a tiny per-slot mood cat, served / now / pending status, footer stats (streak, weight, kibble left).
3. **Quiet hours** — Sleepy cat with Z's, schedule preview ribbon (24-hour bar, quiet wedge highlighted, now-line at 22:14), toggle.

### Visual system
- **Canvas:** Aubergine `#1a1226` with dual radial highlights (top-left magenta, bottom-right wine).
- **Phone frame:** 360×720 rounded rect, 1px top hairline, no real status bar — title row and screen-label live there.
- **Type:** headlines Georgia 28–30 px (italic accents on key nouns), body Inter 13–15, mono for time.
- **Cards:** `rgba(255,255,255,0.04)` glass + 1 px stroke + 22 px radius + 20 px backdrop blur.
- **Pill kicker:** `rgba(255,179,193,0.14)` over `#ffb3c1` accent text, 10 px uppercase 0.18 em letter-spacing.
- **Accent:** `#ffb3c1` (warm pink); deeper `#d96a82` for gradient bottom.
- **Cat stage:** radial pink wash → transparent. Cat sits centered with drop-shadow `0 8px 24px rgba(0,0,0,0.5)`. Bowl is a 110 px half-ellipse.

### To-build (next pass on the phone app)
- Onboarding (pair feeder, set weight goal, name cat).
- Per-meal detail sheet on tap (food brand, calories, change portion, skip).
- History tab with weight trend + meal-time variance chart.
- Notifications spec — only "missed meal" wakes the lock screen; other events go to the badge.
- Real photo frame for the cat (silhouette stays as a friendly fallback).

---

## 3. Round IoT knob — `FeedMeKnob.html`

### Hardware target
- **Display:** 1.28″ round, 240 × 240 px, GC9A01 driver behind LVGL.
- **Inputs:**
  - Rotary encoder — CW / CCW with detents, momentary press, long-press.
  - Capacitive touch overlay — single-tap, double-tap, long-tap, drag.
- **Constraint:** knob may later be reassigned at OS level to global app/shortcut switching. **Every action it triggers must remain reachable via touch.** That's a non-negotiable design rule.

### Twelve screens

| # | Screen | Purpose | Knob | Touch |
|---:|---|---|---|---|
| 01 | Boot | Splash | — | — |
| 02 | Idle | Time + ambient cat + next meal | press → menu | tap → menu |
| 03 | Menu | Four orbiting glyphs (Feed/Schedule/Quiet/Settings) | rotate to highlight, press to open | tap any glyph |
| 04 | Feed: confirm | Hungry cat hero, 270° portion arc, "40 g" | rotate ±5 g, press → pour | tap → portion adjust |
| 05 | Pouring | Filling perimeter ring | (locked) | long-tap → cancel |
| 06 | Fed | Fed cat, heart, "next 13:00" | press → idle | tap → idle (auto after 1.5 s) |
| 07 | Schedule | 4 slots at 12/3/6/9 o'clock | rotate to next slot | tap → idle |
| 08 | Quiet hours | Moon glyph, 24-h ring with quiet wedge | (read-only) | tap → toggle |
| 09 | Hopper level | Big % + arc + days remaining | (read-only) | tap → idle |
| 10 | Portion adjust | Big number, ±5 g | rotate ±5 g | tap → save |
| 11 | Settings | Vertical 4-row list with selection arc | rotate to scroll, press → edit | tap row → edit |
| 12 | Lock confirm | "Hold to confirm" with filling arc | long-press 2 s | long-tap 2 s |

### Gesture map (canonical)

| Gesture | Knob | Touch | Reserved for |
|---|---|---|---|
| **Primary advance** | press | tap | Open / confirm / next step |
| **Secondary / contextual** | rotate | tap on contextual region | Adjust value, scroll, switch |
| **Cancel / parental gate** | long-press | long-tap | Stop pour, lock screen, destructive confirm |
| **Continuous adjust** | rotate (with detents) | drag (slider region) | Portion, schedule scrub |

Both inputs route into the same state-machine — implement gesture handlers as a thin layer above one shared FSM.

### Visual system (LVGL-friendly)
- **Palette per screen:** background, ink, dim ink, **one** accent. No gradients on screen surface (one mild radial allowed in idle for ambience).
- **Type:** Georgia for hero numerals/words; Inter for labels; mono for time/numbers in chips. Min 12 px (chip kickers); body 14–16; hero 22–44.
- **Bezel:** schematic by default — 1.5 px stroke ring + 40 dotted ticks (every 9°, every 5th major) + small accent triangle indicating knob orientation. Switchable to *dashed* (concept) or *solid* (firmware preview) via Tweaks.
- **Arcs over text** — circular progress is the round screen's superpower; lean on it for portion, pour, hopper, lock-confirm, schedule perimeter.
- **Cat per screen:** silhouette fills ~⅓ of the disc max; never crosses the bezel. Sits at logical y=110 typically.

### Themes
- `aubergine` — production target, dark.
- `cream` — light wood/marble countertop.
- `mono` — wireframe mode, no color, helps verify legibility.

### State machine (informal)

```
boot → idle ─┬─ press/tap ─→ menu ─┬─ Feed     ─→ feedConfirm ─┬─ press        ─→ pouring ──auto──→ fed ──auto──→ idle
             │                     ├─ Schedule ─→ schedule    │                              ↑                ↑
             │                     ├─ Quiet    ─→ quiet       └─ tap          ─→ portion ──tap──→ feedConfirm │
             │                     └─ Settings ─→ settings ──press──→ (edit)                                    │
             │                                                                                                  │
             └─ long-press / long-tap (anywhere) ─→ lockConfirm ──hold 2 s──→ destructive action ──────────────┘
                                                                       └──release early──→ previous screen
```

Cancel during `pouring` is `long-tap` on the screen (not touch tap — too easy to misfire). Cancel during `lockConfirm` is releasing the press before 2 s.

### Firmware notes
- Use LVGL's `lv_arc_t` for all circular progress; bind to a single int 0–100.
- `lv_btnmatrix_t` is too rectangular for the orbiting menu — implement that as four `lv_obj_t` containers placed by trig and animated `width/height` for selected.
- 240×240 round means `lv_disp_set_rotation` is irrelevant but **mask the corners** with `lv_obj_set_style_clip_corner` true (or a circular `lv_canvas_t`) so any rectangular widget that overhangs is hidden.
- Keep the framebuffer in PSRAM, swap by full redraw on screen transition (250 ms cubic-bezier mimic). Partial redraws fine within a screen.
- Touch debounce: 30 ms; long-press threshold: 600 ms (matches the `KnobBezel` JS hero).

### To-build (next pass on the device)
- Animation timing curves verified on actual GC9A01 + ESP32-S3 — current JS uses 250 ms cubic-bezier; some hardware likes 180 ms linear better for round arcs.
- Real LVGL theme port — the JSX is illustrative, not a literal LVGL style sheet.
- Sound design: the click feedback should mirror knob detents; pour has a subtle stream sound. Not in scope here.
- Pairing screen (BLE) — missing from this round.
- Firmware update screen — missing.
- Error states: feeder jam, hopper empty, motor stall — missing, add three banner styles.
- Battery / power-loss UI — missing.

---

## 4. Shared assets

- `cats4/` — twelve silhouette PNGs (each in two variants: black on transparent, white on transparent). Source: three SVG cat sheets in `uploads/`, separated programmatically via connected-component labelling so neighbouring cats don't bleed into each other.
- `tweaks-panel.jsx` — drop-in tweak system (slider/radio/select/color/text). Both surfaces use it.
- Type / color tokens duplicated in each `*App.jsx`; would consolidate in a real codebase.

---

## 5. File index

```
FeedMe.html              ← phone, opens FeedMeApp.jsx
FeedMeApp.jsx
FeedMeKnob.html          ← device, opens icons + screens + app
FeedMeKnobIcons.jsx      ← line icons (1.5 px stroke, 24 viewBox)
FeedMeKnobScreens.jsx    ← KnobBezel + 12 screen renderers + theme palettes
FeedMeKnobApp.jsx        ← state machine, hero, flow grid, tweaks
tweaks-panel.jsx         ← shared
cats4/                   ← shared silhouettes
```
