import { useEffect, useMemo, useState } from "react";
import { useNavigate } from "react-router-dom";
import { ApiError, type AuthLogEntry, api, auth } from "../lib/api";

// Auth/pair log viewer — drives /auth-log. TEMPORARY diagnostic
// surface added at user request to debug pairing flow failures.
// Read-only, last 100 events for the signed-in home; per-kind filter.
// Auto-refreshes every 30 s.
//
// Each row:
//   ✓ ok    14:32  pair-confirm  feedme-abc…d4e5f6   12ms
//   ✗ error 06:32  login         (hid=foo)  "wrong PIN"   8ms
//   ✓ ok    02:32  quick-setup   feedme-abc…d4e5f6  alreadyPaired   3ms
//
// Maps directly onto auth_logs table (migration 0011). Same
// 100-row ring buffer + 30 s poll cadence as SyncLogPage.
const POLL_MS = 30_000;

const KIND_LABELS: Record<string, string> = {
  "pair-start":         "Pair start",
  "pair-confirm":       "Pair confirm",
  "pair-cancel":        "Pair cancel",
  "pair-forget":        "Pair forget",
  "setup":              "Setup",
  "login":              "Login",
  "quick-setup":        "Quick-Start",
  "login-token-create": "Login QR mint",
  "login-qr":           "Login QR exchange",
  "set-pin":            "Set PIN",
};

export default function AuthLogPage() {
  const navigate = useNavigate();
  const [entries, setEntries] = useState<AuthLogEntry[] | null>(null);
  const [filter, setFilter] = useState<string>("");        // "" = all kinds
  const [err, setErr] = useState<string | null>(null);

  async function load() {
    try {
      const res = await api.authLogList(filter || undefined, 100);
      setEntries(res.entries);
    } catch (e) {
      if (e instanceof ApiError && e.status === 401) {
        auth.clear();
        location.replace("/login");
        return;
      }
      setErr(e instanceof Error ? e.message : "load failed");
    }
  }

  useEffect(() => {
    load();
    const id = window.setInterval(load, POLL_MS);
    return () => window.clearInterval(id);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [filter]);

  const grouped = useMemo(() => groupByDay(entries ?? []), [entries]);
  // Distinct kinds present in the data — drives the filter dropdown.
  const knownKinds = useMemo(() => {
    const s = new Set<string>();
    for (const e of entries ?? []) s.add(e.kind);
    return Array.from(s).sort();
  }, [entries]);

  return (
    <>
      <h1>Pair / login log</h1>
      <p className="muted">
        Last 100 auth events for <b>{auth.hid() ?? "your home"}</b>.
        Auto-refreshes every 30 s. Includes pair-start, pair-confirm,
        login, quick-setup, login-QR, and set-PIN attempts.
      </p>

      <div className="card" style={{ padding: "12px 16px" }}>
        <div className="dash-feeder-row" style={{ borderTop: 0, paddingTop: 0 }}>
          <span>Kind</span>
          <select value={filter} onChange={(e) => setFilter(e.target.value)}>
            <option value="">All events</option>
            {knownKinds.map((k) => (
              <option key={k} value={k}>{KIND_LABELS[k] ?? k}</option>
            ))}
          </select>
        </div>
      </div>

      {err && <p className="error">{err}</p>}
      {!entries && !err && <p className="muted">Loading…</p>}
      {entries && entries.length === 0 && (
        <div className="card">
          <p className="muted">No auth events for this home yet.</p>
        </div>
      )}

      {Object.entries(grouped).map(([day, rows]) => (
        <div key={day} className="card">
          <h2>{day}</h2>
          {rows.map((r) => (
            <div key={r.id} className={`syncrow syncrow-${r.result}`}>
              <span className="syncrow-icon" title={r.result}>
                {r.result === "ok" ? "✓" : "✗"}
              </span>
              <span className="syncrow-time">{formatTime(r.ts)}</span>
              <span className="syncrow-device" title={r.identifier ?? ""}>
                {KIND_LABELS[r.kind] ?? r.kind}
              </span>
              <span className="syncrow-detail">
                {r.identifier && (
                  <span className="muted">{shortId(r.identifier)} </span>
                )}
                {r.result === "error" && r.error_message && (
                  <span style={{ color: "#f87171" }}>· {r.error_message}</span>
                )}
                {r.result === "ok" && r.error_message && (
                  // "ok" rows can carry an info note (e.g. "alreadyPaired")
                  // — show in muted, not red.
                  <span className="muted">· {r.error_message}</span>
                )}
                <span className="muted"> · {r.duration_ms}ms</span>
              </span>
            </div>
          ))}
        </div>
      ))}

      <button
        className="secondary"
        onClick={() => navigate("/settings")}
        style={{ width: "100%", marginTop: 12 }}
      >
        Back to settings
      </button>
    </>
  );
}

// ── formatters / grouping ───────────────────────────────────────
function formatTime(ts: number): string {
  const d = new Date(ts * 1000);
  const hh = String(d.getHours()).padStart(2, "0");
  const mm = String(d.getMinutes()).padStart(2, "0");
  const ss = String(d.getSeconds()).padStart(2, "0");
  return `${hh}:${mm}:${ss}`;
}

function formatDay(ts: number): string {
  const d = new Date(ts * 1000);
  const today = new Date();
  const sameDay = d.toDateString() === today.toDateString();
  if (sameDay) return "Today";
  const yesterday = new Date(today);
  yesterday.setDate(today.getDate() - 1);
  if (d.toDateString() === yesterday.toDateString()) return "Yesterday";
  return d.toLocaleDateString(undefined, {
    weekday: "short", month: "short", day: "numeric",
  });
}

function groupByDay(entries: AuthLogEntry[]): Record<string, AuthLogEntry[]> {
  const out: Record<string, AuthLogEntry[]> = {};
  for (const e of entries) {
    const day = formatDay(e.ts);
    (out[day] ??= []).push(e);
  }
  return out;
}

// Truncate long identifiers (deviceIds / hids) for display.
function shortId(id: string): string {
  if (id.length <= 16) return id;
  return id.slice(0, 4) + "…" + id.slice(-6);
}
