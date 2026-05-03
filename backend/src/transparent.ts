// Transparent accounts + Login QR + set-PIN — Phase F.
//
// Three flavours of "auth without typing a PIN":
//
//   1. Quick-setup (POST /api/auth/quick-setup):
//      Creates a TRANSPARENT home — no name to pick, no PIN to choose.
//      Generates an opaque hid like "home-{8hex}" and inserts the
//      household with empty pin_salt/pin_hash (the in-band sentinel
//      for "no PIN required"). Atomically claims the device that
//      scanned the QR. Designed for the user who wants the device
//      working in 30 s without thinking about identity. Cookie set.
//
//   2. Login QR (POST /api/auth/login-token-create + login-qr):
//      An ALREADY-PAIRED device shows a one-shot QR encoding a
//      short-lived (60 s) token. A phone scans → /qr-login route on
//      the webapp → POST /api/auth/login-qr exchanges the token for
//      a UserToken + cookie. Lets a phone re-sign-in to a paired home
//      without retyping anything (and without the user even knowing
//      the home name).
//
//   3. Set-PIN (POST /api/auth/set-pin):
//      Promotes a transparent account to PIN-protected. Once a PIN is
//      set, the home participates in the regular /api/auth/login flow.
//      Demotion (drop the PIN) is not supported — once protected,
//      forever protected.
//
// Tokens minted in (2) live in the login_qr_tokens table from
// migration 0010. Backfill: empty pin_salt/pin_hash strings on
// pre-Phase-F homes mean "PIN-protected" (legacy behaviour); the
// distinction is "" vs the 64-char hex hash. Existing rows have a
// real hash so they're unambiguous.

import { hashPin, issueUserToken, buildSessionCookie, type AuthInfo } from "./auth";
import type { Env } from "./env";

const QR_TOKEN_TTL_SEC      = 60;      // single-use, very short — UX = scan in <60 s
const QUICK_SETUP_PREFIX    = "home-"; // prepended to the random hid
const QUICK_SETUP_RAND_HEX  = 8;       // bytes; "home-{16 hex chars}"

// Same json/cookie helper shape as the rest of the worker. Keep
// inline so this file stays import-free of index.ts.
function json(data: unknown, init: ResponseInit = {}, cookie?: string): Response {
  const headers: Record<string, string> = {
    "content-type": "application/json",
    "access-control-allow-origin": "*",
    ...(init.headers as Record<string, string> | undefined ?? {}),
  };
  if (cookie) headers["set-cookie"] = cookie;
  return new Response(JSON.stringify(data), { ...init, headers });
}

// Random hex string. Webcrypto guarantees the entropy.
function randomHex(bytes: number): string {
  const buf = new Uint8Array(bytes);
  crypto.getRandomValues(buf);
  return Array.from(buf, b => b.toString(16).padStart(2, "0")).join("");
}

// ── POST /api/auth/quick-setup { deviceId } ────────────────────────
// No auth required (the device is unpaired and trying to start fresh).
// The deviceId binds the new home to the device that scanned. Idempotency:
// if the device already has an active pairing, return its existing
// home + a fresh UserToken instead of double-creating.
export async function postQuickSetup(
  env: Env, body: unknown, secret: string,
): Promise<Response> {
  const b = (body ?? {}) as { deviceId?: string };
  if (typeof b.deviceId !== "string" || !b.deviceId.trim()) {
    return json({ error: "deviceId required" }, { status: 400 });
  }
  const deviceId = b.deviceId.trim();

  // If this device already paired (e.g. user tapped quick-setup
  // twice), reuse the existing home rather than minting a second.
  const existing = await env.DB.prepare(
    "SELECT home_hid FROM pairings WHERE device_id = ? AND is_deleted = 0 ORDER BY id DESC LIMIT 1",
  ).bind(deviceId).first<{ home_hid: string }>();
  if (existing) {
    const token = await issueUserToken(existing.home_hid, secret);
    return json(
      { token, hid: existing.home_hid, alreadyPaired: true },
      { status: 200 },
      buildSessionCookie(token),
    );
  }

  // Fresh transparent home: generate hid, insert with empty
  // pin_salt/pin_hash (the "no PIN" sentinel), claim the device.
  const hid = QUICK_SETUP_PREFIX + randomHex(QUICK_SETUP_RAND_HEX);
  const now = Math.floor(Date.now() / 1000);

  try {
    await env.DB.prepare(
      `INSERT INTO households
         (hid, pin_salt, pin_hash, created_at, name, updated_at, is_deleted)
       VALUES (?, '', '', ?, '', ?, 0)`,
    ).bind(hid, now, now).run();

    await env.DB.prepare(
      `INSERT INTO pairings (device_id, home_hid, created_at, updated_at, is_deleted)
       VALUES (?, ?, ?, ?, 0)`,
    ).bind(deviceId, hid, now, now).run();

    // Mirror into the legacy devices table so /api/feed translation
    // (the firmware-facing path) finds this home immediately, before
    // the device upgrades to the device-token sync path.
    await env.DB.prepare(
      `INSERT INTO devices (device_id, home_hid, joined_at, created_at, updated_at, is_deleted)
       VALUES (?, ?, ?, ?, ?, 0)
       ON CONFLICT(device_id) DO UPDATE SET
         home_hid = excluded.home_hid, updated_at = excluded.updated_at, is_deleted = 0`,
    ).bind(deviceId, hid, now, now, now).run();
  } catch (e) {
    const msg = e instanceof Error ? e.message : String(e);
    console.error(`[quick-setup] insert failed: ${msg}`);
    return json({ error: `quick-setup failed: ${msg}` }, { status: 500 });
  }

  const token = await issueUserToken(hid, secret);
  console.log(`[quick-setup] created transparent home '${hid}' for device '${deviceId}'`);
  return json({ token, hid }, { status: 200 }, buildSessionCookie(token));
}

