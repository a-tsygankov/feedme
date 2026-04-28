// FeedMeKnobScreens.jsx — one round screen frame + 10 screen contents.
// Real device: 240×240 round LVGL display. We render at 240×240 logical px and
// optionally upscale via CSS transform for the static grid.
//
// Conventions:
//   - <KnobBezel size={N} children=<screen> />  — schematic bezel + screen
//   - All screen contents must clip to a 240px circle.
//   - Use INK / DIM / ACCENT vars (from props) for theme.

const SCREEN_PX = 240;

// ── Theme palettes (LVGL-friendly: max 3 colors per screen + accent) ──
const THEMES = {
  aubergine: {
    bg: '#1a1226',           // device idle bg
    screenBg: '#221636',     // active screen base
    ink: '#f6f1e6',
    dim: 'rgba(246,241,230,0.6)',
    faint: 'rgba(246,241,230,0.32)',
    accent: '#ffb3c1',
    accentSoft: '#d96a82',
    line: 'rgba(246,241,230,0.15)',
    bezel: '#0e0817',
    name: 'Aubergine',
  },
  cream: {
    bg: '#efe7d6',
    screenBg: '#1a1226',
    ink: '#f6f1e6',
    dim: 'rgba(246,241,230,0.6)',
    faint: 'rgba(246,241,230,0.32)',
    accent: '#ffb3c1',
    accentSoft: '#d96a82',
    line: 'rgba(246,241,230,0.18)',
    bezel: '#3a2f23',
    name: 'Cream',
  },
  mono: {
    bg: '#0a0a0a',
    screenBg: '#000000',
    ink: '#ffffff',
    dim: 'rgba(255,255,255,0.55)',
    faint: 'rgba(255,255,255,0.28)',
    accent: '#ffffff',
    accentSoft: '#888888',
    line: 'rgba(255,255,255,0.2)',
    bezel: '#000000',
    name: 'Mono',
  },
};

