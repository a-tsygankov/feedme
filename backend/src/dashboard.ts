// Dashboard endpoints — auth-required, derive home from the bearer
// token. These are the webapp's equivalent of the firmware-facing
// /api/state, /api/feed, /api/history trio: they use the same events
// table and the same per-cat scoping but never accept a `hid` from
// the client (the token is the authority).
//
// Why a separate set of endpoints rather than reusing the firmware
// ones with optional auth? Two reasons:
//   1. The firmware endpoints accept an arbitrary hid in the request
//      and translate it via the `devices` table — a model that makes
//      sense for unauthenticated devices but is wrong for a logged-in
//      webapp where the home is fixed by the session.
//   2. The dashboard wants per-cat *aggregates* (last event + today's
//      count) for every cat in one round-trip, which the firmware
//      endpoints don't expose.

import type { Env } from "./env";

// ── shared helpers ────────────────────────────────────────────────
const json = (data: unknown, init: ResponseInit = {}) =>
  new Response(JSON.stringify(data), {
    ...init,
    headers: {
      "content-type": "application/json",
      "access-control-allow-origin": "*",
      ...(init.headers ?? {}),
    },
  });

// "Today" boundary in UTC seconds, but rolled over at the *local*
// midnight of the user's timezone. Mirrors the same math used by the
// firmware-facing /api/state — same offset semantics so a cat fed
// "today" looks the same in both surfaces.
const startOfLocalDayInUtc = (utcNow: number, tzOffsetSec: number) => {
  const localNow = utcNow + tzOffsetSec;
  const localDayStart = localNow - ((localNow % 86400) + 86400) % 86400;
  return localDayStart - tzOffsetSec;
};

// Cat referencing convention: the firmware POSTs `cat: "<slot_id>"`
// (string of the uint8 slot id) to /api/feed; events.cat stores it
// verbatim. The dashboard does the same — passes the slot id as a
// string in the `cat` field. The legacy default of "primary" only
// applies to events written before the multi-cat firmware shipped.

interface CatRow {
  slot_id: number;
  name: string;
  color: number;
  slug: string;
  default_portion_g: number;
  hungry_threshold_sec: number;
}

interface LastEventRow {
  ts: number;
  type: string;
  by: string;
}

// GET /api/dashboard/cats?tzOffset=-300
//
// Returns every active cat in the signed-in home, each enriched with
// the last event (ts/by/type) and today's feed count. One round-trip
// is enough to render the entire grid of mood rings + status lines
// the mockup describes.
//
// `tzOffset` is signed minutes east of UTC (the same unit as the
// firmware's TimeZone::offsetMin). Defaults to 0 (UTC) if omitted.
// The "today" cutoff is local midnight in that offset.
export async function getDashboardCats(
  env: Env,
  hid: string,
  url: URL,
): Promise<Response> {
  const tzOffsetMinRaw = url.searchParams.get("tzOffset");
  const tzOffsetMin = tzOffsetMinRaw !== null && !isNaN(Number(tzOffsetMinRaw))
    ? Number(tzOffsetMinRaw)
    : 0;
  const tzOffsetSec = tzOffsetMin * 60;

  const now = Math.floor(Date.now() / 1000);
  const todayStart = startOfLocalDayInUtc(now, tzOffsetSec);

  // Cats list (small N — capped at 4 by the firmware's MAX_CATS).
  const catsRes = await env.DB.prepare(
    `SELECT slot_id, name, color, slug, default_portion_g, hungry_threshold_sec
     FROM cats WHERE hid = ? AND deleted_at IS NULL
     ORDER BY slot_id ASC`,
  ).bind(hid).all<CatRow>();
  const cats = catsRes.results ?? [];

  // For each cat, fetch (a) last event and (b) today's feed count.
  // N is small so per-cat round-trips are fine; if N grows to dozens
  // we can collapse into a single window-function query.
  const enriched = await Promise.all(cats.map(async (c) => {
    const catId = String(c.slot_id);

    const last = await env.DB.prepare(
      "SELECT ts, type, by FROM events WHERE hid = ? AND cat = ? ORDER BY ts DESC LIMIT 1",
    ).bind(hid, catId).first<LastEventRow>();

    const todayRow = await env.DB.prepare(
      "SELECT COUNT(*) AS n FROM events WHERE hid = ? AND cat = ? AND type = 'feed' AND ts >= ?",
    ).bind(hid, catId, todayStart).first<{ n: number }>();

    return {
      slotId:             c.slot_id,
      name:               c.name,
      color:              c.color,
      slug:               c.slug,
      defaultPortionG:    c.default_portion_g,
      hungryThresholdSec: c.hungry_threshold_sec,
      lastFedAt:          last?.ts ?? null,
      lastFedBy:          last?.by ?? null,
      lastEventType:      last?.type ?? null,
      secondsSince:       last ? now - last.ts : null,
      todayCount:         todayRow?.n ?? 0,
    };
  }));

  return json({ now, tzOffsetMin, cats: enriched });
}

