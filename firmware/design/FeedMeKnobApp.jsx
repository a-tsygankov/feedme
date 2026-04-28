// FeedMeKnobApp.jsx — round-knob LVGL device workflow.
// Composes: KnobBezel + 12 screens + interactive hero + flow grid + Tweaks panel.

const { useState, useEffect, useMemo, useRef } = React;

// Mood→cat (mirrors phone FeedMe; persisted via EDITMODE block)
const KNOB_DEFAULTS = /*EDITMODE-BEGIN*/{
  "happy": "C2",
  "neutral": "B1",
  "hungry": "B2",
  "sleepy": "B3",
  "fed": "C4",
  "theme": "aubergine",
  "bezel": "schematic",
  "rotation": 0,
  "activeScreen": 2
}/*EDITMODE-END*/;

const ALL_CATS = ['A1','A2','A3','A4','B1','B2','B3','B4','C1','C2','C3','C4'];

// ── Annotation helpers (interaction labels around screens in the flow grid) ──
function GestureChip({ kind, label }) {
  const Icons = {
    rotate_cw: window.IcRotateCW, rotate_ccw: window.IcRotateCCW,
    press: window.IcTap, long_press: window.IcHold,
    tap: window.IcTap, double_tap: window.IcTap,
    long_tap: window.IcHold, drag: window.IcArrowRight,
  };
  const Icon = Icons[kind] || window.IcTap;
  return (
    <div style={{
      display: 'inline-flex', alignItems: 'center', gap: 6,
      padding: '5px 10px',
      borderRadius: 999,
      border: '1px solid rgba(246,241,230,0.18)',
      background: 'rgba(246,241,230,0.04)',
      color: 'rgba(246,241,230,0.78)',
      fontSize: 10, letterSpacing: '0.14em', textTransform: 'uppercase',
      fontFamily: 'ui-monospace, monospace',
    }}>
      <Icon size={12} stroke={1.6}/>
      <span>{label}</span>
    </div>
  );
}

// ── Flow card: bezel + step number + caption + gesture chips that lead to next ──
function FlowStep({ n, title, sub, theme, bezel, mapping, rotation = 0, gestures = [], children }) {
  return (
    <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 10 }}>
      <div style={{
        display: 'flex', alignItems: 'center', gap: 10, alignSelf: 'flex-start',
        marginLeft: 6,
      }}>
        <div style={{
          width: 24, height: 24, borderRadius: 999,
          border: `1px solid ${theme.line}`,
          display:'flex', alignItems:'center', justifyContent:'center',
          fontFamily: 'ui-monospace, monospace', fontSize: 11, color: theme.dim,
        }}>{n}</div>
        <div>
          <div style={{ fontFamily: 'Georgia, serif', fontSize: 16, color: theme.ink, lineHeight: 1.1 }}>{title}</div>
          {sub && <div style={{ fontSize: 11, color: theme.faint, marginTop: 2 }}>{sub}</div>}
        </div>
      </div>
      <window.KnobBezel size={260} theme={theme} bezel={bezel} rotation={rotation}>
        {children}
      </window.KnobBezel>
      {gestures.length > 0 && (
        <div style={{ display: 'flex', flexWrap: 'wrap', gap: 6, justifyContent: 'center', maxWidth: 260 }}>
          {gestures.map((g, i) => <GestureChip key={i} {...g}/>)}
        </div>
      )}
    </div>
  );
}

// ── Section header for grouped flow ──
function SectionTitle({ theme, kicker, title, sub }) {
  return (
    <div style={{ gridColumn: '1 / -1', margin: '36px 0 4px' }}>
      <div style={{
        fontSize: 11, color: theme.faint, letterSpacing: '0.22em', textTransform: 'uppercase',
        marginBottom: 8, fontFamily: 'ui-monospace, monospace',
      }}>{kicker}</div>
      <div style={{
        fontFamily: 'Georgia, serif', fontSize: 32, color: theme.ink, letterSpacing: '-0.5px',
        lineHeight: 1.1, marginBottom: 6,
      }}>{title}</div>
      {sub && <div style={{ fontSize: 14, color: theme.dim, maxWidth: 720 }}>{sub}</div>}
    </div>
  );
}

