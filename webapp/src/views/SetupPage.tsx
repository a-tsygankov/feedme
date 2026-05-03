import { useEffect, useState } from "react";
import { useNavigate, useSearchParams } from "react-router-dom";
import { ApiError, api, auth } from "../lib/api";

// /setup landing page. Reached by scanning the QR on the FeedMe
// device's pairing screen — the QR encodes:
//
//   https://feedme-webapp.pages.dev/setup?device=feedme-{mac6}
//
// (Earlier versions used `?hid=…`; we still accept it for backwards
// compatibility with already-flashed firmware that hard-codes the
// query parameter name. Internally, this is the *device id*, not
// the home id — those are separate post-migration-0004.)
//
// What the user does here:
//   1. Picks a UNIQUE home NAME (e.g. "Smith Family"). That name
//      becomes the home's identifier — login later requires typing
//      this exact string.
//   2. Sets a PIN.
//   3. Hits "Create home".
//
// Backend behaviour:
//   - Validates the name (1-64 chars, no control chars)
//   - 409 if the name is taken → we redirect to /login with the
//     name + device prefilled so the user just enters the PIN.
//   - On success: returns a session token bound to the home name
//     AND atomically claims the device into that home (devices table).
//
// Escape hatch: "Already have a home? Log in" navigates to /login
// preserving the device query param so the device gets claimed into
// the existing home after sign-in.
//
// Token-aware: if a valid stored token already matches a home that
// owns this device, we silently navigate("/") — re-scans don't
// re-prompt for anything.
type Phase = "checking" | "form" | "missing" | "error";

