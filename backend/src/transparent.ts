// Transparent accounts + Login QR + set-PIN — Phase F.
//
// Three flavours of "auth without typing a PIN":
//
//   1. Quick-setup (POST /api/auth/quick-setup):
//      Creates a TRANSPARENT home — no name to pick, no PIN to choose.
//      Generates an opaque hid like "home-{16 hex chars}" and inserts
//      the household with empty pin_salt/pin_hash (the in-band sentinel
//      for "no PIN required"). Atomically claims the device that
//      scanned the QR — including writing the DeviceToken into the
//      pending_pairings row, so the firmware's next /pair/check returns
//      confirmed and there's NO follow-up confirm-banner click on the
//      dashboard. Designed for the user who wants the device working
//      in 30 s without thinking about identity. Cookie set.
//
//   2. Login QR (POST /api/auth/login-token-create + login-qr):
//      An ALREADY-PAIRED device shows a one-shot QR encoding a
//      short-lived (60 s) token. A phone scans → /qr-login route on
//      the webapp → POST /api/auth/login-qr exchanges the token for
//      a UserToken + cookie. Lets a phone re-sign-in to a paired home
//      without retyping anything (and without the user even knowing
//      the home name). Both endpoints gate on an active pairings row
//      (is_deleted = 0), so a forgotten device's still-valid
//      DeviceToken can't mint phone-login QRs into its former home.
//
//   3. Set-PIN (POST /api/auth/set-pin):
//      Promotes a transparent account to PIN-protected. Once a PIN is
//      set, the home participates in the regular /api/auth/login flow.
//      Demotion (drop the PIN) is not supported — once protected,
//      forever protected.
//
// Tokens minted in (2) live in the login_qr_tokens table from
// migration 0010. Backfill: empty pin_salt/pin_hash strings on
// pre-Phase-F homes mean "transparent (no PIN)"; existing rows
// before Phase F all have a real 32-hex salt + 64-hex hash so the
// sentinel is unambiguous. Use isTransparentHome() from auth.ts —
// don't open-code the check.

import {
  hashPin, isTransparentHome, issueUserToken,
  buildSessionCookie, type AuthInfo,
} from "./auth";
import type { Env } from "./env";
import { confirmPairingFor } from "./pair";

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
// The deviceId binds the new home to the device that scanned.
//
// Single-shot pairing: in addition to creating the household + claiming
// the device, this also writes the DeviceToken back into the device's
// pending_pairings row so the firmware's next /api/pair/check returns
// confirmed. The user does NOT need to click a confirm-pairing banner
// on the dashboard (Phase F regression: prior implementation did
// require that, and would silently leave the device in pairing-poll
// limbo if its 3-min /pair/start window had expired between QR-scan
// and Quick-Start tap). Refuses with 409 when the pending_pairings
// row is missing / cancelled / past its expires_at, so the user is
// told to re-tap Pair on the device rather than getting a half-paired
// state.
//
// Idempotency: if the device is already in an active pairings row
// (e.g. user tapped Quick-Start twice), reuse the existing home and
// issue a fresh UserToken without minting a second household.
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

  // Pre-check: validate pending_pairings is fresh BEFORE we create
  // any rows. Avoids leaving an orphan household behind when the
  // device's pair window expired between QR-scan and submit. Same
  // queries as confirmPairingFor's first step (it re-validates as
  // defence in depth), but bailing here means no rollback to do.
  const now = Math.floor(Date.now() / 1000);
  const pending = await env.DB.prepare(
    `SELECT expires_at, cancelled_at, confirmed_at
     FROM pending_pairings WHERE device_id = ?`,
  ).bind(deviceId).first<{
    expires_at: number; cancelled_at: number | null; confirmed_at: number | null;
  }>();
  if (!pending) {
    return json(
      { error: "device hasn't requested pairing — tap Pair on the device first" },
      { status: 409 },
    );
  }
  if (pending.cancelled_at) {
    return json(
      { error: "device cancelled pairing — re-tap Pair on the device" },
      { status: 410 },
    );
  }
  if (pending.expires_at <= now && !pending.confirmed_at) {
    return json(
      { error: "device's pairing window expired — re-tap Pair on the device" },
      { status: 410 },
    );
  }

  // Fresh transparent home: generate hid, insert with empty
  // pin_salt/pin_hash (the "no PIN" sentinel), then complete pairing
  // inline via confirmPairingFor so the device's next /pair/check
  // returns confirmed without any further user interaction.
  const hid = QUICK_SETUP_PREFIX + randomHex(QUICK_SETUP_RAND_HEX);

  try {
    await env.DB.prepare(
      `INSERT INTO households
         (hid, pin_salt, pin_hash, created_at, name, updated_at, is_deleted)
       VALUES (?, '', '', ?, '', ?, 0)`,
    ).bind(hid, now, now).run();
  } catch (e) {
    const msg = e instanceof Error ? e.message : String(e);
    console.error(`[quick-setup] household insert failed: ${msg}`);
    return json({ error: `quick-setup failed: ${msg}` }, { status: 500 });
  }

  // Defence-in-depth: helper re-validates pending_pairings (could
  // race with /api/pair/cancel) and does the full claim work.
  const pair = await confirmPairingFor(env, hid, deviceId, secret);
  if (!pair.ok) {
    console.warn(`[quick-setup] '${hid}' created but pairing failed (race?): ${pair.error}`);
    return json({ error: pair.error }, { status: pair.status });
  }

  const userToken = await issueUserToken(hid, secret);
  console.log(`[quick-setup] created transparent home '${hid}' for device '${deviceId}' (paired in one shot)`);
  return json({ token: userToken, hid }, { status: 200 }, buildSessionCookie(userToken));
}

