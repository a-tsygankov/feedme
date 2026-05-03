// Cats CRUD endpoints. Mirror the firmware's CatRoster shape so the
// web UI and the device read the same canonical state. slot_id is the
// stable uint8_t from the firmware (0..3); server assigns the lowest
// unused slot on POST so the firmware's load-from-NVS code keeps
// working when both sources of truth converge in a later sync pass.

import type { Env } from "./env";

const MAX_CATS = 4;
const DEFAULT_PORTION_G    = 40;
const DEFAULT_THRESHOLD_S  = 5 * 3600;
const DEFAULT_SLUG         = "C2";

interface CatRow {
  slot_id: number;
  name: string;
  color: number;
  slug: string;
  default_portion_g: number;
  hungry_threshold_sec: number;
  uuid: string | null;
}

const catShape = (r: CatRow) => ({
  uuid:               r.uuid ?? undefined,
  slotId:             r.slot_id,
  name:               r.name,
  color:              r.color,
  slug:               r.slug,
  defaultPortionG:    r.default_portion_g,
  hungryThresholdSec: r.hungry_threshold_sec,
});

export async function listCats(env: Env, hid: string): Promise<CatRow[]> {
  const { results } = await env.DB.prepare(
    `SELECT slot_id, name, color, slug, default_portion_g, hungry_threshold_sec, uuid
     FROM cats WHERE hid = ? AND deleted_at IS NULL
     ORDER BY slot_id ASC`,
  ).bind(hid).all<CatRow>();
  return results ?? [];
}

export async function getCats(env: Env, hid: string): Promise<Response> {
  const cats = await listCats(env, hid);
  return jsonOk({ cats: cats.map(catShape) });
}

interface CreateBody {
  name?:               string;
  color?:              number;
  slug?:               string;
  defaultPortionG?:    number;
  hungryThresholdSec?: number;
}

export async function createCat(env: Env, hid: string, body: unknown): Promise<Response> {
  const b = (body ?? {}) as CreateBody;

  // Pick the lowest unused slot 0..MAX_CATS-1. Includes soft-deleted
  // rows in the search so we don't reuse a slot whose history might
  // still reference the gone cat — fresh slot_ids only. If all four
  // slots are used (active or tombstoned), refuse.
  const taken = await env.DB.prepare(
    "SELECT slot_id FROM cats WHERE hid = ? ORDER BY slot_id ASC",
  ).bind(hid).all<{ slot_id: number }>();
  const used = new Set((taken.results ?? []).map(r => r.slot_id));
  let slotId = -1;
  for (let i = 0; i < MAX_CATS; i++) {
    if (!used.has(i)) { slotId = i; break; }
  }
  if (slotId < 0) {
    return jsonErr(409, "no free slots — remove a cat first");
  }

  const name  = (b.name  ?? `Cat ${slotId}`).slice(0, 15);
  const color = b.color  ?? 0;
  const slug  = (b.slug  ?? DEFAULT_SLUG).slice(0, 3);
  const port  = b.defaultPortionG    ?? DEFAULT_PORTION_G;
  const thr   = b.hungryThresholdSec ?? DEFAULT_THRESHOLD_S;
  // Phase D: mint a stable per-row uuid at INSERT time so the
  // response can teach the client its identity. The unique index
  // on cats.uuid prevents collisions if two webapp tabs both
  // create concurrently — second loses with a SQLITE_CONSTRAINT
  // (caller treats as a benign retry signal).
  const uuid = crypto.randomUUID().replace(/-/g, "").toLowerCase();

  await env.DB.prepare(
    `INSERT INTO cats (hid, slot_id, name, color, slug,
                       default_portion_g, hungry_threshold_sec, uuid)
     VALUES (?, ?, ?, ?, ?, ?, ?, ?)`,
  ).bind(hid, slotId, name, color, slug, port, thr, uuid).run();

  return jsonOk({
    cat: { uuid, slotId, name, color, slug, defaultPortionG: port, hungryThresholdSec: thr },
  });
}

interface UpdateBody {
  name?:               string;
  color?:              number;
  slug?:               string;
  defaultPortionG?:    number;
  hungryThresholdSec?: number;
}

