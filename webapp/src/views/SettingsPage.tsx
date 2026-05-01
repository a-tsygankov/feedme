import { useNavigate } from "react-router-dom";
import { auth } from "../lib/api";

// Settings stub. Phase 1 holds the household ID, the version label,
// and Sign-out. Phase 2 will gain: change PIN, statistics window
// preference, push-notification opt-in, etc.
export default function SettingsPage() {
  const navigate = useNavigate();
  const hid = auth.hid();

  function signOut() {
    if (!confirm("Sign out of FeedMe on this device?")) return;
    auth.clear();
    navigate("/login", { replace: true });
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

      <button className="danger" onClick={signOut} style={{ width: "100%" }}>
        Sign out
      </button>
    </>
  );
}
