# FeedMeKnob Implementation Plan

> Source: design handoff bundle `FeedMe-handoff.zip` from Claude Design.
> Reference assets staged in `firmware/design/` (gitignored).
>
> Companion documents: [handoff.md](../handoff.md), [docs/roadmap.md](roadmap.md),
> [docs/architecture.md](architecture.md), [docs/session-handoff.md](session-handoff.md).

---

## What just changed

The design pivots the product UX from a **single-screen mood tracker**
(what we have on `dev-3`/`dev-4`) to a **12-screen interactive
feeder** with a proper state machine, twelve cat-silhouette PNGs,
and a richer visual system. The hardware target (1.28″ round
GC9A01, knob + cap-touch) **matches our existing CrowPanel** —
this is purely a UX/firmware redesign, not a hardware change.

### Honest scoping note

The design assumes an **active feeder with a motor + hopper** —
"Pouring" screen, "X g of 40", hopper-level percentage, cancel-pour
gesture. **We don't have that hardware** — our device is a passive
*tracker* (the human feeds the cat, the device records it). Two
plausible interpretations:

1. **Adopt the design literally** as the *eventual* product, and
   skip motor-dependent screens until/unless someone bolts a
   feeder on. Pouring/Hopper become deferred.
2. **Re-frame the design** as a tracker UX: "Feed Confirm" becomes
   "Log meal, X g", "Pouring" becomes a brief logging animation
   (no motor), "Hopper" hides behind a feature flag.

This plan uses **interpretation (2)** for the implementable
present, while keeping screen geometry compatible with (1) for a
future motor pivot. Specifically:

| Screen | Tracker semantics (now) | Feeder semantics (later) |
|---|---|---|
| Idle | Time + cat + next reminder slot | same |
| Menu | Same 4 glyphs | same |
| Feed Confirm | "Log meal at Xg" | "Pour Xg" |
| Pouring | Brief logging animation (no motor) | Real pour with motor PWM |
| Fed | Same | same |
| Schedule | View slots | view + auto-feed at slot times |
| Quiet | Mute reminders 22-06 | mute + disable auto-feed |
| Hopper | (hidden / feature-flag off) | hopper level via load cell |
| Portion | Default-meal-size (g) | actual dispensed grams |
| Settings | Wi-Fi / wake / quiet / calibrate | + motor-cal screens |
| Lock confirm | Cancel today's reminders | cancel today's auto-feeds |
| Boot | Splash | splash |

---

## What we keep from `dev-3`/`dev-4`

Nothing thrown away — all of this stays:

- **Build system.** PlatformIO 3-env split (`esp32-s3-lcd-1_28`, `simulator`, `native`), wokwi.toml, merge_simulator_bin.py, board_upload overrides — all stays.
- **Hardware adapters.** `Cst816TapSensor`, `EncoderButtonSensor`, `LedRing`, `LvglDisplay` (the LVGL/TFT_eSPI bridge), `LittleFsStorage`, `NvsPreferences`, `ArduinoClock` — all stay.
- **Domain layer.** `FeedingState`, `Mood`, `MoodCalculator`, `RingProgress`, `FeedingService`, `DisplayCoordinator` — stay; will be extended.
- **Wi-Fi/NTP scaffolding** (Phase 1 on `dev-4`).
- **History persistence** (`/history.jsonl` rotation).
- **Native domain unit tests.**

## What we replace

