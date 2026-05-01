import { useEffect, useState } from "react";
import { ApiError, type Cat, type HomeInfo, api, auth } from "../lib/api";
import { resolveCatColor, toCssHex } from "../lib/palette";

// Dashboard / home. v1 shows the home name + cats with their colour
// swatches. Later iterations layer in:
//   - last-fed times per cat (today's count)
//   - quick-feed buttons
//   - tiny daily-feed sparkline
//   - notification status
export default function HomePage() {
  const [home, setHome] = useState<HomeInfo | null>(null);
  const [cats, setCats] = useState<Cat[] | null>(null);
  const [err, setErr] = useState<string | null>(null);

  useEffect(() => {
    let cancelled = false;
    (async () => {
      try {
        // Fire both calls in parallel — they're independent. Cats is
        // the dominant payload; /me is a few-byte JSON.
        const [meRes, catsRes] = await Promise.all([
          api.me().catch(() => null),
          api.catsList(),
        ]);
        if (cancelled) return;
        if (meRes) setHome(meRes);
        setCats(catsRes.cats);
      } catch (e) {
        if (cancelled) return;
        if (e instanceof ApiError && e.status === 401) {
          auth.clear();
          location.replace("/login");
          return;
        }
        setErr(e instanceof Error ? e.message : "load failed");
      }
    })();
    return () => { cancelled = true; };
  }, []);

  // hid IS the home name (post-migration-0004). Use the live /me
  // value when it lands, fall back to the cached auth.hid() so the
  // page renders something during the brief request.
  const displayName = home?.hid || auth.hid() || "Home";
  const deviceCount = home?.deviceCount;

  return (
    <>
      <h1>{displayName}</h1>
      {deviceCount !== undefined && (
        <p className="muted">
          {deviceCount} device{deviceCount === 1 ? "" : "s"} paired
        </p>
      )}
      <div className="card">
        <h2>Cats</h2>
        {err && <p className="error">{err}</p>}
        {!cats && !err && <p className="muted">Loading…</p>}
        {cats && cats.length === 0 && <p className="muted">No cats yet. Add one in Cats.</p>}
        {cats?.map(c => (
          <div key={c.slotId} className="row">
            <span className="swatch" style={{ background: toCssHex(resolveCatColor(c.slotId, c.color)) }} />
            <span style={{ flex: 1 }}>{c.name}</span>
            <span className="muted">{c.defaultPortionG} g</span>
          </div>
        ))}
      </div>
    </>
  );
}