// ── Bezel: schematic (default), dashed (concept), solid (firmware preview) ──
function KnobBezel({ size = 320, theme = THEMES.aubergine, bezel = 'schematic',
                    rotation = 0, children, label, hint, onScreenTap, onScreenHold,
                    onKnobPress, onKnobLongPress, onRotate, interactive = false,
                    style = {} }) {
  const padding = 40; // bezel ring thickness in display px
  const screenSize = size - padding * 2;
  const ref = React.useRef(null);
  const holdTimer = React.useRef(null);
  const pressTimer = React.useRef(null);

  // wheel → rotate
  React.useEffect(() => {
    if (!interactive || !onRotate) return;
    const el = ref.current;
    const onWheel = (e) => {
      e.preventDefault();
      const delta = Math.sign(e.deltaY) * 12; // 12° per notch
      onRotate(delta);
    };
    el?.addEventListener('wheel', onWheel, { passive: false });
    return () => el?.removeEventListener('wheel', onWheel);
  }, [interactive, onRotate]);

  // bezel ring style
  const bezelBorder =
    bezel === 'schematic' ? `1.5px solid ${theme.line}`
    : bezel === 'dashed'   ? `1.5px dashed ${theme.line}`
    : `2px solid ${theme.bezel}`;
  const bezelBg =
    bezel === 'solid' ? theme.bezel : 'transparent';

  // tick marks around bezel (40 ticks every 9°)
  const ticks = Array.from({ length: 40 }).map((_, i) => i * 9);

  return (
    <div style={{ display: 'inline-block', textAlign: 'center', ...style }}>
      <div
        ref={ref}
        style={{
          width: size, height: size, position: 'relative',
          borderRadius: '50%',
          background: bezelBg,
          border: bezelBorder,
          boxShadow: bezel === 'solid' ? '0 20px 40px -16px rgba(0,0,0,0.6), inset 0 0 0 4px rgba(255,255,255,0.04)' : 'none',
          cursor: interactive ? 'grab' : 'default',
          userSelect: 'none',
        }}
        onPointerDown={interactive ? (e) => {
          // bezel-press detection: if pointer is in ring (not center)
          const rect = ref.current.getBoundingClientRect();
          const cx = rect.left + rect.width / 2;
          const cy = rect.top + rect.height / 2;
          const dx = e.clientX - cx, dy = e.clientY - cy;
          const r = Math.hypot(dx, dy);
          const inScreen = r < screenSize / 2;
          const handler = inScreen ? onScreenTap : onKnobPress;
          const longHandler = inScreen ? onScreenHold : onKnobLongPress;
          pressTimer.current = { down: Date.now(), inScreen };
          if (longHandler) {
            holdTimer.current = setTimeout(() => {
              longHandler();
              pressTimer.current = null;
            }, 600);
          }
        } : undefined}
        onPointerUp={interactive ? () => {
          if (holdTimer.current) clearTimeout(holdTimer.current);
          holdTimer.current = null;
          if (!pressTimer.current) return;
          const { inScreen } = pressTimer.current;
          const handler = inScreen ? onScreenTap : onKnobPress;
          handler && handler();
          pressTimer.current = null;
        } : undefined}
        onPointerLeave={() => { if (holdTimer.current) { clearTimeout(holdTimer.current); holdTimer.current = null; } pressTimer.current = null; }}
      >
        {/* tick marks (rotate with knob) */}
        <div style={{
          position: 'absolute', inset: 0,
          transform: `rotate(${rotation}deg)`,
          transition: 'transform .25s cubic-bezier(.4,0,.2,1)',
        }}>
          {ticks.map((deg, i) => {
            const major = i % 5 === 0;
            return (
              <div key={i} style={{
                position: 'absolute', left: '50%', top: '50%',
                width: 0, height: 0,
                transform: `rotate(${deg}deg) translateY(-${size/2 - 4}px)`,
                transformOrigin: '0 0',
              }}>
                <div style={{
                  width: major ? 2 : 1,
                  height: major ? 8 : 4,
                  background: major ? theme.dim : theme.faint,
                  marginLeft: -0.5,
                }}/>
              </div>
            );
          })}
          {/* knob orientation indicator (sits on rotated ring) */}
          <div style={{
            position: 'absolute', left: '50%', top: 6,
            width: 4, height: 14, marginLeft: -2,
            background: theme.accent, borderRadius: 2,
          }}/>
        </div>

        {/* the round screen */}
        <div style={{
          position: 'absolute',
          left: padding, top: padding,
          width: screenSize, height: screenSize,
          borderRadius: '50%',
          overflow: 'hidden',
          background: theme.screenBg,
          // simulate the inner bezel
          boxShadow: `inset 0 0 0 1px ${theme.line}, 0 0 0 4px ${theme.bg}`,
        }}>
          {/* logical screen at 240x240 — scale up to fit */}
          <div style={{
            width: SCREEN_PX, height: SCREEN_PX,
            transform: `scale(${screenSize / SCREEN_PX})`,
            transformOrigin: '0 0',
            position: 'relative',
            color: theme.ink,
            fontFamily: 'Inter, system-ui, sans-serif',
          }}>
            {children}
          </div>
        </div>
      </div>
      {label && <div style={{
        marginTop: 12,
        fontFamily: 'ui-monospace, monospace',
        fontSize: 11, color: theme.dim,
        letterSpacing: '0.18em', textTransform: 'uppercase',
      }}>{label}</div>}
      {hint && <div style={{
        marginTop: 4, fontSize: 11, color: theme.faint,
        fontStyle: 'italic',
      }}>{hint}</div>}
    </div>
  );
}

// ── Round-screen helper: arc of any sweep + arc-text + circular progress ──
// Build an SVG arc path on a 240×240 canvas
function arcPath(cx, cy, r, startDeg, endDeg) {
  const toRad = d => (d - 90) * Math.PI / 180;
  const sx = cx + r * Math.cos(toRad(startDeg));
  const sy = cy + r * Math.sin(toRad(startDeg));
  const ex = cx + r * Math.cos(toRad(endDeg));
  const ey = cy + r * Math.sin(toRad(endDeg));
  const large = Math.abs(endDeg - startDeg) > 180 ? 1 : 0;
  const sweep = endDeg > startDeg ? 1 : 0;
  return `M ${sx} ${sy} A ${r} ${r} 0 ${large} ${sweep} ${ex} ${ey}`;
}