- **`LvglDisplay`'s scene graph.** Currently builds one cat-face scene; will become a screen manager with twelve scene builders, each in its own file.
- **`CatFace` LVGL-primitive cat.** Replaced by embedded cat-silhouette PNGs (`lv_img_dsc_t` C arrays from `cats4/*.png`). The "no bitmaps" line in [handoff.md:80](../handoff.md:80) was an early design guess — the formal handoff explicitly uses bitmaps now. Drop the constraint.
- **Palette.** Switch from current ad-hoc colors to the **Aubergine** theme as the production default. Keep `cream` and `mono` themes available as compile-time alternates.
- **Mood label / outer ring layout.** The ring is a per-screen device (portion arc, pour arc, quiet wedge, lock-confirm, hopper) — not a single mood-driven indicator.
- **Single-tap = log feed.** Becomes: tap → menu (or idle's primary advance). The "log a feed" path now goes through Feed Confirm → Pouring → Fed.

---

## Phased implementation

Each phase is a shippable branch, with a working device at every step. Estimates assume one engineer with the dev-3/dev-4 codebase loaded.

### Phase A — Visual baseline + asset pipeline · ~1 day

Goal: device boots into a faithful new **Idle** screen using the Aubergine palette and a real cat PNG. Inputs still drive the *old* single-screen logic (no state machine yet). Smallest visible win.

- **A.1** PNG → `lv_img_dsc_t` conversion pipeline. Use the [official LVGL image converter](https://lvgl.io/tools/imageconverter) (web tool, batch export). Resize each cat in `firmware/design/cats4/` from 1500×1500 down to 130×130 and 88×88 RGB565+alpha (CF_TRUE_COLOR_ALPHA). Produces `cats4_C2_130.c`, `cats4_B2_88.c`, etc. — about 12 cats × 2 sizes = 24 files. Stage in `firmware/src/assets/cats/`.
- **A.2** New `firmware/src/views/Theme.h` carrying the three palettes (`aubergine`, `cream`, `mono`). Default selected via `-DFEEDME_THEME=aubergine` build flag.
- **A.3** Re-skin the current cat scene in `LvglDisplay::buildScene()`: dark Aubergine bg (`#1a1226`), 240×240 internal layout, top time label (Georgia 38), middle cat PNG (centered y=58%, ~120 px), bottom kicker label. Drop the outer mood arc for now; replace with the per-screen arc concept later.
- **A.4** Verify on hardware. Snap a photo, compare side-by-side to the `ScrIdle` JSX layout. Iterate sizing until silhouette + time alignment match the source.

Ship: `dev-5` first commit. Device looks like FeedMeKnob's **Idle**, behaves like dev-4 internally (single-screen).

### Phase B — Screen manager + state machine · ~1.5 days

Goal: real screen navigation. Tap/press from Idle opens Menu; Menu rotates between glyphs; selecting any opens its sub-screen; long-press anywhere → Lock Confirm; long-tap during a destructive screen cancels.

- **B.1** New abstraction: `IView` interface + `ScreenManager` registry. Each screen is a class implementing `enter(ctx) / leave() / render(ctx) / handleInput(event)`. Owns its LVGL widgets, lazily built on first enter, hidden on leave.
- **B.2** Implement the FSM exactly as drawn in [handoff.md §3 state machine](../firmware/design/handoff.md):
  ```
  Idle ─tap/press─→ Menu ─rotate─→ (Feed/Schedule/Quiet/Settings)
       ─long─→ LockConfirm ─hold 2 s─→ destructive
  ```
  State transitions go through `ScreenManager.transition(NewState)`, which picks the matching `IView` and animates a 250 ms cubic-bezier crossfade (per [handoff.md firmware notes](../firmware/design/handoff.md)).
- **B.3** Bind input events to the FSM. Reuse our 8-event enum (`Tap`, `DoubleTap`, `LongTouch`, `Press`, `DoublePress`, `LongPress`, `RotateCW`, `RotateCCW`). Map per the canonical gesture table:

  | Gesture | Knob | Touch | Reserved for |
  |---|---|---|---|
  | Primary advance | `Press` | `Tap` | open / confirm / next step |
  | Secondary | `RotateCW`/`RotateCCW` | tap on contextual region | adjust / scroll |
  | Cancel / parental gate | `LongPress` | `LongTouch` | stop pour, lock, destructive confirm |

  The "every action reachable via touch" non-negotiable means *every* knob handler also has a touch equivalent. Enforce this via a unit test.
- **B.4** Implement Idle (already from A) + Menu + a *stub* for Feed Confirm / Schedule / Quiet / Settings (just label + back-on-press). End of B = full nav skeleton, blank inner content.

Ship: second commit on `dev-5`. Hardware demo: tap to open menu, rotate between glyphs, press to enter sub-screen, long-press to abort.

### Phase C — Build out individual screens · ~2 days

Goal: every screen renders the full content per the JSX reference. Domain models extended only where the screen needs them.

Implement in priority order; each is independent enough to ship as its own commit:

1. **C.1 Feed Confirm + Portion adjust + Pouring + Fed**
   - New `Portion` domain object (default 40 g, range 5-60, persisted in NVS).
   - Feed Confirm renders portion arc + cat hero + portion text per `ScrFeedConfirm`.
   - Pouring renders the perimeter ring filling 0→100% over 1.5 s (matches JSX).
   - Fed renders success state with heart accent (`IcHeart`) + "next 13:00" stub.
   - Tap-on-cat-region → Portion Adjust (ScrPortionAdjust); from there tap → save → back to Confirm.
   - On `dev-3`'s tracker semantics: "Pouring" just animates and logs the meal to `history.jsonl` — no motor.

2. **C.2 Schedule (read-only)**
   - New `MealSlot` model: 4 slots × {time, label, mood-tint}. Persist defaults: 07/13/18/22.
   - `ScrSchedule` perimeter layout: 4 slot circles at 12/3/6/9 o'clock, served-state visualization (filled accent if past, current bordered if "now").
   - Read-only this round; editing comes later.

3. **C.3 Quiet hours**
   - `QuietWindow` model (start, end). Default 22:00–06:30.
   - `ScrQuiet` shows a 24-h ring with the quiet wedge highlighted, a "now" tick at the current time, plus moon glyph + range label.
   - Tap → toggle on/off (boolean, persisted).
   - Depends on real clock (`Phase 1.1 NTP`) — already shipping on `dev-4`.

4. **C.4 Settings list**
   - Vertical list with selection arc on left edge; rotate scrolls; press → enter sub-editor (deferred to C.6).
   - Items: Wi-Fi (read-only summary), Wake time, Quiet on/off, Calibrate (no-op).
   - Selection arc per `ScrSettings`.

5. **C.5 Lock confirm**
   - `ScrLockConfirm` arc fills as the user holds; release before 2 s aborts; full hold → destructive action ("clear today's schedule" — no-op for v0, just logs).
   - Long-press / long-touch gates here regardless of which screen is active (per the FSM).

6. **C.6 Boot splash + Hopper (skip)**
   - `ScrBoot` for ~1 s on power-on (logo + small cat).
   - Hopper screen behind a `-DFEEDME_HAS_HOPPER` build flag; default off, no-op screen if pressed.

Each commits as a separate "Add C.x screen" patch. End of C = device feels like the full FeedMeKnob.html prototype.

### Phase D — Settings sub-editors + persistence · ~2 days

Goal: the menu items in Settings actually do things, and the household-level entity lists (cats, users) become editable.

Per-device tuning:

- **D.1** Wake-time editor — sub-screen with hour/minute selectors. Rotate adjusts focused field, Press advances focus / saves on the last field. (Long-press is shadowed by the cross-cutting LockConfirm interception from C.5; release returns to the editor.)
- **D.2** Quiet hours start/end editor — same pattern, two HH:MM pickers (start, end).
- **D.3** Threshold editor *(absorbs the dev-3 knob-rotate-tunes-threshold behavior — it now lives on a dedicated screen instead of always-on)*.
- **D.4** Wi-Fi reset (clears NVS Wi-Fi creds, reboots into setup mode — depends on roadmap Phase 2.4 captive portal landing first OR build-flag re-flash).

Household-level entity management (closes the multi-cat / multi-user gap):

- **D.5** Cats editor — sub-screen lists the household's cats; per-cat: name, slug (one of `cats4/`), default portion, hungry threshold. Add / rename / delete. Always visible in Settings (it's a management surface, not a selector — even N=1 households need a path to add a second cat). Reachable from Settings → Cats row.
- **D.6** Users editor — sub-screen lists the household's users + provides "+ Add user". Devices are shared (multiple users may feed from the same knob), so there is **no** "signed-in user" pointer per device — the roster is just the set of names. Always visible in Settings (same management-vs-selector reasoning). Per-feed attribution for N≥2 happens via a separate picker inserted into the Feed flow (deferred from D.6 — see follow-up below). v0 with N=1: the only user's name silently stamps every event.

#### D.6 follow-up: "by whom?" picker

Once the roster has 2+ users, the Feed flow needs an explicit attribution step. Likely placement: between **Feed Confirm** and **Pouring** (so the user picks who they are *before* the meal is logged, not after). Picker UX: same horizontal/vertical scroll list as CatsListView. The picker fires *every* time someone feeds — there's no remembered "last feeder", because the whole point of the multi-user model is that *anyone* in the household can use the device. Tracked separately because it requires a 6-state Feed FSM extension.

#### Caveats for D.5 / D.6 (apply to both)

1. **Knob character entry is tedious.** The character-picker UX (Phase 1.3) is acceptable for an SSID once; for cat names and user names that the user actually wants to read, it's painful. The on-device editor in D.5/D.6 ships as a **fallback**; the **primary editing path** is the captive portal (roadmap Phase 2.4) — phone keyboard, done in seconds. Design the captive portal's Cats / Users tabs alongside the on-device editor so they share data shape.
2. **No backend yet → list is local-only.** Cats and users are household scope per [handoff.md § "Entities…"](../handoff.md), but until Phase 2.1 (`WifiNetwork`) + a `cats` / `devices` API ship, the firmware can only persist to NVS. D.5/D.6 land as "this device's view of the household"; full sync arrives with Phase E.
3. **Renames need stable IDs, not display names.** Events stamp `by` as free-form text and (in the planned schema) `cat` as a slug. Before exposing rename, switch the event model to reference cat-by-id and user-by-id so historical rows don't orphan when display names change. This is a pre-requisite for D.5/D.6 to ship safely; do it first or scope D.5/D.6 to "add only, no rename".

#### Adaptive-UI rule reminder

Per [handoff.md § "Entities…"](../handoff.md), screens never offer a selector for cardinality 1. Concretely:

- The **Cats** and **Users** management rows in Settings are always present (management ≠ selection).
- The **cat-selector gesture** (planned: long-rotate from Idle) only fires when N≥2 cats.
- The **"fed by …" attribution prompt** in the Feed flow only appears when N≥2 users; with N=1 the signed-in user is implicit.

### Phase E — Backend integration touch-up · folded into roadmap Phase 2

The schedule, quiet hours, and feed-event log all want to sync to Cloudflare Workers. The work in `docs/roadmap.md` Phase 2.1 (`WifiNetwork` adapter) lands these for free once it ships. Order:
- Roadmap 2.1: `WifiNetwork` posts feed events / pulls latest from `/api/state`.
- Roadmap 2.2: drain pending queue.
- New: extend backend with `/api/schedule` GET/PUT for the four slot times.

---

## Asset pipeline detail (for Phase A)

The PNGs are 1500×1500. We need them at 130×130 and 88×88 in `lv_img_dsc_t` format.
Keep orogonals in repo for later use with higher definition devices.

Two options:

### Option 1 — LVGL online converter (simplest, what to use)

For each `cats4/<slug>-white.png`:
1. Open https://lvgl.io/tools/imageconverter (LVGL v8 mode).
2. Resize source to target (e.g. 130×130) using a normal image editor first.
3. Color format: `CF_TRUE_COLOR_ALPHA` (16-bit color + 8-bit alpha → RGB565A8 internal).
4. Output: C array.
5. Save as `firmware/src/assets/cats/cat_<slug>_<size>.c`.

Each output is ~17 KB. 12 slugs × 2 sizes = ~400 KB total — tight on a 4 MB partition but fine on our 16 MB.

For day-to-day iteration we only really need the 5 mood cats (Happy=C2, Neutral=B1, Hungry=B2, Sleepy=B3, Fed=C4). Start with those = 5 × 2 = ~170 KB.

### Option 2 — Local converter via Python + Pillow

If the user wants reproducible, scriptable conversion:

```python
# firmware/scripts/convert_cats.py
from PIL import Image
import sys, os

CATS = ['A1','A2','A3','A4','B1','B2','B3','B4','C1','C2','C3','C4']
SIZES = [130, 88]
SRC = 'firmware/design/cats4'
DST = 'firmware/src/assets/cats'

# Pillow → LVGL CF_TRUE_COLOR_ALPHA writer (compatible with v8.4)
# ~50 LOC; trivially writeable. Defer until we hit a friction point
# with the online tool.
```

Defer Option 2 unless Option 1 becomes annoying.

---

## Open questions to confirm before A.1

(Cheap to clarify now, expensive to redo later.)

1. **Tracker vs feeder semantics.** Confirm we're going with interpretation (2) — "Pouring" is a brief logging animation, not motor control. Affects copy on Feed Confirm and timing on Pouring.
2. **5 cats now or all 12.** Save flash by shipping only the 5 mood cats first (130 KB), or stage all 12 to enable the Tweaks-panel swap UI later (~400 KB)? My vote: 5 now, 12 later.
3. **Dropping the dev-3/dev-4 LVGL-primitive `CatFace`.** Once PNGs are in, the `CatFace.{h,cpp}` files are dead code. Delete in Phase A, or keep around as a fallback for the simulator (where we may want primitives instead of bitmaps for size)?
4. **Mood inference.** Today the cat changes per `MoodCalculator` (time-since-feed). The handoff implies a more complex flow — the cat that appears on Idle is the same one mapped per current "mood" (still time-driven), but Feed Confirm forces `mapping.hungry`, Fed forces `mapping.fed`, etc. **Check** that this matches the design intent — it does per [handoff.md §1 mood mapping locked](../firmware/design/handoff.md), so no change needed.

---

## What to do first concretely

1. **Merge PR #3** (`dev-4` → `main`) so the NTP/prefs work ships first; the redesign builds on top.
2. **Branch `dev-5` from updated `main`.**
3. **Convert C2 (happy) and B2 (hungry) cats to `lv_img_dsc_t`** at 130 px and 88 px — manual upload through the LVGL online tool, ~10 min.
4. **Start Phase A.1**: drop the converted `.c` files into `firmware/src/assets/cats/`, register them in `LvglDisplay`, replace the `CatFace` widget with `lv_img` for the Idle screen.

Then iterate to A.4 (Aubergine palette + idle layout) and **stop** there for review — that's the smallest cohesive ship.
