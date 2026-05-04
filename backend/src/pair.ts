// Pair lifecycle endpoints.
//
// Three-step handshake (see docs/sync-implementation-handoff.md §7):
//
//   1. Device → POST /api/pair/start { deviceId }
//        Creates a pending_pairings row with expires_at = now + 180s.
//        Idempotent: re-POSTing for the same deviceId resets the row
//        (lets the user re-tap "Pair" without confusing state).
//
//   2. Device polls GET /api/pair/check?deviceId=X every 15s
//        Returns one of:
//          { status: "pending",   expiresAt }
//          { status: "confirmed", token, hid, expiresAt? }
//          { status: "expired" }
//          { status: "cancelled" }
//        Once "confirmed" is returned the row is harmless to leave
//        in place (the device caches the token in NVS); a cron purge
//        is a future enhancement.
//
//   3. Webapp (after user signs in) → POST /api/pair/confirm { deviceId }
//        Auth: UserToken. Server:
//          (a) Looks up the pending_pairings row and validates not
//              expired / not cancelled.
//          (b) Creates a `pairings` row (active relationship).
//          (c) Issues a DeviceToken for (deviceId, authed.hid).
//          (d) Updates pending_pairings { home_hid, confirmed_at,
//              device_token } so the device's next /check picks it up.
//          (e) Atomically registers the device into the legacy
//              `devices` table too — keeps /api/feed translation
//              working without a follow-up.
//
// Plus DELETE /api/pair/{deviceId} for "Forget device" (UserToken)
// and "Unpair this device" (DeviceToken). Both soft-delete the
// active pairings row + remove the devices row + clear any in-flight
// pending_pairings entry.

import { issueDeviceToken, type AuthInfo } from "./auth";
import { recordAuthLog } from "./audit";
import type { Env } from "./env";

const PAIRING_TTL_SEC = 180;          // 3-minute handshake window

const json = (data: unknown, init: ResponseInit = {}) =>
  new Response(JSON.stringify(data), {
    ...init,
    headers: {
      "content-type": "application/json",
      "access-control-allow-origin": "*",
      ...(init.headers ?? {}),
    },
  });

// Lightweight device-id validation. Format established by firmware:
// `feedme-` + lowercase hex. Allow 8..32 hex chars after the prefix
// to cover the legacy 12-hex (MAC-derived), the new 16-hex (random),
// and any reset-counter suffixes (e.g. `feedme-{hex}-1`).
export function validateDeviceId(raw: unknown): string | null {
  if (typeof raw !== "string") return null;
  const id = raw.trim();
  if (id.length < 1 || id.length > 64) return null;
  // Allow alphanumerics + hyphens. The web side sees this in URL
  // params so paranoia about other chars is warranted.
  if (!/^[a-zA-Z0-9-]+$/.test(id)) return null;
  return id;
}

// ── POST /api/pair/start ─────────────────────────────────────────
export async function postPairStart(env: Env, body: unknown): Promise<Response> {
  const startMs = Date.now();
  const b = (body ?? {}) as { deviceId?: string };
  const deviceId = validateDeviceId(b.deviceId);
  if (!deviceId) {
    await recordAuthLog(env, null, "pair-start", null, "error", "deviceId required", startMs);
    return json({ error: "deviceId required" }, { status: 400 });
  }

  const now = Math.floor(Date.now() / 1000);
  const expiresAt = now + PAIRING_TTL_SEC;

  // INSERT OR REPLACE so the user re-tapping Pair (or restarting the
  // device mid-flow) gets a fresh window without an UPDATE-vs-INSERT
  // dance. Clears any prior cancelled/confirmed state.
  await env.DB.prepare(
    `INSERT OR REPLACE INTO pending_pairings
       (device_id, requested_at, expires_at, home_hid, confirmed_at,
        device_token, cancelled_at)
     VALUES (?, ?, ?, NULL, NULL, NULL, NULL)`,
  ).bind(deviceId, now, expiresAt).run();

  console.log(`[pair] start deviceId='${deviceId}' expires=${expiresAt}`);
  // Pre-pair we don't know the hid yet; log with null hid. The
  // later confirmPairingFor entry will tie it back.
  await recordAuthLog(env, null, "pair-start", deviceId, "ok", null, startMs);
  return json({ status: "pending", expiresAt });
}

