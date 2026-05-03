// Tiny fetch wrapper for the FeedMe Worker API. Reads the bearer
// token from localStorage on every call (so a logout in one tab is
// honoured by sibling tabs on the next call) and surfaces 401 as a
// typed error so the router can bounce the user back to /login.

const STORAGE_TOKEN = "feedme.token";
const STORAGE_HID   = "feedme.hid";

// In dev (`vite dev`) the proxy in vite.config.ts targets the Worker;
// for built artifacts hosted somewhere other than the same origin,
// override at build time via VITE_API_BASE=https://feedme.workers.dev.
const API_BASE = (import.meta.env.VITE_API_BASE as string | undefined) ?? "";

export class ApiError extends Error {
  status: number;
  constructor(status: number, message: string) {
    super(message);
    this.name = "ApiError";
    this.status = status;
  }
}

// Token shape mirrors backend/src/auth.ts → TokenPayload. The token
// itself is `base64url(JSON{hid,exp}).base64url(hmacSig)`. We never
// verify the signature client-side (the server does that on every
// request) — we just peek at hid + exp to make routing decisions
// without round-tripping. A tampered payload would fail server-side
// verification on the first authed call and bounce the user back
// to /login, so the worst case is "user clicks something and gets
// re-prompted", not auth bypass.
interface TokenPayload {
  hid: string;
  exp: number;   // unix seconds
}

function decodeTokenPayload(token: string): TokenPayload | null {
  try {
    const [payloadB64] = token.split(".");
    if (!payloadB64) return null;
    // base64url → standard base64 for atob.
    let b64 = payloadB64.replace(/-/g, "+").replace(/_/g, "/");
    while (b64.length % 4) b64 += "=";
    const json = atob(b64);
    const obj = JSON.parse(json);
    if (typeof obj?.hid !== "string" || typeof obj?.exp !== "number") return null;
    return obj as TokenPayload;
  } catch {
    return null;
  }
}

export const auth = {
  token(): string | null { return localStorage.getItem(STORAGE_TOKEN); },
  hid():   string | null { return localStorage.getItem(STORAGE_HID); },
  set(token: string, hid: string) {
    localStorage.setItem(STORAGE_TOKEN, token);
    localStorage.setItem(STORAGE_HID, hid);
  },
  clear() {
    localStorage.removeItem(STORAGE_TOKEN);
    localStorage.removeItem(STORAGE_HID);
  },

  // Returns the decoded payload if a token is stored AND not expired.
  // Returns null otherwise (no token / malformed / past exp). Does
  // NOT verify the HMAC — the server does that on every API call.
  // Used by routing logic that wants to skip the login dance when the
  // user already has a live session.
  validPayload(): TokenPayload | null {
    const t = this.token();
    if (!t) return null;
    const p = decodeTokenPayload(t);
    if (!p) return null;
    if (p.exp <= Math.floor(Date.now() / 1000)) return null;
    return p;
  },
};

interface ApiOpts {
  method?: "GET" | "POST" | "PATCH" | "DELETE";
  body?:   unknown;
}

async function apiRaw<T>(path: string, opts: ApiOpts = {}): Promise<T> {
  const headers: Record<string, string> = {
    "content-type": "application/json",
  };
  const token = auth.token();
  if (token) headers["authorization"] = `Bearer ${token}`;

  const res = await fetch(`${API_BASE}${path}`, {
    method: opts.method ?? "GET",
    headers,
    body: opts.body !== undefined ? JSON.stringify(opts.body) : undefined,
  });

  let payload: unknown = null;
  const text = await res.text();
  if (text) {
    try { payload = JSON.parse(text); } catch { payload = { raw: text }; }
  }

  if (!res.ok) {
    const msg = (payload as { error?: string } | null)?.error ?? `HTTP ${res.status}`;
    throw new ApiError(res.status, msg);
  }
  return payload as T;
}

// ── Domain types — match the JSON shapes from src/cats.ts etc. ────
export interface Cat {
  slotId: number;
  name: string;
  color: number;            // 0xRRGGBB; 0 = "auto" (let backend pick on save)
  slug: string;
  defaultPortionG: number;
  hungryThresholdSec: number;
}

export interface User {
  slotId: number;
  name: string;
  color: number;
}

// One row in /api/dashboard/cats — static cat fields plus the live
// per-cat state used to drive the mood ring + status line.
export interface DashboardCat extends Cat {
  lastFedAt:     number | null;   // unix sec; null = never fed
  lastFedBy:     string | null;
  lastEventType: "feed" | "snooze" | string | null;
  secondsSince:  number | null;   // computed server-side from `now`
  todayCount:    number;          // feed events in local "today"
}

