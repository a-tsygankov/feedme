// Sync endpoints — Phase B of the multi-device rework.
//
// POST /api/sync (DeviceToken)
//   Device sends its full local state (home, cats, users) plus the
//   timestamp of its last successful sync. Server merges each entity
//   under Last-Write-Wins on `updated_at`, returns the resulting
//   server-canonical view (including tombstones so the device drops
//   any locally-active rows the server has marked deleted), and
//   appends one row to sync_logs.
//
// GET /api/sync/log (UserToken)
//   Read-only audit list, last 100 rows for the signed-in home,
//   optionally filtered by device. Drives the webapp's /sync-log
//   viewer.
//
// Wire format and merge rules: docs/sync-implementation-handoff.md §5.
//
// What's NOT in this module:
//   - UUID-based identity (Phase D). Today the merge keys cats and
//     users on (hid, slot_id) — collisions when two unpaired devices
//     each create slot 0 are an accepted v1 limitation.
//   - Schedule promoted to its own table (Q2 = embedded in Cat).
//   - Server-side delta encoding. Each sync uploads + downloads the
//     full per-home state; cheap at our scale (≤4 cats, ≤4 users).

import type { AuthInfo } from "./auth";
import type { Env } from "./env";

const SCHEMA_VERSION = 1;
const DEFAULT_SYNC_INTERVAL_SEC = 4 * 60 * 60;     // 4h fallback if migration 0009 not yet applied
const SYNC_LOG_RETENTION = 100;                     // ring-buffer cap per home
const CONFLICT_WINDOW_SEC = 5;                      // |Δupdated_at|<5s = "racey"
const DEFAULT_SCHEDULE_JSON = "[7,12,18,21]";       // firmware MealSchedule defaults

// ── helpers ──────────────────────────────────────────────────────
const json = (data: unknown, init: ResponseInit = {}) =>
  new Response(JSON.stringify(data), {
    ...init,
    headers: {
      "content-type": "application/json",
      "access-control-allow-origin": "*",
      ...(init.headers ?? {}),
    },
  });

// Wire-format types — match docs/sync-implementation-handoff.md §5.1.
//
// Phase D: `uuid` is the canonical identity. slotId is now device-
// local rendering ordering. Legacy clients that don't send uuid fall
// through to (hid, slot_id) lookup; the response always includes
// uuid so the client picks it up + uses it on the next sync.
interface SyncCat {
  uuid?:              string;     // 32-char lowercase hex; optional on req
  slotId:             number;
  name:               string;
  color:              number;
  slug:               string;
  defaultPortionG:    number;
  hungryThresholdSec: number;
  scheduleHours:      number[];   // 4 ints, hour-of-day 0..23
  createdAt:          number;
  updatedAt:          number;
  isDeleted:          boolean;
}

interface SyncUser {
  uuid?:     string;     // 32-char lowercase hex; optional on req
  slotId:    number;
  name:      string;
  color:     number;
  createdAt: number;
  updatedAt: number;
  isDeleted: boolean;
}

interface SyncHome {
  name:      string;
  updatedAt: number;
}

interface SyncRequest {
  schemaVersion: number;
  deviceId:      string;
  lastSyncAt:    number | null;
  home:          SyncHome;
  cats:          SyncCat[];
  users:         SyncUser[];
}

// ── type guards (defensive parsing of JSON bodies from a device
//    that might be running mismatched firmware) ────────────────────
export function isInt(n: unknown): n is number {
  return typeof n === "number" && Number.isFinite(n) && Number.isInteger(n);
}
// 32-char lowercase hex (16 bytes). Lowercase enforced so two
// devices that disagree on case can't both register the "same" uuid.
const UUID_RE = /^[0-9a-f]{32}$/;
export function isUuid(x: unknown): x is string {
  return typeof x === "string" && UUID_RE.test(x);
}
export function isSyncCat(c: unknown): c is SyncCat {
  if (typeof c !== "object" || c === null) return false;
  const o = c as Record<string, unknown>;
  if (o.uuid !== undefined && !isUuid(o.uuid))                          return false;
  if (!isInt(o.slotId) || o.slotId < 0 || o.slotId > 255)              return false;
  if (typeof o.name !== "string")                                       return false;
  if (!isInt(o.color))                                                  return false;
  if (typeof o.slug !== "string")                                       return false;
  if (!isInt(o.defaultPortionG))                                        return false;
  if (typeof o.hungryThresholdSec !== "number")                         return false;
  if (!Array.isArray(o.scheduleHours) || o.scheduleHours.length !== 4)  return false;
  if (!o.scheduleHours.every(h => isInt(h) && h >= 0 && h < 24))        return false;
  if (!isInt(o.createdAt) || !isInt(o.updatedAt))                       return false;
  if (typeof o.isDeleted !== "boolean")                                 return false;
  return true;
}
export function isSyncUser(u: unknown): u is SyncUser {
  if (typeof u !== "object" || u === null) return false;
  const o = u as Record<string, unknown>;
  if (o.uuid !== undefined && !isUuid(o.uuid))             return false;
  if (!isInt(o.slotId) || o.slotId < 0 || o.slotId > 255) return false;
  if (typeof o.name !== "string")                          return false;
  if (!isInt(o.color))                                     return false;
  if (!isInt(o.createdAt) || !isInt(o.updatedAt))          return false;
  if (typeof o.isDeleted !== "boolean")                    return false;
  return true;
}

