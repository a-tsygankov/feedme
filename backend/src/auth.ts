// PIN-based home authentication + dual-token issuance for the
// web app (UserToken) and paired devices (DeviceToken).
//
// PIN handling:
//   - Hash:    PBKDF2-SHA256, 100k iterations, 32-byte derived key.
//   - Salt:    16 bytes per home, generated at setup time.
//   - Format:  hex strings stored in households.pin_salt + .pin_hash.
//
// Session tokens:
//   - Format:  base64url(payload).base64url(signature)
//   - Payload: JSON {type, hid, exp [, deviceId]}
//   - Signature: HMAC-SHA256 of the base64url payload with AUTH_SECRET.
//   - TTL by type:
//       UserToken    — 30 days (web sessions; cookie-stored on the
//                       phone, refreshed on every authed call past
//                       half-life)
//       DeviceToken  — 365 days (paired-device long-lived auth;
//                       device unpaires either via the device's
//                       Reset action or webapp's Forget Device,
//                       both of which invalidate the row server-side
//                       so the long TTL doesn't leak access)
//
// AUTH_SECRET comes from a Worker secret (`wrangler secret put AUTH_SECRET`).
// In local dev (`wrangler dev`) it's read from .dev.vars; if missing we
// fall back to a hard-coded dev string with a loud warning so we don't
// silently sign with empty bytes in production.

const PBKDF2_ITERATIONS = 100_000;
const PBKDF2_KEY_BITS   = 256;
const SALT_BYTES        = 16;

// Per-token-type TTLs in seconds.
const USER_TOKEN_TTL_SEC   =  30 * 24 * 60 * 60;   // 30 days
const DEVICE_TOKEN_TTL_SEC = 365 * 24 * 60 * 60;   // 1 year

const enc = new TextEncoder();

// ── hex / base64url helpers ──────────────────────────────────────
const toHex = (buf: ArrayBuffer | Uint8Array): string =>
  Array.from(buf instanceof Uint8Array ? buf : new Uint8Array(buf),
             b => b.toString(16).padStart(2, "0")).join("");

const fromHex = (hex: string): Uint8Array => {
  const out = new Uint8Array(hex.length / 2);
  for (let i = 0; i < out.length; i++) out[i] = parseInt(hex.substr(i * 2, 2), 16);
  return out;
};

const b64urlEncode = (s: string): string => {
  // btoa is available in Workers. Convert to URL-safe variant.
  return btoa(s).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, "");
};

const b64urlDecode = (s: string): string => {
  s = s.replace(/-/g, "+").replace(/_/g, "/");
  while (s.length % 4) s += "=";
  return atob(s);
};

const constantTimeEqual = (a: string, b: string): boolean => {
  if (a.length !== b.length) return false;
  let diff = 0;
  for (let i = 0; i < a.length; i++) diff |= a.charCodeAt(i) ^ b.charCodeAt(i);
  return diff === 0;
};

// ── Transparent-home sentinel ────────────────────────────────────
//
// Phase F homes created via /api/auth/quick-setup have empty
// pin_salt + pin_hash strings — the in-band sentinel for "no PIN
// required". PIN-protected homes have a 32-hex salt + 64-hex hash.
// Centralised here so handlers don't disagree on which column to
// inspect (early Phase F shipped with three subtly-different checks
// across index.ts + transparent.ts; one of them might have stayed
// behind if a future migration ever wrote one column but not the
// other after a partial repair).
export interface PinFields {
  pin_salt: string | null;
  pin_hash: string | null;
}
export function isTransparentHome(row: PinFields | null | undefined): boolean {
  if (!row) return false;
  // EITHER column empty → transparent. Defensive against partial
  // writes (e.g. a future migration that updated pin_salt but
  // crashed before pin_hash). Treating "half-set" as transparent
  // lets set-pin recover — the alternative (treat as PIN-protected)
  // would lock the user out, since /api/auth/login's verifyPin
  // would compare against an empty salt and always fail.
  const saltEmpty = !row.pin_salt || row.pin_salt.length === 0;
  const hashEmpty = !row.pin_hash || row.pin_hash.length === 0;
  return saltEmpty || hashEmpty;
}

