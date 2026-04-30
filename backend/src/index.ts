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
  // Idempotency key — 128-bit hex UUID. Replays of the same eventId
  // get silently dropped via the unique index on `event_id`. Optional
  // for back-compat with pre-Phase-2.x clients that don't generate
  // ids; NULL writes are still unique-distinct in SQLite.
  eventId?: string;
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

// "Today" boundary in UTC seconds, but rolled over at the *local*
// midnight of the device's timezone. Math: shift utcNow by the
// offset to get localNow, floor to local midnight, then shift back
// by the same offset to land on the UTC ts cutoff that filter on
// `events.ts >= cutoff` selects today-local feeds.
//
// `tzOffsetSec` matches the firmware's TimeZone::offsetSec — signed
// seconds east of UTC. Defaults to 0 (UTC) for back-compat with
// pre-tz clients.
const startOfLocalDayInUtc = (utcNow: number, tzOffsetSec: number) => {
  const localNow = utcNow + tzOffsetSec;
  const localDayStart = localNow - ((localNow % 86400) + 86400) % 86400;
  return localDayStart - tzOffsetSec;
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
      // Optional timezone offset in MINUTES from UTC, signed
      // (matches firmware TimeZone::offsetMin). Used for the
      // `todayCount` boundary so the count rolls over at local
      // midnight, not UTC midnight. Omitted or unparseable → 0
      // (UTC); back-compat with pre-tz clients.
      const tzOffsetMinRaw = url.searchParams.get("tzOffset");
      const tzOffsetMin = tzOffsetMinRaw !== null && !isNaN(Number(tzOffsetMinRaw))
        ? Number(tzOffsetMinRaw)
        : 0;
      const tzOffsetSec = tzOffsetMin * 60;

      const last = await env.DB.prepare(
        "SELECT ts, type, by FROM events WHERE hid = ? AND cat = ? ORDER BY ts DESC LIMIT 1",
      )
        .bind(hid, cat)
        .first<{ ts: number; type: string; by: string }>();

      const now = Math.floor(Date.now() / 1000);
      const todayStart = startOfLocalDayInUtc(now, tzOffsetSec);
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
        tzOffsetMin,
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
      const eventId = body.eventId ?? null;
      // INSERT OR IGNORE silently dedups on the unique event_id
      // index. Replay of the same eventId (pending-queue retry that
      // the server already processed) is a no-op. Returns ok=true
      // either way — the event landed; the client doesn't need to
      // know whether this specific call wrote the row.
      await env.DB.prepare(
        "INSERT OR IGNORE INTO events (hid, ts, type, by, note, cat, event_id) " +
          "VALUES (?, ?, ?, ?, ?, ?, ?)",
      )
        .bind(body.hid, ts, type, body.by, body.note ?? null, cat, eventId)
        .run();
      return json({ ok: true, ts, type, by: body.by, cat, eventId });
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
            "SELECT id, ts, type, by, note, cat, event_id FROM events WHERE hid = ? AND cat = ? ORDER BY ts DESC LIMIT ?",
          ).bind(hid, cat, n)
        : env.DB.prepare(
            "SELECT id, ts, type, by, note, cat, event_id FROM events WHERE hid = ? ORDER BY ts DESC LIMIT ?",
          ).bind(hid, n);

      const { results } = await stmt.all();
      return json({ events: results });
    }

    return json({ error: "not found" }, { status: 404 });
  },
} satisfies ExportedHandler<Env>;
