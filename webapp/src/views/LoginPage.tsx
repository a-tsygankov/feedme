import { useEffect, useState } from "react";
import { useNavigate, useSearchParams } from "react-router-dom";
import { ApiError, api, auth } from "../lib/api";

// /login — sign in to an EXISTING home with its name + PIN. The home
// name IS the unique identifier (post-migration-0004); there is no
// separate hid to remember.
//
// Optional ?hid=…&device=… query params pre-fill the form and trigger
// a device claim on success. Two ways to land here:
//
//   1. From SetupPage's "Log in to an existing home" button (or its
//      409 redirect when the chosen name is taken). Both pass through
//      the device id so the device gets claimed into the home.
//
//   2. By typing /login directly (no params) — generic sign-in.
//
// Skip-if-already-signed-in: if a valid token is in localStorage,
// silently navigate("/"). Re-opening the app shouldn't make the
// user re-type credentials.
//
// Errors:
//   - 404 "no such home" → suggest "create a new home" link
//   - 401 "wrong PIN"   → inline error, retry
export default function LoginPage() {
  const navigate = useNavigate();
  const [params] = useSearchParams();
  const initialHid    = (params.get("hid") ?? "").trim();
  const initialDevice = (params.get("device") ?? "").trim();

  const [hid, setHid] = useState(initialHid);
  const [pin, setPin] = useState("");
  const [busy, setBusy] = useState(false);
  const [err,  setErr]  = useState<string | null>(null);
  const [unknown, setUnknown] = useState(false);   // 404 path → show create CTA

  // If we already have a valid session, skip the page.
  useEffect(() => {
    const payload = auth.validPayload();
    if (payload) navigate("/", { replace: true });
  }, [navigate]);

  async function submit() {
    setErr(null);
    setUnknown(false);
    if (!hid.trim())   { setErr("Type your home name");        return; }
    if (pin.length < 4) { setErr("PIN must be at least 4 digits"); return; }
    setBusy(true);
    try {
      const { token, hid: returnedHid } = await api.login(
        hid.trim(),
        pin,
        initialDevice || undefined,
      );
      auth.set(token, returnedHid);
      navigate("/", { replace: true });
    } catch (e) {
      if (e instanceof ApiError) {
        if (e.status === 404) { setUnknown(true); return; }
        if (e.status === 401) { setErr("Wrong PIN"); return; }
      }
      setErr(e instanceof Error ? e.message : "login failed");
    } finally {
      setBusy(false);
    }
  }

  function goCreate() {
    // Keep the device query so /setup can claim it on create.
    const url = "/setup" + (initialDevice ? `?device=${encodeURIComponent(initialDevice)}` : "");
    navigate(url, { replace: true });
  }

  return (
    <>
      <h1>FeedMe</h1>
      <div className="card">
        <h2>Sign in</h2>
        {initialDevice && (
          <p className="muted" style={{ marginTop: -4 }}>
            Pairing device <code>{initialDevice}</code> into the home
            you sign in to.
          </p>
        )}

        <label>Home name</label>
        <input
          autoFocus={!initialHid}
          autoCapitalize="off"
          autoCorrect="off"
          spellCheck={false}
          placeholder="Smith Family"
          value={hid}
          onChange={(e) => setHid(e.target.value)}
          maxLength={64}
        />

        <label>PIN</label>
        <input
          autoFocus={!!initialHid}
          type="password"
          inputMode="numeric"
          pattern="[0-9]*"
          autoComplete="current-password"
          value={pin}
          onChange={(e) => setPin(e.target.value)}
          onKeyDown={(e) => { if (e.key === "Enter") submit(); }}
        />

        <button disabled={busy} onClick={submit} style={{ marginTop: 16 }}>
          {busy ? "..." : "Sign in"}
        </button>

        {unknown && (
          <div style={{ marginTop: 16 }}>
            <p className="error" style={{ marginBottom: 8 }}>
              No home named <code>{hid}</code>.
            </p>
            <button
              className="secondary"
              onClick={goCreate}
              style={{ width: "100%" }}
            >
              Create a new home instead
            </button>
          </div>
        )}

        {err && <p className="error">{err}</p>}

        {!unknown && initialDevice && (
          <div style={{
            marginTop: 24, paddingTop: 16,
            borderTop: "1px solid var(--theme-line, #2e2440)",
          }}>
            <button
              className="secondary"
              onClick={goCreate}
              style={{ width: "100%" }}
            >
              No home yet — create one
            </button>
          </div>
        )}
      </div>
    </>
  );
}
