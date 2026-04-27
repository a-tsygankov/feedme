import { useState, useEffect } from "react";

const C = {
  green:    "#4ade80",
  yellow:   "#facc15",
  orange:   "#fb923c",
  red:      "#f87171",
  bg:       "#0f0f14",
  surface:  "#1a1a24",
  border:   "rgba(255,255,255,0.08)",
  text:     "#f0f0f5",
  muted:    "#6b6b80",
  accent:   "#818cf8",
  catLine:  "#1c1c1c",
  catWhite: "#f4f4ef",
  catPink:  "#ffb3c1",
};

function formatAgo(m) {
  if (m < 60) return `${m}m ago`;
  const h = Math.floor(m / 60), r = m % 60;
  return r ? `${h}h ${r}m ago` : `${h}h ago`;
}

// ── Arc ring ──────────────────────────────────────────────────────────────
function ArcRing({ progress, color, size = 200, sw = 9 }) {
  const r = (size - sw) / 2, cx = size / 2, cy = size / 2;
  const circ = 2 * Math.PI * r;
  return (
    <svg width={size} height={size} style={{ position: "absolute", top: 0, left: 0 }}>
      <circle cx={cx} cy={cy} r={r} fill="none" stroke="rgba(255,255,255,0.06)" strokeWidth={sw}/>
      <circle cx={cx} cy={cy} r={r} fill="none" stroke={color} strokeWidth={sw}
        strokeLinecap="round"
        strokeDasharray={circ}
        strokeDashoffset={circ * (1 - progress)}
        transform={`rotate(-90 ${cx} ${cy})`}
        style={{ transition: "stroke-dashoffset 0.8s ease, stroke 0.8s ease",
                 filter: `drop-shadow(0 0 6px ${color})` }}/>
    </svg>
  );
}

// ── Simon's Cat style SVG character ──────────────────────────────────────
// Viewbox 0 0 120 120. Big round head (cx60,cy50,r32), chunky outlines,
// expressive eyes, ears with pink inner, classic whiskers.
const SW = 2.6;   // stroke weight
const W  = C.catWhite;
const BL = C.catLine;
const PK = C.catPink;

// Shared body parts rendered UNDER the head layer
function Body({ extra }) {
  return (
    <>
      {/* tail */}
      <path d="M76 100 C96 88 100 70 88 64" fill="none" stroke={BL} strokeWidth={SW} strokeLinecap="round"/>
      {/* body */}
      <ellipse cx={60} cy={97} rx={19} ry={14} fill={W} stroke={BL} strokeWidth={SW}/>
      {/* front paws */}
      <ellipse cx={49} cy={109} rx={9} ry={5.5} fill={W} stroke={BL} strokeWidth={SW}/>
      <ellipse cx={71} cy={109} rx={9} ry={5.5} fill={W} stroke={BL} strokeWidth={SW}/>
      {extra}
    </>
  );
}

// Head shell — drawn on top of body
function Head() {
  return (
    <>
      {/* left ear outer */}
      <polygon points="29,27 24,8 46,22" fill={W} stroke={BL} strokeWidth={SW} strokeLinejoin="round"/>
      {/* left ear inner */}
      <polygon points="32,25 28,12 43,22" fill={PK} stroke="none"/>
      {/* right ear outer */}
      <polygon points="91,27 96,8 74,22" fill={W} stroke={BL} strokeWidth={SW} strokeLinejoin="round"/>
      {/* right ear inner */}
      <polygon points="88,25 92,12 77,22" fill={PK} stroke="none"/>
      {/* head circle */}
      <circle cx={60} cy={50} r={33} fill={W} stroke={BL} strokeWidth={SW}/>
      {/* whiskers left */}
      <line x1={27} y1={50} x2={46} y2={52} stroke={BL} strokeWidth={1.4} strokeLinecap="round"/>
      <line x1={27} y1={56} x2={46} y2={56} stroke={BL} strokeWidth={1.4} strokeLinecap="round"/>
      <line x1={27} y1={62} x2={46} y2={60} stroke={BL} strokeWidth={1.4} strokeLinecap="round"/>
      {/* whiskers right */}
      <line x1={93} y1={50} x2={74} y2={52} stroke={BL} strokeWidth={1.4} strokeLinecap="round"/>
      <line x1={93} y1={56} x2={74} y2={56} stroke={BL} strokeWidth={1.4} strokeLinecap="round"/>
      <line x1={93} y1={62} x2={74} y2={60} stroke={BL} strokeWidth={1.4} strokeLinecap="round"/>
      {/* nose — small inverted triangle */}
      <path d="M57,57 L60,61 L63,57 Z" fill={PK} stroke={BL} strokeWidth={1.2} strokeLinejoin="round"/>
    </>
  );
}