// POST /api/dashboard/feed { catSlotId, by, type?, note?, eventId? }
//
// Logs a feed (or snooze) event for the signed-in home. `catSlotId`
// is the integer slot_id; we store it stringified in events.cat to
// match the firmware's convention. `by` is the friendly user name.
// `type` defaults to "feed" — pass "snooze" for the "just begging"
// path that doesn't reset the hungry timer's mood semantics on the
// client side.
//
// `eventId` (optional but recommended) is a client-generated UUID
// that makes the call idempotent: the same id replayed (e.g. a
// network retry where the server actually processed the original)
// is silently dropped via the UNIQUE INDEX on `events.event_id`.
// Mirrors the firmware-facing `/api/feed` pattern. Without it,
// every retry creates a duplicate event row.
export async function postDashboardFeed(
  env: Env,
  hid: string,
  body: unknown,
): Promise<Response> {
  const b = (body ?? {}) as {
    catSlotId?: number;
    by?: string;
    type?: "feed" | "snooze";
    note?: string;
    eventId?: string;
  };
  if (typeof b.catSlotId !== "number" || !b.by) {
    return json({ error: "catSlotId and by required" }, { status: 400 });
  }
  const type = b.type === "snooze" ? "snooze" : "feed";
  const catId = String(b.catSlotId);
  const ts = Math.floor(Date.now() / 1000);
  // Cap eventId length defensively; UUIDs are 36 chars.
  const eventId = (typeof b.eventId === "string" && b.eventId.length <= 64)
    ? b.eventId : null;

  // INSERT OR IGNORE: the UNIQUE INDEX on events.event_id makes a
  // retry with the same eventId a no-op. Without an eventId we fall
  // back to plain INSERT (legacy behaviour — every call inserts).
  await env.DB.prepare(
    "INSERT OR IGNORE INTO events (hid, ts, type, by, note, cat, event_id) VALUES (?, ?, ?, ?, ?, ?, ?)",
  ).bind(hid, ts, type, b.by, b.note ?? null, catId, eventId).run();

  return json({ ok: true, ts, type, by: b.by, catSlotId: b.catSlotId, eventId });
}

// GET /api/dashboard/history?cat=<slotId>&n=<limit>
//
// Returns recent events for a single cat (or for all cats in the
// home when `cat` is omitted). `n` defaults to 10, capped at 50.
export async function getDashboardHistory(
  env: Env,
  hid: string,
  url: URL,
): Promise<Response> {
  const cat = url.searchParams.get("cat");
  const n = Math.min(Math.max(Number(url.searchParams.get("n") ?? 10), 1), 50);

  const stmt = cat
    ? env.DB.prepare(
        "SELECT id, ts, type, by, note, cat FROM events WHERE hid = ? AND cat = ? ORDER BY ts DESC LIMIT ?",
      ).bind(hid, cat, n)
    : env.DB.prepare(
        "SELECT id, ts, type, by, note, cat FROM events WHERE hid = ? ORDER BY ts DESC LIMIT ?",
      ).bind(hid, n);

  const { results } = await stmt.all();
  return json({ events: results ?? [] });
}
