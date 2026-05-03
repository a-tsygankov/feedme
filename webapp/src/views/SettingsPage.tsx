import { useEffect, useState } from "react";
import { useNavigate } from "react-router-dom";
import { ApiError, type HomeInfo, type PairedDevice, api, auth } from "../lib/api";

// Settings. Holds the home name (= hid post-migration-0004), the
// list of paired devices with per-row Forget, sign-out, and the
// "forget home" hard-reset. Coming-soon: rename home, change PIN,
// statistics window preference, push-notification opt-in.
export default function SettingsPage() {
  const navigate = useNavigate();
  const hid = auth.hid();
  const [home, setHome] = useState<HomeInfo | null>(null);
  const [devices, setDevices] = useState<PairedDevice[] | null>(null);
  const [busy, setBusy] = useState(false);
  const [forgetting, setForgetting] = useState<string | null>(null);

  // Pull /api/auth/me + /api/pair/list in parallel. Both are cheap
  // and cover the whole "Home" + "Devices" cards in one round-trip.
  useEffect(() => {
    let cancelled = false;
    Promise.all([
      api.me().catch(() => null),
      api.pairList().catch(() => ({ devices: [] as PairedDevice[] })),
    ]).then(([info, devs]) => {
      if (cancelled) return;
      if (info) setHome(info);
      setDevices(devs.devices);
    });
    return () => { cancelled = true; };
  }, []);

  async function forgetDevice(deviceId: string) {
    if (!confirm(
      `Forget device ${deviceId}?\n\n` +
      `It will stop syncing into this home immediately. To pair the ` +
      `same physical unit again, the user must tap Reset on the device, ` +
      `then re-scan its QR.`,
    )) return;
    setForgetting(deviceId);
    try {
      await api.pairForget(deviceId);
      setDevices((cur) => cur?.filter((d) => d.deviceId !== deviceId) ?? null);
      // Refresh /me so the device count in the Home card is accurate.
      api.me().then(setHome).catch(() => { /* keep stale */ });
    } catch (e) {
      const msg = e instanceof ApiError ? e.message : (e as Error).message;
      alert(`Forget failed: ${msg}`);
    } finally {
      setForgetting(null);
    }
  }

  function signOut() {
    if (!confirm("Sign out of FeedMe on this device?")) return;
    auth.clear();
    navigate("/login", { replace: true });
  }

  // Wipe the home record + every per-home row (cats, users) for the
  // currently signed-in hid. Events stay (orphaned rows are harmless
  // and let firmware backfills survive). Local token is cleared
  // regardless — even on a partial backend failure — so the user
  // lands on /login and can pair a fresh ID.
  async function forgetHome() {
    const ok = confirm(
      `Delete home "${hid}" from the FeedMe server?\n\n` +
      `This removes the PIN, the cats list, and the users list. ` +
      `Feed history is kept (orphaned). You'll be signed out.\n\n` +
      `If you want a totally fresh start, also long-press the QR ` +
      `screen on the device to rotate its home ID.`,
    );
    if (!ok) return;
    setBusy(true);
    try {
      await api.forgetHousehold();
    } catch (e) {
      // Don't block sign-out on a server error — the local creds are
      // useless once the user wants out. Log and continue.
      console.warn("[settings] forget home failed:", e);
      alert("Server delete failed; signing out locally anyway.");
    } finally {
      auth.clear();
      navigate("/login", { replace: true });
    }
  }

  return (
    <>
      <h1>Settings</h1>
      <div className="card">
        <h2>Home</h2>
        <div className="row">
          <span style={{ flex: 1 }}>Name</span>
          <span>{home?.hid ?? hid}</span>
        </div>
        {home && (
          <div className="row">
            <span style={{ flex: 1 }}>Devices</span>
            <span className="muted">{home.deviceCount}</span>
          </div>
        )}
      </div>

      <div className="card">
        <h2>Devices</h2>
        {devices === null && <p className="muted">Loading…</p>}
        {devices !== null && devices.length === 0 && (
          <p className="muted">
            No devices paired yet. Scan a device's QR to pair one.
          </p>
        )}
        {devices?.map((d) => (
          <div key={d.deviceId} className="device-row">
            <span className="device-id" title={d.deviceId}>{d.deviceId}</span>
            <span className="device-meta">{relativeTime(d.createdAt)}</span>
            <button
              className="forget-btn"
              disabled={forgetting === d.deviceId}
              onClick={() => forgetDevice(d.deviceId)}
            >
              {forgetting === d.deviceId ? "…" : "Forget"}
            </button>
          </div>
        ))}
      </div>

      <button
        className="secondary"
        onClick={() => navigate("/sync-log")}
        style={{ width: "100%", marginBottom: 12 }}
      >
        View sync log
      </button>

      <div className="card">
        <h2>Coming soon</h2>
        <p className="muted">
          Statistics dashboard, change PIN, push notifications.
        </p>
      </div>

      <button onClick={signOut} style={{ width: "100%", marginBottom: 12 }}>
        Sign out
      </button>

      <button
        className="danger"
        onClick={forgetHome}
        disabled={busy}
        style={{ width: "100%" }}
      >
        {busy ? "Forgetting…" : "Forget this home"}
      </button>
    </>
  );
}

// "paired 3 days ago" style for the Devices card. Falls back to
// the absolute date past 30 days so the row stays compact and
// human-friendly even for long-tenured devices.
function relativeTime(unixSec: number): string {
  const ageSec = Math.max(0, Math.floor(Date.now() / 1000) - unixSec);
  if (ageSec < 60)        return "just now";
  if (ageSec < 3600)      return `${Math.floor(ageSec / 60)}m ago`;
  if (ageSec < 86400)     return `${Math.floor(ageSec / 3600)}h ago`;
  if (ageSec < 30 * 86400) return `${Math.floor(ageSec / 86400)}d ago`;
  return new Date(unixSec * 1000).toLocaleDateString(undefined, {
    month: "short", day: "numeric", year: "numeric",
  });
}