// One row in /api/dashboard/history.
export interface HistoryEvent {
  id:    number;
  ts:    number;
  type:  string;       // "feed" | "snooze" | …
  by:    string;
  note:  string | null;
  cat:   string;       // stringified slot_id; "primary" for legacy
}

// One row in /api/sync/log — drives the SyncLogPage.
export interface SyncLogEntry {
  id:             number;
  device_id:      string;
  ts:             number;
  result:         "ok" | "error" | "cancelled" | string;
  error_message:  string | null;
  entities_in:    number;
  entities_out:   number;
  conflicts:      number;
  duration_ms:    number;
}

// One row in /api/pair/list — drives Settings → Devices.
export interface PairedDevice {
  deviceId:  string;
  createdAt: number;
  updatedAt: number;
}

// ── Auth ──────────────────────────────────────────────────────────
export interface HomeInfo {
  hid: string;          // the home name — its unique identifier
  created_at: number;   // unix seconds
  deviceCount: number;  // how many physical devices have claimed in
  hasPin: boolean;      // false → transparent (Phase F quick-setup) home
}

// Both setup and login optionally take a `deviceId` to claim — the
// firmware-generated id from the QR (`feedme-{mac6}`). When provided,
// the backend writes a row in `devices` mapping device_id → home_hid
// so subsequent /api/feed calls from that device land in this home.
export const api = {
  exists: (hid: string) =>
    apiRaw<{ exists: boolean }>("/api/auth/exists", { method: "POST", body: { hid } }),
  // Creates a NEW home named `hid` (validated server-side: 1-64 chars,
  // no control chars, must be unique). On success returns a session
  // token bound to the same hid; caller should auth.set + navigate.
  // 409 → name taken (caller should redirect to login with hid prefilled).
  setup: (hid: string, pin: string, deviceId?: string) =>
    apiRaw<{ token: string; hid: string }>(
      "/api/auth/setup",
      { method: "POST", body: { hid, pin, deviceId } },
    ),
  // Authenticates against an EXISTING home. Same shape as setup +
  // 401 (wrong PIN) / 404 (no such home).
  login: (hid: string, pin: string, deviceId?: string) =>
    apiRaw<{ token: string; hid: string }>(
      "/api/auth/login",
      { method: "POST", body: { hid, pin, deviceId } },
    ),
  // Returns the signed-in home's metadata. Used by HomePage /
  // SettingsPage to render the title + device count.
  me: () => apiRaw<HomeInfo>("/api/auth/me"),

  // ── Phase F: transparent accounts + Login QR + set-PIN ────────
  // Quick-setup: create a new transparent (no-PIN) home in one shot.
  // The backend mints an opaque "home-{16hex}" hid for us. The deviceId
  // is mandatory — the whole point is to get the user's just-scanned
  // QR working with zero typing. Returns { token, hid, alreadyPaired? }.
  // alreadyPaired=true when the device was already in a home (the
  // backend short-circuits and returns the existing home + a fresh
  // token instead of creating a duplicate).
  quickSetup: (deviceId: string) =>
    apiRaw<{ token: string; hid: string; alreadyPaired?: boolean }>(
      "/api/auth/quick-setup", { method: "POST", body: { deviceId } },
    ),
  // Login-QR exchange: phone scans a QR shown by an already-paired
  // device, hits this endpoint with { deviceId, token }, and gets a
  // session token + cookie back. Server-side replay-protected via
  // single-use consumption. 404 token, 403 deviceId mismatch, 410
  // expired or already-consumed.
  loginQr: (deviceId: string, token: string) =>
    apiRaw<{ token: string; hid: string }>(
      "/api/auth/login-qr", { method: "POST", body: { deviceId, token } },
    ),
  // Promote the signed-in (transparent) home to PIN-protected. 409
  // if the home already has a PIN — change-PIN is a separate op.
  setPin: (pin: string) =>
    apiRaw<{ ok: true }>(
      "/api/auth/set-pin", { method: "POST", body: { pin } },
    ),

  // ── Pair lifecycle (Phase A, dev-23) ──────────────────────────
  // After auth, the webapp shows a "Confirm pairing" CTA when it
  // arrived via /setup?device=… or /login?device=… (carried through
  // to / as ?pair=…). Clicking the CTA calls this; on success the
  // device's polling /api/pair/check returns confirmed + token.
  // Returns { ok, deviceId, hid } or throws ApiError on non-2xx
  // (404 = device not in pairing window, 410 = device cancelled).
  pairConfirm: (deviceId: string) =>
    apiRaw<{ ok: true; deviceId: string; hid: string }>(
      "/api/pair/confirm", { method: "POST", body: { deviceId } },
    ),
  // Webapp-side "Forget device" — soft-deletes the active pairings
  // row + clears the legacy devices row so the device's events
  // stop landing in this home. The device itself isn't notified;
  // its DeviceToken is now orphaned and will start failing /api/sync.
  pairForget: (deviceId: string) =>
    apiRaw<{ ok: true; deviceId: string }>(
      `/api/pair/${encodeURIComponent(deviceId)}`, { method: "DELETE" },
    ),
  // Active pairings for the signed-in home. Used by Settings →
  // Devices card to render one row per device with a Forget button.
  pairList: () =>
    apiRaw<{ devices: PairedDevice[] }>("/api/pair/list"),

  // ── Sync log (Phase B, dev-24) ────────────────────────────────
  // Read-only audit list capped at 100/home. Filter by device with
  // ?device=<id> to narrow to a single unit's history.
  syncLogList: (deviceId?: string, n = 50) => {
    const params = new URLSearchParams({ n: String(n) });
    if (deviceId) params.set("device", deviceId);
    return apiRaw<{ entries: SyncLogEntry[] }>(`/api/sync/log?${params.toString()}`);
  },
  // Wipes the home + all per-home records (cats, users) for the
  // currently authenticated session. The backend route + db column are
  // still spelled "household" — that's the on-the-wire identifier and
  // changing it would be a breaking schema migration. UI strings show
  // "Home"; the API path stays /api/auth/household for compatibility.
  // Caller is responsible for clearing localStorage + redirecting to
  // /login afterwards.
  forgetHousehold: () =>
    apiRaw<{ ok: true; hid: string }>("/api/auth/household", { method: "DELETE" }),

  // Cats CRUD.
  catsList:   ()                                          => apiRaw<{ cats:  Cat[]  }>("/api/cats"),
  catsCreate: (body: Partial<Cat>)                        => apiRaw<{ cat:   Cat   }>("/api/cats",            { method: "POST",   body }),
  catsUpdate: (slotId: number, patch: Partial<Cat>)       => apiRaw<{ cat:   Cat   }>(`/api/cats/${slotId}`,  { method: "PATCH",  body: patch }),
  catsDelete: (slotId: number)                            => apiRaw<{ ok:    true  }>(`/api/cats/${slotId}`,  { method: "DELETE" }),

  // ── Dashboard ──────────────────────────────────────────────────
  // Per-cat live state (last event + today count + the static cat
  // fields). One round-trip drives the entire main dashboard grid.
  // tzOffsetMin defaults to the browser's current offset so "today"
  // is local-midnight to local-midnight.
  dashboardCats: (tzOffsetMin?: number) => {
    const off = tzOffsetMin ?? -new Date().getTimezoneOffset();
    return apiRaw<{ now: number; tzOffsetMin: number; cats: DashboardCat[] }>(
      `/api/dashboard/cats?tzOffset=${off}`,
    );
  },
  // eventId (UUID) makes the call idempotent — a network retry where
  // the server actually processed the original is silently dropped
  // by the UNIQUE INDEX on events.event_id. Generated client-side
  // per click via crypto.randomUUID(); without it, every retry
  // creates a duplicate feeding event.
  dashboardFeed: (
    catSlotId: number, by: string,
    type: "feed" | "snooze" = "feed",
    opts?: { note?: string; eventId?: string },
  ) =>
    apiRaw<{ ok: true; ts: number; type: string; by: string; catSlotId: number; eventId: string | null }>(
      "/api/dashboard/feed",
      { method: "POST", body: { catSlotId, by, type, note: opts?.note, eventId: opts?.eventId } },
    ),
  dashboardHistory: (catSlotId?: number, n = 10) => {
    const params = new URLSearchParams();
    if (catSlotId !== undefined) params.set("cat", String(catSlotId));
    params.set("n", String(n));
    return apiRaw<{ events: HistoryEvent[] }>(
      `/api/dashboard/history?${params.toString()}`,
    );
  },

  // Users CRUD.
  usersList:   ()                                         => apiRaw<{ users: User[] }>("/api/users"),
  usersCreate: (body: Partial<User>)                      => apiRaw<{ user:  User  }>("/api/users",           { method: "POST",   body }),
  usersUpdate: (slotId: number, patch: Partial<User>)     => apiRaw<{ user:  User  }>(`/api/users/${slotId}`, { method: "PATCH",  body: patch }),
  usersDelete: (slotId: number)                           => apiRaw<{ ok:    true  }>(`/api/users/${slotId}`, { method: "DELETE" }),
};