// ── POST /api/auth/login-token-create (DeviceToken) ────────────────
// Device-side step of the Login QR flow. Mints a 60-s random token
// bound to the device's already-paired home and returns it for the
// firmware to encode in a QR. Idempotency: each call mints a NEW
// token (no dedup) — the prior one stays valid until expiry but the
// device discards it. Replay-protection comes from single-use
// consumption in /login-qr.
//
// PAIRING REVOCATION GATE: a DeviceToken is HMAC-signed with a 365-day
// TTL and there's no token-revocation list, so a forgotten device
// keeps verifying after the user clicks "Forget device" in Settings.
// Without this gate, that forgotten device could still mint phone-
// login QRs that grant strangers session access into the home that
// kicked it out. We refuse here when there's no active pairings row
// for (deviceId, hid).
export async function postLoginTokenCreate(
  env: Env, authed: Extract<AuthInfo, { type: "device" }>,
): Promise<Response> {
  const pair = await env.DB.prepare(
    `SELECT id FROM pairings
     WHERE device_id = ? AND home_hid = ? AND is_deleted = 0
     ORDER BY id DESC LIMIT 1`,
  ).bind(authed.deviceId, authed.hid).first<{ id: number }>();
  if (!pair) {
    console.warn(`[login-token-create] revoked-device attempt: device='${authed.deviceId}' hid='${authed.hid}'`);
    return json(
      { error: "device pairing has been revoked; re-pair from H menu" },
      { status: 401 },
    );
  }

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
//
// Single-use is enforced ATOMICALLY via the WHERE clause on the
// UPDATE. The earlier check-then-update sequence had a race where
// two concurrent /login-qr calls with the same token both passed
// the consumed_at IS NULL check and both wrote consumed_at, both
// returning UserTokens. The window was small but real — D1 calls
// are remote and concurrent requests for the same token aren't
// hypothetical. By making the UPDATE itself the source of truth
// (and reading meta.changes to detect the loser), we get true
// once-and-only-once semantics without a transaction.
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
  // deviceId-mismatch is checked BEFORE the consume attempt so a
  // probing attacker who somehow learned a token can't trip the
  // single-use lock by guessing wrong deviceIds (the legit phone
  // would then get "already consumed" instead of being able to use
  // its valid token).
  if (row.device_id !== b.deviceId) {
    return json({ error: "deviceId mismatch" }, { status: 403 });
  }
  const now = Math.floor(Date.now() / 1000);
  if (row.expires_at <= now) {
    return json({ error: "token expired (60s TTL)" }, { status: 410 });
  }
  if (row.consumed_at !== null) {
    // Cheap pre-check before attempting the UPDATE. Doesn't change
    // correctness (the conditional UPDATE would catch this anyway)
    // but saves the round-trip when the token's been consumed
    // long enough that a second caller is just retrying.
    return json({ error: "token already consumed" }, { status: 410 });
  }

  // Conditional UPDATE: only proceeds if consumed_at IS NULL right
  // now. If two concurrent calls reach here, exactly one's UPDATE
  // returns changes=1 and the other returns changes=0.
  const res = await env.DB.prepare(
    "UPDATE login_qr_tokens SET consumed_at = ? WHERE token = ? AND consumed_at IS NULL",
  ).bind(now, b.token).run();
  if ((res.meta.changes ?? 0) === 0) {
    return json({ error: "token already consumed" }, { status: 410 });
  }

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
    "SELECT pin_salt, pin_hash FROM households WHERE hid = ?",
  ).bind(authed.hid).first<{ pin_salt: string; pin_hash: string }>();
  if (!row) return json({ error: "no such home" }, { status: 404 });
  if (!isTransparentHome(row)) {
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
