export interface Env {
  DB: D1Database;
}

interface FeedBody {
  hid: string;
  by: string;
  type?: "feed" | "snooze";
  note?: string;
  // Per-cat scope. Defaults server-side to "primary" — the silent
  // single-cat household value. The firmware passes the stable
  // Cat::id when it learns to send per-cat events.
  cat?: string;
}

const json = (data: unknown, init: ResponseInit = {}) =>
  new Response(JSON.stringify(data), {
    ...init,
    headers: {
      "content-type": "application/json",
      "access-control-allow-origin": "*",
      ...(init.headers ?? {}),
    },
  });

const startOfTodayUtc = (now: number) => {
  const d = new Date(now * 1000);
  d.setUTCHours(0, 0, 0, 0);
  return Math.floor(d.getTime() / 1000);
};

export default {
  async fetch(req: Request, env: Env): Promise<Response> {
    const url = new URL(req.url);

    if (req.method === "OPTIONS") {
      return new Response(null, {
        headers: {
          "access-control-allow-origin": "*",
          "access-control-allow-methods": "GET,POST,OPTIONS",
          "access-control-allow-headers": "content-type",
        },
      });
    }

    if (url.pathname === "/api/state" && req.method === "GET") {
      const hid = url.searchParams.get("hid");
      if (!hid) return json({ error: "hid required" }, { status: 400 });
      // Optional per-cat filter. Omitted = "primary" (silent default
      // for single-cat households + back-compat for pre-multi-cat
      // clients that don't send the field).
      const cat = url.searchParams.get("cat") ?? "primary";

      const last = await env.DB.prepare(
        "SELECT ts, type, by FROM events WHERE hid = ? AND cat = ? ORDER BY ts DESC LIMIT 1",
      )
        .bind(hid, cat)
        .first<{ ts: number; type: string; by: string }>();

      const now = Math.floor(Date.now() / 1000);
      const todayStart = startOfTodayUtc(now);
      const countRow = await env.DB.prepare(
        "SELECT COUNT(*) AS c FROM events WHERE hid = ? AND cat = ? AND type = 'feed' AND ts >= ?",
      )
        .bind(hid, cat, todayStart)
        .first<{ c: number }>();

      return json({
        last,
        secondsSince: last ? now - last.ts : null,
        todayCount: countRow?.c ?? 0,
        cat,
        now,
      });
    }

    if (url.pathname === "/api/feed" && req.method === "POST") {
      const body = (await req.json().catch(() => null)) as FeedBody | null;
      if (!body?.hid || !body?.by) {
        return json({ error: "hid and by required" }, { status: 400 });
      }
      const ts = Math.floor(Date.now() / 1000);
      const type = body.type ?? "feed";
      const cat = body.cat ?? "primary";
      await env.DB.prepare(
        "INSERT INTO events (hid, ts, type, by, note, cat) VALUES (?, ?, ?, ?, ?, ?)",
      )
        .bind(body.hid, ts, type, body.by, body.note ?? null, cat)
        .run();
      return json({ ok: true, ts, type, by: body.by, cat });
    }

    if (url.pathname === "/api/history" && req.method === "GET") {
      const hid = url.searchParams.get("hid");
      const n = Math.min(Number(url.searchParams.get("n") ?? 5), 50);
      if (!hid) return json({ error: "hid required" }, { status: 400 });
      // Optional cat filter — omitted returns events across all cats
      // (multi-cat households want the cross-cat timeline for the
      // history overlay; per-cat filter is for cat-specific UIs).
      const cat = url.searchParams.get("cat");

      const stmt = cat
        ? env.DB.prepare(
            "SELECT id, ts, type, by, note, cat FROM events WHERE hid = ? AND cat = ? ORDER BY ts DESC LIMIT ?",
          ).bind(hid, cat, n)
        : env.DB.prepare(
            "SELECT id, ts, type, by, note, cat FROM events WHERE hid = ? ORDER BY ts DESC LIMIT ?",
          ).bind(hid, n);

      const { results } = await stmt.all();
      return json({ events: results });
    }

    return json({ error: "not found" }, { status: 404 });
  },
} satisfies ExportedHandler<Env>;