// ── merge primitives ─────────────────────────────────────────────
// Each merge function returns the conflict count contribution: 1
// if the incoming and existing updated_at are within CONFLICT_WINDOW_SEC
// of each other AND DIFFERENT (suggests two devices wrote near-
// simultaneously). 0 when they're identical (steady-state echo —
// same data round-tripping unchanged) or when one is far ahead of
// the other (one side is just stale).
//
// The `diff > 0` guard is the bug fix that stops every sync from
// reporting a conflict for every entity. Pre-fix, a freshly-paired
// device whose roster hadn't changed would still show N conflicts
// (one per cat/user) on every sync because `Math.abs(X - X) = 0`
// was being counted as "within the window."
function isConflict(clientUpdatedAt: number, serverUpdatedAt: number): boolean {
  const diff = Math.abs(clientUpdatedAt - serverUpdatedAt);
  return diff > 0 && diff < CONFLICT_WINDOW_SEC;
}

async function mergeHome(env: Env, hid: string, h: SyncHome): Promise<number> {
  const row = await env.DB.prepare(
    "SELECT updated_at FROM households WHERE hid = ?",
  ).bind(hid).first<{ updated_at: number }>();
  // pairing check upstream guarantees the row exists; if it doesn't,
  // the device is paired to a deleted home — treat as no-op.
  if (!row) return 0;
  if (h.updatedAt > row.updated_at) {
    // Renaming a home means rewriting hid (the PK) AND every cats /
    // users / events / pairings reference. That's an explicit
    // "rename" feature, out of scope here. We just bump updated_at
    // so the device's next sync sees an acknowledgement.
    await env.DB.prepare(
      "UPDATE households SET updated_at = ? WHERE hid = ?",
    ).bind(h.updatedAt, hid).run();
  }
  return isConflict(h.updatedAt, row.updated_at) ? 1 : 0;
}

// uuid lookup wins over (hid, slot_id) when the client provides one.
// Lets two devices each adding "slot 0" coexist as separate rows
// keyed by their UUIDs — Phase D's whole point.
async function findCatRow(
  env: Env, hid: string, c: SyncCat,
): Promise<{ updated_at: number; uuid: string | null; slot_id: number } | null> {
  if (c.uuid) {
    const byUuid = await env.DB.prepare(
      "SELECT updated_at, uuid, slot_id FROM cats WHERE hid = ? AND uuid = ?",
    ).bind(hid, c.uuid).first<{ updated_at: number; uuid: string | null; slot_id: number }>();
    if (byUuid) return byUuid;
  }
  // Fallback: legacy device that doesn't know its uuid yet (or a
  // brand-new uuid that doesn't exist server-side). Matches by
  // (hid, slot_id) so the response can teach the client its uuid.
  return await env.DB.prepare(
    "SELECT updated_at, uuid, slot_id FROM cats WHERE hid = ? AND slot_id = ?",
  ).bind(hid, c.slotId).first<{ updated_at: number; uuid: string | null; slot_id: number }>();
}

// Generate a 32-char lowercase hex UUID matching the firmware's
// format (`esp_random()` 4× joined). Server uses crypto.randomUUID
// then strips the hyphens and lowercases — same entropy.
function newUuid(): string {
  return crypto.randomUUID().replace(/-/g, "").toLowerCase();
}

