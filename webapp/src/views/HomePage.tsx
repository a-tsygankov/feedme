import { useCallback, useEffect, useRef, useState } from "react";
import { ApiError, type DashboardCat, type HistoryEvent, type HomeInfo,
         type User, api, auth } from "../lib/api";
import ArcRing from "../components/ArcRing";
import CatFace from "../components/CatFace";
import { formatAgo, moodFor, ringProgress } from "../lib/mood";

// Dashboard — webapp equivalent of the device's idle screen, one card
// per cat. Translates the mockup workflows:
//
//   W1 At-a-glance → mood ring (drains over hungry threshold) +
//                    expression (Happy/Neutral/Hungry) + status line
//                    + meal dots for today's feeds.
//   W2 Logging    → "Feed" button per cat. Optimistic UI update +
//                    toast confirmation; then refetch.
//   W3 Snoozing   → "Just begging" button. Logs a snooze event
//                    without resetting the hungry timer.
//   W4 Sync       → 30s poll + refetch on window focus. Two-tab /
//                    two-device updates show up within ~30s of the
//                    other side's tap.
//   W5 History    → Per-card "History" toggle reveals last 10 events.
//
// Auth: all requests go through /api/dashboard/* which derives the
// home from the session token. The hid in localStorage is only used
// for the "Forget home" flow in Settings; rendering reads /me + /cats.
const POLL_MS = 30_000;