export async function updateCat(env: Env, hid: string, slotId: number, body: unknown): Promise<Response> {
  const b = (body ?? {}) as UpdateBody;

  // Build a dynamic SET clause from whichever fields the client sent.
  // Empty body → 400; we won't issue an UPDATE that touches nothing.
  const fields: string[] = [];
  const values: (string | number)[] = [];
  if (typeof b.name === "string")              { fields.push("name = ?");                values.push(b.name.slice(0, 15)); }
  if (typeof b.color === "number")             { fields.push("color = ?");               values.push(b.color); }
  if (typeof b.slug === "string")              { fields.push("slug = ?");                values.push(b.slug.slice(0, 3)); }
  if (typeof b.defaultPortionG === "number")   { fields.push("default_portion_g = ?");   values.push(b.defaultPortionG); }
  if (typeof b.hungryThresholdSec === "number"){ fields.push("hungry_threshold_sec = ?");values.push(b.hungryThresholdSec); }
  if (fields.length === 0) return jsonErr(400, "no editable fields");

  const sql = `UPDATE cats SET ${fields.join(", ")} WHERE hid = ? AND slot_id = ? AND deleted_at IS NULL`;
  values.push(hid, slotId);
  const res = await env.DB.prepare(sql).bind(...values).run();
  if (res.meta.changes === 0) return jsonErr(404, "cat not found");

  // Read back the updated row so the client can swap into local state.
  const row = await env.DB.prepare(
    `SELECT slot_id, name, color, slug, default_portion_g, hungry_threshold_sec, uuid
     FROM cats WHERE hid = ? AND slot_id = ? AND deleted_at IS NULL`,
  ).bind(hid, slotId).first<CatRow>();
  if (!row) return jsonErr(404, "cat vanished after update");
  return jsonOk({ cat: catShape(row) });
}

export async function deleteCat(env: Env, hid: string, slotId: number): Promise<Response> {
  // Look up the cat in any state first. Distinguishes:
  //   - never existed         → 404
  //   - already soft-deleted  → 200 idempotent (network-retry path)
  //   - active                → soft-delete + 200
  // Without this distinction a network-retried DELETE that originally
  // succeeded would 404, confusing the client into thinking the
  // delete failed.
  const existing = await env.DB.prepare(
    "SELECT deleted_at FROM cats WHERE hid = ? AND slot_id = ?",
  ).bind(hid, slotId).first<{ deleted_at: number | null }>();
  if (!existing) return jsonErr(404, "cat not found");
  if (existing.deleted_at !== null) {
    return jsonOk({ ok: true, alreadyDeleted: true });
  }

  // Refuse to delete the last cat — preserves the firmware's N>=1
  // invariant. The web app should disable the delete button at that
  // boundary, but we enforce here too.
  const liveCount = await env.DB.prepare(
    "SELECT COUNT(*) AS c FROM cats WHERE hid = ? AND deleted_at IS NULL",
  ).bind(hid).first<{ c: number }>();
  if ((liveCount?.c ?? 0) <= 1) return jsonErr(409, "can't remove last cat");

  // Soft delete: stamp deleted_at. Keeps event-by-cat lookups stable
  // for orphan history rendering ("fed by Mochi (removed)").
  const ts = Math.floor(Date.now() / 1000);
  await env.DB.prepare(
    "UPDATE cats SET deleted_at = ?, is_deleted = 1, updated_at = ? WHERE hid = ? AND slot_id = ?",
  ).bind(ts, ts, hid, slotId).run();
  return jsonOk({ ok: true });
}

// ── tiny JSON helpers (kept here to avoid a cycle with index.ts) ──
const jsonOk = (data: unknown) =>
  new Response(JSON.stringify(data), {
    status: 200,
    headers: corsHeaders(),
  });

const jsonErr = (status: number, msg: string) =>
  new Response(JSON.stringify({ error: msg }), {
    status,
    headers: corsHeaders(),
  });

const corsHeaders = (): HeadersInit => ({
  "content-type": "application/json",
  "access-control-allow-origin": "*",
});