// ── HAPPY — big bright eyes, wide smile ──────────────────────────────────
function CatHappy() {
  return (
    <g>
      <Body/>
      <Head/>
      {/* eyes: full round, dark with shine */}
      <ellipse cx={47} cy={44} rx={10} ry={11} fill={BL}/>
      <ellipse cx={73} cy={44} rx={10} ry={11} fill={BL}/>
      {/* primary shine */}
      <ellipse cx={51} cy={40} rx={3.8} ry={4.2} fill={W}/>
      <ellipse cx={77} cy={40} rx={3.8} ry={4.2} fill={W}/>
      {/* secondary micro shine */}
      <circle cx={44} cy={49} r={1.5} fill={W} opacity={0.7}/>
      <circle cx={70} cy={49} r={1.5} fill={W} opacity={0.7}/>
      {/* wide happy smile */}
      <path d="M48,64 Q60,76 72,64" fill="none" stroke={BL} strokeWidth={SW} strokeLinecap="round"/>
      {/* blush cheeks */}
      <ellipse cx={38} cy={57} rx={6} ry={3.5} fill={PK} opacity={0.45}/>
      <ellipse cx={82} cy={57} rx={6} ry={3.5} fill={PK} opacity={0.45}/>
    </g>
  );
}

// ── NEUTRAL — half-lidded eyes, flat mouth ───────────────────────────────
function CatNeutral() {
  return (
    <g>
      <Body/>
      <Head/>
      {/* eyes base */}
      <ellipse cx={47} cy={46} rx={10} ry={11} fill={BL}/>
      <ellipse cx={73} cy={46} rx={10} ry={11} fill={BL}/>
      {/* upper eyelid mask */}
      <ellipse cx={47} cy={39} rx={11.5} ry={8} fill={W} stroke="none"/>
      <ellipse cx={73} cy={39} rx={11.5} ry={8} fill={W} stroke="none"/>
      {/* lid crease line */}
      <path d="M37,46 Q47,38 57,46" fill="none" stroke={BL} strokeWidth={SW} strokeLinecap="round"/>
      <path d="M63,46 Q73,38 83,46" fill="none" stroke={BL} strokeWidth={SW} strokeLinecap="round"/>
      {/* shine */}
      <ellipse cx={51} cy={47} rx={3} ry={3} fill={W}/>
      <ellipse cx={77} cy={47} rx={3} ry={3} fill={W}/>
      {/* flat mouth */}
      <line x1={52} y1={67} x2={68} y2={67} stroke={BL} strokeWidth={SW} strokeLinecap="round"/>
    </g>
  );
}