export default function HomePage() {
  const [home, setHome] = useState<HomeInfo | null>(null);
  const [cats, setCats] = useState<DashboardCat[] | null>(null);
  const [users, setUsers] = useState<User[]>([]);
  const [feeder, setFeeder] = useState<string>("");   // currently-selected user name
  const [err, setErr] = useState<string | null>(null);
  const [toast, setToast] = useState<string | null>(null);
  const [busySlot, setBusySlot] = useState<number | null>(null);
  const [openHistorySlot, setOpenHistorySlot] = useState<number | null>(null);
  const [history, setHistory] = useState<HistoryEvent[] | null>(null);

  // Hold the latest-known users list in a ref so the feed handler
  // doesn't capture a stale closure when called from the auto-poll.
  const usersRef = useRef<User[]>([]);
  usersRef.current = users;

  // ── Load + refetch ────────────────────────────────────────────
  const load = useCallback(async (silent = false) => {
    if (!silent) setErr(null);
    try {
      // /me + /users only need to be re-fetched rarely; /cats is the
      // hot path. Run all three in parallel on the cold load and only
      // /cats on subsequent polls.
      const [meRes, usersRes, catsRes] = await Promise.all([
        home ? Promise.resolve(home) : api.me().catch(() => null),
        users.length ? Promise.resolve({ users }) : api.usersList().catch(() => ({ users: [] as User[] })),
        api.dashboardCats(),
      ]);
      if (meRes && !home) setHome(meRes);
      if (usersRes.users.length && !users.length) {
        setUsers(usersRes.users);
        if (!feeder) setFeeder(usersRes.users[0]?.name ?? "User 0");
      }
      setCats(catsRes.cats);
    } catch (e) {
      if (e instanceof ApiError && e.status === 401) {
        auth.clear();
        location.replace("/login");
        return;
      }
      if (!silent) setErr(e instanceof Error ? e.message : "load failed");
    }
  }, [home, users, feeder]);

  // First-mount load + poll loop + refetch on tab focus.
  useEffect(() => {
    load();
    const id = window.setInterval(() => load(true), POLL_MS);
    const onFocus = () => load(true);
    window.addEventListener("focus", onFocus);
    return () => {
      window.clearInterval(id);
      window.removeEventListener("focus", onFocus);
    };
    // load is intentionally omitted from deps — we want a single
    // mount-time effect; ref captures ensure freshness.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // ── Actions ───────────────────────────────────────────────────
  function flashToast(msg: string) {
    setToast(msg);
    window.setTimeout(() => setToast(null), 1700);
  }

  async function feed(cat: DashboardCat, type: "feed" | "snooze") {
    if (busySlot !== null) return;
    const by = feeder || usersRef.current[0]?.name || "User 0";
    setBusySlot(cat.slotId);
    // Optimistic update: mark the cat as just-fed locally so the ring
    // snaps to full + the cat smiles before the round-trip lands.
    if (type === "feed") {
      setCats((cur) => cur && cur.map((c) => c.slotId === cat.slotId
        ? { ...c, lastFedAt: Math.floor(Date.now() / 1000),
                  lastFedBy: by, lastEventType: "feed",
                  secondsSince: 0, todayCount: c.todayCount + 1 }
        : c));
    }
    try {
      await api.dashboardFeed(cat.slotId, by, type);
      flashToast(type === "feed" ? `Fed ${cat.name} ✓` : `Snoozed ${cat.name}`);
      // Refetch so we converge on server truth (and pick up any other
      // device's events that landed between optimistic + now).
      load(true);
    } catch (e) {
      flashToast(e instanceof Error ? e.message : "action failed");
      load(true);   // revert optimistic update via fresh data
    } finally {
      setBusySlot(null);
    }
  }

  async function toggleHistory(cat: DashboardCat) {
    if (openHistorySlot === cat.slotId) {
      setOpenHistorySlot(null);
      setHistory(null);
      return;
    }
    setOpenHistorySlot(cat.slotId);
    setHistory(null);
    try {
      const res = await api.dashboardHistory(cat.slotId, 10);
      setHistory(res.events);
    } catch {
      setHistory([]);
    }
  }

  // ── Render ────────────────────────────────────────────────────
  const displayName = home?.hid || auth.hid() || "Home";

  return (
    <>
      <h1>{displayName}</h1>
      <p className="muted">
        {home ? `${home.deviceCount} device${home.deviceCount === 1 ? "" : "s"} paired` : " "}
      </p>

      {err && <p className="error">{err}</p>}
      {!cats && !err && <p className="muted">Loading…</p>}
      {cats && cats.length === 0 && (
        <div className="card">
          <p className="muted">
            No cats yet. Add one in the Cats tab to start tracking
            feedings.
          </p>
        </div>
      )}

      {users.length > 1 && (
        <div className="card" style={{ padding: "12px 16px" }}>
          <div className="dash-feeder-row" style={{ borderTop: 0, paddingTop: 0 }}>
            <span>Logging as</span>
            <select value={feeder} onChange={(e) => setFeeder(e.target.value)}>
              {users.map((u) => (
                <option key={u.slotId} value={u.name}>{u.name}</option>
              ))}
            </select>
          </div>
        </div>
      )}

      {cats?.map((c) => {
        const m = moodFor(c.secondsSince, c.hungryThresholdSec);
        const overdue = m.mood === "hungry";
        const totalMeals = 3;   // mockup convention; configurable later
        return (
          <div key={c.slotId} className="dash-cat-card">
            <h2>
              <span
                className="swatch"
                style={{ background: cssColor(c.color, c.slotId) }}
              />
              {c.name}
            </h2>

            <div className="dash-ring">
              <ArcRing
                progress={ringProgress(c.secondsSince, c.hungryThresholdSec)}
                color={m.color}
                pulse={overdue}
              />
              <div className="dash-ring-inner">
                <CatFace mood={m.mood} size={96}/>
                <div className="dash-ring-time">{formatAgo(c.secondsSince)}</div>
                {c.lastFedBy && <div className="dash-ring-by">by {c.lastFedBy}</div>}
                <div className="dash-meal-dots" style={{ marginTop: 6 }}>
                  {Array.from({ length: totalMeals }).map((_, i) => (
                    <div
                      key={i}
                      className={`dash-meal-dot${i < c.todayCount ? " fed" : ""}`}
                    />
                  ))}
                </div>
              </div>
            </div>

            <div className={`dash-status-line${overdue ? " urgent" : ""}`}>
              {overdue ? "FEED ME!" : m.label}
            </div>

            <div className="dash-actions">
              <button
                disabled={busySlot !== null}
                onClick={() => feed(c, "feed")}
              >
                {busySlot === c.slotId ? "…" : `Feed ${c.defaultPortionG}g`}
              </button>
              <button
                className="secondary"
                disabled={busySlot !== null}
                onClick={() => feed(c, "snooze")}
              >
                Just begging
              </button>
            </div>

            <button
              className="dash-toggle"
              onClick={() => toggleHistory(c)}
            >
              {openHistorySlot === c.slotId ? "Hide history" : "Show history"}
            </button>

            {openHistorySlot === c.slotId && (
              <div className="dash-history">
                {history === null && <div className="dash-history-empty">Loading…</div>}
                {history?.length === 0 && <div className="dash-history-empty">No events yet.</div>}
                {history?.map((ev) => (
                  <div
                    key={ev.id}
                    className={`dash-history-row${ev.type === "snooze" ? " snooze" : ""}`}
                  >
                    <span className="marker"/>
                    <span className="ts">{formatTs(ev.ts)}</span>
                    <span className="by">
                      {ev.by} <span className="muted">· {ev.type}</span>
                    </span>
                  </div>
                ))}
              </div>
            )}
          </div>
        );
      })}

      {toast && <div className="dash-toast">{toast}</div>}
    </>
  );
}

// ── tiny helpers ────────────────────────────────────────────────
// CatRoster's color sentinel: 0 = "auto" → fall back to a stable
// per-slot palette so cards aren't all the same swatch.
const AUTO_PALETTE = ["#ffb3c1", "#a78bfa", "#5eead4", "#fbbf24"];

function cssColor(color: number, slotId: number): string {
  if (color === 0) return AUTO_PALETTE[slotId % AUTO_PALETTE.length];
  return "#" + color.toString(16).padStart(6, "0");
}

function formatTs(unixSec: number): string {
  const d = new Date(unixSec * 1000);
  const today = new Date();
  const sameDay = d.getFullYear() === today.getFullYear()
               && d.getMonth() === today.getMonth()
               && d.getDate() === today.getDate();
  const hh = String(d.getHours()).padStart(2, "0");
  const mm = String(d.getMinutes()).padStart(2, "0");
  if (sameDay) return `${hh}:${mm}`;
  // < 7 days → "Mon 14:32"; older → "May 1"
  const ageDays = (today.getTime() - d.getTime()) / 86400000;
  if (ageDays < 7) {
    const dow = ["Sun","Mon","Tue","Wed","Thu","Fri","Sat"][d.getDay()];
    return `${dow} ${hh}:${mm}`;
  }
  return d.toLocaleDateString(undefined, { month: "short", day: "numeric" });
}
