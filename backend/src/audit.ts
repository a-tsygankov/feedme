// Audit logging for auth + pair events. TEMPORARY (per user request)
// — supplements sync_logs with a per-home trail of every pair-start,
// pair-check, login, setup, quick-setup, login-qr, and set-pin call.
// Lets the user inspect why a pairing or login flow failed without
// grepping Worker logs.
//
// Same ring-buffer-of-100 retention as sync_logs. Inserts are
// fire-and-forget from the caller's perspective — failure to log
// must NOT fail the underlying request.

import type { Env } from "./env";

export type AuthLogKind =
  | "pair-start"
  | "pair-confirm"
  | "pair-cancel"
  | "pair-forget"
  | "setup"
  | "login"
  | "quick-setup"
  | "login-token-create"
  | "login-qr"
  | "set-pin";

const RETENTION = 100;

export async function recordAuthLog(
  env: Env,
  hid: string | null,
  kind: AuthLogKind,
  identifier: string | null,
  result: "ok" | "error",
  errorMessage: string | null,
  startMs: number,
): Promise<void> {
  try {
    const ts = Math.floor(Date.now() / 1000);
    const durationMs = Date.now() - startMs;
    await env.DB.prepare(
      `INSERT INTO auth_logs
         (home_hid, ts, kind, identifier, result, error_message, duration_ms)
       VALUES (?, ?, ?, ?, ?, ?, ?)`,
    ).bind(hid, ts, kind, identifier, result, errorMessage, durationMs).run();
    // Ring-buffer prune. We retain 100 per home OR (when hid is null)
    // 100 globally for the no-hid bucket. Cheap one-DELETE-per-write
    // pattern, same as sync_logs in sync.ts.
    if (hid !== null) {
      await env.DB.prepare(
        `DELETE FROM auth_logs
         WHERE home_hid = ? AND id NOT IN (
           SELECT id FROM auth_logs WHERE home_hid = ?
           ORDER BY ts DESC LIMIT ?
         )`,
      ).bind(hid, hid, RETENTION).run();
    } else {
      await env.DB.prepare(
        `DELETE FROM auth_logs
         WHERE home_hid IS NULL AND id NOT IN (
           SELECT id FROM auth_logs WHERE home_hid IS NULL
           ORDER BY ts DESC LIMIT ?
         )`,
      ).bind(RETENTION).run();
    }
  } catch (e) {
    // Audit logging must never break the calling endpoint. If the
    // table doesn't exist (migration not yet applied) or any other
    // DB error happens, swallow it and log to console for the
    // operator to investigate.
    console.warn(`[audit] recordAuthLog failed: ${e instanceof Error ? e.message : e}`);
  }
}

// GET /api/auth/log?n=50&kind=pair-confirm
//
// Returns the last N auth events for the signed-in home (UserToken).
// Optional ?kind=… filter narrows to one event type.
export async function getAuthLog(
  env: Env, hid: string, url: URL,
): Promise<Response> {
  const limit = Math.min(Math.max(Number(url.searchParams.get("n") ?? 100), 1), 100);
  const kindFilter = url.searchParams.get("kind")?.trim();

  // Always include null-hid rows that mention this device (login
  // attempts that failed before auth). Skipped for now — per-home
  // bucketing is enough for the immediate debugging use case.
  const stmt = kindFilter
    ? env.DB.prepare(
        `SELECT id, ts, kind, identifier, result, error_message, duration_ms
         FROM auth_logs
         WHERE home_hid = ? AND kind = ?
         ORDER BY ts DESC LIMIT ?`,
      ).bind(hid, kindFilter, limit)
    : env.DB.prepare(
        `SELECT id, ts, kind, identifier, result, error_message, duration_ms
         FROM auth_logs
         WHERE home_hid = ?
         ORDER BY ts DESC LIMIT ?`,
      ).bind(hid, limit);

  const { results } = await stmt.all();
  return new Response(JSON.stringify({ entries: results ?? [] }), {
    headers: {
      "content-type": "application/json",
      "access-control-allow-origin": "*",
    },
  });
}