// Cat in a round screen — sized for 240px circle
function ScreenCat({ slug, size = 130, opacity = 1, y = 110, theme }) {
  return (
    <img src={`cats4/${slug}-white.png`}
         style={{
           position: 'absolute', left: '50%', top: y,
           transform: `translate(-50%, -50%)`,
           width: size, height: size, objectFit: 'contain',
           opacity,
           // tint cats with theme.ink color via filter when not using white-png
         }}/>
  );
}

// ── SCREENS ──────────────────────────────────────────────────────
// Each screen is a function returning JSX inside a 240×240 box.

// 01 IDLE / time + cat
function ScrIdle({ theme, mood = 'neutral', mapping }) {
  const slug = mapping[mood] || mapping.neutral;
  return (
    <div style={{ width: SCREEN_PX, height: SCREEN_PX, position: 'relative' }}>
      {/* ambient cat */}
      <img src={`cats4/${slug}-white.png`} style={{
        position: 'absolute', left: '50%', top: '58%',
        transform: 'translate(-50%, -50%)',
        width: 120, height: 120, objectFit: 'contain',
        opacity: 0.85,
      }}/>
      {/* time */}
      <div style={{
        position: 'absolute', left: 0, right: 0, top: 36, textAlign: 'center',
        fontFamily: 'Georgia, serif', fontSize: 38, letterSpacing: '-1px',
        color: theme.ink,
      }}>7:42</div>
      <div style={{
        position: 'absolute', left: 0, right: 0, top: 80, textAlign: 'center',
        fontSize: 11, color: theme.dim, letterSpacing: '0.18em', textTransform: 'uppercase',
      }}>tue · day 12</div>
      {/* status arc bottom */}
      <div style={{
        position: 'absolute', left: 0, right: 0, bottom: 22, textAlign: 'center',
        fontSize: 12, color: theme.dim,
      }}>next · 13:00 lunch</div>
    </div>
  );
}

// 02 MENU (rotate to highlight)
function ScrMenu({ theme, selected = 0 }) {
  const items = [
    { icon: 'IcBowl',  label: 'Feed' },
    { icon: 'IcClock', label: 'Schedule' },
    { icon: 'IcMoon',  label: 'Quiet' },
    { icon: 'IcGear',  label: 'Settings' },
  ];
  // arrange around a circle
  const R = 70;
  return (
    <div style={{ width: SCREEN_PX, height: SCREEN_PX, position: 'relative' }}>
      {/* center label */}
      <div style={{
        position: 'absolute', left: 0, right: 0, top: 96, textAlign: 'center',
        fontFamily: 'Georgia, serif', fontSize: 22, color: theme.ink,
      }}>{items[selected].label}</div>
      <div style={{
        position: 'absolute', left: 0, right: 0, top: 128, textAlign: 'center',
        fontSize: 10, color: theme.faint, letterSpacing: '0.18em', textTransform: 'uppercase',
      }}>press to open</div>
      {/* orbiting items */}
      {items.map((it, i) => {
        const ang = -90 + (360 / items.length) * i;
        const rad = ang * Math.PI / 180;
        const x = 120 + R * Math.cos(rad);
        const y = 120 + R * Math.sin(rad);
        const Icon = window[it.icon];
        const isSel = i === selected;
        return (
          <div key={i} style={{
            position: 'absolute',
            left: x - 18, top: y - 18,
            width: 36, height: 36, borderRadius: 999,
            border: `1.5px solid ${isSel ? theme.accent : theme.line}`,
            background: isSel ? theme.accent : 'transparent',
            color: isSel ? theme.bg : theme.ink,
            display: 'flex', alignItems: 'center', justifyContent: 'center',
            transition: 'all .2s',
          }}>
            <Icon size={18}/>
          </div>
        );
      })}
    </div>
  );
}

