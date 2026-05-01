import { useState } from "react";
import { useNavigate } from "react-router-dom";
import { ApiError, api, auth } from "../lib/api";

// PIN-based household login. Two-step:
//   1. Enter the household ID (matches what the device captured at
//      first-time setup — e.g. "home-andrey"). We probe the backend
//      to see if this household already exists.
//   2a. exists → "Enter PIN" → /api/auth/login.
//   2b. doesn't → "Set a PIN" → /api/auth/setup (creates household,
//                returns a session token in the same call).
//
// Token + hid are stored in localStorage; subsequent API calls send
// Authorization: Bearer <token>.
export default function LoginPage() {
  const navigate = useNavigate();
  const [hid, setHid] = useState("");
  const [pin, setPin] = useState("");
  const [pin2, setPin2] = useState("");
  const [phase, setPhase] = useState<"hid" | "login" | "setup">("hid");
  const [busy, setBusy] = useState(false);
  const [err, setErr] = useState<string | null>(null);

  async function checkHid() {
    if (!hid.trim()) { setErr("Household ID is required"); return; }
    setBusy(true); setErr(null);
    try {
      const { exists } = await api.exists(hid.trim());
      setPhase(exists ? "login" : "setup");
    } catch (e) {
      setErr(e instanceof Error ? e.message : "network error");
    } finally {
      setBusy(false);
    }
  }

  async function submitLogin() {
    if (pin.length < 4) { setErr("PIN must be at least 4 digits"); return; }
    setBusy(true); setErr(null);
    try {
      const { token } = await api.login(hid.trim(), pin);
      auth.set(token, hid.trim());
      navigate("/", { replace: true });
    } catch (e) {
      if (e instanceof ApiError && e.status === 401) setErr("Wrong PIN");
      else setErr(e instanceof Error ? e.message : "login failed");
    } finally {
      setBusy(false);
    }
  }

  async function submitSetup() {
    if (pin.length < 4) { setErr("PIN must be at least 4 digits"); return; }
    if (pin !== pin2)   { setErr("PINs don't match"); return; }
    setBusy(true); setErr(null);
    try {
      const { token } = await api.setup(hid.trim(), pin);
      auth.set(token, hid.trim());
      navigate("/", { replace: true });
    } catch (e) {
      setErr(e instanceof Error ? e.message : "setup failed");
    } finally {
      setBusy(false);
    }
  }

  return (
    <>
      <h1>FeedMe</h1>
      <div className="card">
        {phase === "hid" && (
          <>
            <h2>Open your household</h2>
            <label>Household ID</label>
            <input
              autoFocus
              autoCapitalize="off"
              autoCorrect="off"
              spellCheck={false}
              placeholder="home-andrey"
              value={hid}
              onChange={(e) => setHid(e.target.value)}
              onKeyDown={(e) => { if (e.key === "Enter") checkHid(); }}
            />
            <p className="muted">
              The same string your FeedMe device shows on its
              "Switch&nbsp;Wi-Fi" screen.
            </p>
            <button disabled={busy} onClick={checkHid} style={{ marginTop: 16 }}>
              {busy ? "..." : "Continue"}
            </button>
          </>
        )}

        {phase === "login" && (
          <>
            <h2>Enter PIN for {hid}</h2>
            <label>PIN</label>
            <input
              autoFocus
              type="password"
              inputMode="numeric"
              pattern="[0-9]*"
              autoComplete="current-password"
              value={pin}
              onChange={(e) => setPin(e.target.value)}
              onKeyDown={(e) => { if (e.key === "Enter") submitLogin(); }}
            />
            <button disabled={busy} onClick={submitLogin} style={{ marginTop: 16 }}>
              {busy ? "..." : "Sign in"}
            </button>
            <button className="secondary" onClick={() => { setPhase("hid"); setPin(""); setErr(null); }} style={{ marginTop: 8, width: "100%" }}>
              Use a different household
            </button>
          </>
        )}

        {phase === "setup" && (
          <>
            <h2>Set a PIN for {hid}</h2>
            <p className="muted">
              No household yet. Pick a PIN — anyone in the home will
              use it to access this app.
            </p>
            <label>PIN (4+ digits)</label>
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
              {busy ? "..." : "Create household"}
            </button>
            <button className="secondary" onClick={() => { setPhase("hid"); setPin(""); setPin2(""); setErr(null); }} style={{ marginTop: 8, width: "100%" }}>
              Back
            </button>
          </>
        )}

        {err && <p className="error">{err}</p>}
      </div>
    </>
  );
}
