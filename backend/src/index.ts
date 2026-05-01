import {
  authFromRequest,
  hashPin,
  issueToken,
  verifyPin,
} from "./auth";
import {
  createCat,
  deleteCat,
  getCats,
  updateCat,
} from "./cats";
import type { Env } from "./env";
import {
  createUser,
  deleteUser,
  getUsers,
  updateUser,
} from "./users";

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

// AUTH_SECRET fallback — production deployment MUST set this via
// `wrangler secret put AUTH_SECRET`. The fallback exists for
// `wrangler dev` smoke tests only; logging a loud warning every
// request so a missing prod secret is impossible to miss.
const DEV_FALLBACK_SECRET = "dev-only-replace-via-wrangler-secret-put";
const resolveSecret = (env: Env): string => {
  if (env.AUTH_SECRET && env.AUTH_SECRET.length > 0) return env.AUTH_SECRET;
  console.warn("[auth] AUTH_SECRET not set — using dev fallback. NOT FOR PRODUCTION.");
  return DEV_FALLBACK_SECRET;
};

// Match `/api/cats/123` etc. Returns parsed slot id (uint8) or null
// for the bare collection path. -1 sentinel = malformed slot.
const parseSlotPath = (pathname: string, collection: string): number | null => {
  const prefix = `/api/${collection}/`;
  if (!pathname.startsWith(prefix)) return null;
  const tail = pathname.slice(prefix.length);
  const n = Number(tail);
  if (!Number.isInteger(n) || n < 0 || n > 255) return -1;
  return n;
};

export default {
  async fetch(req: Request, env: Env): Promise<Response> {
    const url = new URL(req.url);

    if (req.method === "OPTIONS") {
      return new Response(null, {
        headers: {
          "access-control-allow-origin": "*",
          "access-control-allow-methods": "GET,POST,PATCH,DELETE,OPTIONS",
          "access-control-allow-headers": "content-type,authorization",
        },
      });
    }

    // ── Existing firmware-facing endpoints ────────────────────────
    // No auth — the device knows its own hid and predates the
    // session-token model. The web app uses a different surface
    // below, all bearer-token gated.

    if (url.pathname === "/api/state" && req.method === "GET") {
      const hid = url.searchParams.get("hid");
      if (!hid) return json({ error: "hid required" }, { status: 400 });
      const cat = url.searchParams.get("cat") ?? "primary";
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

    // ── Web/phone-app auth ────────────────────────────────────────

    // POST /api/auth/exists { hid } → { exists: boolean }
    // Used by the login screen to decide between "enter PIN" and
    // "set up new household".
    if (url.pathname === "/api/auth/exists" && req.method === "POST") {
      const body = (await req.json().catch(() => null)) as { hid?: string } | null;
      if (!body?.hid) return json({ error: "hid required" }, { status: 400 });
      const row = await env.DB.prepare(
        "SELECT 1 AS x FROM households WHERE hid = ?",
      ).bind(body.hid).first<{ x: number }>();
      return json({ exists: !!row });
    }

    // POST /api/auth/setup { hid, pin } → { token } (issues a session)
    // 409 if the household already exists — call /api/auth/login instead.
    if (url.pathname === "/api/auth/setup" && req.method === "POST") {
      const body = (await req.json().catch(() => null)) as { hid?: string; pin?: string } | null;
      if (!body?.hid || !body?.pin) return json({ error: "hid and pin required" }, { status: 400 });
      if (body.pin.length < 4) return json({ error: "pin too short (min 4)" }, { status: 400 });

      const existing = await env.DB.prepare(
        "SELECT 1 AS x FROM households WHERE hid = ?",
      ).bind(body.hid).first<{ x: number }>();
      if (existing) return json({ error: "household exists — log in" }, { status: 409 });

      const { salt, hash } = await hashPin(body.pin);
      await env.DB.prepare(
        "INSERT INTO households (hid, pin_salt, pin_hash, created_at) VALUES (?, ?, ?, ?)",
      ).bind(body.hid, salt, hash, Math.floor(Date.now() / 1000)).run();

      const token = await issueToken(body.hid, resolveSecret(env));
      return json({ token, hid: body.hid });
    }

    // POST /api/auth/login { hid, pin } → { token }
    // 404 if household doesn't exist, 401 if PIN wrong.
    if (url.pathname === "/api/auth/login" && req.method === "POST") {
      const body = (await req.json().catch(() => null)) as { hid?: string; pin?: string } | null;
      if (!body?.hid || !body?.pin) return json({ error: "hid and pin required" }, { status: 400 });

      const row = await env.DB.prepare(
        "SELECT pin_salt, pin_hash FROM households WHERE hid = ?",
      ).bind(body.hid).first<{ pin_salt: string; pin_hash: string }>();
      if (!row) return json({ error: "household not found" }, { status: 404 });

      const ok = await verifyPin(body.pin, row.pin_salt, row.pin_hash);
      if (!ok) return json({ error: "wrong PIN" }, { status: 401 });

      const token = await issueToken(body.hid, resolveSecret(env));
      return json({ token, hid: body.hid });
    }

    // ── Web/phone-app authenticated routes ────────────────────────
    // Everything below this guard requires Authorization: Bearer <token>
    // and the token's hid must match the URL/body hid.

    const authed = await authFromRequest(req, resolveSecret(env));
    const requireAuth = (): Response | null => {
      if (!authed) return json({ error: "unauthorized" }, { status: 401 });
      return null;
    };

    // GET /api/cats — list active cats for the authed household.
    if (url.pathname === "/api/cats" && req.method === "GET") {
      const denied = requireAuth(); if (denied) return denied;
      return getCats(env, authed!.hid);
    }
    if (url.pathname === "/api/cats" && req.method === "POST") {
      const denied = requireAuth(); if (denied) return denied;
      const body = await req.json().catch(() => null);
      return createCat(env, authed!.hid, body);
    }

    {
      const slot = parseSlotPath(url.pathname, "cats");
      if (slot !== null) {
        const denied = requireAuth(); if (denied) return denied;
        if (slot < 0) return json({ error: "bad slot id" }, { status: 400 });
        if (req.method === "PATCH") {
          const body = await req.json().catch(() => null);
          return updateCat(env, authed!.hid, slot, body);
        }
        if (req.method === "DELETE") {
          return deleteCat(env, authed!.hid, slot);
        }
        return json({ error: "method not allowed" }, { status: 405 });
      }
    }

    if (url.pathname === "/api/users" && req.method === "GET") {
      const denied = requireAuth(); if (denied) return denied;
      return getUsers(env, authed!.hid);
    }
    if (url.pathname === "/api/users" && req.method === "POST") {
      const denied = requireAuth(); if (denied) return denied;
      const body = await req.json().catch(() => null);
      return createUser(env, authed!.hid, body);
    }
    {
      const slot = parseSlotPath(url.pathname, "users");
      if (slot !== null) {
        const denied = requireAuth(); if (denied) return denied;
        if (slot < 0) return json({ error: "bad slot id" }, { status: 400 });
        if (req.method === "PATCH") {
          const body = await req.json().catch(() => null);
          return updateUser(env, authed!.hid, slot, body);
        }
        if (req.method === "DELETE") {
          return deleteUser(env, authed!.hid, slot);
        }
        return json({ error: "method not allowed" }, { status: 405 });
      }
    }

    return json({ error: "not found" }, { status: 404 });
  },
} satisfies ExportedHandler<Env>;
