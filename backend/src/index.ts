import {
  authFromRequest,
  hashPin,
  issueToken,
  requireType,
  verifyPin,
} from "./auth";
import {
  createCat,
  deleteCat,
  getCats,
  updateCat,
} from "./cats";
import {
  getDashboardCats,
  getDashboardHistory,
  postDashboardFeed,
} from "./dashboard";
import type { Env } from "./env";
import {
  deletePair,
  getPairCheck,
  getPairList,
  postPairCancel,
  postPairConfirm,
  postPairStart,
} from "./pair";
import {
  getSyncLog,
  postSync,
} from "./sync";
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

// Translate a firmware-supplied device id (its self-generated `hid`,
// e.g. feedme-a8b3c1d4e5f6) into the home id (households.hid =
// user-chosen home name) it's been claimed into. Falls back to the
// raw device id when no claim exists — preserves single-device legacy
// behaviour for devices created before migration 0004 (the seed in
// that migration registers each existing household as its own
// device, so the lookup hits and returns the same value).
//
// Used by /api/feed, /api/state, /api/history — the firmware-facing
// endpoints that don't go through bearer-token auth.
async function resolveDeviceHome(env: Env, deviceId: string): Promise<string> {
  const row = await env.DB.prepare(
    "SELECT home_hid FROM devices WHERE device_id = ?",
  ).bind(deviceId).first<{ home_hid: string }>();
  return row?.home_hid ?? deviceId;
}

