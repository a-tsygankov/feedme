import { useEffect, useState } from "react";
import { useNavigate, useSearchParams } from "react-router-dom";
import { ApiError, api, auth } from "../lib/api";

// Device-pairing landing page. Reached by scanning the QR code shown
// on the FeedMe device's pairing screen — the QR encodes:
//
//   https://feedme-webapp.pages.dev/setup?hid=feedme-abcdef
//
// Three terminal states based on whether the hid is already known to
// the backend:
//
//   missing   — query param absent or empty. Probably someone typed
//               the URL by hand. Show a friendly explainer + link to
//               regular /login.
//
//   new       — household doesn't exist yet. Show a PIN setup form;
//               on submit calls /api/auth/setup which atomically
//               creates the household and returns a session token.
//
//   exists    — household already has a PIN. The user either re-scanned
//               the QR after first pairing, or someone else paired
//               this device. Offer to sign in (carrying the hid
//               through to /login as a query string).
//
// All three states are entered from the initial "checking" state which
// fires the /api/auth/exists probe.
type Phase = "checking" | "new" | "exists" | "missing" | "error";

export default function SetupPage() {
  const [params] = useSearchParams();
  const navigate = useNavigate();
  const hid = (params.get("hid") ?? "").trim();

  const [phase, setPhase] = useState<Phase>(hid ? "checking" : "missing");
  const [pin, setPin]     = useState("");
  const [pin2, setPin2]   = useState("");
  const [busy, setBusy]   = useState(false);
  const [err,  setErr]    = useState<string | null>(null);

  // Probe the backend once on mount when we have an hid in the URL.
  useEffect(() => {
    if (!hid) return;
    setPhase("checking");
    api
      .exists(hid)
      .then(({ exists }) => setPhase(exists ? "exists" : "new"))
      .catch((e) => {
        setPhase("error");
        setErr(e instanceof Error ? e.message : "network error");
      });
  }, [hid]);

  async function submitSetup() {
    if (pin.length < 4) { setErr("PIN must be at least 4 digits"); return; }
    if (pin !== pin2)   { setErr("PINs don't match"); return; }
    setBusy(true); setErr(null);
    try {
      const { token } = await api.setup(hid, pin);
      auth.set(token, hid);
      navigate("/", { replace: true });
    } catch (e) {
      // 409 — race with another tab that paired in between. Refresh
      // the phase so the UI offers sign-in instead.
      if (e instanceof ApiError && e.status === 409) {
        setPhase("exists");
        setErr(null);
      } else {
        setErr(e instanceof Error ? e.message : "setup failed");
      }
    } finally {
      setBusy(false);
    }
  }

  return (
    <>
      <h1>FeedMe</h1>
      <div className="card">
        {phase === "missing" && (
          <>
            <h2>Pair a device</h2>
            <p className="muted">
              This page is the landing target for the QR code on your
              FeedMe device's pairing screen. Scan it with your phone
              and you'll come back here with the household ID
              auto-filled.
            </p>
            <p className="muted">
              Already paired? Open the regular sign-in page.
            </p>
            <button onClick={() => navigate("/login")} style={{ marginTop: 16 }}>
              Go to sign in
            </button>
          </>
        )}

        {phase === "checking" && (
          <>
            <h2>Pairing {hid}…</h2>
            <p className="muted">Checking with the server.</p>
          </>
        )}

        {phase === "error" && (
          <>
            <h2>Couldn't reach the server</h2>
            <p className="muted">{err ?? "Try again in a moment."}</p>
            <button onClick={() => setPhase("checking")} style={{ marginTop: 16 }}>
              Retry
            </button>
          </>
        )}

        {phase === "exists" && (
          <>
            <h2>Already paired</h2>
            <p className="muted">
              <code>{hid}</code> already has a PIN. Sign in to manage it
              — or, if you've forgotten the PIN, reset pairing on the
              device (long-press the QR screen) to start over with a
              fresh ID.
            </p>
            <button
              onClick={() =>
                navigate(`/login?hid=${encodeURIComponent(hid)}`, { replace: true })
              }
              style={{ marginTop: 16 }}
            >
              Continue to sign in
            </button>
          </>
        )}

        {phase === "new" && (
          <>
            <h2>Set a PIN for {hid}</h2>
            <p className="muted">
              Anyone in your home will use this PIN to open the app on
              their phone. 4+ digits.
            </p>
            <label>PIN</label>
            <input
              autoFocus
              type="password"
              inputMode="numeric"
              pattern="[0-9]*"
              autoComplete="new-password"
              value={pin}
              onChange={(e) => setPin(e.target.value)}
            />
            <label>Confirm PIN</label>
            <input
              type="password"
              inputMode="numeric"
              pattern="[0-9]*"
              autoComplete="new-password"
              value={pin2}
              onChange={(e) => setPin2(e.target.value)}
              onKeyDown={(e) => { if (e.key === "Enter") submitSetup(); }}
            />
            <button disabled={busy} onClick={submitSetup} style={{ marginTop: 16 }}>
              {busy ? "..." : "Pair this device"}
            </button>
          </>
        )}

        {err && phase !== "error" && <p className="error">{err}</p>}
      </div>
    </>
  );
}