// ── PIN hashing ──────────────────────────────────────────────────
export async function hashPin(pin: string): Promise<{salt: string; hash: string}> {
  const saltBytes = crypto.getRandomValues(new Uint8Array(SALT_BYTES));
  const salt = toHex(saltBytes);
  const hash = await derivePin(pin, salt);
  return { salt, hash };
}

export async function verifyPin(pin: string, salt: string, expected: string): Promise<boolean> {
  const got = await derivePin(pin, salt);
  return constantTimeEqual(got, expected);
}

async function derivePin(pin: string, saltHex: string): Promise<string> {
  const keyMaterial = await crypto.subtle.importKey(
    "raw", enc.encode(pin), "PBKDF2", false, ["deriveBits"],
  );
  const bits = await crypto.subtle.deriveBits(
    {
      name: "PBKDF2",
      salt: fromHex(saltHex),
      iterations: PBKDF2_ITERATIONS,
      hash: "SHA-256",
    },
    keyMaterial,
    PBKDF2_KEY_BITS,
  );
  return toHex(bits);
}

// ── Session tokens ───────────────────────────────────────────────
//
// Discriminated union by `type`. Defensive consumers should switch
// on `type` and assert the discriminant before using deviceId.
export type TokenType = "user" | "device";

export interface UserTokenPayload {
  type: "user";
  hid:  string;
  exp:  number;     // unix seconds
}

export interface DeviceTokenPayload {
  type:     "device";
  hid:      string;
  deviceId: string;
  exp:      number;
}

export type TokenPayload = UserTokenPayload | DeviceTokenPayload;

// AuthInfo is what handler code consumes after authFromRequest.
// type stays in the union so callers can require a specific role
// (e.g. "must be a UserToken to confirm a pairing").
export type AuthInfo =
  | { type: "user";   hid: string;                 }
  | { type: "device"; hid: string; deviceId: string };

async function hmacKey(secret: string): Promise<CryptoKey> {
  return crypto.subtle.importKey(
    "raw", enc.encode(secret),
    { name: "HMAC", hash: "SHA-256" },
    false, ["sign", "verify"],
  );
}

async function signPayload(payload: TokenPayload, secret: string): Promise<string> {
  const payloadEnc = b64urlEncode(JSON.stringify(payload));
  const key = await hmacKey(secret);
  const sigBuf = await crypto.subtle.sign("HMAC", key, enc.encode(payloadEnc));
  const sigBin = String.fromCharCode(...new Uint8Array(sigBuf));
  return `${payloadEnc}.${b64urlEncode(sigBin)}`;
}

// Issue a UserToken — 30-day TTL, what the web/phone app gets after
// /api/auth/login or /api/auth/setup. The home is identified by `hid`;
// no device association.
export async function issueUserToken(hid: string, secret: string): Promise<string> {
  return signPayload(
    { type: "user", hid, exp: Math.floor(Date.now() / 1000) + USER_TOKEN_TTL_SEC },
    secret,
  );
}

// Issue a DeviceToken — 365-day TTL, what a paired device gets after
// the webapp side of pairing confirms it. Carries both home + device
// id so /api/sync can assert the device is allowed to act for this
// home without an extra DB lookup on every call.
export async function issueDeviceToken(
  hid: string, deviceId: string, secret: string,
): Promise<string> {
  return signPayload(
    { type: "device", hid, deviceId,
      exp: Math.floor(Date.now() / 1000) + DEVICE_TOKEN_TTL_SEC },
    secret,
  );
}

// Back-compat alias — old call sites used `issueToken(hid, secret)`
// implicitly meaning a UserToken. Keep the same signature so the
// rest of index.ts doesn't churn during the dual-token rollout.
export const issueToken = issueUserToken;

// Verify + decode any token. Discriminant `type` is preserved so
// callers can require a specific role.
export async function verifyToken(token: string, secret: string): Promise<TokenPayload | null> {
  const parts = token.split(".");
  if (parts.length !== 2) return null;
  const [payloadEnc, sigEnc] = parts;

  const key = await hmacKey(secret);
  const sigBin = b64urlDecode(sigEnc);
  const sigBuf = new Uint8Array(sigBin.length);
  for (let i = 0; i < sigBin.length; i++) sigBuf[i] = sigBin.charCodeAt(i);
  const ok = await crypto.subtle.verify("HMAC", key, sigBuf, enc.encode(payloadEnc));
  if (!ok) return null;

  let payload: unknown;
  try {
    payload = JSON.parse(b64urlDecode(payloadEnc));
  } catch {
    return null;
  }
  if (!isTokenPayload(payload)) return null;
  if (payload.exp < Math.floor(Date.now() / 1000)) return null;
  return payload;
}