export default function SetupPage() {
  const [params] = useSearchParams();
  const navigate = useNavigate();

  // Accept either ?device= or legacy ?hid=. Trim and lowercase
  // anything we got — the firmware always emits lowercase hex.
  const deviceId = (params.get("device") ?? params.get("hid") ?? "").trim();

  const [phase, setPhase] = useState<Phase>(deviceId ? "checking" : "missing");
  const [name, setName]   = useState("");
  const [pin, setPin]     = useState("");
  const [pin2, setPin2]   = useState("");
  const [busy, setBusy]   = useState(false);
  const [err,  setErr]    = useState<string | null>(null);

  // Mount: if user already has a valid token, skip the page entirely.
  useEffect(() => {
    if (!deviceId) return;
    const payload = auth.validPayload();
    if (payload) {
      // Already signed in to *some* home. Send them home; the
      // device claim status is independent. If they want to claim
      // this device into a different home, they'll sign out first.
      navigate("/", { replace: true });
      return;
    }
    setPhase("form");
  }, [deviceId, navigate]);

  // Phase F — "Quick start" path. Skips name + PIN entirely; the
  // backend mints an opaque "home-{16hex}" hid and returns a session
  // token + cookie in one round-trip. The user can promote to a
  // PIN-protected home later via Settings → Set a PIN.
  async function submitQuickStart() {
    setErr(null);
    if (!deviceId) { setErr("Missing device id — re-scan the QR"); return; }
    setBusy(true);
    try {
      const { token, hid } = await api.quickSetup(deviceId);
      auth.set(token, hid);
      // Same /?pair=… handoff as the PIN flow — the device is still
      // polling /api/pair/check and needs the dashboard's confirm-
      // pairing banner to fire POST /api/pair/confirm.
      navigate(`/?pair=${encodeURIComponent(deviceId)}`, { replace: true });
    } catch (e) {
      setErr(e instanceof Error ? e.message : "quick start failed");
    } finally {
      setBusy(false);
    }
  }

  async function submitCreate() {
    setErr(null);
    if (name.trim().length < 1) { setErr("Pick a home name"); return; }
    if (name.trim().length > 64) { setErr("Home name too long (max 64)"); return; }
    if (pin.length < 4)   { setErr("PIN must be at least 4 digits"); return; }
    if (pin !== pin2)     { setErr("PINs don't match"); return; }
    setBusy(true);
    try {
      const { token, hid } = await api.setup(name.trim(), pin, deviceId);
      auth.set(token, hid);
      // Carry the deviceId through to the dashboard so the
      // confirm-pairing banner appears. Phase A pair flow: the
      // device is in its "Pairing..." screen polling /api/pair/check;
      // the user clicks Confirm on the dashboard banner; that sends
      // POST /api/pair/confirm and the device picks up its
      // DeviceToken on the next poll.
      const next = deviceId
        ? `/?pair=${encodeURIComponent(deviceId)}`
        : "/";
      navigate(next, { replace: true });
    } catch (e) {
      if (e instanceof ApiError && e.status === 409) {
        // Name taken — bounce to login with name + device prefilled
        // so the next click just types the PIN.
        const url = `/login?hid=${encodeURIComponent(name.trim())}` +
                    (deviceId ? `&device=${encodeURIComponent(deviceId)}` : "");
        navigate(url, { replace: true });
        return;
      }
      // Surface the real server message so deploy/migration mismatches
      // (the most common failure right now) are visible instead of
      // landing the user in a confusing loop.
      setErr(e instanceof Error ? e.message : "create failed");
    } finally {
      setBusy(false);
    }
  }

  function goLogin() {
    const url = "/login" + (deviceId ? `?device=${encodeURIComponent(deviceId)}` : "");
    navigate(url, { replace: true });
  }

  return (
    <>
      <h1>FeedMe</h1>
      <div className="card">
        {phase === "missing" && (
          <>
            <h2>Pair a device</h2>
            <p className="muted">
              This page is the QR-scan landing target. Open the device's
              pairing screen and scan its QR — you'll come back here
              with the device ID auto-filled.
            </p>
            <button onClick={() => navigate("/login")} style={{ marginTop: 16 }}>
              Or log in with a home name
            </button>
          </>
        )}

        {phase === "checking" && (
          <>
            <h2>Pairing…</h2>
            <p className="muted">Checking session.</p>
          </>
        )}

        {phase === "error" && (
          <>
            <h2>Something went wrong</h2>
            <p className="muted">{err ?? "Try again in a moment."}</p>
            <button onClick={() => setPhase("form")} style={{ marginTop: 16 }}>
              Retry
            </button>
          </>
        )}

        {phase === "form" && (
          <>
            <h2>Create a new home</h2>
            <p className="muted">
              Pairing device <code>{deviceId}</code>. The fastest path
              is "Quick start" — we'll generate a private home ID for
              you and the device starts working immediately. You can
              add a PIN later from Settings if you want extra
              protection. Or, pick your own home name + PIN below.
            </p>
            <button
              disabled={busy}
              onClick={submitQuickStart}
              style={{ width: "100%", marginBottom: 12 }}
            >
              {busy ? "..." : "Quick start (no PIN)"}
            </button>
            <div style={{
              margin: "12px 0", paddingTop: 12,
              borderTop: "1px solid var(--theme-line, #2e2440)",
            }}>
              <p className="muted" style={{ marginTop: 0, marginBottom: 8 }}>
                Or create a PIN-protected home with a name you choose:
              </p>
            </div>
            <label>Home name</label>
            <input
              autoFocus
              type="text"
              maxLength={64}
              autoComplete="off"
              spellCheck={false}
              placeholder="Smith Family"
              value={name}
              onChange={(e) => setName(e.target.value)}
            />
            <label>PIN (4+ digits)</label>
            <input
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
              onKeyDown={(e) => { if (e.key === "Enter") submitCreate(); }}
            />
            <button disabled={busy} onClick={submitCreate} style={{ marginTop: 16 }}>
              {busy ? "..." : "Create home"}
            </button>

            <div style={{
              marginTop: 24, paddingTop: 16,
              borderTop: "1px solid var(--theme-line, #2e2440)",
            }}>
              <p className="muted" style={{ marginTop: 0 }}>
                Already have a home on another device? Sign in with
                its name + PIN — this device will join that home so
                its events show up there too.
              </p>
              <button
                className="secondary"
                onClick={goLogin}
                style={{ width: "100%" }}
              >
                Log in to an existing home
              </button>
            </div>
          </>
        )}

        {err && phase === "form" && <p className="error">{err}</p>}
      </div>
    </>
  );
}