// ── Interactive hero: one big device that responds to gestures ──
function HeroDevice({ tweaks, setTweak }) {
  const theme = window.THEMES[tweaks.theme] || window.THEMES.aubergine;
  const [state, setState] = useState({
    screen: 'idle',          // idle | menu | feedConfirm | pouring | fed | schedule | quiet | portion | lock | settings | hopper
    menuIdx: 0,
    portion: 35,
    settingsIdx: 1,
    rotation: 0,
    pourProgress: 0,
    holdProgress: 0,
    lastInput: 'idle',
  });

  // Ticking pour animation
  useEffect(() => {
    if (state.screen !== 'pouring') return;
    let raf;
    const start = performance.now();
    const tick = (t) => {
      const p = Math.min(1, (t - start) / 1500);
      setState(s => ({ ...s, pourProgress: p }));
      if (p >= 1) {
        setTimeout(() => setState(s => ({ ...s, screen: 'fed', pourProgress: 0 })), 300);
      } else {
        raf = requestAnimationFrame(tick);
      }
    };
    raf = requestAnimationFrame(tick);
    return () => cancelAnimationFrame(raf);
  }, [state.screen]);

  const flash = (msg) => setState(s => ({ ...s, lastInput: msg }));

  const onRotate = (delta) => {
    setState(s => {
      const newRot = s.rotation + delta;
      let next = { ...s, rotation: newRot };
      if (s.screen === 'menu') {
        const items = 4;
        const step = delta > 0 ? 1 : -1;
        next.menuIdx = (s.menuIdx + step + items) % items;
      } else if (s.screen === 'portion') {
        next.portion = Math.max(5, Math.min(60, s.portion + (delta > 0 ? 5 : -5)));
      } else if (s.screen === 'settings') {
        const step = delta > 0 ? 1 : -1;
        next.settingsIdx = Math.max(0, Math.min(3, s.settingsIdx + step));
      } else if (s.screen === 'feedConfirm') {
        next.portion = Math.max(5, Math.min(60, s.portion + (delta > 0 ? 5 : -5)));
      }
      next.lastInput = delta > 0 ? 'rotate cw' : 'rotate ccw';
      return next;
    });
  };

  const onKnobPress = () => {
    flash('knob press');
    setState(s => {
      if (s.screen === 'idle') return { ...s, screen: 'menu' };
      if (s.screen === 'menu') {
        const dest = ['feedConfirm', 'schedule', 'quiet', 'settings'][s.menuIdx];
        return { ...s, screen: dest };
      }
      if (s.screen === 'feedConfirm') return { ...s, screen: 'pouring', pourProgress: 0 };
      if (s.screen === 'pouring') return s; // ignore
      if (s.screen === 'fed') return { ...s, screen: 'idle' };
      return { ...s, screen: 'idle' }; // back-to-idle from any
    });
  };

  const onKnobLongPress = () => {
    flash('knob long-press');
    setState(s => ({ ...s, screen: 'lock' }));
  };

  const onScreenTap = () => {
    flash('screen tap');
    setState(s => {
      if (s.screen === 'idle') return { ...s, screen: 'menu' };
      if (s.screen === 'menu') {
        const dest = ['feedConfirm', 'schedule', 'quiet', 'settings'][s.menuIdx];
        return { ...s, screen: dest };
      }
      if (s.screen === 'feedConfirm') return { ...s, screen: 'portion' };
      if (s.screen === 'portion') return { ...s, screen: 'feedConfirm' };
      if (s.screen === 'schedule') return { ...s, screen: 'idle' };
      if (s.screen === 'quiet') return { ...s, screen: 'idle' };
      if (s.screen === 'settings') return { ...s, screen: 'hopper' };
      if (s.screen === 'hopper') return { ...s, screen: 'idle' };
      if (s.screen === 'lock') return { ...s, screen: 'idle' };
      return s;
    });
  };

  const onScreenHold = () => {
    flash('screen long-tap');
    setState(s => {
      if (s.screen === 'pouring') return { ...s, screen: 'menu', pourProgress: 0 }; // cancel
      return { ...s, screen: 'lock' };
    });
  };

  // map current state→screen
  const mapping = tweaks; // mood-to-cat lives directly on tweaks
  const screen = (() => {
    switch (state.screen) {
      case 'idle': return <window.ScrIdle theme={theme} mapping={mapping} mood="neutral"/>;
      case 'menu': return <window.ScrMenu theme={theme} selected={state.menuIdx}/>;
      case 'feedConfirm': return <window.ScrFeedConfirm theme={theme} mapping={mapping} portion={state.portion}/>;
      case 'pouring': return <window.ScrPouring theme={theme} mapping={mapping} progress={state.pourProgress}/>;
      case 'fed': return <window.ScrFed theme={theme} mapping={mapping}/>;
      case 'schedule': return <window.ScrSchedule theme={theme} mapping={mapping} now={2}/>;
      case 'quiet': return <window.ScrQuiet theme={theme} mapping={mapping}/>;
      case 'portion': return <window.ScrPortionAdjust theme={theme} portion={state.portion}/>;
      case 'lock': return <window.ScrLockConfirm theme={theme} progress={0.6}/>;
      case 'settings': return <window.ScrSettings theme={theme} idx={state.settingsIdx}/>;
      case 'hopper': return <window.ScrHopper theme={theme} pct={0.32}/>;
      default: return null;
    }
  })();

  return (
    <div style={{
      display: 'flex', alignItems: 'center', gap: 56,
      padding: '36px 40px',
      background: 'rgba(255,255,255,0.025)',
      borderRadius: 28,
      border: `1px solid ${theme.line}`,
    }}>
      {/* Device */}
      <div style={{ flex: '0 0 auto', display:'flex', flexDirection:'column', alignItems:'center', gap: 14 }}>
        <window.KnobBezel
          size={420}
          theme={theme}
          bezel={tweaks.bezel}
          rotation={state.rotation}
          interactive
          onRotate={onRotate}
          onKnobPress={onKnobPress}
          onKnobLongPress={onKnobLongPress}
          onScreenTap={onScreenTap}
          onScreenHold={onScreenHold}
        >
          {screen}
        </window.KnobBezel>
        <div style={{
          display: 'inline-flex', alignItems: 'center', gap: 8,
          padding: '6px 14px', borderRadius: 999,
          background: 'rgba(255,179,193,0.12)',
          color: theme.accent,
          fontSize: 11, letterSpacing: '0.18em', textTransform: 'uppercase',
          fontFamily: 'ui-monospace, monospace',
        }}>
          <span style={{ width: 6, height: 6, borderRadius: 999, background: theme.accent }}/>
          {state.lastInput}
        </div>
      </div>

      {/* Right column: how to drive it */}
      <div style={{ flex: 1, color: theme.ink, maxWidth: 420 }}>
        <div style={{
          fontSize: 11, color: theme.faint, letterSpacing: '0.22em', textTransform: 'uppercase',
          fontFamily: 'ui-monospace, monospace', marginBottom: 12,
        }}>Try the device</div>
        <div style={{
          fontFamily: 'Georgia, serif', fontSize: 30, color: theme.ink,
          letterSpacing: '-0.5px', lineHeight: 1.15, marginBottom: 18,
        }}>One knob, four gestures, ten screens.</div>

        <div style={{ display: 'grid', gridTemplateColumns: 'auto 1fr', gap: '12px 14px', fontSize: 13 }}>
          <Gesture theme={theme} icon={<window.IcRotateCW size={16}/>} title="Scroll wheel"   text="Rotate the knob — moves through menu, adjusts portion."/>
          <Gesture theme={theme} icon={<window.IcTap size={16}/>}      title="Click bezel"    text="Press knob — selects, confirms, pours."/>
          <Gesture theme={theme} icon={<window.IcHold size={16}/>}     title="Hold bezel"     text="Long-press knob — opens parental lock / cancel."/>
          <Gesture theme={theme} icon={<window.IcTap size={16}/>}      title="Click screen"   text="Tap touch — context action (e.g. open portion adjust)."/>
          <Gesture theme={theme} icon={<window.IcHold size={16}/>}     title="Hold screen"    text="Long-tap — cancel a pour, force lock screen."/>
        </div>

        <div style={{
          marginTop: 22, padding: '14px 16px',
          borderRadius: 14, border: `1px dashed ${theme.line}`,
          fontSize: 12, color: theme.dim, lineHeight: 1.55,
        }}>
          <strong style={{ color: theme.ink }}>Why duplicate?</strong> The knob may later be reassigned to global app/shortcut switching. Every action it triggers must remain reachable via touch — same affordance, same state.
        </div>

        <button
          onClick={() => setState({ screen: 'idle', menuIdx: 0, portion: 35, settingsIdx: 1, rotation: 0, pourProgress: 0, holdProgress: 0, lastInput: 'idle' })}
          style={{
            marginTop: 18,
            padding: '10px 16px', borderRadius: 999,
            background: 'transparent',
            border: `1px solid ${theme.line}`,
            color: theme.dim,
            fontSize: 12, letterSpacing: '0.14em', textTransform: 'uppercase',
            cursor: 'pointer',
            fontFamily: 'ui-monospace, monospace',
          }}>Reset device</button>
      </div>
    </div>
  );
}

