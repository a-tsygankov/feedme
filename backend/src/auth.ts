// PIN-based household authentication for the web/phone app.
//
// PIN handling:
//   - Hash:    PBKDF2-SHA256, 100k iterations, 32-byte derived key.
//   - Salt:    16 bytes per household, generated at setup time.
//   - Format:  hex strings stored in households.pin_salt + .pin_hash.
//
// Session tokens:
//   - Format:  base64url(payload).base64url(signature)
//   - Payload: JSON {hid, exp} — exp is unix seconds.
//   - Signature: HMAC-SHA256 of the base64url payload with AUTH_SECRET.
//   - TTL:     30 days. The web app re-issues on every successful API call
//              when the token is past half-life (sliding session).
//
// AUTH_SECRET comes from a Worker secret (`wrangler secret put AUTH_SECRET`).
// In local dev (`wrangler dev`) it's read from .dev.vars; if missing we
// fall back to a hard-coded dev string with a loud warning so we don't
// silently sign with empty bytes in production.

const PBKDF2_ITERATIONS = 100_000;
const PBKDF2_KEY_BITS   = 256;
const SALT_BYTES        = 16;
const TOKEN_TTL_SEC     = 30 * 24 * 60 * 60;   // 30 days

const enc = new TextEncoder();
const dec = new TextDecoder();

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
export interface TokenPayload {
  hid: string;
  exp: number;   // unix seconds
}

async function hmacKey(secret: string): Promise<CryptoKey> {
  return crypto.subtle.importKey(
    "raw", enc.encode(secret),
    { name: "HMAC", hash: "SHA-256" },
    false, ["sign", "verify"],
  );
}

export async function issueToken(hid: string, secret: string): Promise<string> {
  const payload: TokenPayload = {
    hid,
    exp: Math.floor(Date.now() / 1000) + TOKEN_TTL_SEC,
  };
  const payloadEnc = b64urlEncode(JSON.stringify(payload));
  const key = await hmacKey(secret);
  const sigBuf = await crypto.subtle.sign("HMAC", key, enc.encode(payloadEnc));
  // base64url-encode the raw signature bytes (binary string → btoa).
  const sigBin = String.fromCharCode(...new Uint8Array(sigBuf));
  const sigEnc = b64urlEncode(sigBin);
  return `${payloadEnc}.${sigEnc}`;
}

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

  let payload: TokenPayload;
  try {
    payload = JSON.parse(b64urlDecode(payloadEnc));
  } catch {
    return null;
  }
  if (typeof payload.hid !== "string" || typeof payload.exp !== "number") return null;
  if (payload.exp < Math.floor(Date.now() / 1000)) return null;
  return payload;
}

// Convenience: pull Authorization: Bearer <token>, verify, return the
// payload. Null if missing / invalid / expired.
export async function authFromRequest(req: Request, secret: string): Promise<TokenPayload | null> {
  const auth = req.headers.get("authorization") ?? "";
  if (!auth.startsWith("Bearer ")) return null;
  const token = auth.slice("Bearer ".length).trim();
  if (!token) return null;
  return verifyToken(token, secret);
}
