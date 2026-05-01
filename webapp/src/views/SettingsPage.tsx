import { useEffect, useState } from "react";
import { useNavigate } from "react-router-dom";
import { type HomeInfo, api, auth } from "../lib/api";

// Settings. Holds the home name (= hid post-migration-0004), the
// number of paired devices, sign-out, and the "forget home"
// hard-reset. Coming-soon: rename home, change PIN, statistics
// window preference, push-notification opt-in.
export default function SettingsPage() {
  const navigate = useNavigate();
  const hid = auth.hid();
  const [home, setHome] = useState<HomeInfo | null>(null);
  const [busy, setBusy] = useState(false);

  // Pull /api/auth/me for the live device count + canonical hid.
  useEffect(() => {
    let cancelled = false;
    api.me()
      .then((info) => { if (!cancelled) setHome(info); })
      .catch(() => { /* leave home=null; UI falls back to cached hid */ });
    return () => { cancelled = true; };
  }, []);

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