function isTokenPayload(x: unknown): x is TokenPayload {
  if (typeof x !== "object" || x === null) return false;
  const o = x as Record<string, unknown>;
  if (typeof o.hid !== "string" || typeof o.exp !== "number") return false;
  if (o.type === "user")   return true;
  if (o.type === "device") return typeof o.deviceId === "string";
  // Legacy tokens (pre-dual-token rollout) had no `type` field — they
  // were all UserTokens. Treat them as such for backwards-compat;
  // the next /login call rotates them into the typed format.
  if (o.type === undefined) {
    (o as Record<string, unknown>).type = "user";
    return true;
  }
  return false;
}

// Phase F — read the session cookie set on auth responses. Cookies
// are sent automatically by the browser with every same-origin
// request (the Pages Function proxies /api/* to the Worker, so
// "same origin" includes our Worker). Falls back to nothing when
// the cookie isn't present; the Authorization header path still
// works for non-browser clients (firmware, curl).
const SESSION_COOKIE_NAME = "feedme.session";
function readSessionCookie(req: Request): string | null {
  const header = req.headers.get("cookie") ?? "";
  // Tiny parser — RFC 6265 grammar is more permissive than this
  // covers but we only ever set our own one cookie, so a single-name
  // lookup is enough. Bail on the first match.
  for (const pair of header.split(";")) {
    const eq = pair.indexOf("=");
    if (eq < 0) continue;
    const name = pair.slice(0, eq).trim();
    if (name !== SESSION_COOKIE_NAME) continue;
    return pair.slice(eq + 1).trim();
  }
  return null;
}

// Build the Set-Cookie header value for a freshly-issued UserToken.
// HttpOnly so XSS can't read it; Secure so it's only sent over HTTPS;
// SameSite=Lax so cross-site GETs (e.g. user clicking a link) carry
// it but cross-site POSTs don't (CSRF defence). Max-Age = 30 days
// matches the UserToken TTL.
export function buildSessionCookie(token: string): string {
  return `${SESSION_COOKIE_NAME}=${token}; HttpOnly; Secure; SameSite=Lax; Path=/; Max-Age=2592000`;
}
export function buildClearSessionCookie(): string {
  return `${SESSION_COOKIE_NAME}=; HttpOnly; Secure; SameSite=Lax; Path=/; Max-Age=0`;
}

// Convenience: pull Authorization: Bearer <token>, OR the session
// cookie, verify, return the AuthInfo (type + hid [+ deviceId]).
// Null if missing / invalid / expired.
export async function authFromRequest(
  req: Request, secret: string,
): Promise<AuthInfo | null> {
  const header = req.headers.get("authorization") ?? "";
  let token = "";
  if (header.startsWith("Bearer ")) {
    token = header.slice("Bearer ".length).trim();
  } else {
    // Fall back to the session cookie — set by every auth endpoint
    // since Phase F. Browsers send it automatically; the Authorization
    // header path stays the canonical one for the firmware + curl.
    token = readSessionCookie(req) ?? "";
  }
  if (!token) return null;
  const payload = await verifyToken(token, secret);
  if (!payload) return null;
  if (payload.type === "device") {
    return { type: "device", hid: payload.hid, deviceId: payload.deviceId };
  }
  return { type: "user", hid: payload.hid };
}

// Helper for handlers that require a specific token type (e.g.
// /api/sync requires a DeviceToken; /api/auth/me requires a UserToken).
// Returns the AuthInfo if it matches, null otherwise.
export function requireType<T extends AuthInfo["type"]>(
  authed: AuthInfo | null, type: T,
): Extract<AuthInfo, { type: T }> | null {
  if (!authed || authed.type !== type) return null;
  return authed as Extract<AuthInfo, { type: T }>;
}