async function mergeCat(env: Env, hid: string, c: SyncCat): Promise<number> {
  const row = await findCatRow(env, hid, c);
  const scheduleJson = JSON.stringify(c.scheduleHours);
  // Resolve final uuid: client's wins if present, else server's
  // existing row, else mint a fresh one for INSERT.
  const uuid = c.uuid ?? row?.uuid ?? newUuid();

  if (!row) {
    // Server didn't know about this cat (under either uuid OR slot_id
    // — both lookups missed). INSERT verbatim with the resolved uuid.
    await env.DB.prepare(
      `INSERT INTO cats
        (hid, slot_id, name, color, slug, default_portion_g,
         hungry_threshold_sec, schedule_hours,
         created_at, updated_at, is_deleted, uuid)
       VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`,
    ).bind(hid, c.slotId, c.name, c.color, c.slug,
           c.defaultPortionG, c.hungryThresholdSec, scheduleJson,
           c.createdAt, c.updatedAt, c.isDeleted ? 1 : 0, uuid).run();
    return 0;
  }

  const conflict = isConflict(c.updatedAt, row.updated_at) ? 1 : 0;
  if (c.updatedAt <= row.updated_at) return conflict;     // server wins

  // Client wins → UPDATE. Backfill uuid if the row was missing one
  // (legacy migration: row created before migration 0008 ran).
  // Use the row's slot_id (not c.slotId) to handle the case where
  // the client renumbered slots locally but the uuid still matches.
  await env.DB.prepare(
    `UPDATE cats SET name = ?, color = ?, slug = ?,
       default_portion_g = ?, hungry_threshold_sec = ?, schedule_hours = ?,
       updated_at = ?, is_deleted = ?,
       deleted_at = CASE WHEN ? = 1 THEN ? ELSE NULL END,
       uuid = COALESCE(uuid, ?)
     WHERE hid = ? AND slot_id = ?`,
  ).bind(c.name, c.color, c.slug,
         c.defaultPortionG, c.hungryThresholdSec, scheduleJson,
         c.updatedAt, c.isDeleted ? 1 : 0,
         c.isDeleted ? 1 : 0, c.updatedAt,
         uuid,
         hid, row.slot_id).run();
  return conflict;
}

async function findUserRow(
  env: Env, hid: string, u: SyncUser,
): Promise<{ updated_at: number; uuid: string | null; slot_id: number } | null> {
  if (u.uuid) {
    const byUuid = await env.DB.prepare(
      "SELECT updated_at, uuid, slot_id FROM users WHERE hid = ? AND uuid = ?",
    ).bind(hid, u.uuid).first<{ updated_at: number; uuid: string | null; slot_id: number }>();
    if (byUuid) return byUuid;
  }
  return await env.DB.prepare(
    "SELECT updated_at, uuid, slot_id FROM users WHERE hid = ? AND slot_id = ?",
  ).bind(hid, u.slotId).first<{ updated_at: number; uuid: string | null; slot_id: number }>();
}

async function mergeUser(env: Env, hid: string, u: SyncUser): Promise<number> {
  const row = await findUserRow(env, hid, u);
  const uuid = u.uuid ?? row?.uuid ?? newUuid();

  if (!row) {
    await env.DB.prepare(
      `INSERT INTO users
        (hid, slot_id, name, color, created_at, updated_at, is_deleted, uuid)
       VALUES (?, ?, ?, ?, ?, ?, ?, ?)`,
    ).bind(hid, u.slotId, u.name, u.color,
           u.createdAt, u.updatedAt, u.isDeleted ? 1 : 0, uuid).run();
    return 0;
  }

  const conflict = isConflict(u.updatedAt, row.updated_at) ? 1 : 0;
  if (u.updatedAt <= row.updated_at) return conflict;

  await env.DB.prepare(
    `UPDATE users SET name = ?, color = ?,
       updated_at = ?, is_deleted = ?,
       deleted_at = CASE WHEN ? = 1 THEN ? ELSE NULL END,
       uuid = COALESCE(uuid, ?)
     WHERE hid = ? AND slot_id = ?`,
  ).bind(u.name, u.color,
         u.updatedAt, u.isDeleted ? 1 : 0,
         u.isDeleted ? 1 : 0, u.updatedAt,
         uuid,
         hid, row.slot_id).run();
  return conflict;
}

// ── read-back: what the server returns to the device ────────────
// Includes is_deleted=true rows so the device drops them locally
// (instead of resurrecting them on the next add()).

interface SyncResponse {
  schemaVersion:    number;
  now:              number;
  home:             SyncHome;
  cats:             SyncCat[];
  users:            SyncUser[];
  conflicts:        number;
  syncIntervalSec:  number;
}

