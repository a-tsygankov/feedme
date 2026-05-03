import { useEffect, useRef, useState } from "react";
import { useNavigate, useSearchParams } from "react-router-dom";
import { ApiError, api, auth } from "../lib/api";

// /qr-login — Phase F. Phone-side landing for the device's Login QR.
//
// The device is showing a one-shot QR encoding:
//
//   https://feedme-webapp.pages.dev/qr-login?device=feedme-{mac6}&token={32hex}
//
// The token is short-lived (60 s), single-use, and bound to the
// device that minted it. We exchange it for a session token + cookie
// on POST /api/auth/login-qr and bounce to /. No PIN typing, no home
// name to remember — that's the entire point of the flow.
//
// Failure modes (all from the server's response):
//   404 unknown token   — the QR is stale or never existed
//   403 deviceId mismatch — device + token don't match (replay attempt)
//   410 already consumed / expired — someone else scanned this QR, or
//        we waited >60 s. Tell the user to re-show the QR on the device.
//
// Token-aware: if a valid stored session is already present we skip
// the exchange entirely and navigate("/"). Re-opening the link on a
// signed-in browser shouldn't waste the one-shot token.
export default function QrLoginPage() {
  const [params] = useSearchParams();
  const navigate = useNavigate();
  const deviceId = (params.get("device") ?? "").trim();
  const token    = (params.get("token")  ?? "").trim();

  const [phase, setPhase] = useState<"idle" | "exchanging" | "done" | "error">("idle");
  const [err,   setErr]   = useState<string | null>(null);
  // React 18 StrictMode mounts every component twice in dev, which would
  // burn through our one-shot QR token instantly. Guard with a ref so
  // the exchange only fires once per real mount.
  const fired = useRef(false);

  useEffect(() => {
    if (fired.current) return;
    fired.current = true;

    if (!deviceId || !token) {
      setErr("Missing device or token in the QR URL.");
      setPhase("error");
      return;
    }
    // Already signed in — skip the exchange. The user can navigate
    // to a different home by signing out first; that's deliberate.
    if (auth.validPayload()) {
      navigate("/", { replace: true });
      return;
    }

    setPhase("exchanging");
    api.loginQr(deviceId, token)
      .then(({ token: sessionToken, hid }) => {
        auth.set(sessionToken, hid);
        setPhase("done");
        navigate("/", { replace: true });
      })
      .catch((e) => {
        if (e instanceof ApiError) {
          if (e.status === 404) setErr("This QR is unknown — re-show the QR on the device.");
          else if (e.status === 410) setErr("This QR has expired or was already used. Re-show it on the device.");
          else if (e.status === 403) setErr("Device mismatch — the QR doesn't match its source device.");
          else setErr(e.message);
        } else {
          setErr(e instanceof Error ? e.message : "login failed");
        }
        setPhase("error");
      });
  }, [deviceId, token, navigate]);

  return (
    <>
      <h1>FeedMe</h1>
      <div className="card">
        {phase === "idle" || phase === "exchanging" ? (
          <>
            <h2>Signing in…</h2>
            <p className="muted">
              Pairing this phone with device <code>{deviceId || "?"}</code>.
            </p>
          </>
        ) : phase === "done" ? (
          <>
            <h2>Signed in</h2>
            <p className="muted">Taking you to your home…</p>
          </>
        ) : (
          <>
            <h2>Couldn't sign in</h2>
            <p className="error">{err}</p>
            <button
              onClick={() => navigate("/login", { replace: true })}
              style={{ marginTop: 16, width: "100%" }}
            >
              Sign in with home name + PIN
            </button>
          </>
        )}
      </div>
    </>
  );
}