// ── HUNGRY — redesigned for maximum clarity at small sizes ───────────────
// Cat sits upright, one arm raised with index-finger-style paw CLEARLY
// pointing at its wide-open yowling mouth. Teary eyes, anguished brows.
// Everything kept well clear of the head circle so nothing gets clipped.
function CatHungry() {
  return (
    <g>
      {/* ── BODY (compact, sitting) ── */}
      {/* tail curling behind */}
      <path d="M74 104 C90 96 96 80 84 74" fill="none" stroke={BL} strokeWidth={SW} strokeLinecap="round"/>
      {/* torso */}
      <ellipse cx={60} cy={98} rx={17} ry={13} fill={W} stroke={BL} strokeWidth={SW}/>
      {/* right paw resting down */}
      <ellipse cx={71} cy={110} rx={9} ry={5} fill={W} stroke={BL} strokeWidth={SW}/>

      {/* ── RAISED LEFT ARM — clearly away from head ── */}
      {/* upper arm from body */}
      <path d="M48 88 C40 80 34 70 33 60"
        fill="none" stroke={BL} strokeWidth={SW+0.4} strokeLinecap="round"/>
      {/* forearm bends and aims finger UP toward mouth */}
      <path d="M33 60 C32 52 36 46 42 42"
        fill="none" stroke={BL} strokeWidth={SW+0.4} strokeLinecap="round"/>
      {/* paw pad — round mitt */}
      <ellipse cx={45} cy={39} rx={9} ry={7.5} fill={W} stroke={BL} strokeWidth={SW}/>
      {/* single raised "finger" / toe pointing clearly upward */}
      <ellipse cx={45} cy={29} rx={4} ry={5.5} fill={W} stroke={BL} strokeWidth={SW}/>
      {/* two side toe nubs */}
      <ellipse cx={38} cy={35} rx={3.5} ry={3} fill={W} stroke={BL} strokeWidth={1.8}/>
      <ellipse cx={52} cy={35} rx={3.5} ry={3} fill={W} stroke={BL} strokeWidth={1.8}/>

      {/* ── HEAD (shifted slightly right so arm reads clearly) ── */}
      {/* left ear */}
      <polygon points="51,24 47,8 64,20" fill={W} stroke={BL} strokeWidth={SW} strokeLinejoin="round"/>
      <polygon points="53,22 50,11 62,20" fill={PK} stroke="none"/>
      {/* right ear */}
      <polygon points="86,24 91,8 74,20" fill={W} stroke={BL} strokeWidth={SW} strokeLinejoin="round"/>
      <polygon points="84,22 88,11 76,20" fill={PK} stroke="none"/>
      {/* head — shifted right (cx=68) to clear the arm */}
      <circle cx={68} cy={50} r={31} fill={W} stroke={BL} strokeWidth={SW}/>
      {/* whiskers right side only (left side hidden by arm context) */}
      <line x1={99} y1={48} x2={82} y2={50} stroke={BL} strokeWidth={1.4} strokeLinecap="round"/>
      <line x1={99} y1={54} x2={82} y2={54} stroke={BL} strokeWidth={1.4} strokeLinecap="round"/>
      <line x1={99} y1={60} x2={82} y2={58} stroke={BL} strokeWidth={1.4} strokeLinecap="round"/>
      {/* whiskers left */}
      <line x1={37} y1={50} x2={54} y2={52} stroke={BL} strokeWidth={1.4} strokeLinecap="round" opacity={0.7}/>
      <line x1={37} y1={56} x2={54} y2={56} stroke={BL} strokeWidth={1.4} strokeLinecap="round" opacity={0.7}/>
      {/* nose */}
      <path d="M65,58 L68,62 L71,58 Z" fill={PK} stroke={BL} strokeWidth={1.2} strokeLinejoin="round"/>

      {/* ── FACE EXPRESSIONS — maximum desperation ── */}
      {/* anguished eyebrows — sharply angled inward */}
      <path d="M52,30 Q60,24 68,29" fill="none" stroke={BL} strokeWidth={2.4} strokeLinecap="round"/>
      <path d="M68,29 Q76,24 84,29" fill="none" stroke={BL} strokeWidth={2.4} strokeLinecap="round"/>

      {/* HUGE teary eyes */}
      <ellipse cx={58} cy={43} rx={11} ry={12} fill={BL}/>
      <ellipse cx={79} cy={43} rx={11} ry={12} fill={BL}/>
      {/* big square shine — the "glistening with tears" look */}
      <ellipse cx={63} cy={37} rx={5} ry={5.5} fill={W}/>
      <ellipse cx={84} cy={37} rx={5} ry={5.5} fill={W}/>
      <circle cx={54} cy={48} r={2} fill={W} opacity={0.7}/>
      <circle cx={75} cy={48} r={2} fill={W} opacity={0.7}/>
      {/* tear drops */}
      <path d="M54,55 Q53,60 55,62" fill="none" stroke="#93c5fd" strokeWidth={1.8} strokeLinecap="round" opacity={0.8}/>
      <path d="M75,55 Q74,60 76,62" fill="none" stroke="#93c5fd" strokeWidth={1.8} strokeLinecap="round" opacity={0.8}/>

      {/* WIDE OPEN yowling mouth — big D-shape */}
      <path d="M56,66 Q58,63 62,63 Q66,63 68,66 L67,74 Q64,79 60,79 Q56,79 53,74 Z"
        fill={BL} stroke={BL} strokeWidth={1.5} strokeLinejoin="round"/>
      {/* mouth interior pink */}
      <path d="M57,67 Q59,65 62,65 Q65,65 67,67 L66,73 Q63,77 60,77 Q57,77 54,73 Z"
        fill="#ff6b8a" stroke="none"/>
      {/* tongue */}
      <ellipse cx={61} cy={74} rx={4} ry={3} fill="#ff8fa3" stroke="none"/>
      {/* upper teeth nubs */}
      <rect x={59} y={64} width={4} height={3} rx={1.5} fill={W} stroke="none"/>

      {/* ── MOTION / URGENCY lines radiating from head ── */}
      <line x1={100} y1={20} x2={94} y2={26} stroke={BL} strokeWidth={1.5} strokeLinecap="round" opacity={0.5}/>
      <line x1={107} y1={32} x2={99} y2={35} stroke={BL} strokeWidth={1.5} strokeLinecap="round" opacity={0.5}/>
      <line x1={102} y1={45} x2={100} y2={52} stroke={BL} strokeWidth={1.5} strokeLinecap="round" opacity={0.4}/>
    </g>
  );
}

// ── FED / SATISFIED — ^_^ squint eyes, big grin ──────────────────────────
function CatFed() {
  return (
    <g>
      <Body/>
      <Head/>
      {/* squint-closed happy eyes: arc shapes ^_^ */}
      <path d="M38,45 Q47,36 57,45" fill="none" stroke={BL} strokeWidth={3} strokeLinecap="round"/>
      <path d="M63,45 Q72,36 82,45" fill="none" stroke={BL} strokeWidth={3} strokeLinecap="round"/>
      {/* big satisfied grin — filled */}
      <path d="M47,63 Q60,78 73,63" fill={BL} stroke={BL} strokeWidth={SW} strokeLinecap="round" strokeLinejoin="round"/>
      <path d="M47,63 Q60,72 73,63" fill={W} stroke="none"/>
      {/* rosy cheeks */}
      <ellipse cx={38} cy={58} rx={7} ry={4} fill={PK} opacity={0.55}/>
      <ellipse cx={82} cy={58} rx={7} ry={4} fill={PK} opacity={0.55}/>
      {/* floating heart */}
      <text x={85} y={26} fontSize={15} fill="#f87171" style={{ userSelect: "none" }}>♥</text>
    </g>
  );
}