async function readHomeState(env: Env, hid: string): Promise<{
  home: SyncHome; cats: SyncCat[]; users: SyncUser[];
}> {
  const homeRow = await env.DB.prepare(
    "SELECT hid AS name, updated_at FROM households WHERE hid = ?",
  ).bind(hid).first<{ name: string; updated_at: number }>();

  const catsRes = await env.DB.prepare(
    `SELECT slot_id, name, color, slug, default_portion_g,
            hungry_threshold_sec, schedule_hours,
            created_at, updated_at, is_deleted, uuid
     FROM cats WHERE hid = ?`,
  ).bind(hid).all<{
    slot_id: number; name: string; color: number; slug: string;
    default_portion_g: number; hungry_threshold_sec: number;
    schedule_hours: string;
    created_at: number; updated_at: number; is_deleted: number;
    uuid: string | null;
  }>();

  const usersRes = await env.DB.prepare(
    `SELECT slot_id, name, color, created_at, updated_at, is_deleted, uuid
     FROM users WHERE hid = ?`,
  ).bind(hid).all<{
    slot_id: number; name: string; color: number;
    created_at: number; updated_at: number; is_deleted: number;
    uuid: string | null;
  }>();

  return {
    home: { name: homeRow?.name ?? hid, updatedAt: homeRow?.updated_at ?? 0 },
    cats: (catsRes.results ?? []).map(r => ({
      uuid:               r.uuid ?? undefined,
      slotId:             r.slot_id,
      name:               r.name,
      color:              r.color,
      slug:               r.slug,
      defaultPortionG:    r.default_portion_g,
      hungryThresholdSec: r.hungry_threshold_sec,
      scheduleHours:      parseSchedule(r.schedule_hours),
      createdAt:          r.created_at,
      updatedAt:          r.updated_at,
      isDeleted:          !!r.is_deleted,
    })),
    users: (usersRes.results ?? []).map(r => ({
      uuid:      r.uuid ?? undefined,
      slotId:    r.slot_id,
      name:      r.name,
      color:     r.color,
      createdAt: r.created_at,
      updatedAt: r.updated_at,
      isDeleted: !!r.is_deleted,
    })),
  };
}

export function parseSchedule(json: string): number[] {
  try {
    const arr = JSON.parse(json);
    if (Array.isArray(arr) && arr.length === 4 && arr.every(n => isInt(n))) return arr;
  } catch { /* fall through */ }
  // Bad / missing → fall back to firmware defaults so the device's
  // next sync overwrites with truth.
  return JSON.parse(DEFAULT_SCHEDULE_JSON);
}

// ── audit log ────────────────────────────────────────────────────
async function recordSyncLog(
  env: Env,
  hid: string, deviceId: string,
  result: "ok" | "error" | "cancelled", errorMessage: string | null,
  entitiesIn: number, entitiesOut: number, conflicts: number, durationMs: number,
): Promise<void> {
  const ts = Math.floor(Date.now() / 1000);
  await env.DB.prepare(
    `INSERT INTO sync_logs
       (home_hid, device_id, ts, result, error_message,
        entities_in, entities_out, conflicts, duration_ms)
     VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)`,
  ).bind(hid, deviceId, ts, result, errorMessage,
         entitiesIn, entitiesOut, conflicts, durationMs).run();

  // Ring-buffer prune: keep last SYNC_LOG_RETENTION rows per home.
  // Cheap (one DELETE per write) and bounded — D1 row counts won't
  // creep up over time.
  await env.DB.prepare(
    `DELETE FROM sync_logs
     WHERE home_hid = ? AND id NOT IN (
       SELECT id FROM sync_logs WHERE home_hid = ?
       ORDER BY ts DESC LIMIT ?
     )`,
  ).bind(hid, hid, SYNC_LOG_RETENTION).run();
}