// Validation for user-chosen home names. Trim, cap, allow common
// printable characters. The hid lives in URLs, JSON bodies, and the
// SQLite primary key — so we forbid anything that needs special
// escaping (control chars, newlines). Spaces are fine; users will
// type "Smith Family" and we keep it as-is.
function validateHomeName(raw: string | undefined): string | null {
  if (typeof raw !== "string") return null;
  const trimmed = raw.trim();
  if (trimmed.length < 1 || trimmed.length > 64) return null;
  // Reject control chars (0x00-0x1F, 0x7F) — covers tab, newline,
  // null, etc. Everything else (letters, digits, spaces, punct,
  // unicode) is fine.
  if (/[\x00-\x1f\x7f]/.test(trimmed)) return null;
  return trimmed;
}

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
      const deviceHid = url.searchParams.get("hid");
      if (!deviceHid) return json({ error: "hid required" }, { status: 400 });
      // Firmware sends its own device id; translate to the home it
      // was claimed into so the events lookup hits the right rows.
      // Falls back to the raw value for unclaimed devices.
      const hid = await resolveDeviceHome(env, deviceHid);
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
      const hid = await resolveDeviceHome(env, body.hid);
      const ts = Math.floor(Date.now() / 1000);
      const type = body.type ?? "feed";
      const cat = body.cat ?? "primary";
      const eventId = body.eventId ?? null;
      await env.DB.prepare(
        "INSERT OR IGNORE INTO events (hid, ts, type, by, note, cat, event_id) " +
          "VALUES (?, ?, ?, ?, ?, ?, ?)",
      )
        .bind(hid, ts, type, body.by, body.note ?? null, cat, eventId)
        .run();
      return json({ ok: true, ts, type, by: body.by, cat, eventId });
    }

    if (url.pathname === "/api/history" && req.method === "GET") {
      const deviceHid = url.searchParams.get("hid");
      const n = Math.min(Number(url.searchParams.get("n") ?? 5), 50);
      if (!deviceHid) return json({ error: "hid required" }, { status: 400 });
      const hid = await resolveDeviceHome(env, deviceHid);
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

    // ── Pair lifecycle (Phase A — sync rework) ────────────────────
    // Three unauthenticated endpoints that the device drives during
    // its 3-min pairing window. The fourth (POST /api/pair/confirm)
    // is auth-required and lives below the auth guard. See pair.ts
    // for the full handshake sequence.

    if (url.pathname === "/api/pair/start" && req.method === "POST") {
      const body = await req.json().catch(() => null);
      return postPairStart(env, body);
    }
    if (url.pathname === "/api/pair/check" && req.method === "GET") {
      return getPairCheck(env, url);
    }
    if (url.pathname === "/api/pair/cancel" && req.method === "POST") {
      const body = await req.json().catch(() => null);
      return postPairCancel(env, body);
    }

    // POST /api/auth/exists { hid } → { exists: boolean }
    // Probes whether a home with this name (= hid post-migration-0004)
    // already exists. Webapp uses it to decide between "Create" and
    // "Log in" UI states.
    if (url.pathname === "/api/auth/exists" && req.method === "POST") {
      const body = (await req.json().catch(() => null)) as { hid?: string } | null;
      const hid = validateHomeName(body?.hid);
      if (!hid) return json({ error: "valid hid required" }, { status: 400 });
      const row = await env.DB.prepare(
        "SELECT 1 AS x FROM households WHERE hid = ?",
      ).bind(hid).first<{ x: number }>();
      return json({ exists: !!row });
    }

    // POST /api/auth/setup { hid, pin, deviceId? } → { token, hid }
    // Creates a NEW home. `hid` is the user-chosen home name (the
    // unique identifier — see migration 0004). `deviceId` is an
    // optional firmware id to claim into this home immediately, so
    // the QR-scan flow is one round-trip from "create" to "device
    // events flow into my home".
    //
    // 400 — hid invalid (empty / too long / control chars) or pin <4
    // 409 — name already taken
    // 200 — { token, hid }; auth.set on the client + navigate /
    if (url.pathname === "/api/auth/setup" && req.method === "POST") {
      const body = (await req.json().catch(() => null)) as
        { hid?: string; pin?: string; deviceId?: string } | null;
      const hid = validateHomeName(body?.hid);
      if (!hid) return json({ error: "valid home name required (1-64 chars)" }, { status: 400 });
      if (!body?.pin || body.pin.length < 4) {
        return json({ error: "pin too short (min 4)" }, { status: 400 });
      }

      const existing = await env.DB.prepare(
        "SELECT 1 AS x FROM households WHERE hid = ?",
      ).bind(hid).first<{ x: number }>();
      if (existing) return json({ error: "home name taken" }, { status: 409 });

      try {
        const { salt, hash } = await hashPin(body.pin);
        const now = Math.floor(Date.now() / 1000);
        // We still write to `name` for forward-compat; current code
        // displays hid (= name) but the column lives on for now.
        await env.DB.prepare(
          "INSERT INTO households (hid, pin_salt, pin_hash, created_at, name) VALUES (?, ?, ?, ?, ?)",
        ).bind(hid, salt, hash, now, hid).run();

        // Claim the device that scanned the QR, if one was passed.
        if (body.deviceId && typeof body.deviceId === "string") {
          await env.DB.prepare(
            "INSERT OR REPLACE INTO devices (device_id, home_hid, joined_at) VALUES (?, ?, ?)",
          ).bind(body.deviceId, hid, now).run();
          console.log(`[auth] setup '${hid}' claimed device '${body.deviceId}'`);
        } else {
          console.log(`[auth] setup '${hid}' (no device to claim)`);
        }
      } catch (e) {
        // Surface the underlying error so the client can show
        // something useful instead of a generic 500. Most likely
        // cause: migrations 0003/0004 not applied (missing column /
        // missing table). Reading e.message is safe — it's a string
        // describing the SQLite or D1 error.
        const msg = e instanceof Error ? e.message : String(e);
        console.error(`[auth] setup '${hid}' INSERT failed: ${msg}`);
        return json({ error: `setup failed: ${msg}` }, { status: 500 });
      }

      const token = await issueToken(hid, resolveSecret(env));
      return json({ token, hid });
    }

    // POST /api/auth/login { hid, pin, deviceId? } → { token, hid }
    // Authenticates against an EXISTING home. `hid` = home name.
    // `deviceId` is an optional firmware id to claim — used when a
    // newly-paired device's QR sends the user here ("I already have
    // a home; this device should join it").
    //
    // 400 — hid invalid or pin missing
    // 404 — no such home
    // 401 — wrong pin
    // 200 — { token, hid } + side effect: device claimed
    if (url.pathname === "/api/auth/login" && req.method === "POST") {
      const body = (await req.json().catch(() => null)) as
        { hid?: string; pin?: string; deviceId?: string } | null;
      const hid = validateHomeName(body?.hid);
      if (!hid || !body?.pin) {
        return json({ error: "hid and pin required" }, { status: 400 });
      }

      const row = await env.DB.prepare(
        "SELECT pin_salt, pin_hash FROM households WHERE hid = ?",
      ).bind(hid).first<{ pin_salt: string; pin_hash: string }>();
      if (!row) return json({ error: "no such home" }, { status: 404 });

      const ok = await verifyPin(body.pin, row.pin_salt, row.pin_hash);
      if (!ok) return json({ error: "wrong PIN" }, { status: 401 });

      // Optional device claim. We use INSERT OR REPLACE so re-claiming
      // an already-paired device just updates the joined_at timestamp
      // (and atomically moves it to a different home if the user
      // signed in elsewhere).
      if (body.deviceId && typeof body.deviceId === "string") {
        try {
          await env.DB.prepare(
            "INSERT OR REPLACE INTO devices (device_id, home_hid, joined_at) VALUES (?, ?, ?)",
          ).bind(body.deviceId, hid, Math.floor(Date.now() / 1000)).run();
          console.log(`[auth] login '${hid}' claimed device '${body.deviceId}'`);
        } catch (e) {
          // Don't fail the login on a claim-write error; just log.
          // The user will see they're signed in, and the device can
          // re-claim on the next QR scan. Most likely cause: missing
          // devices table → migration 0004 not applied.
          console.error(`[auth] device claim failed: ${e instanceof Error ? e.message : e}`);
        }
      }

      const token = await issueToken(hid, resolveSecret(env));
      return json({ token, hid });
    }

    // ── Web/phone-app authenticated routes ────────────────────────
    // Everything below this guard requires Authorization: Bearer <token>
    // and the token's hid must match the URL/body hid.

    const authed = await authFromRequest(req, resolveSecret(env));
    const requireAuth = (): Response | null => {
      if (!authed) return json({ error: "unauthorized" }, { status: 401 });
      return null;
    };

    // POST /api/sync (DeviceToken) — full LWW merge of cats / users
    // / home with the device's local state. Returns the canonical
    // server view (including tombstones). See sync.ts for the
    // protocol contract.
    if (url.pathname === "/api/sync" && req.method === "POST") {
      const deviceAuth = requireType(authed, "device");
      if (!deviceAuth) return json({ error: "device token required" }, { status: 401 });
      const body = await req.json().catch(() => null);
      return postSync(env, deviceAuth, body);
    }

    // GET /api/sync/log (UserToken) — drives the webapp's /sync-log
    // viewer. Optional ?n=<limit> (1..100) and ?device=<id>.
    if (url.pathname === "/api/sync/log" && req.method === "GET") {
      const userAuth = requireType(authed, "user");
      if (!userAuth) return json({ error: "user token required" }, { status: 401 });
      return getSyncLog(env, userAuth.hid, url);
    }

    // GET /api/pair/list (UserToken) — list of currently-paired
    // devices for the signed-in home. Drives Settings → Devices.
    if (url.pathname === "/api/pair/list" && req.method === "GET") {
      const userAuth = requireType(authed, "user");
      if (!userAuth) return json({ error: "user token required" }, { status: 401 });
      return getPairList(env, userAuth.hid);
    }

    // POST /api/pair/confirm { deviceId } (UserToken)
    // Webapp side of the pairing handshake. Creates the active pairings
    // row, mints a DeviceToken for the device, and stashes it on the
    // pending_pairings row so the device's next /api/pair/check returns
    // confirmed + token. See pair.ts for the full state machine.
    if (url.pathname === "/api/pair/confirm" && req.method === "POST") {
      const userAuth = requireType(authed, "user");
      if (!userAuth) return json({ error: "user token required" }, { status: 401 });
      const body = await req.json().catch(() => null);
      return postPairConfirm(env, userAuth, body, resolveSecret(env));
    }

    // DELETE /api/pair/<deviceId> (UserToken OR DeviceToken)
    // "Forget device" from the webapp side or "Reset" from the device
    // side, both unwind the active pairing and clear the legacy
    // devices-table row. Authorisation is checked inside deletePair —
    // a UserToken can only delete pairings in its own home, a
    // DeviceToken can only delete its own deviceId.
    {
      const m = url.pathname.match(/^\/api\/pair\/([a-zA-Z0-9-]+)$/);
      if (m && req.method === "DELETE") {
        const denied = requireAuth(); if (denied) return denied;
        return deletePair(env, authed!, m[1]);
      }
    }

    // GET /api/auth/me → { hid, created_at, deviceCount }
    // Returns the signed-in home's metadata: the name (= hid post-
    // migration-0004), creation time, and how many devices have
    // claimed into this home. Used by HomePage / SettingsPage so
    // they don't have to piggy-back on cats/users responses just to
    // pick up the display name. Auth required.
    if (url.pathname === "/api/auth/me" && req.method === "GET") {
      const denied = requireAuth(); if (denied) return denied;
      const row = await env.DB.prepare(
        "SELECT hid, created_at FROM households WHERE hid = ?",
      ).bind(authed!.hid).first<{ hid: string; created_at: number }>();
      if (!row) return json({ error: "no such home" }, { status: 404 });
      const devCountRow = await env.DB.prepare(
        "SELECT COUNT(*) AS n FROM devices WHERE home_hid = ?",
      ).bind(authed!.hid).first<{ n: number }>();
      return json({
        hid: row.hid,
        created_at: row.created_at,
        deviceCount: devCountRow?.n ?? 0,
      });
    }

    // DELETE /api/auth/household — "Forget this household". Wipes the
    // households row + every per-household record (cats, users) the
    // signed-in user owns. Events stay (they're keyed by hid; orphaned
    // rows are harmless and let firmware backfills survive). After
    // success the webapp clears its localStorage token + drops back
    // to /login. Re-pairing the same hid (or a fresh one from a
    // device-side reset) is then a clean "Set a PIN" flow.
    //
    // Auth required so a random POSTer can't nuke someone else's
    // household. The token's hid is the authority — we ignore any
    // body/path hid.
    if (url.pathname === "/api/auth/household" && req.method === "DELETE") {
      const denied = requireAuth(); if (denied) return denied;
      const hid = authed!.hid;
      // Run the deletes inline; no foreign keys means order doesn't
      // matter. D1 doesn't support multi-statement transactions in
      // free-tier so we do them as separate prepares.
      await env.DB.prepare("DELETE FROM cats        WHERE hid = ?").bind(hid).run();
      await env.DB.prepare("DELETE FROM users       WHERE hid = ?").bind(hid).run();
      await env.DB.prepare("DELETE FROM devices     WHERE home_hid = ?").bind(hid).run();
      await env.DB.prepare("DELETE FROM households  WHERE hid = ?").bind(hid).run();
      console.log(`[auth] forgot home '${hid}'`);
      return json({ ok: true, hid });
    }

    // ── Dashboard endpoints (auth-required, derive home from token) ──
    // Same data the firmware-facing /api/state + /api/feed + /api/history
    // expose, but: (a) auth-bound to the signed-in home so the client
    // never gets to pick a hid, and (b) one-shot per-cat aggregates
    // for the dashboard grid (last event + today's count) so rendering
    // is a single round-trip.

    // GET /api/dashboard/cats?tzOffset=<min>
    if (url.pathname === "/api/dashboard/cats" && req.method === "GET") {
      const denied = requireAuth(); if (denied) return denied;
      return getDashboardCats(env, authed!.hid, url);
    }
    // POST /api/dashboard/feed { catSlotId, by, type?, note? }
    if (url.pathname === "/api/dashboard/feed" && req.method === "POST") {
      const denied = requireAuth(); if (denied) return denied;
      const body = await req.json().catch(() => null);
      return postDashboardFeed(env, authed!.hid, body);
    }
    // GET /api/dashboard/history?cat=<slot>&n=<limit>
    if (url.pathname === "/api/dashboard/history" && req.method === "GET") {
      const denied = requireAuth(); if (denied) return denied;
      return getDashboardHistory(env, authed!.hid, url);
    }

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