// 03 FEED CONFIRM (cat hero + portion arc)
function ScrFeedConfirm({ theme, mapping, portion = 40 }) {
  const pct = Math.min(1, portion / 60);
  const sweep = pct * 270; // 270° max
  return (
    <div style={{ width: SCREEN_PX, height: SCREEN_PX, position: 'relative' }}>
      {/* portion arc */}
      <svg width={SCREEN_PX} height={SCREEN_PX} style={{ position:'absolute', inset:0 }}>
        <path d={arcPath(120, 120, 105, -135, 135)} stroke={theme.line} strokeWidth="3" fill="none"/>
        <path d={arcPath(120, 120, 105, -135, -135 + sweep)} stroke={theme.accent} strokeWidth="4" fill="none" strokeLinecap="round"/>
      </svg>
      {/* hungry cat */}
      <img src={`cats4/${mapping.hungry}-white.png`} style={{
        position: 'absolute', left: '50%', top: 58,
        transform: 'translateX(-50%)',
        width: 88, height: 88, objectFit: 'contain', opacity: 0.95,
      }}/>
      {/* portion text */}
      <div style={{
        position: 'absolute', left: 0, right: 0, top: 152, textAlign: 'center',
        fontFamily: 'Georgia, serif', fontSize: 30, color: theme.ink, letterSpacing: '-0.5px',
      }}>{portion}<span style={{ fontSize: 14, color: theme.dim, marginLeft: 4 }}>g</span></div>
      <div style={{
        position: 'absolute', left: 0, right: 0, top: 186, textAlign: 'center',
        fontSize: 10, color: theme.faint, letterSpacing: '0.16em', textTransform: 'uppercase',
      }}>turn · adjust · press · pour</div>
    </div>
  );
}

// 04 POURING (animated dots)
function ScrPouring({ theme, mapping, progress = 0.5 }) {
  const sweep = progress * 360;
  return (
    <div style={{ width: SCREEN_PX, height: SCREEN_PX, position: 'relative' }}>
      <svg width={SCREEN_PX} height={SCREEN_PX} style={{ position:'absolute', inset:0 }}>
        <circle cx="120" cy="120" r="105" stroke={theme.line} strokeWidth="2" fill="none"/>
        <path d={arcPath(120, 120, 105, 0, Math.max(0.1, sweep))} stroke={theme.accent} strokeWidth="3" fill="none" strokeLinecap="round" transform="rotate(-90 120 120)"/>
      </svg>
      <img src={`cats4/${mapping.neutral}-white.png`} style={{
        position: 'absolute', left: '50%', top: 50,
        transform: 'translateX(-50%)',
        width: 80, height: 80, objectFit: 'contain', opacity: 0.5,
      }}/>
      <div style={{
        position: 'absolute', left: 0, right: 0, top: 138, textAlign: 'center',
        fontFamily: 'Georgia, serif', fontSize: 22, color: theme.ink,
      }}>Pouring</div>
      <div style={{
        position: 'absolute', left: 0, right: 0, top: 170, textAlign: 'center',
        fontSize: 12, color: theme.dim,
      }}>{Math.round(progress * 40)} g of 40</div>
      <div style={{
        position: 'absolute', left: 0, right: 0, top: 192, textAlign: 'center',
        fontSize: 9, color: theme.faint, letterSpacing: '0.16em', textTransform: 'uppercase',
      }}>hold · cancel</div>
    </div>
  );
}

// 05 FED (success)
function ScrFed({ theme, mapping }) {
  return (
    <div style={{ width: SCREEN_PX, height: SCREEN_PX, position: 'relative' }}>
      <img src={`cats4/${mapping.fed}-white.png`} style={{
        position: 'absolute', left: '50%', top: '52%',
        transform: 'translate(-50%, -50%)',
        width: 130, height: 130, objectFit: 'contain',
      }}/>
      {/* heart marker */}
      <div style={{
        position: 'absolute', right: 60, top: 56,
        color: theme.accent, display: 'flex', alignItems: 'center', justifyContent: 'center',
        width: 26, height: 26,
      }}>
        <window.IcHeart size={20} stroke={2}/>
      </div>
      <div style={{
        position: 'absolute', left: 0, right: 0, bottom: 38, textAlign: 'center',
        fontFamily: 'Georgia, serif', fontSize: 22, color: theme.ink,
      }}>Mochi is fed</div>
      <div style={{
        position: 'absolute', left: 0, right: 0, bottom: 18, textAlign: 'center',
        fontSize: 11, color: theme.dim,
      }}>40 g · next 13:00</div>
    </div>
  );
}