function Gesture({ icon, title, text, theme }) {
  return (
    <>
      <div style={{
        width: 32, height: 32, borderRadius: 999,
        border: `1px solid ${theme.line}`,
        display:'flex', alignItems:'center', justifyContent:'center',
        color: theme.ink,
      }}>{icon}</div>
      <div>
        <div style={{ fontFamily: 'Georgia, serif', fontSize: 15, color: theme.ink }}>{title}</div>
        <div style={{ color: theme.dim, fontSize: 12, lineHeight: 1.5 }}>{text}</div>
      </div>
    </>
  );
}

// ── Tweaks panel ────────────────────────────────────────────────
function KnobTweaks({ tweaks, setTweak }) {
  const moods = ['happy', 'neutral', 'hungry', 'sleepy', 'fed'];
  return (
    <window.TweaksPanel title="Tweaks">
      <window.TweakSection title="Theme">
        <window.TweakRadio
          value={tweaks.theme}
          onChange={(v) => setTweak('theme', v)}
          options={[
            { value: 'aubergine', label: 'Aubergine' },
            { value: 'cream', label: 'Cream' },
            { value: 'mono', label: 'Mono' },
          ]}/>
      </window.TweakSection>
      <window.TweakSection title="Bezel finish">
        <window.TweakRadio
          value={tweaks.bezel}
          onChange={(v) => setTweak('bezel', v)}
          options={[
            { value: 'schematic', label: 'Schematic' },
            { value: 'dashed', label: 'Dashed' },
            { value: 'solid', label: 'Solid' },
          ]}/>
      </window.TweakSection>
      <window.TweakSection title="Knob rotation (manual)">
        <window.TweakSlider
          value={tweaks.rotation} onChange={(v)=>setTweak('rotation', v)}
          min={-180} max={180} step={5} suffix="°"/>
        <div style={{ fontSize: 11, color: '#888', marginTop: 6, lineHeight: 1.4 }}>
          (only affects the static grid — the hero is fully interactive: scroll, click, hold)
        </div>
      </window.TweakSection>
      <window.TweakSection title="Cat per mood">
        {moods.map(m => (
          <div key={m} style={{ marginBottom: 14 }}>
            <div style={{ fontSize: 11, color: '#666', letterSpacing: '0.12em', textTransform: 'uppercase', marginBottom: 6 }}>{m}</div>
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(6, 1fr)', gap: 4 }}>
              {ALL_CATS.map(c => (
                <button key={c} onClick={() => setTweak(m, c)} style={{
                  aspectRatio: '1/1', padding: 0,
                  border: tweaks[m] === c ? `2px solid #ffb3c1` : '1px solid #ddd',
                  borderRadius: 6,
                  background: '#1a1226',
                  cursor: 'pointer',
                  display: 'flex', alignItems: 'center', justifyContent: 'center',
                }}>
                  <img src={`cats4/${c}-white.png`} style={{ width: '85%', height: '85%', objectFit: 'contain' }}/>
                </button>
              ))}
            </div>
          </div>
        ))}
      </window.TweakSection>
    </window.TweaksPanel>
  );
}