// ── GET /api/pair/check ───────────────────────────────────────────
export async function getPairCheck(env: Env, url: URL): Promise<Response> {
  const deviceId = validateDeviceId(url.searchParams.get("deviceId"));
  if (!deviceId) return json({ error: "deviceId required" }, { status: 400 });

  const row = await env.DB.prepare(
    `SELECT requested_at, expires_at, home_hid, confirmed_at,
            device_token, cancelled_at
     FROM pending_pairings WHERE device_id = ?`,
  ).bind(deviceId).first<{
    requested_at: number; expires_at: number;
    home_hid: string | null; confirmed_at: number | null;
    device_token: string | null; cancelled_at: number | null;
  }>();

  if (!row) {
    // Never started, or GC'd. Treat as expired so the device's UI
    // reflects "this pairing window doesn't exist" rather than spinning
    // forever.
    return json({ status: "expired" });
  }
  if (row.cancelled_at) return json({ status: "cancelled" });
  if (row.confirmed_at && row.device_token && row.home_hid) {
    return json({
      status: "confirmed",
      token:  row.device_token,
      hid:    row.home_hid,
    });
  }
  const now = Math.floor(Date.now() / 1000);
  if (row.expires_at <= now) return json({ status: "expired" });
  return json({ status: "pending", expiresAt: row.expires_at });
}

// ── POST /api/pair/cancel ────────────────────────────────────────
// Either side (device long-tap, user-initiated abort) can cancel.
// No auth required so the device's "long-tap on Pairing screen"
// can call it without yet having a token. Authentication-by-knowledge
// of the deviceId is sufficient — anyone can already see the QR.
export async function postPairCancel(env: Env, body: unknown): Promise<Response> {
  const b = (body ?? {}) as { deviceId?: string };
  const deviceId = validateDeviceId(b.deviceId);
  if (!deviceId) return json({ error: "deviceId required" }, { status: 400 });

  const now = Math.floor(Date.now() / 1000);
  const res = await env.DB.prepare(
    `UPDATE pending_pairings
       SET cancelled_at = ?
     WHERE device_id = ? AND confirmed_at IS NULL AND cancelled_at IS NULL`,
  ).bind(now, deviceId).run();

  console.log(`[pair] cancel deviceId='${deviceId}' rows=${res.meta.changes}`);
  return json({ status: "cancelled", changed: res.meta.changes ?? 0 });
}

// Result discriminator for confirmPairingFor — lets callers decide
// whether to bubble the error up as an HTTP status or silently move
// on (the /api/auth/login path passes deviceId optimistically; if the
// device's pair window expired we want the user to still sign in).
export type ConfirmPairingResult =
  | { ok: true;  alreadyPaired: boolean; deviceToken: string }
  | { ok: false; status: number; error: string };