// ── SLEEPY — droopy eyes, yawn, Zzz ──────────────────────────────────────
function CatSleepy() {
  return (
    <g>
      <Body/>
      <Head/>
      {/* sleepy droopy eyes */}
      <ellipse cx={47} cy={48} rx={10} ry={9} fill={BL}/>
      <ellipse cx={73} cy={48} rx={10} ry={9} fill={BL}/>
      {/* heavy top eyelid */}
      <ellipse cx={47} cy={41} rx={11.5} ry={8} fill={W} stroke="none"/>
      <ellipse cx={73} cy={41} rx={11.5} ry={8} fill={W} stroke="none"/>
      <path d="M37,48 Q47,41 57,48" fill="none" stroke={BL} strokeWidth={SW} strokeLinecap="round"/>
      <path d="M63,48 Q73,41 83,48" fill="none" stroke={BL} strokeWidth={SW} strokeLinecap="round"/>
      <ellipse cx={51} cy={49} rx={2.5} ry={2.5} fill={W}/>
      <ellipse cx={77} cy={49} rx={2.5} ry={2.5} fill={W}/>
      {/* sleepy mouth — small curve */}
      <path d="M54,66 Q60,70 66,66" fill="none" stroke={BL} strokeWidth={SW} strokeLinecap="round"/>
      {/* Zzz */}
      <text x={78} y={30} fontSize={10} fill={C.muted} fontWeight="700" fontFamily="sans-serif">z</text>
      <text x={84} y={21} fontSize={13} fill={C.muted} fontWeight="700" fontFamily="sans-serif">z</text>
      <text x={91} y={11} fontSize={16} fill={C.muted} fontWeight="700" fontFamily="sans-serif">Z</text>
    </g>
  );
}

function CatFace({ mood, size = 100 }) {
  const map = { happy: CatHappy, neutral: CatNeutral, hungry: CatHungry, fed: CatFed, sleepy: CatSleepy };
  const Face = map[mood] || CatHappy;
  // viewBox sized to show full character including raised arm
  return (
    <svg viewBox="0 0 120 118" width={size} height={size * 118 / 120} style={{ display: "block", overflow: "visible" }}>
      <Face/>
    </svg>
  );
}

// ── Meal dots ─────────────────────────────────────────────────────────────
function MealDots({ fed, total = 3 }) {
  return (
    <div style={{ display: "flex", gap: 5, justifyContent: "center" }}>
      {Array.from({ length: total }).map((_, i) => (
        <div key={i} style={{
          width: 7, height: 7, borderRadius: "50%",
          background: i < fed ? C.green : "rgba(255,255,255,0.12)",
          boxShadow: i < fed ? `0 0 5px ${C.green}` : "none",
          transition: "all 0.3s ease",
        }}/>
      ))}
    </div>
  );
}

// ── Device bezel ──────────────────────────────────────────────────────────
function DeviceFrame({ children, label, note, highlight }) {
  return (
    <div style={{ display: "flex", flexDirection: "column", alignItems: "center", gap: 10 }}>
      <div style={{
        width: 220, height: 220, borderRadius: "50%",
        background: "#111118",
        border: `3px solid ${highlight || "#2a2a38"}`,
        boxShadow: highlight
          ? `0 0 0 2px ${highlight}44, 0 10px 36px #00000099`
          : "0 8px 32px #00000099",
        position: "relative", overflow: "hidden",
        display: "flex", alignItems: "center", justifyContent: "center",
        transition: "box-shadow 0.4s ease, border-color 0.4s ease",
      }}>
        <div style={{
          width: 200, height: 200, borderRadius: "50%",
          background: C.bg, position: "relative", overflow: "hidden",
          display: "flex", alignItems: "center", justifyContent: "center",
        }}>
          {children}
        </div>
      </div>
      <div style={{ textAlign: "center" }}>
        <div style={{ color: C.text, fontWeight: 600, fontSize: 13 }}>{label}</div>
        {note && <div style={{ color: C.muted, fontSize: 11, marginTop: 2 }}>{note}</div>}
      </div>
    </div>
  );
}

// ── Screen variants ───────────────────────────────────────────────────────
function ScreenIdle({ minutesAgo, fedBy, mealsFed, mood, color }) {
  const progress = Math.max(0, 1 - minutesAgo / 300);
  return (
    <div style={{ position: "relative", width: 200, height: 200, display: "flex", alignItems: "center", justifyContent: "center" }}>
      <ArcRing progress={progress} color={color} size={200} sw={9}/>
      <div style={{ display: "flex", flexDirection: "column", alignItems: "center", gap: 0, zIndex: 1 }}>
        <CatFace mood={mood} size={98}/>
        <div style={{ color: C.text, fontWeight: 700, fontSize: 13, marginTop: -4 }}>{formatAgo(minutesAgo)}</div>
        <div style={{ color: C.muted, fontSize: 10 }}>by {fedBy}</div>
        <div style={{ marginTop: 5 }}><MealDots fed={mealsFed}/></div>
      </div>
    </div>
  );
}

