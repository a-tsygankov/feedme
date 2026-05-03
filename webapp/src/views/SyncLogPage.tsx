import { useEffect, useMemo, useState } from "react";
import { useNavigate } from "react-router-dom";
import { ApiError, type PairedDevice, type SyncLogEntry, api, auth } from "../lib/api";

// Sync log viewer — drives /sync-log. Read-only, last 100 rows for the
// signed-in home; per-device filter via dropdown. Auto-refreshes every
// 30 s while the tab is visible so a sync that just completed on a
// device shows up without a manual refresh.
//
// Each row:
//   ✓ ok        2026-05-03 14:32  feedme-abc   12 in / 12 out / 0 ⚠   95ms
//   ✗ error     2026-05-03 06:32  feedme-abc   "no such home"
//   ⊘ cancelled 2026-05-03 02:32  feedme-abc   long-tap dismiss
//
// Failure-mode hints in the error_message column let the user
// triage without diving into the Worker logs (e.g. "pairing
// revoked" → re-pair, "schemaVersion mismatch" → upgrade firmware).
const POLL_MS = 30_000;

export default function SyncLogPage() {
  const navigate = useNavigate();
  const [entries, setEntries] = useState<SyncLogEntry[] | null>(null);
  const [devices, setDevices] = useState<PairedDevice[]>([]);
  const [filter, setFilter] = useState<string>("");        // "" = all devices
  const [err, setErr] = useState<string | null>(null);

  async function load() {
    try {
      const [logRes, devRes] = await Promise.all([
        api.syncLogList(filter || undefined, 100),
        devices.length ? Promise.resolve({ devices }) : api.pairList(),
      ]);
      setEntries(logRes.entries);
      if (devRes.devices.length && devices.length === 0) setDevices(devRes.devices);
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
    // re-load whenever the device filter changes too
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [filter]);

  const grouped = useMemo(() => groupByDay(entries ?? []), [entries]);

  return (
    <>
      <h1>Sync log</h1>
      <p className="muted">
        Last 100 sync attempts for <b>{auth.hid() ?? "your home"}</b>.
        Auto-refreshes every 30 s.
      </p>

      <div className="card" style={{ padding: "12px 16px" }}>
        <div className="dash-feeder-row" style={{ borderTop: 0, paddingTop: 0 }}>
          <span>Device</span>
          <select value={filter} onChange={(e) => setFilter(e.target.value)}>
            <option value="">All devices</option>
            {devices.map((d) => (
              <option key={d.deviceId} value={d.deviceId}>{d.deviceId}</option>
            ))}
          </select>
        </div>
      </div>

      {err && <p className="error">{err}</p>}
      {!entries && !err && <p className="muted">Loading…</p>}
      {entries && entries.length === 0 && (
        <div className="card">
          <p className="muted">No sync attempts yet for this home.</p>
        </div>
      )}

      {Object.entries(grouped).map(([day, rows]) => (
        <div key={day} className="card">
          <h2>{day}</h2>
          {rows.map((r) => (
            <div key={r.id} className={`syncrow syncrow-${r.result}`}>
              <span className="syncrow-icon" title={r.result}>{iconFor(r.result)}</span>
              <span className="syncrow-time">{formatTime(r.ts)}</span>
              <span className="syncrow-device" title={r.device_id}>
                {shortDevice(r.device_id)}
              </span>
              <span className="syncrow-detail">
                {r.result === "ok" && (
                  <>
                    <span className="muted">{r.entities_in} in / {r.entities_out} out</span>
                    {r.conflicts > 0 && (
                      <> · <span style={{ color: "#facc15" }}>
                        ⚠ {r.conflicts} conflict{r.conflicts === 1 ? "" : "s"}
                      </span></>
                    )}
                    {" · "}<span className="muted">{r.duration_ms}ms</span>
                  </>
                )}
                {r.result === "error" && (
                  <span style={{ color: "#f87171" }}>{r.error_message ?? "(no detail)"}</span>
                )}
                {r.result === "cancelled" && (
                  <span className="muted">{r.error_message ?? "client aborted"}</span>
                )}
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
function iconFor(result: string): string {
  if (result === "ok")        return "✓";
  if (result === "error")     return "✗";
  if (result === "cancelled") return "⊘";
  return "?";
}

function formatTime(ts: number): string {
  const d = new Date(ts * 1000);
  const hh = String(d.getHours()).padStart(2, "0");
  const mm = String(d.getMinutes()).padStart(2, "0");
  return `${hh}:${mm}`;
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

function groupByDay(entries: SyncLogEntry[]): Record<string, SyncLogEntry[]> {
  const out: Record<string, SyncLogEntry[]> = {};
  for (const e of entries) {
    const day = formatDay(e.ts);
    (out[day] ??= []).push(e);
  }
  return out;
}

// Truncate device IDs for display: "feedme-a8b3c1d4e5f6" → "…d4e5f6"
function shortDevice(id: string): string {
  if (id.length <= 12) return id;
  return "…" + id.slice(-6);
}