// ── confirmPairingFor (shared helper) ─────────────────────────────
// Performs the device-claim work that used to live exclusively in
// /api/pair/confirm. Now also called inline from /api/auth/setup,
// /api/auth/login, and /api/auth/quick-setup so the user doesn't
// have to click a "Confirm pairing" banner on the dashboard after
// signing in — the pairing completes in the same round-trip as the
// auth request, and the device's next /api/pair/check returns
// confirmed within ~15 s without any further webapp interaction.
//
// Steps:
//   1. Look up pending_pairings for the device. Refuse with
//      404/410 if missing / cancelled / expired.
//   2. If already confirmed for THIS home: re-issue token (idempotent).
//      If confirmed for a DIFFERENT home: 409.
//   3. Otherwise: mint DeviceToken, INSERT/UPDATE pairings row,
//      INSERT/UPDATE legacy devices row, write the token + home_hid +
//      confirmed_at into pending_pairings so /api/pair/check sees it.
//
// Callers should pass `secret` from resolveSecret(env). The
// returned deviceToken is whatever the device will pick up on its
// next /pair/check; auth handlers don't need it (the user gets a
// UserToken from the calling endpoint), but the helper returns it
// for symmetry + so a future "show device token in webapp" feature
// can use it without a second SQL round-trip.
export async function confirmPairingFor(
  env: Env, hid: string, deviceId: string, secret: string,
): Promise<ConfirmPairingResult> {
  const row = await env.DB.prepare(
    `SELECT expires_at, home_hid, confirmed_at, cancelled_at
     FROM pending_pairings WHERE device_id = ?`,
  ).bind(deviceId).first<{
    expires_at: number;
    home_hid: string | null;
    confirmed_at: number | null;
    cancelled_at: number | null;
  }>();
  if (!row) {
    return { ok: false, status: 404,
             error: "no pending pairing for this device — ask user to re-tap Pair on device" };
  }
  if (row.cancelled_at) {
    return { ok: false, status: 410, error: "device cancelled the pairing" };
  }
  const now = Math.floor(Date.now() / 1000);
  if (row.expires_at <= now && !row.confirmed_at) {
    return { ok: false, status: 410,
             error: "pairing window expired (3 min) — ask user to re-tap Pair" };
  }

  // Idempotency: already confirmed for the same home → re-issue the
  // token (lets a double-call from a retried auth request succeed
  // silently). Confirmed for a different home → caller should
  // surface the 409 to the user.
  if (row.confirmed_at && row.home_hid) {
    if (row.home_hid !== hid) {
      return { ok: false, status: 409,
               error: "device already paired to a different home" };
    }
    const token = await issueDeviceToken(hid, deviceId, secret);
    await env.DB.prepare(
      "UPDATE pending_pairings SET device_token = ? WHERE device_id = ?",
    ).bind(token, deviceId).run();
    return { ok: true, alreadyPaired: true, deviceToken: token };
  }

  // Fresh confirmation. issueDeviceToken signature is
  // (hid, deviceId, secret) — careful not to swap; an earlier bug
  // minted tokens with the two values crossed and every /api/sync
  // call returned 401 "pairing revoked".
  const token = await issueDeviceToken(hid, deviceId, secret);

  // Soft-restore the active pairings row if one was previously
  // tombstoned for this device (re-pair after unpair). Otherwise insert.
  const existing = await env.DB.prepare(
    "SELECT id, is_deleted FROM pairings WHERE device_id = ? ORDER BY id DESC LIMIT 1",
  ).bind(deviceId).first<{ id: number; is_deleted: number }>();

  if (existing) {
    await env.DB.prepare(
      `UPDATE pairings SET home_hid = ?, updated_at = ?, is_deleted = 0
       WHERE id = ?`,
    ).bind(hid, now, existing.id).run();
  } else {
    await env.DB.prepare(
      `INSERT INTO pairings (device_id, home_hid, created_at, updated_at, is_deleted)
       VALUES (?, ?, ?, ?, 0)`,
    ).bind(deviceId, hid, now, now).run();
  }

  // Mirror into the legacy devices table so /api/feed translation
  // (added in migration 0004) keeps mapping device → home for
  // firmware-facing endpoints that haven't migrated to /api/sync yet.
  await env.DB.prepare(
    `INSERT INTO devices (device_id, home_hid, joined_at, created_at, updated_at, is_deleted)
       VALUES (?, ?, ?, ?, ?, 0)
     ON CONFLICT(device_id) DO UPDATE SET
       home_hid   = excluded.home_hid,
       updated_at = excluded.updated_at,
       is_deleted = 0`,
  ).bind(deviceId, hid, now, now, now).run();

  await env.DB.prepare(
    `UPDATE pending_pairings
       SET home_hid = ?, confirmed_at = ?, device_token = ?
     WHERE device_id = ?`,
  ).bind(hid, now, token, deviceId).run();

  console.log(`[pair] confirmed deviceId='${deviceId}' hid='${hid}'`);
  return { ok: true, alreadyPaired: false, deviceToken: token };
}

