// Per-home settings — Phase E.
//
// Today there's exactly one setting (sync_interval_sec) so the
// surface is small. Both endpoints are auth-gated to the home's
// UserToken; settings are intentionally NOT exposed via DeviceToken
// because changing the interval should be a deliberate webapp action,
// not something a compromised device could rewrite.
//
//   GET   /api/home/settings → { syncIntervalSec }
//   PATCH /api/home/settings { syncIntervalSec? } → { syncIntervalSec }
//
// PATCH validation: 3600 (1h) ≤ syncIntervalSec ≤ 86400 (24h).
// Outside that range = 400. Range chosen for sane battery / freshness
// tradeoffs on a USB-powered device; bump if a real use case appears.

import type { Env } from "./env";

const MIN_SYNC_INTERVAL_SEC =     3600;   //  1 h
const MAX_SYNC_INTERVAL_SEC =    86400;   // 24 h
const DEFAULT_SYNC_INTERVAL_SEC = 14400;  //  4 h

const json = (data: unknown, init: ResponseInit = {}) =>
  new Response(JSON.stringify(data), {
    ...init,
    headers: {
      "content-type": "application/json",
      "access-control-allow-origin": "*",
      ...(init.headers ?? {}),
    },
  });

export interface HomeSettings {
  syncIntervalSec: number;
}

export async function getHomeSettings(
  env: Env, hid: string,
): Promise<Response> {
  // Defensive read: the column may not exist if migration 0009
  // hasn't run yet — fall back to the default in that case so the
  // settings UI can still render.
  let syncIntervalSec = DEFAULT_SYNC_INTERVAL_SEC;
  try {
    const row = await env.DB.prepare(
      "SELECT sync_interval_sec FROM households WHERE hid = ?",
    ).bind(hid).first<{ sync_interval_sec: number }>();
    if (row?.sync_interval_sec) syncIntervalSec = row.sync_interval_sec;
  } catch {
    /* migration not applied — return default */
  }
  return json({ syncIntervalSec });
}

export async function patchHomeSettings(
  env: Env, hid: string, body: unknown,
): Promise<Response> {
  const b = (body ?? {}) as { syncIntervalSec?: unknown };
  if (typeof b.syncIntervalSec !== "number" || !Number.isInteger(b.syncIntervalSec)) {
    return json({ error: "syncIntervalSec required (integer seconds)" },
                { status: 400 });
  }
  if (b.syncIntervalSec < MIN_SYNC_INTERVAL_SEC ||
      b.syncIntervalSec > MAX_SYNC_INTERVAL_SEC) {
    return json({
      error: `syncIntervalSec out of range (${MIN_SYNC_INTERVAL_SEC}..${MAX_SYNC_INTERVAL_SEC})`,
    }, { status: 400 });
  }
  await env.DB.prepare(
    "UPDATE households SET sync_interval_sec = ? WHERE hid = ?",
  ).bind(b.syncIntervalSec, hid).run();
  return json({ syncIntervalSec: b.syncIntervalSec });
}