// ── POST /api/auth/login-token-create (DeviceToken) ────────────────
// Device-side step of the Login QR flow. Mints a 60-s random token
// bound to the device's already-paired home and returns it for the
// firmware to encode in a QR. Idempotency: each call mints a NEW
// token (no dedup) — the prior one stays valid until expiry but the
// device discards it. Replay-protection comes from single-use
// consumption in /login-qr.
export async function postLoginTokenCreate(
  env: Env, authed: Extract<AuthInfo, { type: "device" }>,
): Promise<Response> {
  const token = randomHex(16);
  const now = Math.floor(Date.now() / 1000);
  const expiresAt = now + QR_TOKEN_TTL_SEC;

  await env.DB.prepare(
    `INSERT INTO login_qr_tokens
       (token, device_id, home_hid, created_at, expires_at, consumed_at)
     VALUES (?, ?, ?, ?, ?, NULL)`,
  ).bind(token, authed.deviceId, authed.hid, now, expiresAt).run();

  return json({ token, expiresAt });
}

// ── POST /api/auth/login-qr { deviceId, token } ────────────────────
// Phone-side step of the Login QR flow. No auth required — the
// short-lived token IS the credential. Server validates token is
// not expired, not consumed, matches the deviceId, then marks it
// consumed and issues a UserToken + cookie for the home.
export async function postLoginQr(
  env: Env, body: unknown, secret: string,
): Promise<Response> {
  const b = (body ?? {}) as { deviceId?: string; token?: string };
  if (typeof b.deviceId !== "string" || typeof b.token !== "string") {
    return json({ error: "deviceId and token required" }, { status: 400 });
  }
  const row = await env.DB.prepare(
    `SELECT home_hid, expires_at, consumed_at, device_id
     FROM login_qr_tokens WHERE token = ?`,
  ).bind(b.token).first<{
    home_hid: string; expires_at: number;
    consumed_at: number | null; device_id: string;
  }>();
  if (!row) return json({ error: "unknown token" }, { status: 404 });
  if (row.device_id !== b.deviceId) {
    return json({ error: "deviceId mismatch" }, { status: 403 });
  }
  if (row.consumed_at !== null) {
    return json({ error: "token already consumed" }, { status: 410 });
  }
  const now = Math.floor(Date.now() / 1000);
  if (row.expires_at <= now) {
    return json({ error: "token expired (60s TTL)" }, { status: 410 });
  }

  await env.DB.prepare(
    "UPDATE login_qr_tokens SET consumed_at = ? WHERE token = ?",
  ).bind(now, b.token).run();

  const token = await issueUserToken(row.home_hid, secret);
  console.log(`[login-qr] consumed token for device '${b.deviceId}' → home '${row.home_hid}'`);
  return json(
    { token, hid: row.home_hid },
    { status: 200 },
    buildSessionCookie(token),
  );
}

// ── POST /api/auth/set-pin { pin } (UserToken) ─────────────────────
// Promotes a transparent home to PIN-protected. Refuses to overwrite
// an existing PIN — change-PIN is a separate operation that's not in
// scope here (it would require verifying the OLD PIN first). 4-char
// minimum matches /api/auth/setup.
export async function postSetPin(
  env: Env, authed: Extract<AuthInfo, { type: "user" }>, body: unknown,
): Promise<Response> {
  const b = (body ?? {}) as { pin?: string };
  if (typeof b.pin !== "string" || b.pin.length < 4) {
    return json({ error: "pin too short (min 4)" }, { status: 400 });
  }
  const row = await env.DB.prepare(
    "SELECT pin_salt FROM households WHERE hid = ?",
  ).bind(authed.hid).first<{ pin_salt: string }>();
  if (!row) return json({ error: "no such home" }, { status: 404 });
  if (row.pin_salt && row.pin_salt.length > 0) {
    return json({ error: "PIN already set; use change-pin (not yet implemented)" },
                { status: 409 });
  }

  const { salt, hash } = await hashPin(b.pin);
  await env.DB.prepare(
    `UPDATE households SET pin_salt = ?, pin_hash = ?,
       updated_at = ? WHERE hid = ?`,
  ).bind(salt, hash, Math.floor(Date.now() / 1000), authed.hid).run();

  console.log(`[set-pin] home '${authed.hid}' upgraded transparent → PIN-protected`);
  return json({ ok: true });
}
