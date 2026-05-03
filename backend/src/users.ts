// Users CRUD endpoints. Mirror the firmware's UserRoster shape — same
// composite-PK pattern as cats. Identical structure to cats.ts but
// without portion / threshold fields (users don't have those).

import type { Env } from "./env";

const MAX_USERS = 4;

interface UserRow {
  slot_id: number;
  name: string;
  color: number;
  uuid: string | null;
}

const userShape = (r: UserRow) => ({
  uuid:   r.uuid ?? undefined,
  slotId: r.slot_id,
  name:   r.name,
  color:  r.color,
});

export async function listUsers(env: Env, hid: string): Promise<UserRow[]> {
  const { results } = await env.DB.prepare(
    `SELECT slot_id, name, color, uuid FROM users
     WHERE hid = ? AND deleted_at IS NULL
     ORDER BY slot_id ASC`,
  ).bind(hid).all<UserRow>();
  return results ?? [];
}

export async function getUsers(env: Env, hid: string): Promise<Response> {
  const users = await listUsers(env, hid);
  return jsonOk({ users: users.map(userShape) });
}

interface CreateBody {
  name?:  string;
  color?: number;
}

export async function createUser(env: Env, hid: string, body: unknown): Promise<Response> {
  const b = (body ?? {}) as CreateBody;

  // Lowest unused slot, treating soft-deleted rows as "still taken"
  // so we don't reuse a slot whose history still references the gone
  // user (matches cats.ts).
  const taken = await env.DB.prepare(
    "SELECT slot_id FROM users WHERE hid = ? ORDER BY slot_id ASC",
  ).bind(hid).all<{ slot_id: number }>();
  const used = new Set((taken.results ?? []).map(r => r.slot_id));
  let slotId = -1;
  for (let i = 0; i < MAX_USERS; i++) {
    if (!used.has(i)) { slotId = i; break; }
  }
  if (slotId < 0) return jsonErr(409, "no free slots — remove a user first");

  const name  = (b.name ?? `User ${slotId}`).slice(0, 15);
  const color = b.color ?? 0;
  // Phase D: mint a uuid at INSERT — see cats.ts createCat for
  // the same rationale.
  const uuid = crypto.randomUUID().replace(/-/g, "").toLowerCase();

  await env.DB.prepare(
    "INSERT INTO users (hid, slot_id, name, color, uuid) VALUES (?, ?, ?, ?, ?)",
  ).bind(hid, slotId, name, color, uuid).run();

  return jsonOk({ user: { uuid, slotId, name, color } });
}

interface UpdateBody {
  name?:  string;
  color?: number;
}

export async function updateUser(env: Env, hid: string, slotId: number, body: unknown): Promise<Response> {
  const b = (body ?? {}) as UpdateBody;

  const fields: string[] = [];
  const values: (string | number)[] = [];
  if (typeof b.name === "string")  { fields.push("name = ?");  values.push(b.name.slice(0, 15)); }
  if (typeof b.color === "number") { fields.push("color = ?"); values.push(b.color); }
  if (fields.length === 0) return jsonErr(400, "no editable fields");

  const sql = `UPDATE users SET ${fields.join(", ")} WHERE hid = ? AND slot_id = ? AND deleted_at IS NULL`;
  values.push(hid, slotId);
  const res = await env.DB.prepare(sql).bind(...values).run();
  if (res.meta.changes === 0) return jsonErr(404, "user not found");

  const row = await env.DB.prepare(
    "SELECT slot_id, name, color, uuid FROM users WHERE hid = ? AND slot_id = ? AND deleted_at IS NULL",
  ).bind(hid, slotId).first<UserRow>();
  if (!row) return jsonErr(404, "user vanished after update");
  return jsonOk({ user: userShape(row) });
}

export async function deleteUser(env: Env, hid: string, slotId: number): Promise<Response> {
  // Idempotent retry path: if the row is already soft-deleted, return
  // 200 instead of 404 so a network-retried DELETE doesn't confuse
  // the client. Mirrors deleteCat's pattern.
  const existing = await env.DB.prepare(
    "SELECT deleted_at FROM users WHERE hid = ? AND slot_id = ?",
  ).bind(hid, slotId).first<{ deleted_at: number | null }>();
  if (!existing) return jsonErr(404, "user not found");
  if (existing.deleted_at !== null) {
    return jsonOk({ ok: true, alreadyDeleted: true });
  }

  // Same N>=1 invariant as cats — feeds always need a `by`.
  const liveCount = await env.DB.prepare(
    "SELECT COUNT(*) AS c FROM users WHERE hid = ? AND deleted_at IS NULL",
  ).bind(hid).first<{ c: number }>();
  if ((liveCount?.c ?? 0) <= 1) return jsonErr(409, "can't remove last user");

  const ts = Math.floor(Date.now() / 1000);
  await env.DB.prepare(
    "UPDATE users SET deleted_at = ?, is_deleted = 1, updated_at = ? WHERE hid = ? AND slot_id = ?",
  ).bind(ts, ts, hid, slotId).run();
  return jsonOk({ ok: true });
}

// ── tiny JSON helpers (kept here to avoid a cycle with index.ts) ──
const jsonOk = (data: unknown) =>
  new Response(JSON.stringify(data), { status: 200, headers: corsHeaders() });

const jsonErr = (status: number, msg: string) =>
  new Response(JSON.stringify({ error: msg }), { status, headers: corsHeaders() });

const corsHeaders = (): HeadersInit => ({
  "content-type": "application/json",
  "access-control-allow-origin": "*",
});