function ScreenHungry({ minutesAgo, color }) {
  const [p, setP] = useState(false);
  useEffect(() => { const t = setInterval(() => setP(x => !x), 700); return () => clearInterval(t); }, []);
  const progress = Math.max(0, 1 - minutesAgo / 300);
  return (
    <div style={{ position: "relative", width: 200, height: 200, display: "flex", alignItems: "center", justifyContent: "center" }}>
      <ArcRing progress={progress} color={color} size={200} sw={9}/>
      <div style={{ display: "flex", flexDirection: "column", alignItems: "center", gap: 0, zIndex: 1 }}>
        <CatFace mood="hungry" size={98}/>
        <div style={{ color: C.red, fontWeight: 800, fontSize: 12, letterSpacing: "0.5px", marginTop: -4,
                      opacity: p ? 1 : 0.35, transition: "opacity 0.35s ease" }}>FEED ME!</div>
        <div style={{ color: C.muted, fontSize: 10 }}>{formatAgo(minutesAgo)}</div>
        <div style={{ marginTop: 5 }}><MealDots fed={1}/></div>
      </div>
    </div>
  );
}

function ScreenFedConfirm({ owner }) {
  const [sc, setSc] = useState(0.35);
  useEffect(() => { setTimeout(() => setSc(1), 60); }, []);
  return (
    <div style={{ display: "flex", flexDirection: "column", alignItems: "center", justifyContent: "center", height: 200, gap: 2 }}>
      <div style={{ transform: `scale(${sc})`, transition: "transform 0.5s cubic-bezier(0.34,1.56,0.64,1)" }}>
        <CatFace mood="fed" size={102}/>
      </div>
      <div style={{ color: C.green, fontWeight: 800, fontSize: 17, marginTop: -6 }}>Fed!</div>
      <div style={{ color: C.muted, fontSize: 11 }}>by {owner} · 12:34 PM</div>
      <div style={{ marginTop: 5 }}><MealDots fed={2}/></div>
    </div>
  );
}

function ScreenSnooze() {
  return (
    <div style={{ display: "flex", flexDirection: "column", alignItems: "center", justifyContent: "center", height: 200, gap: 2 }}>
      <CatFace mood="sleepy" size={100}/>
      <div style={{ color: C.yellow, fontWeight: 700, fontSize: 13, marginTop: -6 }}>Just begging</div>
      <div style={{ color: C.muted, fontSize: 10 }}>Snoozed 30 min · by Andrey</div>
    </div>
  );
}

function ScreenHistory() {
  const rows = [
    { t: "12:34", by: "Masha",  k: "feed" },
    { t: "09:10", by: "Andrey", k: "feed" },
    { t: "08:45", by: "Andrey", k: "snooze" },
    { t: "06:20", by: "Masha",  k: "feed" },
    { t: "Yesterday", by: "Masha", k: "feed" },
  ];
  return (
    <div style={{ display: "flex", flexDirection: "column", justifyContent: "center", height: 200, padding: "0 22px", gap: 8 }}>
      <div style={{ color: C.accent, fontWeight: 700, fontSize: 10, textAlign: "center", letterSpacing: "0.8px" }}>HISTORY</div>
      {rows.map((r, i) => (
        <div key={i} style={{ display: "flex", justifyContent: "space-between", alignItems: "center" }}>
          <div style={{ display: "flex", alignItems: "center", gap: 5 }}>
            <div style={{ width: 6, height: 6, borderRadius: "50%", background: r.k === "feed" ? C.green : C.yellow, flexShrink: 0 }}/>
            <span style={{ color: C.muted, fontSize: 10 }}>{r.t}</span>
          </div>
          <span style={{ color: C.text, fontSize: 10 }}>{r.by}</span>
          <span style={{ color: r.k === "feed" ? C.green : C.yellow, fontSize: 9 }}>{r.k}</span>
        </div>
      ))}
    </div>
  );
}

function ScreenSync() {
  const [angle, setAngle] = useState(0);
  useEffect(() => { const t = setInterval(() => setAngle(a => (a + 6) % 360), 50); return () => clearInterval(t); }, []);
  return (
    <div style={{ display: "flex", flexDirection: "column", alignItems: "center", justifyContent: "center", height: 200, gap: 10 }}>
      <div style={{ position: "relative", width: 72, height: 72, display: "flex", alignItems: "center", justifyContent: "center" }}>
        <svg width={72} height={72} style={{ position: "absolute", transform: `rotate(${angle}deg)` }}>
          <circle cx={36} cy={36} r={30} fill="none" stroke={C.accent} strokeWidth={3} strokeDasharray="46 140" strokeLinecap="round"/>
        </svg>
        <CatFace mood="neutral" size={54}/>
      </div>
      <div style={{ color: C.text, fontWeight: 600, fontSize: 12 }}>Syncing…</div>
      <div style={{ color: C.muted, fontSize: 10 }}>Masha just fed the cat</div>
    </div>
  );
}

