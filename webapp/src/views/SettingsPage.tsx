import { useState } from "react";
import { useNavigate } from "react-router-dom";
import { api, auth } from "../lib/api";

// Settings stub. Phase 1 holds the household ID, sign-out, and the
// "forget household" hard-reset. Phase 2 will gain: change PIN,
// statistics window preference, push-notification opt-in, etc.
export default function SettingsPage() {
  const navigate = useNavigate();
  const hid = auth.hid();
  const [busy, setBusy] = useState(false);

  function signOut() {
    if (!confirm("Sign out of FeedMe on this device?")) return;
    auth.clear();
    navigate("/login", { replace: true });
  }

  // Wipe the household record + every per-household row (cats, users)
  // for the currently signed-in hid. Events stay (orphaned rows are
  // harmless and let firmware backfills survive). Local token is
  // cleared regardless — even on a partial backend failure — so the
  // user lands on /login and can pair a fresh hid.
  async function forgetHousehold() {
    const ok = confirm(
      `Delete household "${hid}" from the FeedMe server?\n\n` +
      `This removes the PIN, the cats list, and the users list. ` +
      `Feed history is kept (orphaned). You'll be signed out.\n\n` +
      `If you want a totally fresh start, also long-press the QR ` +
      `screen on the device to rotate its household ID.`,
    );
    if (!ok) return;
    setBusy(true);
    try {
      await api.forgetHousehold();
    } catch (e) {
      // Don't block sign-out on a server error — the local creds are
      // useless once the user wants out. Log and continue.
      console.warn("[settings] forget household failed:", e);
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
        <h2>Household</h2>
        <div className="row">
          <span style={{ flex: 1 }}>ID</span>
          <span className="muted">{hid}</span>
        </div>
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
        onClick={forgetHousehold}
        disabled={busy}
        style={{ width: "100%" }}
      >
        {busy ? "Forgetting…" : "Forget this household"}
      </button>
    </>
  );
}