// ── App ────────────────────────────────────────────────────────
function App() {
  const [tweaks, setTweaks] = window.useTweaks(KNOB_DEFAULTS);
  const setTweak = (k, v) => setTweaks({ [k]: v });
  const theme = window.THEMES[tweaks.theme] || window.THEMES.aubergine;
  const mapping = tweaks;
  const rot = tweaks.rotation || 0;

  const ink = theme.ink;
  const dim = theme.dim;
  const faint = theme.faint;

  return (
    <div style={{
      minHeight: '100vh',
      background: tweaks.theme === 'cream'
        ? `radial-gradient(ellipse at 20% 0%, #f5ecd9 0%, transparent 50%), radial-gradient(ellipse at 80% 100%, #e6d8b8 0%, transparent 55%), ${theme.bg}`
        : tweaks.theme === 'mono'
        ? '#0a0a0a'
        : `radial-gradient(ellipse at 20% 0%, #2a1a45 0%, transparent 50%), radial-gradient(ellipse at 80% 100%, #3d1830 0%, transparent 55%), ${theme.bg}`,
      color: ink,
      fontFamily: 'Inter, system-ui, sans-serif',
      padding: '56px 48px 96px',
    }}>
      <header style={{ maxWidth: 1320, margin: '0 auto 36px' }}>
        <span style={{
          fontSize: 10, letterSpacing: '0.22em', textTransform: 'uppercase',
          padding: '5px 12px', borderRadius: 999,
          background: 'rgba(255,179,193,0.14)', color: theme.accent,
          fontFamily: 'ui-monospace, monospace',
        }}>FeedMe · v0.5 · 1.28″ round LVGL</span>
        <h1 style={{
          fontFamily: 'Georgia, serif', fontWeight: 400, fontSize: 60, lineHeight: 1,
          margin: '14px 0 12px', letterSpacing: '-1.5px', color: ink,
        }}>
          The knob speaks <em style={{ color: theme.accent }}>cat.</em>
        </h1>
        <p style={{ maxWidth: 720, color: dim, fontSize: 16, lineHeight: 1.55, margin: 0 }}>
          240×240 capacitive round display + rotary knob. Every action reachable by either input — knob may later be reassigned to app-switching, so the touchscreen carries full control duplication. Twelve LVGL-friendly screens below: schematic bezel, flat fills, type ≥ 14 px, max one accent per screen.
        </p>
      </header>

      {/* Hero */}
      <section style={{ maxWidth: 1320, margin: '0 auto 40px' }}>
        <HeroDevice tweaks={tweaks} setTweak={setTweak}/>
      </section>

      {/* Flow grid */}
      <section style={{ maxWidth: 1320, margin: '0 auto', display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '28px 24px' }}>

        <SectionTitle theme={theme}
          kicker="A · Idle to feed"
          title="Wake → menu → confirm → pour → fed."
          sub="Default flow: cat asks, owner taps or scrolls, food drops. Each step shows what gestures advance it."/>

        <FlowStep n="01" title="Boot" sub="Power on" theme={theme} bezel={tweaks.bezel} mapping={mapping}
          gestures={[{kind:'press', label:'(auto · 1.2 s)'}]}>
          <window.ScrBoot theme={theme} mapping={mapping}/>
        </FlowStep>

        <FlowStep n="02" title="Idle" sub="Ambient cat + clock" theme={theme} bezel={tweaks.bezel} mapping={mapping} rotation={rot}
          gestures={[{kind:'press', label:'press'}, {kind:'tap', label:'tap'}]}>
          <window.ScrIdle theme={theme} mapping={mapping} mood="neutral"/>
        </FlowStep>

        <FlowStep n="03" title="Menu" sub="Rotate to highlight" theme={theme} bezel={tweaks.bezel} mapping={mapping} rotation={rot}
          gestures={[{kind:'rotate_cw', label:'rotate'}, {kind:'press', label:'open'}]}>
          <window.ScrMenu theme={theme} selected={0}/>
        </FlowStep>

        <FlowStep n="04" title="Feed: confirm" sub="Hungry cat asks" theme={theme} bezel={tweaks.bezel} mapping={mapping} rotation={rot}
          gestures={[{kind:'rotate_cw', label:'± portion'}, {kind:'press', label:'pour'}]}>
          <window.ScrFeedConfirm theme={theme} mapping={mapping} portion={40}/>
        </FlowStep>

        <FlowStep n="05" title="Pouring" sub="Live progress" theme={theme} bezel={tweaks.bezel} mapping={mapping}
          gestures={[{kind:'long_tap', label:'cancel'}]}>
          <window.ScrPouring theme={theme} mapping={mapping} progress={0.55}/>
        </FlowStep>

        <FlowStep n="06" title="Fed" sub="Confirmation, 1.5 s" theme={theme} bezel={tweaks.bezel} mapping={mapping}
          gestures={[{kind:'press', label:'(auto)'}]}>
          <window.ScrFed theme={theme} mapping={mapping}/>
        </FlowStep>

        <SectionTitle theme={theme}
          kicker="B · Plan, pause, peek"
          title="Schedule, quiet hours, hopper level."
          sub="Read-only screens that let the owner check the day at a glance — knob rotates to scrub time, touch jumps to detail."/>

        <FlowStep n="07" title="Today's schedule" sub="4 meal slots" theme={theme} bezel={tweaks.bezel} mapping={mapping} rotation={rot}
          gestures={[{kind:'rotate_cw', label:'next slot'}, {kind:'press', label:'back'}]}>
          <window.ScrSchedule theme={theme} mapping={mapping} now={2}/>
        </FlowStep>

        <FlowStep n="08" title="Quiet hours" sub="22:00 – 06:30" theme={theme} bezel={tweaks.bezel} mapping={mapping}
          gestures={[{kind:'tap', label:'tap · toggle'}]}>
          <window.ScrQuiet theme={theme} mapping={mapping}/>
        </FlowStep>

        <FlowStep n="09" title="Hopper level" sub="Days remaining" theme={theme} bezel={tweaks.bezel} mapping={mapping}
          gestures={[{kind:'press', label:'back'}]}>
          <window.ScrHopper theme={theme} pct={0.32}/>
        </FlowStep>

        <FlowStep n="10" title="Portion adjust" sub="Knob = grams" theme={theme} bezel={tweaks.bezel} mapping={mapping} rotation={rot}
          gestures={[{kind:'rotate_cw', label:'+5 g'}, {kind:'rotate_ccw', label:'−5 g'}, {kind:'tap', label:'save'}]}>
          <window.ScrPortionAdjust theme={theme} portion={35}/>
        </FlowStep>

        <SectionTitle theme={theme}
          kicker="C · Edge cases"
          title="Settings, lock confirm."
          sub="Long-press is reserved for destructive or admin actions, mirroring touch long-tap."/>

        <FlowStep n="11" title="Settings" sub="Scroll list" theme={theme} bezel={tweaks.bezel} mapping={mapping} rotation={rot}
          gestures={[{kind:'rotate_cw', label:'scroll'}, {kind:'press', label:'edit'}]}>
          <window.ScrSettings theme={theme} idx={1}/>
        </FlowStep>

        <FlowStep n="12" title="Hold to confirm" sub="Long-press destructive" theme={theme} bezel={tweaks.bezel} mapping={mapping}
          gestures={[{kind:'long_press', label:'hold 2 s'}, {kind:'long_tap', label:'or hold screen'}]}>
          <window.ScrLockConfirm theme={theme} progress={0.7}/>
        </FlowStep>
      </section>

      {/* footer */}
      <footer style={{
        maxWidth: 1320, margin: '64px auto 0',
        paddingTop: 24, borderTop: `1px solid ${theme.line}`,
        color: faint, fontSize: 13, lineHeight: 1.6,
      }}>
        Display: 1.28″ round 240×240 · LVGL · Inputs: rotary encoder (CW/CCW + press + long-press) and capacitive touch (tap, double-tap, long-tap, drag) · See <code style={{ background:'rgba(255,255,255,0.06)', padding:'1px 6px', borderRadius:4 }}>handoff.md</code> for full spec, gesture map, and screen-state machine.
      </footer>

      <KnobTweaks tweaks={tweaks} setTweak={setTweak}/>
    </div>
  );
}

ReactDOM.createRoot(document.getElementById('root')).render(<App/>);