function ScreenSetup({ step }) {
  const s = {
    name: (
      <div style={{ display: "flex", flexDirection: "column", alignItems: "center", gap: 6, padding: "0 16px" }}>
        <CatFace mood="neutral" size={58}/>
        <div style={{ color: C.text, fontWeight: 700, fontSize: 12, marginTop: -4 }}>Your name?</div>
        {["Masha", "Andrey", "Other"].map(n => (
          <div key={n} style={{
            background: n === "Masha" ? C.accent + "33" : "rgba(255,255,255,0.05)",
            border: `1px solid ${n === "Masha" ? C.accent : C.border}`,
            borderRadius: 20, padding: "3px 14px",
            color: n === "Masha" ? C.accent : C.muted, fontSize: 11,
          }}>{n}</div>
        ))}
      </div>
    ),
    threshold: (
      <div style={{ display: "flex", flexDirection: "column", alignItems: "center", gap: 7, padding: "0 12px" }}>
        <div style={{ fontSize: 22 }}>⏱</div>
        <div style={{ color: C.text, fontWeight: 700, fontSize: 12 }}>Hungry after?</div>
        {["3h", "4h", "5h"].map((v, i) => (
          <div key={v} style={{
            display: "flex", justifyContent: "space-between", alignItems: "center", width: "100%",
            background: i === 1 ? C.accent + "33" : "transparent",
            border: `1px solid ${i === 1 ? C.accent : C.border}`,
            borderRadius: 20, padding: "3px 14px",
            color: i === 1 ? C.accent : C.muted, fontSize: 11,
          }}>
            <span>{v}</span>{i === 1 && <span style={{ fontSize: 9 }}>default</span>}
          </div>
        ))}
      </div>
    ),
    wifi: (
      <div style={{ display: "flex", flexDirection: "column", alignItems: "center", gap: 10 }}>
        <div style={{ fontSize: 28 }}>📶</div>
        <div style={{ color: C.text, fontWeight: 700, fontSize: 12 }}>Connecting…</div>
        <div style={{ color: C.muted, fontSize: 10 }}>HomeNetwork_5G</div>
        <div style={{ width: 80, height: 3, background: "rgba(255,255,255,0.1)", borderRadius: 2, overflow: "hidden" }}>
          <div style={{ width: "60%", height: "100%", background: C.accent }}/>
        </div>
      </div>
    ),
    ready: (
      <div style={{ display: "flex", flexDirection: "column", alignItems: "center", gap: 4 }}>
        <CatFace mood="happy" size={84}/>
        <div style={{ color: C.green, fontWeight: 700, fontSize: 14, marginTop: -6 }}>Ready!</div>
        <div style={{ color: C.muted, fontSize: 9 }}>home-4a7f · stick to fridge</div>
      </div>
    ),
  };
  return <div style={{ display: "flex", alignItems: "center", justifyContent: "center", height: 200 }}>{s[step]}</div>;
}

// ── Layout ────────────────────────────────────────────────────────────────
function Section({ title, desc, children }) {
  return (
    <div style={{ marginBottom: 54 }}>
      <div style={{ color: C.accent, fontWeight: 700, fontSize: 10, letterSpacing: "2px", textTransform: "uppercase", marginBottom: 5 }}>{title}</div>
      <div style={{ color: C.muted, fontSize: 13, marginBottom: 26, maxWidth: 700, lineHeight: 1.5 }}>{desc}</div>
      <div style={{ display: "flex", flexWrap: "wrap", gap: 26, alignItems: "flex-start" }}>{children}</div>
    </div>
  );
}

function Arr() {
  return <div style={{ color: C.muted, fontSize: 22, paddingTop: 62, alignSelf: "flex-start" }}>→</div>;
}