// 06 SCHEDULE (4 slots arranged at 12,3,6,9 o'clock around perimeter)
function ScrSchedule({ theme, mapping, now = 2 }) {
  const slots = [
    { time: '07', label: 'Breakfast', mood: 'fed', done: true },
    { time: '13', label: 'Lunch', mood: 'fed', done: true },
    { time: '18', label: 'Dinner', mood: 'hungry', current: true },
    { time: '22', label: 'Treat', mood: 'sleepy' },
  ];
  const angles = [-90, 0, 90, 180];
  return (
    <div style={{ width: SCREEN_PX, height: SCREEN_PX, position: 'relative' }}>
      {/* center */}
      <div style={{
        position: 'absolute', left: 0, right: 0, top: 96, textAlign: 'center',
        fontFamily: 'Georgia, serif', fontSize: 28, color: theme.ink,
      }}>{slots[now].time}<span style={{ fontSize: 16, color: theme.dim }}>:00</span></div>
      <div style={{
        position: 'absolute', left: 0, right: 0, top: 130, textAlign: 'center',
        fontSize: 11, color: theme.dim, letterSpacing: '0.14em', textTransform: 'uppercase',
      }}>{slots[now].label}</div>
      {/* slots around */}
      {slots.map((s, i) => {
        const ang = angles[i];
        const rad = ang * Math.PI / 180;
        const x = 120 + 88 * Math.cos(rad);
        const y = 120 + 88 * Math.sin(rad);
        const isNow = i === now;
        return (
          <div key={i} style={{
            position: 'absolute',
            left: x - 22, top: y - 22,
            width: 44, height: 44, borderRadius: 999,
            border: `1.5px solid ${isNow ? theme.accent : theme.line}`,
            background: s.done ? theme.accent : 'transparent',
            color: s.done ? theme.bg : theme.ink,
            display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center',
            fontSize: 11,
            fontFamily: 'ui-monospace, monospace',
          }}>
            <div style={{ fontWeight: 700, fontSize: 12 }}>{s.time}</div>
            <div style={{ fontSize: 8, opacity: 0.7, marginTop: -2 }}>{s.done ? '✓' : isNow ? 'now' : ''}</div>
          </div>
        );
      })}
    </div>
  );
}

// 07 QUIET (moon + 24h ring with quiet wedge highlighted)
function ScrQuiet({ theme, mapping }) {
  // 22→06 quiet → angles 22*15-90=240° to 06*15-90=0° going CW
  const quietStart = (22 * 15) - 90;
  const quietEnd = (30 * 15) - 90; // 06:00 next day (30h)
  return (
    <div style={{ width: SCREEN_PX, height: SCREEN_PX, position: 'relative' }}>
      <svg width={SCREEN_PX} height={SCREEN_PX} style={{ position:'absolute', inset: 0 }}>
        <circle cx="120" cy="120" r="105" stroke={theme.line} strokeWidth="2" fill="none"/>
        <path d={arcPath(120, 120, 105, quietStart, quietEnd)} stroke={theme.accent} strokeWidth="6" fill="none" strokeLinecap="butt"/>
        {/* now marker at 22:14 */}
        <g transform={`rotate(${(22.23*15) - 90} 120 120)`}>
          <line x1="120" y1="20" x2="120" y2="36" stroke={theme.ink} strokeWidth="2"/>
          <circle cx="120" cy="22" r="3" fill={theme.ink}/>
        </g>
      </svg>
      {/* center moon icon + label */}
      <div style={{
        position: 'absolute', left: '50%', top: 78,
        transform: 'translateX(-50%)',
        color: theme.ink,
      }}>
        <window.IcMoon size={36} stroke={1.5}/>
      </div>
      <div style={{
        position: 'absolute', left: 0, right: 0, top: 124, textAlign: 'center',
        fontFamily: 'Georgia, serif', fontSize: 22, color: theme.ink,
      }}>Quiet hours</div>
      <div style={{
        position: 'absolute', left: 0, right: 0, top: 154, textAlign: 'center',
        fontSize: 12, color: theme.dim,
      }}>22:00 – 06:30</div>
      <div style={{
        position: 'absolute', left: 0, right: 0, top: 178, textAlign: 'center',
        fontSize: 10, color: theme.faint, letterSpacing: '0.14em', textTransform: 'uppercase',
      }}>auto-feeder paused</div>
    </div>
  );
}