// ── POST /api/sync ───────────────────────────────────────────────
export async function postSync(
  env: Env,
  authed: Extract<AuthInfo, { type: "device" }>,
  body: unknown,
): Promise<Response> {
  const startMs = Date.now();
  const hid = authed.hid;
  const deviceId = authed.deviceId;

  // Defensive parse + auth-binding check. The device token already
  // proves the device is paired to *some* home; we re-check against
  // the live pairings table so a soft-deleted pairing (Reset on the
  // device, Forget on the web) immediately stops syncing.
  const pairing = await env.DB.prepare(
    "SELECT id FROM pairings WHERE device_id = ? AND home_hid = ? AND is_deleted = 0",
  ).bind(deviceId, hid).first<{ id: number }>();
  if (!pairing) {
    await recordSyncLog(env, hid, deviceId, "error",
      "device pairing has been revoked", 0, 0, 0, Date.now() - startMs);
    return json({ error: "pairing revoked — re-pair from H menu" }, { status: 401 });
  }

  if (typeof body !== "object" || body === null) {
    await recordSyncLog(env, hid, deviceId, "error",
      "body required", 0, 0, 0, Date.now() - startMs);
    return json({ error: "JSON body required" }, { status: 400 });
  }
  const req = body as Partial<SyncRequest>;
  if (req.schemaVersion !== SCHEMA_VERSION) {
    await recordSyncLog(env, hid, deviceId, "error",
      `schemaVersion ${req.schemaVersion} (server expects ${SCHEMA_VERSION})`,
      0, 0, 0, Date.now() - startMs);
    return json({
      error: `schemaVersion mismatch — server speaks ${SCHEMA_VERSION}`,
    }, { status: 400 });
  }

  const cats  = (req.cats  ?? []).filter(isSyncCat);
  const users = (req.users ?? []).filter(isSyncUser);
  const dropped = (req.cats?.length ?? 0) - cats.length
                + (req.users?.length ?? 0) - users.length;
  const entitiesIn = cats.length + users.length + (req.home ? 1 : 0);

  let conflicts = 0;
  try {
    if (req.home && typeof req.home.name === "string" && isInt(req.home.updatedAt)) {
      conflicts += await mergeHome(env, hid, req.home);
    }
    for (const c of cats)  conflicts += await mergeCat(env, hid, c);
    for (const u of users) conflicts += await mergeUser(env, hid, u);
  } catch (e) {
    const msg = e instanceof Error ? e.message : String(e);
    console.error(`[sync] merge failed for hid='${hid}' device='${deviceId}': ${msg}`);
    await recordSyncLog(env, hid, deviceId, "error", msg,
      entitiesIn, 0, conflicts, Date.now() - startMs);
    return json({ error: `sync failed: ${msg}` }, { status: 500 });
  }

  const out = await readHomeState(env, hid);
  const entitiesOut = out.cats.length + out.users.length + 1;
  const now = Math.floor(Date.now() / 1000);

  // Phase E — read the per-home sync interval (migration 0009).
  // Falls back to the 4 h hardcoded default if the column doesn't
  // exist yet (e.g. the migration hasn't run on this DB) or the
  // home row was somehow nuked between pairing and sync.
  let syncIntervalSec = DEFAULT_SYNC_INTERVAL_SEC;
  try {
    const settingsRow = await env.DB.prepare(
      "SELECT sync_interval_sec FROM households WHERE hid = ?",
    ).bind(hid).first<{ sync_interval_sec: number }>();
    if (settingsRow?.sync_interval_sec) syncIntervalSec = settingsRow.sync_interval_sec;
  } catch {
    /* migration 0009 not applied yet — keep the default */
  }

  const response: SyncResponse = {
    schemaVersion: SCHEMA_VERSION,
    now,
    home: out.home,
    cats: out.cats,
    users: out.users,
    conflicts,
    syncIntervalSec,
  };

  await recordSyncLog(env, hid, deviceId, "ok",
    dropped > 0 ? `dropped ${dropped} malformed entities` : null,
    entitiesIn, entitiesOut, conflicts, Date.now() - startMs);
  return json(response);
}

// ── GET /api/sync/log ────────────────────────────────────────────
// Read-only view for the webapp's /sync-log page. Filters:
//   ?n=<limit>      1..100 (default 50)
//   ?device=<id>    optional, narrow to one device's syncs
export async function getSyncLog(
  env: Env, hid: string, url: URL,
): Promise<Response> {
  const limit = Math.min(Math.max(Number(url.searchParams.get("n") ?? 50), 1), 100);
  const deviceFilter = url.searchParams.get("device")?.trim();

  const stmt = deviceFilter
    ? env.DB.prepare(
        `SELECT id, device_id, ts, result, error_message,
                entities_in, entities_out, conflicts, duration_ms
         FROM sync_logs
         WHERE home_hid = ? AND device_id = ?
         ORDER BY ts DESC LIMIT ?`,
      ).bind(hid, deviceFilter, limit)
    : env.DB.prepare(
        `SELECT id, device_id, ts, result, error_message,
                entities_in, entities_out, conflicts, duration_ms
         FROM sync_logs
         WHERE home_hid = ?
         ORDER BY ts DESC LIMIT ?`,
      ).bind(hid, limit);

  const { results } = await stmt.all();
  return json({ entries: results ?? [] });
}