// ── POST /api/pair/confirm (UserToken auth) ──────────────────────
// Kept for back-compat with any client (or in-flight tab) still
// calling it. New auth flows complete pairing INLINE via
// confirmPairingFor — no banner click required.
export async function postPairConfirm(
  env: Env,
  authed: Extract<AuthInfo, { type: "user" }>,
  body: unknown,
  secret: string,
): Promise<Response> {
  const startMs = Date.now();
  const b = (body ?? {}) as { deviceId?: string };
  const deviceId = validateDeviceId(b.deviceId);
  if (!deviceId) {
    await recordAuthLog(env, authed.hid, "pair-confirm", null, "error", "deviceId required", startMs);
    return json({ error: "deviceId required" }, { status: 400 });
  }

  const res = await confirmPairingFor(env, authed.hid, deviceId, secret);
  if (!res.ok) {
    await recordAuthLog(env, authed.hid, "pair-confirm", deviceId, "error", res.error, startMs);
    return json({ error: res.error }, { status: res.status });
  }
  await recordAuthLog(env, authed.hid, "pair-confirm", deviceId, "ok",
    res.alreadyPaired ? "alreadyPaired" : null, startMs);
  return json({
    ok: true,
    deviceId,
    hid: authed.hid,
    ...(res.alreadyPaired ? { alreadyPaired: true } : {}),
  });
}

// ── GET /api/pair/list (UserToken) ───────────────────────────────
// Returns active pairings for the signed-in home — drives the
// webapp's Settings → Devices card. Soft-deleted pairings are
// excluded; if you want history, query sync_logs filtered by device.
export async function getPairList(
  env: Env, hid: string,
): Promise<Response> {
  const { results } = await env.DB.prepare(
    `SELECT device_id, created_at, updated_at
     FROM pairings
     WHERE home_hid = ? AND is_deleted = 0
     ORDER BY created_at DESC`,
  ).bind(hid).all<{ device_id: string; created_at: number; updated_at: number }>();
  return json({
    devices: (results ?? []).map(r => ({
      deviceId:  r.device_id,
      createdAt: r.created_at,
      updatedAt: r.updated_at,
    })),
  });
}

// ── DELETE /api/pair/{deviceId} ──────────────────────────────────
// Soft-deletes the pairings row for this device. Accepts either
// auth type:
//   UserToken   — webapp "Forget Device" UI; the user's own home only.
//   DeviceToken — device-side "Reset" calling out to unpair itself.
// Both paths additionally clear the legacy devices row so /api/feed
// translation falls back to raw hid (which won't match any home,
// effectively dropping events from the unpaired device on the floor).
export async function deletePair(
  env: Env, authed: AuthInfo, deviceId: string,
): Promise<Response> {
  const id = validateDeviceId(deviceId);
  if (!id) return json({ error: "bad deviceId" }, { status: 400 });

  // Authorisation: a UserToken can only delete pairings in its own
  // home; a DeviceToken can only delete its own deviceId. We check
  // against the *most recent* row for this device (active or
  // tombstoned) so a leaked token can't nuke a pairing in a home
  // it doesn't own — and a retry of an already-completed delete
  // still proves caller is authorised before returning the
  // idempotent OK.
  const row = await env.DB.prepare(
    "SELECT home_hid, is_deleted FROM pairings WHERE device_id = ? ORDER BY id DESC LIMIT 1",
  ).bind(id).first<{ home_hid: string; is_deleted: number }>();

  if (!row) {
    // Genuinely never paired. Return 404 rather than masquerading as
    // success — distinguishes "your delete worked" from "this device
    // was never in our database at all" (which probably means a typo).
    return json({ error: "no such device" }, { status: 404 });
  }

  if (authed.type === "user" && row.home_hid !== authed.hid) {
    return json({ error: "forbidden" }, { status: 403 });
  }
  if (authed.type === "device" && (authed.deviceId !== id || row.home_hid !== authed.hid)) {
    return json({ error: "forbidden" }, { status: 403 });
  }

  // Idempotent: if already soft-deleted, return success without
  // re-tombstoning. Common path: webapp Forget button retried after
  // a network timeout where the original DELETE actually succeeded.
  if (row.is_deleted === 1) {
    return json({ ok: true, deviceId: id, alreadyForgotten: true });
  }

  const now = Math.floor(Date.now() / 1000);
  await env.DB.prepare(
    `UPDATE pairings SET is_deleted = 1, updated_at = ?
     WHERE device_id = ? AND is_deleted = 0`,
  ).bind(now, id).run();
  await env.DB.prepare(
    "DELETE FROM devices WHERE device_id = ?",
  ).bind(id).run();
  await env.DB.prepare(
    "DELETE FROM pending_pairings WHERE device_id = ?",
  ).bind(id).run();

  console.log(`[pair] unpair deviceId='${id}' by ${authed.type}`);
  return json({ ok: true, deviceId: id });
}