// 08 PORTION ADJUST (knob rotation visualized)
function ScrPortionAdjust({ theme, portion = 35 }) {
  return (
    <div style={{ width: SCREEN_PX, height: SCREEN_PX, position: 'relative' }}>
      {/* big portion */}
      <div style={{
        position: 'absolute', left: 0, right: 0, top: 70, textAlign: 'center',
        fontFamily: 'Georgia, serif', fontSize: 64, color: theme.ink, letterSpacing: '-2px',
        lineHeight: 1,
      }}>{portion}</div>
      <div style={{
        position: 'absolute', left: 0, right: 0, top: 138, textAlign: 'center',
        fontSize: 12, color: theme.dim, letterSpacing: '0.18em', textTransform: 'uppercase',
      }}>grams</div>
      {/* − / + at left and right */}
      <div style={{
        position: 'absolute', left: 30, top: 110, color: theme.faint,
      }}><window.IcRotateCCW size={22}/></div>
      <div style={{
        position: 'absolute', right: 30, top: 110, color: theme.faint,
      }}><window.IcRotateCW size={22}/></div>
      <div style={{
        position: 'absolute', left: 0, right: 0, bottom: 36, textAlign: 'center',
        fontSize: 10, color: theme.faint, letterSpacing: '0.16em', textTransform: 'uppercase',
      }}>tap · save</div>
      {/* tick marks bottom showing 0..60 with current marker */}
      <div style={{ position: 'absolute', left: 28, right: 28, bottom: 56, height: 16 }}>
        {Array.from({length:13}).map((_,i)=>(
          <div key={i} style={{
            position:'absolute', left:`${i*100/12}%`, top:0,
            width:1, height: i%3===0?10:5, background: theme.faint,
          }}/>
        ))}
        <div style={{
          position:'absolute', left:`${portion/60*100}%`, top:-2,
          width:2, height:14, background: theme.accent, marginLeft:-1,
        }}/>
      </div>
    </div>
  );
}

// 09 LOCKED / long-press confirm
function ScrLockConfirm({ theme, progress = 0.7 }) {
  return (
    <div style={{ width: SCREEN_PX, height: SCREEN_PX, position: 'relative' }}>
      <svg width={SCREEN_PX} height={SCREEN_PX} style={{ position:'absolute', inset: 0 }}>
        <circle cx="120" cy="120" r="100" stroke={theme.line} strokeWidth="3" fill="none"/>
        <path d={arcPath(120, 120, 100, -90, -90 + progress*360)} stroke={theme.accent} strokeWidth="5" fill="none" strokeLinecap="round"/>
      </svg>
      <div style={{
        position: 'absolute', left: '50%', top: 78,
        transform: 'translateX(-50%)',
        color: theme.accent,
      }}>
        <window.IcHold size={40} stroke={1.5}/>
      </div>
      <div style={{
        position: 'absolute', left: 0, right: 0, top: 130, textAlign: 'center',
        fontFamily: 'Georgia, serif', fontSize: 20, color: theme.ink,
      }}>Hold to confirm</div>
      <div style={{
        position: 'absolute', left: 0, right: 0, top: 158, textAlign: 'center',
        fontSize: 11, color: theme.dim,
      }}>cancel today's schedule</div>
    </div>
  );
}