// ── App ───────────────────────────────────────────────────────────────────
export default function App() {
  return (
    <div style={{
      minHeight: "100vh", background: C.bg,
      fontFamily: "'Inter', system-ui, sans-serif", color: C.text,
      padding: "44px 40px", maxWidth: 980, margin: "0 auto",
    }}>
      {/* header */}
      <div style={{ display: "flex", alignItems: "flex-start", gap: 18, marginBottom: 50 }}>
        <div style={{ flexShrink: 0, marginTop: 2 }}>
          <CatFace mood="hungry" size={72}/>
        </div>
        <div>
          <h1 style={{ margin: 0, fontSize: 26, fontWeight: 800, letterSpacing: "-0.8px" }}>Cat Feeder Tracker</h1>
          <div style={{ color: C.muted, fontSize: 13, marginTop: 2 }}>Waveshare ESP32-S3-LCD-1.28 · 240×240 round display</div>
          <div style={{
            marginTop: 10, background: C.surface, border: `1px solid ${C.border}`,
            borderRadius: 10, padding: "8px 14px", color: C.muted, fontSize: 12, lineHeight: 1.6, maxWidth: 560,
          }}>
            Each circle = the physical 1.28″ round screen. Ring and pulsing animations are live.
          </div>
        </div>
      </div>

      {/* W1 */}
      <Section title="Workflow 1 — At-a-Glance Status"
        desc="No interaction needed. Ring depletes over 5h. Color shifts green → yellow → orange → red. Cat's expression and mood shift to match.">
        <DeviceFrame label="Just fed" note="< 2h · green" highlight={C.green}>
          <ScreenIdle minutesAgo={25} fedBy="Masha" mealsFed={1} mood="happy" color={C.green}/>
        </DeviceFrame>
        <Arr/>
        <DeviceFrame label="Getting close" note="~2h · yellow" highlight={C.yellow}>
          <ScreenIdle minutesAgo={135} fedBy="Andrey" mealsFed={2} mood="neutral" color={C.yellow}/>
        </DeviceFrame>
        <Arr/>
        <DeviceFrame label="Feed soon" note="~3h · orange" highlight={C.orange}>
          <ScreenIdle minutesAgo={205} fedBy="Masha" mealsFed={2} mood="neutral" color={C.orange}/>
        </DeviceFrame>
        <Arr/>
        <DeviceFrame label="Overdue!" note="> 4h · red, pulsing" highlight={C.red}>
          <ScreenHungry minutesAgo={275} color={C.red}/>
        </DeviceFrame>
      </Section>

      {/* W2 */}
      <Section title="Workflow 2 — Logging a Meal"
        desc="Single tap logs a feeding. Confirmation splash shows satisfied cat, then returns to idle.">
        <DeviceFrame label="Cat is begging" note="hungry state">
          <ScreenHungry minutesAgo={280} color={C.red}/>
        </DeviceFrame>
        <Arr/>
        <DeviceFrame label="Tap!" note="single tap" highlight={C.accent}>
          <div style={{ display: "flex", flexDirection: "column", alignItems: "center", justifyContent: "center", height: 200, gap: 10 }}>
            <div style={{ fontSize: 40 }}>👆</div>
            <div style={{ color: C.accent, fontWeight: 700, fontSize: 13 }}>Single tap</div>
            <div style={{ color: C.muted, fontSize: 10 }}>to log feeding</div>
          </div>
        </DeviceFrame>
        <Arr/>
        <DeviceFrame label="Fed!" note="satisfied cat · 2s" highlight={C.green}>
          <ScreenFedConfirm owner="Masha"/>
        </DeviceFrame>
        <Arr/>
        <DeviceFrame label="Back to idle" note="ring resets · green" highlight={C.green}>
          <ScreenIdle minutesAgo={1} fedBy="Masha" mealsFed={2} mood="happy" color={C.green}/>
        </DeviceFrame>
      </Section>

      {/* W3 */}
      <Section title="Workflow 3 — Cat is Just Begging"
        desc="Long press (2s) snoozes the alert 30 min without logging. For when the cat is being dramatic.">
        <DeviceFrame label="Cat begging" note="red alert">
          <ScreenHungry minutesAgo={255} color={C.red}/>
        </DeviceFrame>
        <Arr/>
        <DeviceFrame label="Long press 2s" note="hold gesture" highlight={C.yellow}>
          <div style={{ display: "flex", flexDirection: "column", alignItems: "center", justifyContent: "center", height: 200, gap: 10 }}>
            <div style={{ fontSize: 38 }}>✋</div>
            <div style={{ color: C.yellow, fontWeight: 700, fontSize: 13 }}>Hold 2s</div>
            <div style={{ width: 80, height: 3, background: "rgba(255,255,255,0.1)", borderRadius: 2, overflow: "hidden" }}>
              <div style={{ width: "72%", height: "100%", background: C.yellow }}/>
            </div>
            <div style={{ color: C.muted, fontSize: 10 }}>just begging</div>
          </div>
        </DeviceFrame>
        <Arr/>
        <DeviceFrame label="Snoozed" note="sleepy cat · 30m" highlight={C.yellow}>
          <ScreenSnooze/>
        </DeviceFrame>
        <Arr/>
        <DeviceFrame label="Continues" note="still hungry" highlight={C.orange}>
          <ScreenIdle minutesAgo={260} fedBy="Andrey" mealsFed={1} mood="neutral" color={C.orange}/>
        </DeviceFrame>
      </Section>

      {/* W4 */}
      <Section title="Workflow 4 — Two-Person Sync"
        desc="Masha logs a feed. Andrey's unit syncs via Cloudflare within ~30s and shows identical state.">
        <div style={{ display: "flex", flexDirection: "column", alignItems: "center", gap: 6 }}>
          <div style={{ color: C.muted, fontSize: 11, marginBottom: 4 }}>Masha's fridge unit</div>
          <DeviceFrame label="Masha taps" note="logs feeding" highlight={C.green}>
            <ScreenFedConfirm owner="Masha"/>
          </DeviceFrame>
        </div>
        <div style={{ display: "flex", flexDirection: "column", alignItems: "center", justifyContent: "center", paddingTop: 52, gap: 4 }}>
          <div style={{ color: C.accent, fontSize: 11 }}>☁ sync</div>
          <div style={{ color: C.muted, fontSize: 22 }}>⇄</div>
          <div style={{ color: C.muted, fontSize: 9 }}>~30s</div>
        </div>
        <div style={{ display: "flex", flexDirection: "column", alignItems: "center", gap: 6 }}>
          <div style={{ color: C.muted, fontSize: 11, marginBottom: 4 }}>Andrey's fridge unit</div>
          <DeviceFrame label="Syncing…" note="receives update" highlight={C.accent}>
            <ScreenSync/>
          </DeviceFrame>
        </div>
        <Arr/>
        <div style={{ display: "flex", flexDirection: "column", alignItems: "center", gap: 6 }}>
          <div style={{ color: C.muted, fontSize: 11, marginBottom: 4 }}>Andrey's unit (after)</div>
          <DeviceFrame label="Synced!" note="both show same" highlight={C.green}>
            <ScreenIdle minutesAgo={1} fedBy="Masha" mealsFed={2} mood="happy" color={C.green}/>
          </DeviceFrame>
        </div>
      </Section>

      {/* W5 */}
      <Section title="Workflow 5 — Feeding History"
        desc="Double-tap shows last 5 events with time and name. Auto-returns to idle after 5s.">
        <DeviceFrame label="Idle" note="normal view">
          <ScreenIdle minutesAgo={55} fedBy="Masha" mealsFed={2} mood="happy" color={C.green}/>
        </DeviceFrame>
        <Arr/>
        <DeviceFrame label="Double-tap" note="quick double tap" highlight={C.accent}>
          <div style={{ display: "flex", flexDirection: "column", alignItems: "center", justifyContent: "center", height: 200, gap: 10 }}>
            <div style={{ fontSize: 34 }}>👆👆</div>
            <div style={{ color: C.accent, fontWeight: 700, fontSize: 13 }}>Double tap</div>
            <div style={{ color: C.muted, fontSize: 10 }}>view history</div>
          </div>
        </DeviceFrame>
        <Arr/>
        <DeviceFrame label="History" note="last 5 events" highlight={C.accent}>
          <ScreenHistory/>
        </DeviceFrame>
        <Arr/>
        <DeviceFrame label="Auto-return" note="5s inactivity">
          <ScreenIdle minutesAgo={56} fedBy="Masha" mealsFed={2} mood="happy" color={C.green}/>
        </DeviceFrame>
      </Section>

      {/* W6 */}
      <Section title="Workflow 6 — First-Time Setup"
        desc="Hold BOOT on power-on. Set name → hungry threshold → Wi-Fi → ready.">
        <DeviceFrame label="Name" note="choose owner" highlight={C.accent}>
          <ScreenSetup step="name"/>
        </DeviceFrame>
        <Arr/>
        <DeviceFrame label="Threshold" note="hungry timer" highlight={C.accent}>
          <ScreenSetup step="threshold"/>
        </DeviceFrame>
        <Arr/>
        <DeviceFrame label="Wi-Fi" note="connecting…" highlight={C.accent}>
          <ScreenSetup step="wifi"/>
        </DeviceFrame>
        <Arr/>
        <DeviceFrame label="Ready!" note="stick to fridge" highlight={C.green}>
          <ScreenSetup step="ready"/>
        </DeviceFrame>
      </Section>

      {/* legend */}
      <div style={{ borderTop: `1px solid ${C.border}`, paddingTop: 26 }}>
        <div style={{ color: C.muted, fontSize: 10, fontWeight: 600, letterSpacing: "1px", marginBottom: 14 }}>INTERACTION LEGEND</div>
        <div style={{ display: "flex", flexWrap: "wrap", gap: 22 }}>
          {[
            { i: "👆", l: "Single tap", d: "Log feeding" },
            { i: "✋", l: "Long press 2s", d: "Snooze — just begging" },
            { i: "👆👆", l: "Double tap", d: "View history" },
            { i: "🔘", l: "Hold BOOT", d: "Settings / setup" },
          ].map(x => (
            <div key={x.l} style={{ display: "flex", alignItems: "center", gap: 8 }}>
              <span style={{ fontSize: 16 }}>{x.i}</span>
              <div>
                <div style={{ color: C.text, fontSize: 11, fontWeight: 600 }}>{x.l}</div>
                <div style={{ color: C.muted, fontSize: 10 }}>{x.d}</div>
              </div>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
}
