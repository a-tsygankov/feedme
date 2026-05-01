import { useEffect, useState } from "react";
import { ApiError, type Cat, api, auth } from "../lib/api";
import { resolveCatColor, toCssHex } from "../lib/palette";

// Dashboard / home. v1 just shows the cats with their colour swatches
// and the household ID. Later iterations layer in:
//   - last-fed times per cat (today's count)
//   - quick-feed buttons
//   - tiny daily-feed sparkline
//   - notification status
export default function HomePage() {
  const [cats, setCats] = useState<Cat[] | null>(null);
  const [err, setErr] = useState<string | null>(null);

  useEffect(() => {
    let cancelled = false;
    (async () => {
      try {
        const { cats } = await api.catsList();
        if (!cancelled) setCats(cats);
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

  return (
    <>
      <h1>Home</h1>
      <p className="muted">{auth.hid()}</p>
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