// 10 SETTINGS (list, rotate to scroll)
function ScrSettings({ theme, idx = 1 }) {
  const items = [
    { i: 'IcWifi', label: 'Wi-Fi', val: 'kitchen' },
    { i: 'IcSun', label: 'Wake', val: '06:30' },
    { i: 'IcMoon', label: 'Quiet', val: 'on' },
    { i: 'IcGear', label: 'Calibrate', val: '' },
  ];
  return (
    <div style={{ width: SCREEN_PX, height: SCREEN_PX, position: 'relative' }}>
      {/* center 3 items, fade outer */}
      {items.map((it, i) => {
        const offset = i - idx;
        const Icon = window[it.i];
        const op = Math.max(0, 1 - Math.abs(offset) * 0.45);
        const y = 120 + offset * 38;
        const isSel = offset === 0;
        return (
          <div key={i} style={{
            position: 'absolute', left: 30, right: 30, top: y - 16, height: 32,
            display: 'flex', alignItems: 'center', gap: 12,
            opacity: op,
            color: isSel ? theme.ink : theme.dim,
          }}>
            <div style={{
              width: 28, height: 28, borderRadius: 999,
              border: `1.5px solid ${isSel ? theme.accent : theme.line}`,
              display: 'flex', alignItems: 'center', justifyContent: 'center',
            }}><Icon size={14}/></div>
            <div style={{ flex: 1, fontSize: 14, fontFamily: 'Georgia, serif' }}>{it.label}</div>
            <div style={{ fontSize: 11, color: theme.dim }}>{it.val}</div>
          </div>
        );
      })}
      {/* selection arc on left */}
      <svg width={SCREEN_PX} height={SCREEN_PX} style={{ position:'absolute', inset:0, pointerEvents:'none' }}>
        <path d={arcPath(120, 120, 110, 200, 340)} stroke={theme.accent} strokeWidth="2" fill="none"/>
      </svg>
    </div>
  );
}

// 11 LEVEL / hopper stats
function ScrHopper({ theme, pct = 0.32 }) {
  return (
    <div style={{ width: SCREEN_PX, height: SCREEN_PX, position: 'relative' }}>
      <svg width={SCREEN_PX} height={SCREEN_PX} style={{ position:'absolute', inset: 0 }}>
        <circle cx="120" cy="120" r="100" stroke={theme.line} strokeWidth="3" fill="none"/>
        <path d={arcPath(120, 120, 100, -135, -135 + 270*pct)} stroke={theme.accent} strokeWidth="6" fill="none" strokeLinecap="round"/>
      </svg>
      <div style={{
        position: 'absolute', left: 0, right: 0, top: 84, textAlign: 'center',
        fontFamily: 'Georgia, serif', fontSize: 44, color: theme.ink, letterSpacing: '-1.5px',
      }}>{Math.round(pct*100)}<span style={{ fontSize: 22, color: theme.dim }}>%</span></div>
      <div style={{
        position: 'absolute', left: 0, right: 0, top: 138, textAlign: 'center',
        fontSize: 11, color: theme.dim, letterSpacing: '0.18em', textTransform: 'uppercase',
      }}>hopper</div>
      <div style={{
        position: 'absolute', left: 0, right: 0, top: 162, textAlign: 'center',
        fontSize: 11, color: theme.faint,
      }}>≈ 3 days remaining</div>
    </div>
  );
}

// 12 BOOT / splash
function ScrBoot({ theme, mapping }) {
  return (
    <div style={{ width: SCREEN_PX, height: SCREEN_PX, position: 'relative',
      display:'flex', alignItems:'center', justifyContent:'center', flexDirection:'column' }}>
      <img src={`cats4/${mapping.neutral}-white.png`} style={{
        width: 90, height: 90, objectFit: 'contain', marginBottom: 6, opacity: 0.9,
      }}/>
      <div style={{
        fontFamily: 'Georgia, serif', fontSize: 24, color: theme.ink, letterSpacing: '-0.5px',
      }}>FeedMe</div>
      <div style={{ marginTop: 14, display:'flex', gap: 6 }}>
        {[0,1,2].map(i=>(
          <div key={i} style={{
            width: 5, height: 5, borderRadius: 999, background: theme.accent,
            opacity: 0.3 + (i===1?0.7:0.3),
          }}/>
        ))}
      </div>
    </div>
  );
}

// expose
Object.assign(window, {
  KnobBezel, THEMES, SCREEN_PX,
  ScrIdle, ScrMenu, ScrFeedConfirm, ScrPouring, ScrFed,
  ScrSchedule, ScrQuiet, ScrPortionAdjust, ScrLockConfirm,
  ScrSettings, ScrHopper, ScrBoot,
});
