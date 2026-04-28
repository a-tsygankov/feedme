// Line icons for the round knob device. 1.5px stroke, square/round caps.
// All icons are 24x24 viewBox; size via prop. Color via currentColor.

const Ic = ({ size = 20, stroke = 1.5, children, style }) => (
  <svg width={size} height={size} viewBox="0 0 24 24" fill="none"
       stroke="currentColor" strokeWidth={stroke}
       strokeLinecap="round" strokeLinejoin="round" style={style}>
    {children}
  </svg>
);

// Bowl (a meal) — half-ellipse with kibble bumps
const IcBowl = (p) => (
  <Ic {...p}>
    <path d="M3 11h18"/>
    <path d="M4 11c0 4.4 3.6 8 8 8s8-3.6 8-8"/>
    <circle cx="9" cy="9" r="0.8"/>
    <circle cx="13" cy="8" r="0.8"/>
    <circle cx="16" cy="9.5" r="0.8"/>
  </Ic>
);

// Drop / portion
const IcDrop = (p) => (
  <Ic {...p}>
    <path d="M12 3 c-3.5 4.5 -6 7.5 -6 11 a6 6 0 0 0 12 0 c0 -3.5 -2.5 -6.5 -6 -11z"/>
  </Ic>
);

// Schedule (clock with tick)
const IcClock = (p) => (
  <Ic {...p}>
    <circle cx="12" cy="12" r="9"/>
    <path d="M12 7v5l3 2"/>
  </Ic>
);

// Moon (quiet hours)
const IcMoon = (p) => (
  <Ic {...p}>
    <path d="M20 14.5A8 8 0 1 1 9.5 4 a6.5 6.5 0 0 0 10.5 10.5z"/>
  </Ic>
);

// Sun (wake)
const IcSun = (p) => (
  <Ic {...p}>
    <circle cx="12" cy="12" r="4"/>
    <path d="M12 2v2M12 20v2M2 12h2M20 12h2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4"/>
  </Ic>
);

// Treat (bone-ish pellet)
const IcTreat = (p) => (
  <Ic {...p}>
    <path d="M5 9 a2 2 0 0 1 2-3 a2 2 0 0 1 2 2 l6 6 a2 2 0 0 1 2 2 a2 2 0 0 1-3 2 a2 2 0 0 1-2-3 l-6-6 a2 2 0 0 1-1 0z"/>
  </Ic>
);

// Settings gear (simplified — 6 teeth)
const IcGear = (p) => (
  <Ic {...p}>
    <circle cx="12" cy="12" r="3"/>
    <path d="M12 2v3M12 19v3M22 12h-3M5 12H2M19.07 4.93l-2.12 2.12M7.05 16.95l-2.12 2.12M19.07 19.07l-2.12-2.12M7.05 7.05L4.93 4.93"/>
  </Ic>
);

// Battery (level)
const IcBattery = ({ pct = 0.7, ...p }) => (
  <Ic {...p}>
    <rect x="3" y="8" width="16" height="8" rx="1.5"/>
    <rect x="20" y="10.5" width="2" height="3" rx="0.5" fill="currentColor"/>
    <rect x="5" y="10" width={Math.max(1, 12 * pct)} height="4" fill="currentColor" stroke="none"/>
  </Ic>
);

// WiFi
const IcWifi = (p) => (
  <Ic {...p}>
    <path d="M3 9 a13 13 0 0 1 18 0"/>
    <path d="M6 12.5 a9 9 0 0 1 12 0"/>
    <path d="M9 16 a4.5 4.5 0 0 1 6 0"/>
    <circle cx="12" cy="19" r="0.6" fill="currentColor"/>
  </Ic>
);

// Check (success)
const IcCheck = (p) => (
  <Ic {...p}>
    <path d="M5 12.5l4.5 4.5L19 7.5"/>
  </Ic>
);

// Plus / minus (portion adjust)
const IcPlus = (p) => (<Ic {...p}><path d="M12 5v14M5 12h14"/></Ic>);
const IcMinus = (p) => (<Ic {...p}><path d="M5 12h14"/></Ic>);

// Arrows
const IcArrowUp = (p) => (<Ic {...p}><path d="M12 19V5M6 11l6-6 6 6"/></Ic>);
const IcArrowDown = (p) => (<Ic {...p}><path d="M12 5v14M6 13l6 6 6-6"/></Ic>);
const IcArrowLeft = (p) => (<Ic {...p}><path d="M19 12H5M11 6l-6 6 6 6"/></Ic>);
const IcArrowRight = (p) => (<Ic {...p}><path d="M5 12h14M13 6l6 6-6 6"/></Ic>);

// Rotate-CW / CCW (for knob hint)
const IcRotateCW = (p) => (
  <Ic {...p}>
    <path d="M21 12a9 9 0 1 1-3.5-7.1"/>
    <path d="M21 4v5h-5"/>
  </Ic>
);
const IcRotateCCW = (p) => (
  <Ic {...p}>
    <path d="M3 12a9 9 0 1 0 3.5-7.1"/>
    <path d="M3 4v5h5"/>
  </Ic>
);

// Touch / hand-tap
const IcTap = (p) => (
  <Ic {...p}>
    <circle cx="12" cy="10" r="3"/>
    <path d="M12 15v6"/>
    <path d="M9 18l-2 3M15 18l2 3"/>
    <path d="M12 4v3"/>
    <path d="M5 7l2 2M19 7l-2 2"/>
  </Ic>
);

// Hold (clock-ish dot)
const IcHold = (p) => (
  <Ic {...p}>
    <circle cx="12" cy="12" r="9" strokeDasharray="3 3"/>
    <circle cx="12" cy="12" r="2.5" fill="currentColor" stroke="none"/>
  </Ic>
);

// Pause / play
const IcPause = (p) => (<Ic {...p}><rect x="6" y="5" width="4" height="14" rx="0.5"/><rect x="14" y="5" width="4" height="14" rx="0.5"/></Ic>);
const IcPlay = (p) => (<Ic {...p}><path d="M7 5l12 7-12 7V5z"/></Ic>);

// Heart (fed/love)
const IcHeart = (p) => (
  <Ic {...p}>
    <path d="M12 20s-7-4.6-7-10a4.5 4.5 0 0 1 7-3.5A4.5 4.5 0 0 1 19 10c0 5.4-7 10-7 10z"/>
  </Ic>
);

// Mew speech bubble
const IcMew = (p) => (
  <Ic {...p}>
    <path d="M4 5h16v10h-7l-4 4v-4H4z"/>
  </Ic>
);

// Mic (treat by voice — flourish)
const IcMic = (p) => (
  <Ic {...p}>
    <rect x="9" y="3" width="6" height="11" rx="3"/>
    <path d="M5 11a7 7 0 0 0 14 0M12 18v3M9 21h6"/>
  </Ic>
);

// expose globally
Object.assign(window, {
  IcBowl, IcDrop, IcClock, IcMoon, IcSun, IcTreat, IcGear, IcBattery, IcWifi,
  IcCheck, IcPlus, IcMinus,
  IcArrowUp, IcArrowDown, IcArrowLeft, IcArrowRight,
  IcRotateCW, IcRotateCCW, IcTap, IcHold, IcPause, IcPlay, IcHeart, IcMew, IcMic
});
